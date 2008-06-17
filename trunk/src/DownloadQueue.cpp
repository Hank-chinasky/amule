//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2008 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2002 Merkur ( devs@emule-project.net / http://www.emule-project.net )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//

#include "DownloadQueue.h"	// Interface declarations

#include <protocol/Protocols.h>
#include <protocol/kad/Constants.h>
#include <common/Macros.h>
#include <common/MenuIDs.h>
#include <common/Constants.h>

#include <wx/textfile.h>	// Needed for wxTextFile
#include <wx/utils.h>

#include "Server.h"		// Needed for CServer
#include "Packet.h"		// Needed for CPacket
#include "MemFile.h"		// Needed for CMemFile
#include "ClientList.h"		// Needed for CClientList
#include "updownclient.h"	// Needed for CUpDownClient
#include "ServerList.h"		// Needed for CServerList
#include "ServerConnect.h"	// Needed for CServerConnect
#include "ED2KLink.h"		// Needed for CED2KFileLink
#include "SearchList.h"		// Needed for CSearchFile
#include "SharedFileList.h"	// Needed for CSharedFileList
#include "PartFile.h"		// Needed for CPartFile
#include "Preferences.h"	// Needed for thePrefs
#include "amule.h"		// Needed for theApp
#include "AsyncDNS.h"		// Needed for CAsyncDNS
#include "Statistics.h"		// Needed for theStats
#include "Logger.h"
#include <common/Format.h>	// Needed for CFormat
#include "IPFilter.h"
#include <common/FileFunctions.h>	// Needed for CDirIterator
#include "FileLock.h"		// Needed for CFileLock
#include "GuiEvents.h"		// Needed for Notify_*
#include "UserEvents.h"
#include "MagnetURI.h"		// Needed for CMagnetED2KConverter
#include "ScopedPtr.h"		// Needed for CScopedPtr
#include "PlatformSpecific.h"	// Needed for CanFSHandleLargeFiles

#include "kademlia/kademlia/Kademlia.h"

#include <string>			// Do_not_auto_remove (mingw-gcc-3.4.5)


// Max. file IDs per UDP packet
// ----------------------------
// 576 - 30 bytes of header (28 for UDP, 2 for "E3 9A" edonkey proto) = 546 bytes
// 546 / 16 = 34


#define MAX_FILES_PER_UDP_PACKET	31	// 2+16*31 = 498 ... is still less than 512 bytes!!
#define MAX_REQUESTS_PER_SERVER		35


CDownloadQueue::CDownloadQueue()
// Needs to be recursive that that is can own an observer assigned to itself
	: m_mutex( wxMUTEX_RECURSIVE )
{
	m_datarate = 0;
	m_udpserver = 0;
	m_lastsorttime = 0;
	m_lastudpsearchtime = 0;
	m_lastudpstattime = 0;
	m_udcounter = 0;
	m_nLastED2KLinkCheck = 0;
	m_dwNextTCPSrcReq = 0;
	m_cRequestsSentToServer = 0;
	m_lastDiskCheck = 0;
	SetLastKademliaFileRequest();
}


CDownloadQueue::~CDownloadQueue()
{
	if ( !m_filelist.empty() ) {
		for ( unsigned int i = 0; i < m_filelist.size(); i++ ) {
			printf("\rSaving PartFile %u of %u", i + 1, (unsigned int)m_filelist.size());
			fflush(stdout);
			delete m_filelist[i];
		}
		printf("\nAll PartFiles Saved.\n");
	}
}


void CDownloadQueue::LoadMetFiles(const CPath& path)
{
	printf("Loading temp files from %s.\n",
		(const char *)unicode2char(path.GetPrintable()));
	
	std::vector<CPath> files;

	// Locate part-files to be loaded
	CDirIterator TempDir(path);
	CPath fileName = TempDir.GetFirstFile(CDirIterator::File, wxT("*.part.met"));
	while (fileName.IsOk()) {
		files.push_back(path.JoinPaths(fileName));

		fileName = TempDir.GetNextFile();
	}

	// Loading in order makes it easier to figure which
	// file is broken in case of crashes, or the like.
	std::sort(files.begin(), files.end());

	// Load part-files	
	for ( size_t i = 0; i < files.size(); i++ ) {
		printf("\rLoading PartFile %u of %u", (unsigned int)(i + 1), (unsigned int)files.size());
			
		fileName = files[i].GetFullName();
		
		CPartFile* toadd = new CPartFile();
		bool result = (toadd->LoadPartFile(path, fileName) != 0);
		if (!result) {
			// Try from backup
			result = (toadd->LoadPartFile(path, fileName, true) != 0);
		}
		
		if (result && !IsFileExisting(toadd->GetFileHash())) {
			{
				wxMutexLocker lock(m_mutex);
				m_filelist.push_back(toadd);
			}
		
			NotifyObservers( EventType( EventType::INSERTED, toadd ) );
			Notify_DownloadCtrlAddFile(toadd);
		} else {
			delete toadd;
			
			wxString msg;
			if (result) {
				msg << CFormat(wxT("WARNING: Duplicate partfile with hash '%s' found, skipping: %s"))
					% toadd->GetFileHash().Encode() % fileName;
			} else {
				// If result is false, then reading of both the primary and the backup .met failed
				AddLogLineM(false, 
					_("ERROR: Failed to load backup file. Search http://forum.amule.org for .part.met recovery solutions."));
				msg << CFormat(wxT("ERROR: Failed to load PartFile '%s'")) % fileName;
			}
			
			AddDebugLogLineM(true, logPartFile, msg);
			
			// Newline so that the error stays visible.
			printf(": %s\n", (const char*)unicode2char(msg));
		}
	}

	printf("\nAll PartFiles Loaded.\n");
	
	if ( GetFileCount() == 0 ) {
		AddLogLineM(false, _("No part files found"));
	} else {
		AddLogLineM(false, wxString::Format(wxPLURAL("Found %u part file", "Found %u part files", GetFileCount()), GetFileCount()) );

		DoSortByPriority();
		CheckDiskspace( path );
	}
}


uint16 CDownloadQueue::GetFileCount() const
{
	wxMutexLocker lock( m_mutex );
	
	return m_filelist.size();
}


CServer* CDownloadQueue::GetUDPServer() const
{
	wxMutexLocker lock( m_mutex );

	return m_udpserver;
}


void CDownloadQueue::SetUDPServer( CServer* server )
{
	wxMutexLocker lock( m_mutex );

	m_udpserver = server;
}

	
void CDownloadQueue::SaveSourceSeeds()
{
	for ( uint16 i = 0; i < GetFileCount(); i++ ) {
		GetFileByIndex( i )->SaveSourceSeeds();
	}
}


void CDownloadQueue::LoadSourceSeeds()
{
	for ( uint16 i = 0; i < GetFileCount(); i++ ) {
		GetFileByIndex( i )->LoadSourceSeeds();
	}
}


