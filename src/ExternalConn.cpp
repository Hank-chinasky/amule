//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2005 Kry ( elkry@sourceforge.net / http://www.amule.org )
// Copyright (c) 2003-2005 aMule Team ( admin@amule.org / http://www.amule.org )
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA, 02111-1307, USA
//

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "ExternalConn.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"		// Needed for VERSION and CVSDATE
#endif

#include "ExternalConn.h"	// Interface declarations
#include "OtherFunctions.h"	// Needed for EncodeBase16
#include "updownclient.h"	// Needed for CUpDownClient
#include "Server.h"		// Needed for CServer
#include "ServerList.h"		// Needed for CServerList
#include "SharedFileList.h"	// Needed for CSharedFileList
#include "PartFile.h"		// Needed for CPartFile
#include "ServerConnect.h"		// Needed for CServerConnect
#include "UploadQueue.h"	// Needed for CUploadQueue
#include "DownloadQueue.h"	// Needed for CDownloadQueue
#include "amule.h"		// Needed for theApp
#include "SearchList.h"		// Needed for GetSearchResults
#include "IPFilter.h"		// Needed for CIPFilter
#include "ClientList.h"
#include "Preferences.h"	// Needed for CPreferences
#include "CMD4Hash.h"		// Needed for CMD4Hash
#include "Logger.h"
#include "GuiEvents.h"		// Needed for Notify_* macros
#include "NetworkFunctions.h"	// Needed for Uint32toStringIP()
#include "ECPacket.h"		// Needed for CECPacket, CECTag
#include "ECcodes.h"		// Needed for OPcodes, TAGnames
#include "ECSpecialTags.h"	// Needed for special EC tag creator classes
#include "Statistics.h"
#include "Format.h"

using namespace otherfunctions;

enum
{	// id for sockets
	SERVER_ID = 1000,
	AUTH_ID,
	SOCKET_ID
};


#ifndef AMULE_DAEMON
BEGIN_EVENT_TABLE(ExternalConn, wxEvtHandler)
	EVT_SOCKET(SERVER_ID, ExternalConn::OnServerEvent)
	EVT_SOCKET(AUTH_ID,   ExternalConn::OnSocketEvent)
	EVT_SOCKET(SOCKET_ID, ExternalConn::OnSocketEvent)
END_EVENT_TABLE()
#endif


ExternalConn::ExternalConn(amuleIPV4Address addr, wxString *msg)
#ifdef AMULE_DAEMON
 : wxThread(wxTHREAD_JOINABLE) 
#endif
{
	wxString msgLocal;
	m_ECServer = NULL;
	// Are we allowed to accept External Connections?
	if ( thePrefs::AcceptExternalConnections() && thePrefs::ECUseTCPPort() ) {
		// We must have a valid password, otherwise we will not allow EC connections
		if ( thePrefs::ECPassword().IsEmpty() ) {
			thePrefs::EnableExternalConnections( false );
			*msg += wxT("External connections disabled due to empty password!\n");
			AddLogLineM(true, _("External connections disabled due to empty password!"));
			return;
		}
		
		// Create the socket
#ifdef AMULE_DAEMON
		m_ECServer = new ECSocket(addr, 0);
#else
		m_ECServer = new ECSocket(addr, this, SERVER_ID);
#endif
		int port = addr.Service();
		wxString ip = addr.IPAddress();
		if (m_ECServer->Ok()) {
			msgLocal = wxT("*** TCP socket (ECServer) listening on ") + ip + 
				wxString::Format(wxT(":%d"), port);
			*msg += msgLocal + wxT("\n");
			AddLogLineM(false, msgLocal);
#ifdef AMULE_DAEMON
			if ( Create() != wxTHREAD_NO_ERROR ) {
				AddLogLineM(false, _("ExternalConn: failed to Create thread"));
				delete m_ECServer;
				// This prevents the destructor to do nasty things
				m_ECServer = NULL;
			} else {
				Run();
			}
#endif
		} else {
			msgLocal = wxT("Could not listen for external connections at ") + ip + 
				wxString::Format(wxT(":%d!"), port);
			*msg += msgLocal + wxT("\n");
			AddLogLineM(false, msgLocal);
		}
	} else {
		*msg += wxT("External connections disabled in config file\n");
		AddLogLineM(false,_("External connections disabled in config file"));
	}
}

ExternalConn::~ExternalConn() {
	delete m_ECServer;
}

#ifdef AMULE_DAEMON
void *ExternalConn::Entry()
{
        while ( !TestDestroy() ) {
        	if ( m_ECServer->WaitForAccept(1, 0) ) {
				wxSocketBase *client = m_ECServer->Accept();
				if ( !client ) {
					continue;
				}
				client->Notify(false);
				ExternalConnClientThread *cli_thread = new ExternalConnClientThread(this, client);
				cli_thread->Run();
        	}
	}
	return 0;
}
#else
void ExternalConn::OnServerEvent(wxSocketEvent& WXUNUSED(event)) {
	wxSocketBase *sock;
	// Accept new connection if there is one in the pending
	// connections queue, else exit. We use Accept(FALSE) for
	// non-blocking accept (although if we got here, there
	// should ALWAYS be a pending connection).
	sock = m_ECServer->Accept(false);
	if (sock) {
		AddLogLineM(false, _("New external connection accepted"));
		sock->SetEventHandler(*this, AUTH_ID);
		sock->SetNotify(wxSOCKET_INPUT_FLAG | wxSOCKET_LOST_FLAG);
		sock->Notify(true);
		m_numClients++;
		RegisterSocket(sock);
	} else {
		AddLogLineM(false, _("Error: couldn't accept a new external connection"));
	}
}