void CDownloadQueue::AddSearchToDownload(CSearchFile* toadd, uint8 category)
{
	if ( IsFileExisting(toadd->GetFileHash()) ) {
		return;
	}

	if (toadd->GetFileSize() > OLD_MAX_FILE_SIZE) {
		if (!PlatformSpecific::CanFSHandleLargeFiles(thePrefs::GetTempDir())) {
			AddLogLineM(true, _("Filesystem for Temp directory cannot handle large files."));
			return;
		} else if (!PlatformSpecific::CanFSHandleLargeFiles(theApp->glob_prefs->GetCatPath(category))) {
			AddLogLineM(true, _("Filesystem for Incoming directory cannot handle large files."));
			return;
		}
	}

	CPartFile* newfile = NULL;
	try {
		newfile = new CPartFile(toadd);
	} catch (const CInvalidPacket& WXUNUSED(e)) {
		AddDebugLogLineM(true, logDownloadQueue, wxT("Search-result contained invalid tags, could not add"));
	}
	
	if ( newfile && newfile->GetStatus() != PS_ERROR ) {
		AddDownload( newfile, thePrefs::AddNewFilesPaused(), category );
		// Add any possible sources
		if (toadd->GetClientID() && toadd->GetClientPort()) {
			CMemFile sources(1+4+2);
			sources.WriteUInt8(1);
			sources.WriteUInt32(toadd->GetClientID());
			sources.WriteUInt16(toadd->GetClientPort());
			sources.Reset();
			newfile->AddSources(sources, toadd->GetClientServerIP(), toadd->GetClientServerPort(), SF_SEARCH_RESULT, false);
		}
		for (std::list<CSearchFile::ClientStruct>::const_iterator it = toadd->GetClients().begin(); it != toadd->GetClients().end(); ++it) {
			CMemFile sources(1+4+2);
			sources.WriteUInt8(1);
			sources.WriteUInt32(it->m_ip);
			sources.WriteUInt16(it->m_port);
			sources.Reset();
			newfile->AddSources(sources, it->m_serverIP, it->m_serverPort, SF_SEARCH_RESULT, false);
		}
	} else {
		delete newfile;
	}
}


struct SFindBestPF
{
	void operator()(CPartFile* file) {
		// Check if we should filter out other categories
		if ((m_category != -1) && (file->GetCategory() != m_category)) {
			return;
		} else if (file->GetStatus() != PS_PAUSED) {
			return;
		}
	
		if (!m_result || (file->GetDownPriority() > m_result->GetDownPriority())) {
			m_result = file;				
		}
	}

	//! The category to look for, or -1 if any category is good
	int m_category;
	//! If any acceptable files are found, this variable store their pointer
	CPartFile* m_result;
};


void CDownloadQueue::StartNextFile(CPartFile* oldfile)
{
	if ( thePrefs::StartNextFile() ) {
		SFindBestPF visitor = { -1, NULL };
		
		{
			wxMutexLocker lock(m_mutex);
			
			if (thePrefs::StartNextFileSame()) {
				// Get a download in the same category
				visitor.m_category = oldfile->GetCategory();

				visitor = std::for_each(m_filelist.begin(), m_filelist.end(), visitor);
			}

			if (visitor.m_result == NULL) {
				// Get a download, regardless of category
				visitor.m_category = -1;
				
				visitor = std::for_each(m_filelist.begin(), m_filelist.end(), visitor);
			}	
		}
		
		if (visitor.m_result) {
			visitor.m_result->ResumeFile();
		}
	}
}


void CDownloadQueue::AddDownload(CPartFile* file, bool paused, uint8 category)
{
	wxCHECK_RET(!IsFileExisting(file->GetFileHash()), wxT("Adding duplicate part-file"));

	if (file->GetStatus(true) == PS_ALLOCATING) {
		file->PauseFile();
	} else if (paused && GetFileCount()) {
		file->StopFile();
	}

	{
		wxMutexLocker lock(m_mutex);
		m_filelist.push_back( file );
		DoSortByPriority();
	}

	NotifyObservers( EventType( EventType::INSERTED, file ) );

	file->SetCategory(category);
	Notify_DownloadCtrlAddFile( file );
	AddLogLineM(true, CFormat(_("Downloading %s")) % file->GetFileName() );
}


bool CDownloadQueue::IsFileExisting( const CMD4Hash& fileid ) const 
{
	if (CKnownFile* file = theApp->sharedfiles->GetFileByID(fileid)) {
		if (file->IsPartFile()) {
			AddLogLineM(true, CFormat( _("You are already trying to download the file '%s'") ) % file->GetFileName());
		} else {
			// Check if the file exists, since otherwise the user is forced to 
			// manually reload the shares to download a file again.
			CPath fullpath = file->GetFilePath().JoinPaths(file->GetFileName());
			if (!fullpath.FileExists()) {
				// The file is no longer available, unshare it
				theApp->sharedfiles->RemoveFile(file);
				
				return false;
			}
			
			AddLogLineM(true, CFormat( _("You already have the file '%s'") ) % file->GetFileName());
		}
		
		return true;
	} else if ((file = GetFileByID(fileid))) {
		AddLogLineM(true, CFormat( _("You are already trying to download the file %s") ) % file->GetFileName());
		return true;
	}
	
	return false;
}


void CDownloadQueue::Process()
{
	// send src requests to local server
	ProcessLocalRequests();
	
	{
		wxMutexLocker lock(m_mutex);

		uint32 downspeed = 0;
		if (thePrefs::GetMaxDownload() != UNLIMITED && m_datarate > 1500) {
			downspeed = (((uint32)thePrefs::GetMaxDownload())*1024*100)/(m_datarate+1); 
			if (downspeed < 50) {
				downspeed = 50;
			} else if (downspeed > 200) {
				downspeed = 200;
			}
		}
	
		m_datarate = 0;
		m_udcounter++;
		uint32 cur_datarate = 0;
		uint32 cur_udcounter = m_udcounter;
		
		for ( uint16 i = 0; i < m_filelist.size(); i++ ) {
			CPartFile* file = m_filelist[i];
	
			CMutexUnlocker unlocker(m_mutex);
			
			if ( file->GetStatus() == PS_READY || file->GetStatus() == PS_EMPTY ){
				cur_datarate += file->Process( downspeed, cur_udcounter );
			} else {
				//This will make sure we don't keep old sources to paused and stoped files..
				file->StopPausedFile();
			}
		}
	
		m_datarate += cur_datarate;
	
	
		if (m_udcounter == 5) {
			if (theApp->serverconnect->IsUDPSocketAvailable()) {
				if( (::GetTickCount() - m_lastudpstattime) > UDPSERVERSTATTIME) {
					m_lastudpstattime = ::GetTickCount();
					
					CMutexUnlocker unlocker(m_mutex);
					theApp->serverlist->ServerStats();
				}
			}
		}
	
		if (m_udcounter == 10) {
			m_udcounter = 0;
			if (theApp->serverconnect->IsUDPSocketAvailable()) {
				if ( (::GetTickCount() - m_lastudpsearchtime) > UDPSERVERREASKTIME) {
					SendNextUDPPacket();
				}
			}
		}
	
		if ( (::GetTickCount() - m_lastsorttime) > 10000 ) {
			
			
			DoSortByPriority();
		}
		// Check if any paused files can be resumed
			
		CheckDiskspace(thePrefs::GetTempDir());
	}
	
	
	// Check for new links once per second.
	if ((::GetTickCount() - m_nLastED2KLinkCheck) >= 1000) {
		AddLinksFromFile();
		m_nLastED2KLinkCheck = ::GetTickCount();
	}
}


CPartFile* CDownloadQueue::GetFileByID(const CMD4Hash& filehash) const
{
	wxMutexLocker lock( m_mutex );
	
	for ( uint16 i = 0; i < m_filelist.size(); ++i ) {
		if ( filehash == m_filelist[i]->GetFileHash()) {
			return m_filelist[ i ];
		}
	}
	
	return NULL;
}


CPartFile* CDownloadQueue::GetFileByIndex(unsigned int index)  const
{
	wxMutexLocker lock( m_mutex );
	
	if ( index < m_filelist.size() ) {
		return m_filelist[ index ];
	}
	
	wxASSERT( false );
	return NULL;
}


bool CDownloadQueue::IsPartFile(const CKnownFile* file) const
{
	wxMutexLocker lock(m_mutex);

	for (uint16 i = 0; i < m_filelist.size(); ++i) {
		if (file == m_filelist[i]) {
			return true;
		}
	}
	
	return false;
}


void CDownloadQueue::OnConnectionState(bool bConnected)
{
	wxMutexLocker lock(m_mutex);

	for (uint16 i = 0; i < m_filelist.size(); ++i) {
		if (	m_filelist[i]->GetStatus() == PS_READY ||
			m_filelist[i]->GetStatus() == PS_EMPTY) {
			m_filelist[i]->SetActive(bConnected);
		}
	}
}


void CDownloadQueue::CheckAndAddSource(CPartFile* sender, CUpDownClient* source)
{
	// if we block loopbacks at this point it should prevent us from connecting to ourself
	if ( source->HasValidHash() ) {
		if ( source->GetUserHash() == thePrefs::GetUserHash() ) {
			AddDebugLogLineM( false, logDownloadQueue, wxT("Tried to add source with matching hash to your own.") );
			source->Safe_Delete();
			return;
		}
	}

	if (sender->IsStopped()) {
		source->Safe_Delete();
		return;
	}

	// Filter sources which are known to be dead/useless
	if ( theApp->clientlist->IsDeadSource( source ) || sender->IsDeadSource(source) ) {
		source->Safe_Delete();
		return;
	}

	// Filter sources which are incompatible with our encryption setting (one requires it, and the other one doesn't supports it)
	if ( (source->RequiresCryptLayer() && (!thePrefs::IsClientCryptLayerSupported() || !source->HasValidHash())) || (thePrefs::IsClientCryptLayerRequired() && (!source->SupportsCryptLayer() || !source->HasValidHash()))) {
		source->Safe_Delete();
		return;
	}	
	
	// Find all clients with the same hash
	if ( source->HasValidHash() ) {
		CClientList::SourceList found = theApp->clientlist->GetClientsByHash( source->GetUserHash() );

		CClientList::SourceList::iterator it = found.begin();
		for ( ; it != found.end(); it++ ) {
			CKnownFile* file = (*it)->GetRequestFile();

			// Only check files on the download-queue
			if ( file ) {
				// Is the found source queued for something else?
				if (  file != sender ) {
					// Try to add a request for the other file
					if ( (*it)->AddRequestForAnotherFile(sender)) {
						// Add it to downloadlistctrl
						Notify_DownloadCtrlAddSource(sender, *it, A4AF_SOURCE);
					}
				}
				
				source->Safe_Delete();
				return;
			}
		}
	}



	// Our new source is real new but maybe it is already uploading to us?
	// If yes the known client will be attached to the var "source" and the old
	// source-client will be deleted. However, if the request file of the known 
	// source is NULL, then we have to treat it almost like a new source and if
	// it isn't NULL and not "sender", then we shouldn't move it, but rather add
	// a request for the new file.
	ESourceFrom nSourceFrom = source->GetSourceFrom();
	if ( theApp->clientlist->AttachToAlreadyKnown(&source, 0) ) {
		// Already queued for another file?
		if ( source->GetRequestFile() ) {
			// If we're already queued for the right file, then there's nothing to do
			if ( sender != source->GetRequestFile() ) {	
				// Add the new file to the request list
				source->AddRequestForAnotherFile( sender );
			}
		} else {
			// Source was known, but reqfile NULL.
			source->SetRequestFile( sender );
			if (source->GetSourceFrom() != nSourceFrom) {
				if (source->GetSourceFrom() != SF_NONE) {
					theStats::RemoveSourceOrigin(source->GetSourceFrom());
					theStats::RemoveFoundSource();
				}
				source->SetSourceFrom(nSourceFrom);
			}
			sender->AddSource( source );
			if ( source->GetFileRating() || !source->GetFileComment().IsEmpty() ) {
				sender->UpdateFileRatingCommentAvail();
			}
			
			Notify_DownloadCtrlAddSource(sender, source, UNAVAILABLE_SOURCE);
		}
	} else {
		// Unknown client, add it to the clients list
		source->SetRequestFile( sender );

		theApp->clientlist->AddClient(source);
	
		sender->AddSource( source );
		if ( source->GetFileRating() || !source->GetFileComment().IsEmpty() ) {
			sender->UpdateFileRatingCommentAvail();
		}
	
		Notify_DownloadCtrlAddSource(sender, source, UNAVAILABLE_SOURCE);
	}
}