void ExternalConn::OnSocketEvent(wxSocketEvent& event) {
	wxSocketBase *sock = event.GetSocket();
	CECPacket * request = NULL;
	CECPacket * response = NULL;

	// Now we process the event
	switch(event.GetSocketEvent()) {
	case wxSOCKET_INPUT: {
		// We disable input events, so that the test doesn't trigger
		// wxSocketEvent again.
		sock->SetNotify(wxSOCKET_LOST_FLAG);		
		request = m_ECServer->ReadPacket(sock);		
		if(request == NULL) {
			AddLogLineM(false, _("Invalid EC packet received"));
			break;
		}
		if (event.GetId() == AUTH_ID) {
			response = Authenticate(request);
			delete request;	request = NULL;
			m_ECServer->WritePacket(sock, response);
			if (response->GetOpCode() != EC_OP_AUTH_OK) {
				// Access denied!
				AddLogLineM(false, _("Unauthorized access attempt. Connection closed."));
				delete response; response = NULL;
				sock->Destroy();
				return;
			} else {
				// Authenticated => change socket handler
				delete response; response = NULL;
				sock->SetEventHandler(*this, SOCKET_ID);
			}
		} else {
			response = ProcessRequest2(request, m_part_encoders[sock],
				m_shared_encoders[sock], m_obj_tagmap[sock]);
			delete request; request = NULL;
			m_ECServer->WritePacket(sock, response);
			delete response; response = NULL;
		}		
		// Re-Enable input events again.
		sock->SetNotify(wxSOCKET_LOST_FLAG | wxSOCKET_INPUT_FLAG);
		sock->Notify(true);
		break;
	}
		
	case wxSOCKET_LOST: {
		m_numClients--;
		// Destroy() should be used instead of delete wherever possible,
		// due to the fact that wxSocket uses 'delayed events' (see the
		// documentation for wxPostEvent) and we don't want an event to
		// arrive to the event handler (the frame, here) after the socket
		// has been deleted. Also, we might be doing some other thing with
		// the socket at the same time; for example, we might be in the
		// middle of a test or something. Destroy() takes care of all
		// this for us.
		AddLogLineM(false,_("External connection closed."));
		//sock->Destroy();
		sock->Close();
		UnregisterSocket(sock);
		// remove client data
		m_part_encoders.erase(sock);
		m_shared_encoders.erase(sock);
		m_obj_tagmap.erase(sock);
		break;
	}
	
	default: ;
	}
}
#endif