void CDownloadQueue::CheckAndAddKnownSource(CPartFile* sender,CUpDownClient* source)
{
	// Kad reviewed
	
	if (sender->IsStopped()) {
		return;
	}

	// Filter sources which are known to be dead/useless
	if ( sender->IsDeadSource(source) ) {
		return;
	}

	// "Filter LAN IPs" -- this may be needed here in case we are connected to the internet and are also connected
	// to a LAN and some client from within the LAN connected to us. Though this situation may be supported in future
	// by adding that client to the source list and filtering that client's LAN IP when sending sources to
	// a client within the internet.
	//
	// "IPfilter" is not needed here, because that "known" client was already IPfiltered when receiving OP_HELLO.
	if (!source->HasLowID()) {
		uint32 nClientIP = wxUINT32_SWAP_ALWAYS(source->GetUserIDHybrid());
		if (!IsGoodIP(nClientIP, thePrefs::FilterLanIPs())) { // check for 0-IP, localhost and LAN addresses
			AddDebugLogLineM(false, logIPFilter, wxT("Ignored already known source with IP=%s") + Uint32toStringIP(nClientIP));
			return;
		}
	}	
	
	// Filter sources which are incompatible with our encryption setting (one requires it, and the other one doesn't supports it)
	if ( (source->RequiresCryptLayer() && (!thePrefs::IsClientCryptLayerSupported() || !source->HasValidHash())) || (thePrefs::IsClientCryptLayerRequired() && (!source->SupportsCryptLayer() || !source->HasValidHash())))
	{
		source->Safe_Delete();
		return;
	}		
	
	CPartFile* file = source->GetRequestFile();
	
	// Check if the file is already queued for something else
	if ( file ) {
		if ( file != sender ) {
			if ( source->AddRequestForAnotherFile( sender ) ) {
				Notify_DownloadCtrlAddSource( sender, source, A4AF_SOURCE );
			}
		}
	} else {
		source->SetRequestFile( sender );

		if ( source->GetFileRating() || !source->GetFileComment().IsEmpty() ) {
			sender->UpdateFileRatingCommentAvail();
		}

		source->SetSourceFrom(SF_PASSIVE);
		sender->AddSource( source );
		Notify_DownloadCtrlAddSource( sender, source, UNAVAILABLE_SOURCE);
	}
}


bool CDownloadQueue::RemoveSource(CUpDownClient* toremove, bool	WXUNUSED(updatewindow), bool bDoStatsUpdate)
{
	bool removed = false;
	toremove->DeleteAllFileRequests();
	
	for ( uint16 i = 0; i < GetFileCount(); i++ ) {
		CPartFile* cur_file = GetFileByIndex( i );
		
		// Remove from source-list
		if ( cur_file->DelSource( toremove ) ) {
			cur_file->RemoveDownloadingSource(toremove);
			removed = true;
			if ( bDoStatsUpdate ) {
				cur_file->UpdatePartsInfo();
			}
		}

		// Remove from A4AF-list
		cur_file->RemoveA4AFSource( toremove );
	}


	if ( !toremove->GetFileComment().IsEmpty() || toremove->GetFileRating()>0) {
		toremove->GetRequestFile()->UpdateFileRatingCommentAvail();
	}
	
	toremove->SetRequestFile( NULL );
	toremove->SetDownloadState(DS_NONE);

	// Remove from downloadlist widget
	Notify_DownloadCtrlRemoveSource(toremove, (CPartFile*)NULL);
	toremove->ResetFileStatusInfo();

	return removed;
}


void CDownloadQueue::RemoveFile(CPartFile* file)
{
	RemoveLocalServerRequest( file );

	NotifyObservers( EventType( EventType::REMOVED, file ) );

	wxMutexLocker lock( m_mutex );

	EraseValue( m_filelist, file );
}


CUpDownClient* CDownloadQueue::GetDownloadClientByIP_UDP(uint32 dwIP, uint16 nUDPPort) const
{
	wxMutexLocker lock( m_mutex );

	for ( unsigned int i = 0; i < m_filelist.size(); i++ ) {
		const CPartFile::SourceSet& set = m_filelist[i]->GetSourceList();
		
		for ( CPartFile::SourceSet::const_iterator it = set.begin(); it != set.end(); it++ ) {
			if ( (*it)->GetIP() == dwIP && (*it)->GetUDPPort() == nUDPPort ) {
				return *it;
			}
		}
	}
	return NULL;
}


/**
 * Checks if the specified server is the one we are connected to.
 */
bool IsConnectedServer(const CServer* server)
{
	if (server && theApp->serverconnect->GetCurrentServer()) {
		wxString srvAddr = theApp->serverconnect->GetCurrentServer()->GetAddress();
		uint16 srvPort = theApp->serverconnect->GetCurrentServer()->GetPort();

		return server->GetAddress() == srvAddr && server->GetPort() == srvPort;
	}

	return false;
}


bool CDownloadQueue::SendNextUDPPacket()
{
	if ( m_filelist.empty() || !theApp->serverconnect->IsUDPSocketAvailable() || !theApp->IsConnectedED2K()) {
		return false;
	}

	// Start monitoring the server and the files list
	if ( !m_queueServers.IsActive() ) {
		AddObserver( &m_queueFiles );
		
		theApp->serverlist->AddObserver( &m_queueServers );
	}
	

 	bool packetSent = false;
 	while ( !packetSent ) {
 		// Get max files ids per packet for current server
 		int filesAllowed = GetMaxFilesPerUDPServerPacket();
 
 		if (filesAllowed < 1 || !m_udpserver || IsConnectedServer(m_udpserver)) {
 			// Select the next server to ask, must not be the connected server
 			do {
				m_udpserver = m_queueServers.GetNext();
			} while (IsConnectedServer(m_udpserver));
	
 			m_cRequestsSentToServer = 0;
 			filesAllowed = GetMaxFilesPerUDPServerPacket();
  		}
 
 		
 		// Check if we have asked all servers, in which case we are done
 		if (m_udpserver == NULL) {
 			DoStopUDPRequests();
 			
 			return false;
  		}
  
 		// Memoryfile containing the hash of every file to request
		// 28bytes allocation because 16b + 4b + 8b is the worse case scenario.
 		CMemFile hashlist( 28 );
 		
		CPartFile* file = m_queueFiles.GetNext();
		
		while ( file && filesAllowed ) {
 			uint8 status = file->GetStatus();
 			
			if ( ( status == PS_READY || status == PS_EMPTY ) && file->GetSourceCount() < thePrefs::GetMaxSourcePerFileUDP() ) {
				if (file->IsLargeFile() && !m_udpserver->SupportsLargeFilesUDP()) {
					AddDebugLogLineM(false, logDownloadQueue, wxT("UDP Request for sources on a large file ignored: server doesn't support it")); 	
				} else {
					++m_cRequestsSentToServer;
					hashlist.WriteHash( file->GetFileHash() );
					// See the notes on TCP packet
					if ( m_udpserver->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES2 ) {
						if (file->IsLargeFile()) {
							wxASSERT(m_udpserver->SupportsLargeFilesUDP());
							hashlist.WriteUInt32( 0 );
							hashlist.WriteUInt64( file->GetFileSize() );
						} else {
							hashlist.WriteUInt32( file->GetFileSize() );
						}
					}
					--filesAllowed;
				}
  			}
		
			// Avoid skipping a file if we can't send any more currently
			if ( filesAllowed ) {
				file = m_queueFiles.GetNext();
			}
		}
		
		// See if we have anything to send
 		if ( hashlist.GetLength() ) {
 			packetSent = SendGlobGetSourcesUDPPacket(hashlist);
		}
 		
 		// Check if we've covered every file
		if ( file == NULL ) {
 			// Reset the list of asked files so that the loop will start over
			m_queueFiles.Reset();
			
 			// Unset the server so that the next server will be used
 			m_udpserver = NULL;
  		}
  	}
	
	return true;
}


void CDownloadQueue::StopUDPRequests()
{
	wxMutexLocker lock( m_mutex );

	DoStopUDPRequests();
}


void CDownloadQueue::DoStopUDPRequests()
{
	// No need to observe when we wont be using the results
	theApp->serverlist->RemoveObserver( &m_queueServers );
	RemoveObserver( &m_queueFiles );
	
	m_udpserver = 0;
	m_lastudpsearchtime = ::GetTickCount();
}


// Comparison function needed by sort. Returns true if file1 preceeds file2
bool ComparePartFiles(const CPartFile* file1, const CPartFile* file2) {
	if (file1->GetDownPriority() != file2->GetDownPriority()) {
		// To place high-priority files before low priority files we have to 
		// invert this test, since PR_LOW is lower than PR_HIGH, and since 
		// placing a PR_LOW file before a PR_HIGH file would mean that
		// the PR_LOW file gets sources before the PR_HIGH file ...
		return (file1->GetDownPriority() > file2->GetDownPriority());
	} else {
		int sourcesA = file1->GetSourceCount();
		int sourcesB = file2->GetSourceCount();
		
		int notSourcesA = file1->GetNotCurrentSourcesCount();
		int notSourcesB = file2->GetNotCurrentSourcesCount();
		
		int cmp = CmpAny( sourcesA - notSourcesA, sourcesB - notSourcesB );

		if ( cmp == 0 ) {
			cmp = CmpAny( notSourcesA, notSourcesB );
		}

		return cmp < 0;
	}
}


void CDownloadQueue::DoSortByPriority()
{
	m_lastsorttime = ::GetTickCount();
	sort( m_filelist.begin(), m_filelist.end(), ComparePartFiles );
}


void CDownloadQueue::ResetLocalServerRequests()
{
	wxMutexLocker lock( m_mutex );

	m_dwNextTCPSrcReq = 0;
	m_localServerReqQueue.clear();

	for ( uint16 i = 0; i < m_filelist.size(); i++ ) {
		m_filelist[i]->SetLocalSrcRequestQueued(false);
	}
}


void CDownloadQueue::RemoveLocalServerRequest( CPartFile* file )
{
	wxMutexLocker lock( m_mutex );
	
	EraseValue( m_localServerReqQueue, file );

	file->SetLocalSrcRequestQueued(false);
}


void CDownloadQueue::ProcessLocalRequests()
{
	wxMutexLocker lock( m_mutex );
	
	bool bServerSupportsLargeFiles = theApp->serverconnect 
									&& theApp->serverconnect->GetCurrentServer()
									&& theApp->serverconnect->GetCurrentServer()->SupportsLargeFilesTCP();
	
	if ( (!m_localServerReqQueue.empty()) && (m_dwNextTCPSrcReq < ::GetTickCount()) ) {
		CMemFile dataTcpFrame(22);
		const int iMaxFilesPerTcpFrame = 15;
		int iFiles = 0;
		while (!m_localServerReqQueue.empty() && iFiles < iMaxFilesPerTcpFrame) {
			// find the file with the longest waitingtime
			uint32 dwBestWaitTime = 0xFFFFFFFF;

			std::list<CPartFile*>::iterator posNextRequest = m_localServerReqQueue.end();
			std::list<CPartFile*>::iterator it = m_localServerReqQueue.begin();
			while( it != m_localServerReqQueue.end() ) {
				CPartFile* cur_file = (*it);
				if (cur_file->GetStatus() == PS_READY || cur_file->GetStatus() == PS_EMPTY) {
					uint8 nPriority = cur_file->GetDownPriority();
					if (nPriority > PR_HIGH) {
						wxASSERT(0);
						nPriority = PR_HIGH;
					}

					if (cur_file->GetLastSearchTime() + (PR_HIGH-nPriority) < dwBestWaitTime ){
						dwBestWaitTime = cur_file->GetLastSearchTime() + (PR_HIGH - nPriority);
						posNextRequest = it;
					}

					it++;
				} else {
					it = m_localServerReqQueue.erase(it);
					cur_file->SetLocalSrcRequestQueued(false);
					AddDebugLogLineM( false, logDownloadQueue,
						CFormat(wxT("Local server source request for file '%s' not sent because of status '%s'"))
							% cur_file->GetFileName() % cur_file->getPartfileStatus());
				}
			}

			if (posNextRequest != m_localServerReqQueue.end()) {
				CPartFile* cur_file = (*posNextRequest);
				cur_file->SetLocalSrcRequestQueued(false);
				cur_file->SetLastSearchTime(::GetTickCount());
				m_localServerReqQueue.erase(posNextRequest);
				iFiles++;

				if (!bServerSupportsLargeFiles && cur_file->IsLargeFile()) {
					AddDebugLogLineM(false, logDownloadQueue, wxT("TCP Request for sources on a large file ignored: server doesn't support it"));
				} else {
					AddDebugLogLineM(false, logDownloadQueue,
						CFormat(wxT("Creating local sources request packet for '%s'")) % cur_file->GetFileName());
					// create request packet
					CMemFile data(16 + (cur_file->IsLargeFile() ? 8 : 4));
					data.WriteHash(cur_file->GetFileHash());
					// Kry - lugdunum extended protocol on 17.3 to handle filesize properly.
					// There is no need to check anything, old server ignore the extra 4 bytes.
					// As of 17.9, servers accept a 0 32-bits size and then a 64bits size
					if (cur_file->IsLargeFile()) {
						wxASSERT(bServerSupportsLargeFiles);
						data.WriteUInt32(0);
						data.WriteUInt64(cur_file->GetFileSize());
					} else {
						data.WriteUInt32(cur_file->GetFileSize());
					}
					uint8 byOpcode = 0;
					if (thePrefs::IsClientCryptLayerSupported() && theApp->serverconnect->GetCurrentServer() != NULL && theApp->serverconnect->GetCurrentServer()->SupportsGetSourcesObfuscation()) {
						byOpcode = OP_GETSOURCES_OBFU;
					} else {
						byOpcode = OP_GETSOURCES;
					}
					CPacket packet(data, OP_EDONKEYPROT, byOpcode);
					dataTcpFrame.Write(packet.GetPacket(), packet.GetRealPacketSize());
				}
			}
		}

		int iSize = dataTcpFrame.GetLength();
		if (iSize > 0) {
			// create one 'packet' which contains all buffered OP_GETSOURCES ED2K packets to be sent with one TCP frame
			// server credits: (16+4)*regularfiles + (16+4+8)*largefiles +1
			CPacket* packet = new CPacket(new byte[iSize], dataTcpFrame.GetLength(), true, false);
			dataTcpFrame.Seek(0, wxFromStart);
			dataTcpFrame.Read(packet->GetPacket(), iSize);
			uint32 size = packet->GetPacketSize();
			theApp->serverconnect->SendPacket(packet, true);	// Deletes `packet'.
			AddDebugLogLineM(false, logDownloadQueue, wxT("Sent local sources request packet."));
			theStats::AddUpOverheadServer(size);
		}

		// next TCP frame with up to 15 source requests is allowed to be sent in..
		m_dwNextTCPSrcReq = ::GetTickCount() + SEC2MS(iMaxFilesPerTcpFrame*(16+4));
	}

}