//
// Authentication
//
CECPacket *ExternalConn::Authenticate(const CECPacket *request)
{
	CECPacket *response;

	if (request == NULL) {
		response = new CECPacket(EC_OP_AUTH_FAIL);
		return response;
	}

	// Password must be specified if we are to allow remote connections
	if ( thePrefs::ECPassword().IsEmpty() ) {
		AddLogLineM(true, _("External connection refused due to empty password in preferences!"));	
		
		return new CECPacket(EC_OP_AUTH_FAIL);
	}
	

	if (request->GetOpCode() == EC_OP_AUTH_REQ) {
		const CECTag *clientName = request->GetTagByName(EC_TAG_CLIENT_NAME);
		const CECTag *clientVersion = request->GetTagByName(EC_TAG_CLIENT_VERSION);
		
		AddLogLineM(false, CFormat( _("Connecting client: %s %s") )
			% ( clientName ? clientName->GetStringData() : wxString(_("Unknown")) )
			% ( clientVersion ? clientVersion->GetStringData() : wxString(_("Unknown version")) ) );
		const CECTag *passwd = request->GetTagByName(EC_TAG_PASSWD_HASH);
		const CECTag *protocol = request->GetTagByName(EC_TAG_PROTOCOL_VERSION);
#ifdef CVSDATE
		if (request->GetTagByNameSafe(EC_TAG_CVSDATE)->GetStringData() != wxT(CVSDATE)) {
#else
		if (request->GetTagByName(EC_TAG_CVSDATE)) {
#endif
			response = new CECPacket(EC_OP_AUTH_FAIL);
			response->AddTag(CECTag(EC_TAG_STRING, wxTRANSLATE("Incorrect CVSDATE. Please run core and remote from the same CVS tarball.")));
		} else if (protocol != NULL) {
			uint16 proto_version = protocol->GetInt16Data();
			if (proto_version == EC_CURRENT_PROTOCOL_VERSION) {
				if (passwd && passwd->GetMD4Data() == CMD4Hash(thePrefs::ECPassword())) {
					response = new CECPacket(EC_OP_AUTH_OK);
				} else {
					response = new CECPacket(EC_OP_AUTH_FAIL);
					response->AddTag(CECTag(EC_TAG_STRING, wxTRANSLATE("Authentication failed.")));
				}
			} else {
				response = new CECPacket(EC_OP_AUTH_FAIL);
				response->AddTag(CECTag(EC_TAG_STRING, wxTRANSLATE("Invalid protocol version.")));
			}
		} else {
			response = new CECPacket(EC_OP_AUTH_FAIL);
			response->AddTag(CECTag(EC_TAG_STRING, wxTRANSLATE("Missing protocol version tag.")));
		}
	} else {
		response = new CECPacket(EC_OP_AUTH_FAIL);
		response->AddTag(CECTag(EC_TAG_STRING, wxTRANSLATE("Invalid request, you should first authenticate.")));
	}

	response->AddTag(CECTag(EC_TAG_SERVER_VERSION, wxT(VERSION)));

	if (response->GetOpCode() == EC_OP_AUTH_OK) {
		AddLogLineM(false, _("Access granted."));
	} else {
		AddLogLineM(false, wxGetTranslation(response->GetTagByIndex(0)->GetStringData()));
	}

	return response;
}

CECPacket *Get_EC_Response_StatRequest(const CECPacket *WXUNUSED(request))
{
	CECPacket *response = new CECPacket(EC_OP_STATS);

	//
	// ul/dl speeds
	response->AddTag(CECTag(EC_TAG_STATS_UL_SPEED, (uint32)(theApp.uploadqueue->GetKBps()*1024.0)));
	response->AddTag(CECTag(EC_TAG_STATS_DL_SPEED, (uint32)(theApp.downloadqueue->GetKBps()*1024.0)));
	response->AddTag(CECTag(EC_TAG_STATS_UL_SPEED_LIMIT, (uint32)(thePrefs::GetMaxUpload()*1024.0)));
	response->AddTag(CECTag(EC_TAG_STATS_DL_SPEED_LIMIT, (uint32)(thePrefs::GetMaxDownload()*1024.0)));
	
	response->AddTag(CECTag(EC_TAG_STATS_CURR_UL_COUNT,
		(uint32)theApp.uploadqueue->GetUploadQueueLength()));
	// get the source count
	uint32 stats[2];
	theApp.downloadqueue->GetDownloadStats(stats);
	response->AddTag(CECTag(EC_TAG_STATS_TOTAL_SRC_COUNT, stats[0]));
	response->AddTag(CECTag(EC_TAG_STATS_CURR_DL_COUNT, stats[1]));
	response->AddTag(CECTag(EC_TAG_STATS_TOTAL_DL_COUNT,
		(uint32)theApp.downloadqueue->GetFileCount()));
	response->AddTag(CECTag(EC_TAG_STATS_UL_QUEUE_LEN,
		(uint32)theApp.uploadqueue->GetWaitingUserCount()));

	return response;
}

CECPacket *Get_EC_Response_GetSharedFiles(const CECPacket *request, CKnownFile_Encoder_Map &encoders)
{
	wxASSERT(request->GetOpCode() == EC_OP_GET_SHARED_FILES);

	CECPacket *response = new CECPacket(EC_OP_SHARED_FILES);

	EC_DETAIL_LEVEL detail_level = request->GetDetailLevel();
	//
	// request can contain list of queried items
	CTagSet<CMD4Hash, EC_TAG_KNOWNFILE> queryitems(request);

	encoders.UpdateEncoders(theApp.sharedfiles);
	
	for (uint32 i = 0; i < theApp.sharedfiles->GetFileCount(); ++i) {
		CKnownFile *cur_file = (CKnownFile *)theApp.sharedfiles->GetFileByIndex(i);

		if ( !cur_file || (!queryitems.empty() && !queryitems.count(cur_file->GetFileHash())) ) {
			continue;
		}

		CEC_SharedFile_Tag filetag(cur_file, detail_level);
		CKnownFile_Encoder &enc = encoders[cur_file];
		if ( detail_level != EC_DETAIL_UPDATE ) {
			enc.ResetEncoder();
		}
		enc.Encode(&filetag);
		response->AddTag(filetag);
	}
	return response;
}

CECPacket *Get_EC_Response_GetSharedFiles(CKnownFile_Encoder_Map &encoders, CObjTagMap &tagmap)
{
	CECPacket *response = new CECPacket(EC_OP_SHARED_FILES);

	encoders.UpdateEncoders(theApp.sharedfiles);
	for (uint32 i = 0; i < theApp.sharedfiles->GetFileCount(); ++i) {
		CKnownFile *cur_file = (CKnownFile *)theApp.sharedfiles->GetFileByIndex(i);
		
		//
		// Hashes of tags are maintained on "per-object" basis. So, in this mode only
		// same kind of objects can go into particular query type.
		// Particulary here it means that files from download queue (aka partfiles)
		// will not ne shown as shared files. Remote gui can do combine them if wishes
		//
		if ( !cur_file || cur_file->IsPartFile() ) continue;

		CValueMap &valuemap = tagmap.GetValueMap(cur_file);
		CEC_SharedFile_Tag filetag(cur_file, valuemap);
		CKnownFile_Encoder &enc = encoders[cur_file];
		enc.Encode(&filetag);
		
		response->AddTag(filetag);
	}
	return response;
}

CECPacket *Get_EC_Response_GetWaitQueue(const CECPacket *request)
{
	wxASSERT(request->GetOpCode() == EC_OP_GET_WAIT_QUEUE);
	
	CECPacket *response = new CECPacket(EC_OP_WAIT_QUEUE);
	
	EC_DETAIL_LEVEL detail_level = request->GetDetailLevel();

	//
	// request can contain list of queried items
	CTagSet<uint32, EC_TAG_UPDOWN_CLIENT> queryitems(request);

	POSITION pos = theApp.uploadqueue->GetFirstFromWaitingList();
	while (	pos ) {

		CUpDownClient* cur_client = theApp.uploadqueue->GetNextFromWaitingList(pos);

		if ( !cur_client || (!queryitems.empty() && !queryitems.count(cur_client->GetUserID())) ) {
			continue;
		}
		CEC_UpDownClient_Tag cli_tag(cur_client, detail_level);
		
		response->AddTag(cli_tag);
	}

	return response;
}

CECPacket *Get_EC_Response_GetWaitQueue(CObjTagMap &tagmap)
{
	CECPacket *response = new CECPacket(EC_OP_WAIT_QUEUE);
	
	POSITION pos = theApp.uploadqueue->GetFirstFromWaitingList();
	while (	pos ) {

		CUpDownClient* cur_client = theApp.uploadqueue->GetNextFromWaitingList(pos);

		CValueMap &valuemap = tagmap.GetValueMap(cur_client);
		CEC_UpDownClient_Tag cli_tag(cur_client, valuemap);
		
		response->AddTag(cli_tag);
	}

	return response;
}

CECPacket *Get_EC_Response_GetUpQueue(const CECPacket *request)
{
	wxASSERT(request->GetOpCode() == EC_OP_GET_ULOAD_QUEUE);
	
	CECPacket *response = new CECPacket(EC_OP_ULOAD_QUEUE);
	
	EC_DETAIL_LEVEL detail_level = request->GetDetailLevel();

	//
	// request can contain list of queried items
	CTagSet<uint32, EC_TAG_UPDOWN_CLIENT> queryitems(request);
	
	POSITION pos = theApp.uploadqueue->GetFirstFromUploadList();
	while (	pos ) {

		CUpDownClient* cur_client = theApp.uploadqueue->GetQueueClientAt(pos);
		theApp.uploadqueue->GetNextFromUploadList(pos);

		if ( !cur_client || (!queryitems.empty() && !queryitems.count(cur_client->GetUserID())) ) {
			continue;
		}
		CEC_UpDownClient_Tag cli_tag(cur_client, detail_level);
		
		response->AddTag(cli_tag);
	}
	
	return response;
}	

CECPacket *Get_EC_Response_GetUpQueue(CObjTagMap &tagmap)
{
	CECPacket *response = new CECPacket(EC_OP_ULOAD_QUEUE);
	POSITION pos = theApp.uploadqueue->GetFirstFromUploadList();
	while (	pos ) {

		CUpDownClient* cur_client = theApp.uploadqueue->GetQueueClientAt(pos);
		theApp.uploadqueue->GetNextFromUploadList(pos);

		CValueMap &valuemap = tagmap.GetValueMap(cur_client);
		CEC_UpDownClient_Tag cli_tag(cur_client, valuemap);
		
		response->AddTag(cli_tag);
	}
	return response;
}

CECPacket *Get_EC_Response_GetDownloadQueue(CPartFile_Encoder_Map &encoders, CObjTagMap &tagmap)
{	
	CECPacket *response = new CECPacket(EC_OP_DLOAD_QUEUE);

	encoders.UpdateEncoders(theApp.downloadqueue);
	for (unsigned int i = 0; i < theApp.downloadqueue->GetFileCount(); i++) {
		CPartFile *cur_file = theApp.downloadqueue->GetFileByIndex(i);

		CValueMap &valuemap = tagmap.GetValueMap(cur_file);
		CEC_PartFile_Tag filetag(cur_file, valuemap);
		CPartFile_Encoder &enc = encoders[cur_file];
		enc.Encode(&filetag);
		
		response->AddTag(filetag);
	}	
	return 	response;
}

CECPacket *Get_EC_Response_GetDownloadQueue(const CECPacket *request, CPartFile_Encoder_Map &encoders)
{	
	CECPacket *response = new CECPacket(EC_OP_DLOAD_QUEUE);

	EC_DETAIL_LEVEL detail_level = request->GetDetailLevel();
	//
	// request can contain list of queried items
	CTagSet<CMD4Hash, EC_TAG_PARTFILE> queryitems(request);
	
	encoders.UpdateEncoders(theApp.downloadqueue);

	for (unsigned int i = 0; i < theApp.downloadqueue->GetFileCount(); i++) {
		CPartFile *cur_file = theApp.downloadqueue->GetFileByIndex(i);
	
		if ( !queryitems.empty() && !queryitems.count(cur_file->GetFileHash()) ) {
			continue;
		}

		CEC_PartFile_Tag filetag(cur_file, detail_level);
		
		CPartFile_Encoder &enc = encoders[cur_file];
		if ( detail_level != EC_DETAIL_UPDATE ) {
			enc.ResetEncoder();
		}
		enc.Encode(&filetag);

		response->AddTag(filetag);
	}
	return 	response;
}


CECPacket *Get_EC_Response_PartFile_Cmd(const CECPacket *request)
{
	CECPacket *response = NULL;

	// request can contain multiple files.
	for (int i = 0; i < request->GetTagCount(); ++i) {
		const CECTag *hashtag = request->GetTagByIndex(i);

		wxASSERT(hashtag->GetTagName() == EC_TAG_PARTFILE);

		CMD4Hash hash = hashtag->GetMD4Data();
		CPartFile *pfile = theApp.downloadqueue->GetFileByID( hash );
		
		if ( !pfile ) {
			AddLogLineM(false,_("Remote PartFile command failed: FileHash not found: ") + hash.Encode());
			response = new CECPacket(EC_OP_FAILED);
			response->AddTag(CECTag(EC_TAG_STRING, wxTRANSLATE("FileHash not found: ") + hash.Encode()));
			//return response;
			break;
		}
		switch (request->GetOpCode()) {
			case EC_OP_PARTFILE_REMOVE_NO_NEEDED:
				pfile->CleanUpSources(true,  false, false);
				break;
			case EC_OP_PARTFILE_REMOVE_FULL_QUEUE:
				pfile->CleanUpSources(false, true, false);
				break;
			case EC_OP_PARTFILE_REMOVE_HIGH_QUEUE:
				pfile->CleanUpSources(false, false, true);
				break;
			case EC_OP_PARTFILE_CLEANUP_SOURCES:
				pfile->CleanUpSources(true, true, true);
				break;
			case EC_OP_PARTFILE_SWAP_A4AF_THIS:
				if ((pfile->GetStatus(false) == PS_READY) ||
					(pfile->GetStatus(false) == PS_EMPTY)) {
					CPartFile::SourceSet::iterator it = pfile->A4AFsrclist.begin();
					while ( it != pfile->A4AFsrclist.end() ) {
						CUpDownClient *cur_source = *it++;
						
						cur_source->SwapToAnotherFile(true, false, false, pfile);
					}
				}
				break;
			case EC_OP_PARTFILE_SWAP_A4AF_THIS_AUTO:
				pfile->SetA4AFAuto(!pfile->IsA4AFAuto());
				break;
			case EC_OP_PARTFILE_SWAP_A4AF_OTHERS:
				if ((pfile->GetStatus(false) == PS_READY) ||
				    (pfile->GetStatus(false) == PS_EMPTY)) {
					CPartFile::SourceSet::iterator it = pfile->m_SrcList.begin();
					while ( it != pfile->m_SrcList.end() ) {
						CUpDownClient* cur_source = *it++;

						cur_source->SwapToAnotherFile(false, false, false, NULL);
					}
				}
				break;
			case EC_OP_PARTFILE_PAUSE:
				pfile->PauseFile();
				break;
			case EC_OP_PARTFILE_RESUME:
				pfile->ResumeFile();
				pfile->SavePartFile();
				break;
			case EC_OP_PARTFILE_STOP:
				pfile->StopFile();
				break;
			case EC_OP_PARTFILE_PRIO_SET: {
					int prio = hashtag->GetTagByIndexSafe(0)->GetInt32Data();
					if ( prio == PR_AUTO ) {
						pfile->SetAutoDownPriority(1);
					} else {
						pfile->SetAutoDownPriority(0);
						pfile->SetDownPriority(prio);
					}
				}
				break;
			case EC_OP_PARTFILE_DELETE:
				if ( thePrefs::StartNextFile() && (pfile->GetStatus() == PS_PAUSED) ) {
					theApp.downloadqueue->StartNextFile(pfile);
				}
				pfile->Delete();
				break;

			case EC_OP_PARTFILE_SET_CAT:
				pfile->SetCategory(hashtag->GetTagByIndexSafe(0)->GetInt8Data());
				break;

			default:
				response = new CECPacket(EC_OP_FAILED);
				response->AddTag(CECTag(EC_TAG_STRING, wxTRANSLATE("OOPS! OpCode processing error!")));
				break;
		}
	}
	if (!response) {
		response = new CECPacket(EC_OP_NOOP);
	}
	return response;
}

CECPacket *Get_EC_Response_Server(const CECPacket *request)
{
	CECPacket *response = NULL;
	const CECTag *srv_tag = request->GetTagByIndex(0);
	CServer *srv = 0;
	if ( srv_tag ) {
		uint32 srv_ip = srv_tag->GetIPv4Data().IP();
		for(uint32 i = 0; i < theApp.serverlist->GetServerCount(); i++) {
			CServer *curr_srv = theApp.serverlist->GetServerByIndex(i);
			// lfroen: never saw 2 servers on same IP !
			if ( curr_srv->GetIP() == srv_ip) {
				srv = curr_srv;
				break;
			}
		}
		// server tag passed, but server not found
		if ( !srv ) {
			response = new CECPacket(EC_OP_FAILED);
			response->AddTag(CECTag(EC_TAG_STRING,
						wxString(wxTRANSLATE("server not found: ")) + srv_tag->GetIPv4Data().StringIP()));
			return response;
		}
	}
	switch (request->GetOpCode()) {
		case EC_OP_SERVER_DISCONNECT:
			theApp.serverconnect->Disconnect();
			response = new CECPacket(EC_OP_NOOP);
			break;
		case EC_OP_SERVER_REMOVE:
			if ( srv ) {
				Notify_ServerRemove(srv);
				theApp.serverlist->RemoveServer(srv);
				response = new CECPacket(EC_OP_NOOP);
			} else {
				response = new CECPacket(EC_OP_FAILED);
				response->AddTag(CECTag(EC_TAG_STRING,
							wxTRANSLATE("need to define server to be removed")));
			}
			break;
		case EC_OP_SERVER_CONNECT:
			if ( srv ) {
				theApp.serverconnect->ConnectToServer(srv);
				response = new CECPacket(EC_OP_NOOP);
			} else {
				theApp.serverconnect->ConnectToAnyServer();
				response = new CECPacket(EC_OP_NOOP);
			}
			break;
	}
	if (!response) {
		response = new CECPacket(EC_OP_FAILED);
		response->AddTag(CECTag(EC_TAG_STRING, wxTRANSLATE("OOPS! OpCode processing error!")));
	}
	return response;
}

CECPacket *Get_EC_Response_Search_Results(const CECPacket *request)
{
	CECPacket *response = new CECPacket(
		theApp.searchlist->SearchInProgress() ? EC_OP_SEARCH_RESULTS : EC_OP_SEARCH_RESULTS_DONE);
		
	EC_DETAIL_LEVEL detail_level = request->GetDetailLevel();
	//
	// request can contain list of queried items
	CTagSet<CMD4Hash, EC_TAG_SEARCHFILE> queryitems(request);

	std::vector<CSearchFile*> list(theApp.searchlist->GetSearchResults(0xffff));
	std::vector<CSearchFile*>::const_iterator it = list.begin();
	while (it != list.end()) {
		CSearchFile* sf = *it++;
		if ( !queryitems.empty() && !queryitems.count(sf->GetFileHash()) ) {
			continue;
		}
		response->AddTag(CEC_SearchFile_Tag(sf, detail_level));
	}
	return response;
}

CECPacket *Get_EC_Response_Search_Results(CObjTagMap &tagmap)
{
	CECPacket *response = new CECPacket(
		theApp.searchlist->SearchInProgress() ? EC_OP_SEARCH_RESULTS : EC_OP_SEARCH_RESULTS_DONE);

	std::vector<CSearchFile*> list(theApp.searchlist->GetSearchResults(0xffff));
	std::vector<CSearchFile*>::const_iterator it = list.begin();
	while (it != list.end()) {
		CSearchFile* sf = *it++;
		CValueMap &valuemap = tagmap.GetValueMap(sf);
		response->AddTag(CEC_SearchFile_Tag(sf, valuemap));
	}
	return response;
}

CECPacket *Get_EC_Response_Search_Results_Download(const CECPacket *request)
{
	CECPacket *response = new CECPacket(EC_OP_STRINGS);
	for (int i = 0;i < request->GetTagCount();i++) {
		const CECTag *tag = request->GetTagByIndex(i);
		CMD4Hash hash = tag->GetMD4Data();
		theApp.searchlist->AddFileToDownloadByHash(hash);
	}
	return response;
}

CECPacket *Get_EC_Response_Search_Stop(const CECPacket *WXUNUSED(request))
{
	CECPacket *reply = new CECPacket(EC_OP_MISC_DATA);
	theApp.searchlist->StopGlobalSearch();
	return reply;
}

CECPacket *Get_EC_Response_Search(const CECPacket *request)
{
	wxString response;
	
	CEC_Search_Tag *search_request = (CEC_Search_Tag *)request->GetTagByIndex(0);
	theApp.searchlist->RemoveResults(0xffff);

	wxString text = search_request->SearchText();
	wxString file_type = search_request->SearchFileType();
	wxString ext = search_request->SearchExt();
		
	EC_SEARCH_TYPE search_type = search_request->SearchType();
	bool global_search = false;
	switch(search_type) {
		case EC_SEARCH_GLOBAL:
			global_search = true;
		case EC_SEARCH_LOCAL:
			if (!theApp.searchlist->StartNewSearch(0xffff, global_search, text, file_type, ext, search_request->MinSize(), search_request->MaxSize(), search_request->Avail())) {
				// Not connected?
				response = wxTRANSLATE("aMule is not connected! Cannot do search.");
			} else {
				response = wxTRANSLATE("Search in progress. Refetch results in a moment!");			
			}
			break;
		case EC_SEARCH_WEB:
				response = wxTRANSLATE("WebSearch from remote interface makes no sense.");
			break;
	}
	
	CECPacket *reply = new CECPacket(EC_OP_FAILED);
	// no reply - search in progress
	reply->AddTag(CECTag(EC_TAG_STRING, response));
		
	return reply;
}

CECPacket *Get_EC_Response_Set_SharedFile_Prio(const CECPacket *request)
{
	CECPacket *response = new CECPacket(EC_OP_NOOP);
	for (int i = 0;i < request->GetTagCount();i++) {
		const CECTag *tag = request->GetTagByIndex(i);
		CMD4Hash hash = tag->GetMD4Data();
		uint8 prio = tag->GetTagByIndexSafe(0)->GetInt32Data();
		CKnownFile* cur_file = theApp.sharedfiles->GetFileByID(hash);
		if ( !cur_file ) {
			continue;
		}
		if (prio == PR_AUTO) {
			cur_file->SetAutoUpPriority(1);
			cur_file->UpdateAutoUpPriority();
		} else {
			cur_file->SetAutoUpPriority(0);
			cur_file->SetUpPriority(prio);
		}
	}

	return response;
}

// init with some default size
CPartFile_Encoder::GapBuffer CPartFile_Encoder::m_gap_buffer(128);

// encoder side
CPartFile_Encoder::CPartFile_Encoder(CPartFile *file) :
	m_enc_data(file->GetPartCount(), file->gaplist.GetCount()*2)
{
	m_file = file;
}

// decoder side
CPartFile_Encoder::CPartFile_Encoder(int size): m_enc_data(size, 0)
{
	m_file = 0;
}

CPartFile_Encoder::~CPartFile_Encoder()
{
}
		
// stl side :)
CPartFile_Encoder::CPartFile_Encoder()
{
	m_file = 0;
}

CPartFile_Encoder::CPartFile_Encoder(const CPartFile_Encoder &obj) : m_enc_data(obj.m_enc_data)
{
	m_file = obj.m_file;
}

CPartFile_Encoder &CPartFile_Encoder::operator=(const CPartFile_Encoder &obj)
{
	m_file = obj.m_file;
	m_enc_data = obj.m_enc_data;
	return *this;
}


void CPartFile_Encoder::Encode(CECTag *parent)
{
	size_t gap_list_size = m_file->gaplist.GetCount();
	
	if ( m_gap_buffer.size() < gap_list_size * 2 ) {
		m_gap_buffer.clear();
		m_gap_buffer.resize(gap_list_size * 2);
	} 
	
	POSITION curr_pos = m_file->gaplist.GetHeadPosition();
	GapBuffer::iterator it = m_gap_buffer.begin();
	while ( curr_pos ) {
		Gap_Struct *curr = m_file->gaplist.GetNext(curr_pos);
		*it++ = ENDIAN_HTONL(curr->start);
		*it++ = ENDIAN_HTONL(curr->end);
	}

	m_enc_data.m_gap_status.Realloc(gap_list_size*2*sizeof(uint32));
	int gap_enc_size = 0;
	const unsigned char *gap_enc_data = m_enc_data.m_gap_status.Encode((unsigned char *)&m_gap_buffer[0], gap_enc_size);

	int part_enc_size;
	const unsigned char *part_enc_data = m_enc_data.m_part_status.Encode(m_file->m_SrcpartFrequency, part_enc_size);


	parent->AddTag(CECTag(EC_TAG_PARTFILE_PART_STATUS, part_enc_size, part_enc_data));
	
	//
	// Put data inside of tag in following order:
	// [num_of_gaps] [gap_enc_data]
	//
	unsigned char *tagdata;
	CECTag etag(EC_TAG_PARTFILE_GAP_STATUS,
		sizeof(uint32) + gap_enc_size, (void **)&tagdata);

	// real number of gaps - so remote size can realloc
	RawPokeUInt32( tagdata, ENDIAN_HTONL( gap_list_size ) );
	tagdata += sizeof(uint32);
	memcpy(tagdata, gap_enc_data, gap_enc_size);


	parent->AddTag(etag);

	curr_pos = m_file->requestedblocks_list.GetHeadPosition();
	wxASSERT(m_gap_buffer.size() >= (size_t)m_file->requestedblocks_list.GetCount() * 2);
	it = m_gap_buffer.begin();
	while ( curr_pos ) {
		Requested_Block_Struct* block = m_file->requestedblocks_list.GetNext(curr_pos);
		*it++ = ENDIAN_HTONL(block->StartOffset);
		*it++ = ENDIAN_HTONL(block->EndOffset);
	}
	parent->AddTag(CECTag(EC_TAG_PARTFILE_REQ_STATUS,
		m_file->requestedblocks_list.GetCount() * 2 * sizeof(uint32), (void *)&m_gap_buffer[0]));
}

// encoder side
CKnownFile_Encoder::CKnownFile_Encoder(CKnownFile *file) :
	m_enc_data(file->GetPartCount(), true)
{
	m_file = file;
}

CKnownFile_Encoder::CKnownFile_Encoder()
{
	m_file = 0;
}

CKnownFile_Encoder::~CKnownFile_Encoder()
{
}

CKnownFile_Encoder::CKnownFile_Encoder(const CKnownFile_Encoder &obj) : m_enc_data(obj.m_enc_data)
{
	m_file = obj.m_file;
}

CKnownFile_Encoder &CKnownFile_Encoder::operator=(const CKnownFile_Encoder &obj)
{
	m_file = obj.m_file;
	m_enc_data = obj.m_enc_data;
	return *this;
}

void CKnownFile_Encoder::Encode(CECTag *parent)
{
	int part_enc_size;
	const unsigned char *part_enc_data = m_enc_data.Encode(m_file->m_AvailPartFrequency, part_enc_size);

	parent->AddTag(CECTag(EC_TAG_PARTFILE_PART_STATUS, part_enc_size, part_enc_data));
}

CECPacket *GetStatsGraphs(const CECPacket *request)
{
	CECPacket *response = NULL;

	switch (request->GetDetailLevel()) {
		case EC_DETAIL_FULL: {
			double dTimestamp = 0.0;
			if (request->GetTagByName(EC_TAG_STATSGRAPH_LAST) != NULL) {
				wxString tmp = request->GetTagByName(EC_TAG_STATSGRAPH_LAST)->GetStringData();
				if (!tmp.ToDouble(&dTimestamp)) {
					dTimestamp = 0.0;
				}
			}
			uint16 nScale = request->GetTagByNameSafe(EC_TAG_STATSGRAPH_SCALE)->GetInt16Data();
			uint16 nMaxPoints = request->GetTagByNameSafe(EC_TAG_STATSGRAPH_WIDTH)->GetInt16Data();
			uint32 *graphData;
			unsigned int numPoints = theApp.statistics->GetHistoryForWeb(nMaxPoints, (double)nScale, &dTimestamp, &graphData);
			if (numPoints) {
				response = new CECPacket(EC_OP_STATSGRAPHS);
				response->AddTag(CECTag(EC_TAG_STATSGRAPH_DATA, 3 * numPoints * sizeof(uint32), graphData));
				delete [] graphData;
				response->AddTag(CECTag(EC_TAG_STATSGRAPH_LAST, wxString::Format(wxT("%f"), dTimestamp)));
			} else {
				response = new CECPacket(EC_OP_FAILED);
			}
		}
		case EC_DETAIL_INC_UPDATE:
		case EC_DETAIL_UPDATE:
		case EC_DETAIL_CMD:
			// No graphs
			break;
	}
	if (response) {
		return response;
	} else {
		return new CECPacket(EC_OP_FAILED);
	}
}

CECPacket *ExternalConn::ProcessRequest2(const CECPacket *request,
	CPartFile_Encoder_Map &enc_part_map, CKnownFile_Encoder_Map &enc_shared_map, CObjTagMap &objmap)
{

	if ( !request ) {
		return 0;
	}

	CECPacket *response = NULL;

	switch (request->GetOpCode()) {
		//
		// Misc commands
		//
		case EC_OP_SHUTDOWN:
			response = new CECPacket(EC_OP_NOOP);
			AddLogLineM(true, wxT("ExternalConn: shutdown requested"));
			theApp.ExitMainLoop();
			break;
		case EC_OP_ED2K_LINK: 
			for(int i = 0; i < request->GetTagCount();i++) {
				const CECTag *tag = request->GetTagByIndex(i);
				wxString link = tag->GetStringData();
				
				if ( theApp.downloadqueue->AddED2KLink( link ) ) {
					response = new CECPacket(EC_OP_NOOP);
				} else {
					AddLogLineM(true, _("ExternalConn: Unable to understand ed2k link '") + link + wxT("'."));
					response = new CECPacket(EC_OP_FAILED);
				}
			}
			break;
		//
		// Status requests
		//
		case EC_OP_STAT_REQ:
			response = Get_EC_Response_StatRequest(request);
		case EC_OP_GET_CONNSTATE:
			if (!response) {
				response = new CECPacket(EC_OP_MISC_DATA);
			}
			response->AddTag(CEC_ConnState_Tag(request->GetDetailLevel()));
			break;
		//
		//
		//
		case EC_OP_GET_SHARED_FILES:
			if ( request->GetDetailLevel() == EC_DETAIL_INC_UPDATE ) {
				response = Get_EC_Response_GetSharedFiles(enc_shared_map, objmap);
			} else {
				response = Get_EC_Response_GetSharedFiles(request, enc_shared_map);
			}
			break;
		case EC_OP_GET_DLOAD_QUEUE:
			if ( request->GetDetailLevel() == EC_DETAIL_INC_UPDATE ) {
				response = Get_EC_Response_GetDownloadQueue(enc_part_map, objmap);
			} else {
				response = Get_EC_Response_GetDownloadQueue(request, enc_part_map);
			}
			break;
		case EC_OP_GET_ULOAD_QUEUE:
			if ( request->GetDetailLevel() == EC_DETAIL_INC_UPDATE ) {
				response = Get_EC_Response_GetUpQueue(objmap);
			} else {
				response = Get_EC_Response_GetUpQueue(request);
			}
			break;
		case EC_OP_GET_WAIT_QUEUE:
			if ( request->GetDetailLevel() == EC_DETAIL_INC_UPDATE ) {
				response = Get_EC_Response_GetWaitQueue(objmap);
			} else {
				response = Get_EC_Response_GetWaitQueue(request);
			}
			break;
		case EC_OP_PARTFILE_REMOVE_NO_NEEDED:
		case EC_OP_PARTFILE_REMOVE_FULL_QUEUE:
		case EC_OP_PARTFILE_REMOVE_HIGH_QUEUE:
		case EC_OP_PARTFILE_CLEANUP_SOURCES:
		case EC_OP_PARTFILE_SWAP_A4AF_THIS:
		case EC_OP_PARTFILE_SWAP_A4AF_THIS_AUTO:
		case EC_OP_PARTFILE_SWAP_A4AF_OTHERS:
		case EC_OP_PARTFILE_PAUSE:
		case EC_OP_PARTFILE_RESUME:
		case EC_OP_PARTFILE_STOP:
		case EC_OP_PARTFILE_PRIO_SET:
		case EC_OP_PARTFILE_DELETE:
		case EC_OP_PARTFILE_SET_CAT:
			response = Get_EC_Response_PartFile_Cmd(request);
			break;
		case EC_OP_SHAREDFILES_RELOAD:
			theApp.sharedfiles->Reload();
			response = new CECPacket(EC_OP_NOOP);
			break;

		case EC_OP_SHARED_SET_PRIO:
			response = Get_EC_Response_Set_SharedFile_Prio(request);
			break;
			
		//
		// Server commands
		//
		case EC_OP_SERVER_DISCONNECT:
		case EC_OP_SERVER_CONNECT:
		case EC_OP_SERVER_REMOVE:
			response = Get_EC_Response_Server(request);
			break;
		case EC_OP_GET_SERVER_LIST: {
				response = new CECPacket(EC_OP_SERVER_LIST);
				EC_DETAIL_LEVEL detail_level = request->GetDetailLevel();
				for(uint32 i = 0; i < theApp.serverlist->GetServerCount(); i++) {
					response->AddTag(CEC_Server_Tag(theApp.serverlist->GetServerByIndex(i), detail_level));
				}
			}
			break;
		case EC_OP_SERVER_UPDATE_FROM_URL:
			theApp.serverlist->UpdateServerMetFromURL(request->GetTagByIndexSafe(0)->GetStringData());
			response = new CECPacket(EC_OP_NOOP);
			break;
		//
		// IPFilter
		//
		case EC_OP_IPFILTER_RELOAD:
			theApp.ipfilter->Reload();
			response = new CECPacket(EC_OP_NOOP);
			break;
		//
		// Search
		//
		case EC_OP_SEARCH_START:
			response = Get_EC_Response_Search(request);
			break;

		case EC_OP_SEARCH_STOP:
			response = Get_EC_Response_Search_Stop(request);
			break;

		case EC_OP_SEARCH_RESULTS:
			if ( request->GetDetailLevel() == EC_DETAIL_INC_UPDATE ) {
				response = Get_EC_Response_Search_Results(objmap);
			} else {
				response = Get_EC_Response_Search_Results(request);
			}
			break;

		case EC_OP_DOWNLOAD_SEARCH_RESULT:
			response = Get_EC_Response_Search_Results_Download(request);
			break;
		//
		// Preferences
		//
		case EC_OP_GET_PREFERENCES:
			response = new CEC_Prefs_Packet(request->GetTagByNameSafe(EC_TAG_SELECT_PREFS)->GetInt32Data(), request->GetDetailLevel());
			break;
		case EC_OP_SET_PREFERENCES:
			((CEC_Prefs_Packet *)request)->Apply(true);
			theApp.glob_prefs->Save();
			response = new CECPacket(EC_OP_NOOP);
			break;
		//
		// Logging
		//
		case EC_OP_ADDLOGLINE:
			AddLogLineM( (request->GetTagByName(EC_TAG_LOG_TO_STATUS) != NULL), request->GetTagByNameSafe(EC_TAG_STRING)->GetStringData() );
			response = new CECPacket(EC_OP_NOOP);
			break;
		case EC_OP_ADDDEBUGLOGLINE:
			AddDebugLogLineM( (request->GetTagByName(EC_TAG_LOG_TO_STATUS) != NULL), logGeneral, request->GetTagByNameSafe(EC_TAG_STRING)->GetStringData() );
			response = new CECPacket(EC_OP_NOOP);
			break;
		case EC_OP_GET_LOG:
			response = new CECPacket(EC_OP_LOG);
			response->AddTag(CECTag(EC_TAG_STRING, theApp.GetLog(false)));
			break;
		case EC_OP_GET_DEBUGLOG:
			response = new CECPacket(EC_OP_DEBUGLOG);
			response->AddTag(CECTag(EC_TAG_STRING, theApp.GetDebugLog(false)));
			break;
		case EC_OP_RESET_LOG:
			theApp.GetLog(true);
			response = new CECPacket(EC_OP_NOOP);
			break;
		case EC_OP_RESET_DEBUGLOG:
			theApp.GetDebugLog(true);
			response = new CECPacket(EC_OP_NOOP);
			break;
		case EC_OP_GET_LAST_LOG_ENTRY:
			{
				wxString tmp = theApp.GetLog(false);
				if (tmp.Last() == '\n') {
					tmp.RemoveLast();
				}
				response = new CECPacket(EC_OP_LOG);
				response->AddTag(CECTag(EC_TAG_STRING, tmp.AfterLast('\n')));
			}
			break;
		case EC_OP_GET_SERVERINFO:
			response = new CECPacket(EC_OP_SERVERINFO);
			response->AddTag(CECTag(EC_TAG_STRING, theApp.GetServerLog(false)));
			break;
		case EC_OP_CLEAR_SERVERINFO:
			theApp.GetServerLog(true);
			response = new CECPacket(EC_OP_NOOP);
			break;
		//
		// Statistics
		//
		case EC_OP_GET_STATSGRAPHS:
			response = GetStatsGraphs(request);
//			response = new CECPacket(EC_OP_FAILED);
			break;
		case EC_OP_GET_STATSTREE:
			response = new CECPacket(EC_OP_STATSTREE);
			response->AddTag(CEC_Tree_Tag(theApp.statistics->statstree.begin().begin()));
			if (request->GetDetailLevel() == EC_DETAIL_FULL) {
				response->AddTag(CECTag(EC_TAG_SERVER_VERSION, wxT(PACKAGE_VERSION)));
				response->AddTag(CECTag(EC_TAG_USER_NICK, thePrefs::GetUserNick()));
			}
			break;		
		
		default:
			AddLogLineM(false, _("ExternalConn: invalid opcode received"));
			break;
	}	
	return response;
}


ExternalConnClientThread::ExternalConnClientThread(ExternalConn *owner, wxSocketBase *sock) : wxThread()
{
	m_owner = owner;
	m_sock = sock;
	if ( Create() != wxTHREAD_NO_ERROR ) {
		AddLogLineM(false, _("ExternalConnClientThread: failed to Create thread\n"));
	}
	RegisterSocket(sock);
}

ExternalConnClientThread::~ExternalConnClientThread()
{
	UnregisterSocket(m_sock);
	delete m_sock;
}

void *ExternalConnClientThread::Entry()
{
	CECPacket *request = m_owner->m_ECServer->ReadPacket(m_sock);
	CECPacket *response = m_owner->Authenticate(request);
	m_owner->m_ECServer->WritePacket(m_sock, response);
	if (response->GetOpCode() != EC_OP_AUTH_OK) {
		//
		// Access denied!
		//
		AddLogLineM(false, _("Unauthorized access attempt. Connection closed.\n"));
		return 0;
	}
	//
	// Authenticated
	//
	while ( !TestDestroy() ) {
		if ( m_sock->WaitForLost(0, 0) ) {
			AddLogLineM(false, _("ExternalConnClientThread: connection closed\n"));
			break;
		}
		if (m_sock->WaitForRead(1, 0)) {
			request = m_owner->m_ECServer->ReadPacket(m_sock);
			response = m_owner->ProcessRequest2(request, m_part_encoders, m_shared_encoders, m_obj_tagmap);
			delete request;
			if ( response ) {
				m_owner->m_ECServer->WritePacket(m_sock, response);
				delete response;
			}
		}
	}
	return 0;
}