void CDownloadQueue::SendLocalSrcRequest(CPartFile* sender)
{
	wxMutexLocker lock( m_mutex );
	
	m_localServerReqQueue.push_back(sender);
}


void CDownloadQueue::AddLinksFromFile()
{
	const wxString fullPath = theApp->ConfigDir + wxT("ED2KLinks");
	if (!wxFile::Exists(fullPath)) {
		return;
	}
	
	// Attempt to lock the ED2KLinks file.
	CFileLock lock((const char*)unicode2char(fullPath));

	wxTextFile file(fullPath);
	if ( file.Open() ) {
		for ( unsigned int i = 0; i < file.GetLineCount(); i++ ) {
			wxString line = file.GetLine( i ).Strip( wxString::both );
			
			if ( !line.IsEmpty() ) {
				// Special case! used by a secondary running mule to raise this one.
				if ( line == wxT("RAISE_DIALOG")  ) {
					Notify_ShowGUI();
					continue;
				}
				
				AddLink( line );
			}
		}

		file.Close();
	} else {
		printf("Failed to open ED2KLinks file.\n");
	}
	
	// Delete the file.
	wxRemoveFile(theApp->ConfigDir +  wxT("ED2KLinks"));
}


void CDownloadQueue::ResetCatParts(uint8 cat)
{
	for ( uint16 i = 0; i < GetFileCount(); i++ ) {
		CPartFile* file = GetFileByIndex( i );
		
		if ( file->GetCategory() == cat ) {
			// Reset the category
			file->SetCategory( 0 );
		} else if ( file->GetCategory() > cat ) {
			// Set to the new position of the original category
			file->SetCategory( file->GetCategory() - 1 );
		}
	}
}


void CDownloadQueue::SetCatPrio(uint8 cat, uint8 newprio)
{
	for ( uint16 i = 0; i < GetFileCount(); i++ ) {
		CPartFile* file = GetFileByIndex( i );
		
		if ( !cat || file->GetCategory() == cat ) {
			if ( newprio == PR_AUTO ) {
				file->SetAutoDownPriority(true);
			} else {
				file->SetAutoDownPriority(false);
				file->SetDownPriority(newprio);
			}
		}
	}
}


void CDownloadQueue::SetCatStatus(uint8 cat, int newstatus)
{
	std::list<CPartFile*> files;

	{
		wxMutexLocker lock(m_mutex);
	
		for ( uint16 i = 0; i < m_filelist.size(); i++ ) {
			if ( m_filelist[i]->CheckShowItemInGivenCat(cat) ) {
				files.push_back( m_filelist[i] );
			}
		}
	}
	
	std::list<CPartFile*>::iterator it = files.begin();
		
	for ( ; it != files.end(); it++ ) {
		switch ( newstatus ) {
			case MP_CANCEL:	(*it)->Delete(); 	break;
			case MP_PAUSE:	(*it)->PauseFile();	break;
			case MP_STOP:	(*it)->StopFile();	break;
			case MP_RESUME:	(*it)->ResumeFile();	break;
		}
	}
}


uint16 CDownloadQueue::GetDownloadingFileCount() const
{
	wxMutexLocker lock( m_mutex );
	
	uint16 count = 0;
	for ( uint16 i = 0; i < m_filelist.size(); i++ ) {
		uint8 status = m_filelist[i]->GetStatus();
		if ( status == PS_READY || status == PS_EMPTY ) {
			count++;
		}
	}
	
	return count;
}


uint16 CDownloadQueue::GetPausedFileCount() const
{
	wxMutexLocker lock( m_mutex );

	uint16 count = 0;
	for ( uint16 i = 0; i < m_filelist.size(); i++ ) {
		if ( m_filelist[i]->GetStatus() == PS_PAUSED ) {
			count++;
		}
	}
	
	return count;
}


void CDownloadQueue::CheckDiskspace( const CPath& path )
{
	if ( ::GetTickCount() - m_lastDiskCheck < DISKSPACERECHECKTIME ) {
		return;
	}
	
	m_lastDiskCheck = ::GetTickCount();

	uint64 min = 0;
	// Check if the user has set an explicit limit
	if ( thePrefs::IsCheckDiskspaceEnabled() ) {
		min = thePrefs::GetMinFreeDiskSpace();
	}

	// The very least acceptable diskspace is a single PART
	if ( min < PARTSIZE ) {
		min = PARTSIZE;
	}

	uint64 free = CPath::GetFreeSpaceAt(path);
	if (free == static_cast<uint64>(wxInvalidOffset)) {
		return;
	} else if (free < min) {
		CUserEvents::ProcessEvent(
			CUserEvents::OutOfDiskSpace,
			wxT("Temporary partition"));
	}
	
	for (unsigned int i = 0; i < m_filelist.size(); ++i) {
		CPartFile* file = m_filelist[i];
		
		switch ( file->GetStatus() ) {
			case PS_ERROR:
			case PS_COMPLETING:
			case PS_COMPLETE:
				continue;
		}
	
		if ( free >= min && file->GetInsufficient() ) {
			// We'll try to resume files if there is enough free space
			if ( free - file->GetNeededSpace() > min ) {
				file->ResumeFile();
			}
		} else if ( free < min && !file->IsPaused() ) {
			// No space left, stop the files.
			file->PauseFile( true );
		}
	}
}


int CDownloadQueue::GetMaxFilesPerUDPServerPacket() const
{
	if ( m_udpserver ) {
		if ( m_udpserver->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES ) {
			// get max. file ids per packet
			if ( m_cRequestsSentToServer < MAX_REQUESTS_PER_SERVER ) {
				return std::min(
					MAX_FILES_PER_UDP_PACKET,
					MAX_REQUESTS_PER_SERVER - m_cRequestsSentToServer
				);
			}
		} else if ( m_cRequestsSentToServer < MAX_REQUESTS_PER_SERVER ) {
			return 1;
		}
	}

	return 0;
}


bool CDownloadQueue::SendGlobGetSourcesUDPPacket(CMemFile& data)
{
	if (!m_udpserver) {
		return false;
	}
	
	CPacket packet(data, OP_EDONKEYPROT, ((m_udpserver->GetUDPFlags() & SRV_UDPFLG_EXT_GETSOURCES2) ? OP_GLOBGETSOURCES2 : OP_GLOBGETSOURCES));

	theStats::AddUpOverheadServer(packet.GetPacketSize());
	theApp->serverconnect->SendUDPPacket(&packet,m_udpserver,false);

	return true;
}


void CDownloadQueue::AddToResolve(const CMD4Hash& fileid, const wxString& pszHostname, uint16 port, const wxString& hash, uint8 cryptoptions)
{
	// double checking
	if ( !GetFileByID(fileid) ) {
		return;
	}

	wxMutexLocker lock( m_mutex );
		
	Hostname_Entry entry = { fileid, pszHostname, port, hash, cryptoptions };
	m_toresolve.push_front(entry);

	// Check if there are other DNS lookups on queue
	if (m_toresolve.size() == 1) {
		// Check if it is a simple dot address
		uint32 ip = StringIPtoUint32(pszHostname);

		if (ip) {
			OnHostnameResolved(ip);
		} else {
			CAsyncDNS* dns = new CAsyncDNS(pszHostname, DNS_SOURCE, theApp);

			if ((dns->Create() != wxTHREAD_NO_ERROR) || (dns->Run() != wxTHREAD_NO_ERROR)) {
				dns->Delete();
				m_toresolve.pop_front();
			}
		}
	}
}


void CDownloadQueue::OnHostnameResolved(uint32 ip)
{
	wxMutexLocker lock( m_mutex );

	wxASSERT( m_toresolve.size() );
	
	Hostname_Entry resolved = m_toresolve.front();
	m_toresolve.pop_front();

	if ( ip ) {
		CPartFile* file = GetFileByID( resolved.fileid );
		if ( file ) {
			CMemFile sources(1+4+2);
			sources.WriteUInt8(1); // No. Sources
			sources.WriteUInt32(ip);
			sources.WriteUInt16(resolved.port);
			sources.WriteUInt8(resolved.cryptoptions);
			if (resolved.cryptoptions & 0x80) {
				wxASSERT(!resolved.hash.IsEmpty());
				CMD4Hash sourcehash;
				sourcehash.Decode(resolved.hash);
				sources.WriteHash(sourcehash);
			}
			sources.Seek(0,wxFromStart);
			
			file->AddSources(sources, 0, 0, SF_LINK, true);
		}
	}
	
	while (m_toresolve.size()) {
		Hostname_Entry entry = m_toresolve.front();

		// Check if it is a simple dot address
		uint32 tmpIP = StringIPtoUint32(entry.strHostname);

		if (tmpIP) {
			OnHostnameResolved(tmpIP);
		} else {
			CAsyncDNS* dns = new CAsyncDNS(entry.strHostname, DNS_SOURCE, theApp);

			if ((dns->Create() != wxTHREAD_NO_ERROR) || (dns->Run() != wxTHREAD_NO_ERROR)) {
				dns->Delete();
				m_toresolve.pop_front();
			} else {
				break;
			}
		}
	}
}


bool CDownloadQueue::AddLink( const wxString& link, int category )
{
	wxString uri(link);

	if (link.compare(0, 7, wxT("magnet:")) == 0) {
		uri = CMagnetED2KConverter(link);
		if (uri.empty()) {
			AddLogLineM(true, CFormat(_("Cannot convert magnet link to eD2k: %s")) % link);
			return false;
		}
	}

	if (uri.compare(0, 7, wxT("ed2k://")) == 0) {
		return AddED2KLink(uri, category);
	} else {
		AddLogLineM(true, CFormat(_("Unknown protocol of link: %s")) % link);
		return false;
	}
}


bool CDownloadQueue::AddED2KLink( const wxString& link, int category )
{
	wxASSERT( !link.IsEmpty() );
	wxString URI = link;
	
	// Need the links to end with /, otherwise CreateLinkFromUrl crashes us.
	if ( URI.Last() != wxT('/') ) {
		URI += wxT("/");
	}
	
	try {
		CScopedPtr<CED2KLink> uri(CED2KLink::CreateLinkFromUrl(URI));

		return AddED2KLink( uri.get(), category );
	} catch ( const wxString& err ) {
		AddLogLineM( true, CFormat( _("Invalid eD2k link! ERROR: %s")) % err);
	}
	
	return false;
}


bool CDownloadQueue::AddED2KLink( const CED2KLink* link, int category )
{
	switch ( link->GetKind() ) {
		case CED2KLink::kFile:
			return AddED2KLink( dynamic_cast<const CED2KFileLink*>( link ), category );
			
		case CED2KLink::kServer:
			return AddED2KLink( dynamic_cast<const CED2KServerLink*>( link ) );
			
		case CED2KLink::kServerList:
			return AddED2KLink( dynamic_cast<const CED2KServerListLink*>( link ) );
			
		default:
			return false;
	}
}



bool CDownloadQueue::AddED2KLink( const CED2KFileLink* link, int category )
{
	CPartFile* file = NULL;
	if (IsFileExisting(link->GetHashKey())) {
		// Must be a shared file if we are to add hashes or sources
		if ((file = GetFileByID(link->GetHashKey())) == NULL) {
			return false;
		}
	} else {
		if (link->GetSize() > OLD_MAX_FILE_SIZE) {
			if (!PlatformSpecific::CanFSHandleLargeFiles(thePrefs::GetTempDir())) {
				AddLogLineM(true, _("Filesystem for Temp directory cannot handle large files."));
				return false;
			} else if (!PlatformSpecific::CanFSHandleLargeFiles(theApp->glob_prefs->GetCatPath(category))) {
				AddLogLineM(true, _("Filesystem for Incoming directory cannot handle large files."));
				return false;
			}
		}

		file = new CPartFile(link);
	
		if (file->GetStatus() == PS_ERROR) {
			delete file;
			return false;
		}
		
		AddDownload(file, thePrefs::AddNewFilesPaused(), category);
	}
	
	if (link->HasValidAICHHash()) {
		CAICHHashSet* hashset = file->GetAICHHashset();
		
		if (!hashset->HasValidMasterHash() || (hashset->GetMasterHash() != link->GetAICHHash())) {
			hashset->SetMasterHash(link->GetAICHHash(), AICH_VERIFIED);
			hashset->FreeHashSet();
		}
	}
	
	const CED2KFileLink::CED2KLinkSourceList& list = link->m_sources;
	CED2KFileLink::CED2KLinkSourceList::const_iterator it = list.begin();
	for (; it != list.end(); ++it) {
		AddToResolve(link->GetHashKey(), it->addr, it->port, it->hash, it->cryptoptions);
	}

	return true;	
}


bool CDownloadQueue::AddED2KLink( const CED2KServerLink* link )
{
	CServer *server = new CServer( link->GetPort(), Uint32toStringIP( link->GetIP() ) );
	
	server->SetListName( Uint32toStringIP( link->GetIP() ) );
	
	theApp->serverlist->AddServer(server);
	
	Notify_ServerAdd(server);

	return true;
}


bool CDownloadQueue::AddED2KLink( const CED2KServerListLink* link )
{
	theApp->serverlist->UpdateServerMetFromURL( link->GetAddress() );

	return true;
}


void CDownloadQueue::ObserverAdded( ObserverType* o )
{
	CObservableQueue<CPartFile*>::ObserverAdded( o );
	
	EventType::ValueList list;

	{
		wxMutexLocker lock(m_mutex);
		list.reserve( m_filelist.size() );
		list.insert( list.begin(), m_filelist.begin(), m_filelist.end() );
	}

	NotifyObservers( EventType( EventType::INITIAL, &list ), o );
}

void CDownloadQueue::KademliaSearchFile(uint32 searchID, const Kademlia::CUInt128* pcontactID, const Kademlia::CUInt128* pbuddyID, uint8 type, uint32 ip, uint16 tcp, uint16 udp, uint32 serverip, uint16 serverport, uint8 byCryptOptions)
{
	
	AddDebugLogLineM(false, logKadSearch, wxString::Format(wxT("Search result sources (type %i)"),type));
	
	//Safety measure to make sure we are looking for these sources
	CPartFile* temp = GetFileByKadFileSearchID(searchID);
	if( !temp ) {
		AddDebugLogLineM(false, logKadSearch, wxT("This is not the file we're looking for..."));
		return;
	}
	
	//Do we need more sources?
	if(!(!temp->IsStopped() && thePrefs::GetMaxSourcePerFile() > temp->GetSourceCount())) {
		AddDebugLogLineM(false, logKadSearch, wxT("No more sources needed for this file"));
		return;
	}

	uint32 ED2KID = wxUINT32_SWAP_ALWAYS(ip);
	
	if (theApp->ipfilter->IsFiltered(ED2KID)) {
		AddDebugLogLineM(false, logKadSearch, wxT("Source ip got filtered"));
		AddDebugLogLineM(false, logIPFilter, CFormat(wxT("IPfiltered source IP=%s received from Kademlia")) % Uint32toStringIP(ED2KID));
		return;
	}
	
	if( (ip == Kademlia::CKademlia::GetIPAddress() || ED2KID == theApp->GetED2KID()) && tcp == thePrefs::GetPort()) {
		AddDebugLogLineM(false, logKadSearch, wxT("Trying to add myself as source, ignore"));
		return;
	}
	
	CUpDownClient* ctemp = NULL; 
	switch( type ) {
		case 4:
		case 1: {
			//NonFirewalled users
			if(!tcp) {
				AddDebugLogLineM(false, logKadSearch, CFormat(wxT("Ignored source (IP=%s) received from Kademlia, no tcp port received")) % Uint32toStringIP(ip));
				return;
			}
			if (!IsGoodIP(ED2KID,thePrefs::FilterLanIPs())) {
				AddDebugLogLineM(false, logKadSearch, CFormat(wxT("%s got filtered")) % Uint32toStringIP(ED2KID));
				AddDebugLogLineM(false, logIPFilter, CFormat(wxT("Ignored source (IP=%s) received from Kademlia, filtered")) % Uint32toStringIP(ED2KID));
				return;
			}
			ctemp = new CUpDownClient(tcp,ip,0,0,temp,false, true);
			ctemp->SetSourceFrom(SF_KADEMLIA);
			ctemp->SetServerIP(serverip);
			ctemp->SetServerPort(serverport);
			ctemp->SetKadPort(udp);
			byte cID[16];
			pcontactID->ToByteArray(cID);
			ctemp->SetUserHash(CMD4Hash(cID));
			break;
		}
		case 2: {
			//Don't use this type... Some clients will process it wrong..
			break;
		}
		case 5:
		case 3: {
			//This will be a firewaled client connected to Kad only.
			//We set the clientID to 1 as a Kad user only has 1 buddy.
			ctemp = new CUpDownClient(tcp,1,0,0,temp,false, true);
			//The only reason we set the real IP is for when we get a callback
			//from this firewalled source, the compare method will match them.
			ctemp->SetSourceFrom(SF_KADEMLIA);
			ctemp->SetKadPort(udp);
			byte cID[16];
			pcontactID->ToByteArray(cID);
			ctemp->SetUserHash(CMD4Hash(cID));
			pbuddyID->ToByteArray(cID);
			ctemp->SetBuddyID(cID);
			ctemp->SetBuddyIP(serverip);
			ctemp->SetBuddyPort(serverport);
			break;
		}
		case 6: {
			// firewalled source which supports direct udp callback
			// if we are firewalled ourself, the source is useless to us
			if (theApp->IsFirewalled()) {
				break;
			}

			if ((byCryptOptions & 0x08) == 0){
				AddDebugLogLineM(false, logKadSearch, CFormat(wxT("Received Kad source type 6 (direct callback) which has the direct callback flag not set (%s)")) % Uint32toStringIP(ED2KID));
				break;
			}
			
			ctemp = new CUpDownClient(tcp, 1, 0, 0, temp, false, true);
			ctemp->SetSourceFrom(SF_KADEMLIA);
			ctemp->SetKadPort(udp);
			ctemp->SetIP(ED2KID); // need to set the Ip address, which cannot be used for TCP but for UDP
			byte cID[16];
			pcontactID->ToByteArray(cID);
			ctemp->SetUserHash(CMD4Hash(cID));
			pbuddyID->ToByteArray(cID);
		}
	}

	if (ctemp) {
		// add encryption settings
		ctemp->SetConnectOptions(byCryptOptions);

		AddDebugLogLineM(false, logKadSearch, CFormat(wxT("Happily adding a source (%s) type %d")) % Uint32_16toStringIP_Port(ED2KID, ctemp->GetUserPort()) % type);
		CheckAndAddSource(temp, ctemp);
	}
}

CPartFile* CDownloadQueue::GetFileByKadFileSearchID(uint32 id) const
{
	
	wxMutexLocker lock( m_mutex );
	
	for ( uint16 i = 0; i < m_filelist.size(); ++i ) {
		if ( id == m_filelist[i]->GetKadFileSearchID()) {
			return m_filelist[ i ];
		}
	}
	
	return NULL;
	
}

bool CDownloadQueue::DoKademliaFileRequest()
{
	return ((::GetTickCount() - lastkademliafilerequest) > KADEMLIAASKTIME);
}
// File_checked_for_headers