//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2005 aMule Team ( admin@amule.org / http://www.amule.org )
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
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA, 02111-1307, USA
//

#include "ListenSocket.h"	// Interface declarations

#include "amule.h"		// Needed for theApp
#include "OtherFunctions.h"	// Needed for md4cpy
#include "Server.h"		// Needed for CServer
#include "ServerList.h"		// Needed for CServerList
#include "updownclient.h"	// Needed for CUpDownClient
#include "OPCodes.h"		// Needed for CONNECTION_TIMEOUT
#include "DownloadQueue.h"	// Needed for CDownloadQueue
#include "ClientList.h"		// Needed for CClientList
#include "IPFilter.h"		// Needed for CIPFilter
#include "SharedFileList.h"	// Needed for CSharedFileList
#include "PartFile.h"		// Needed for CPartFile
#include "Preferences.h"	// Needed for CPreferences
#include "MemFile.h"		// Needed for CMemFile
#include "Packet.h"		// Needed for CPacket
#include "UploadQueue.h"	// Needed for CUploadQueue
#include "OtherStructs.h"	// Needed for Requested_Block_Struct
#include "ServerConnect.h"	// Needed for CServerConnect
#include "Statistics.h"		// Needed for theStats
#include "Logger.h"
#include "Format.h"		// Needed for CFormat

#include <wx/listimpl.cpp>
#include <wx/dynarray.h>
#include <wx/arrimpl.cpp>	// this is a magic incantation which must be done!
#include <wx/tokenzr.h> 	// Needed for wxStringTokenizer

#include <vector>

#include "kademlia/kademlia/Kademlia.h"
#include "kademlia/kademlia/Prefs.h"
#include "ClientUDPSocket.h"

//#define __PACKET_RECV_DUMP__

IMPLEMENT_DYNAMIC_CLASS(CClientReqSocket,CEMSocket)

//------------------------------------------------------------------------------
// CClientReqSocketHandler
//------------------------------------------------------------------------------
BEGIN_EVENT_TABLE(CClientReqSocketHandler, wxEvtHandler)
	EVT_SOCKET(CLIENTREQSOCKET_HANDLER, CClientReqSocketHandler::ClientReqSocketHandler)
END_EVENT_TABLE()

CClientReqSocketHandler::CClientReqSocketHandler(CClientReqSocket* )
{
}

void CClientReqSocketHandler::ClientReqSocketHandler(wxSocketEvent& event)
{
	CClientReqSocket *socket = dynamic_cast<CClientReqSocket *>(event.GetSocket());
	wxASSERT(socket);
	if (!socket) {
		return;
	}
	
	if (socket->OnDestroy() || socket->deletethis) {
		return;
	}
	
	switch(event.GetSocketEvent()) {
		case wxSOCKET_LOST:
			socket->OnError(0xFEFF /* SOCKET_LOST is not an error */);
			break;
		case wxSOCKET_INPUT:
			socket->OnReceive(0);
			break;
		case wxSOCKET_OUTPUT:
			socket->OnSend(0);
			break;
		case wxSOCKET_CONNECTION:
			// connection stablished, nothing to do about it?
			socket->OnConnect(socket->Error() ? socket->LastError() : 0);
			break;
		default:
			// Nothing should arrive here...
			wxASSERT(0);
			break;
	}
}

//
// There can be only one. :)
//
static CClientReqSocketHandler g_clientReqSocketHandler;


//------------------------------------------------------------------------------
// CClientReqSocket
//------------------------------------------------------------------------------

WX_DEFINE_OBJARRAY(ArrayOfwxStrings)

CClientReqSocket::CClientReqSocket(CUpDownClient* in_client, const CProxyData *ProxyData)
	: CEMSocket(ProxyData)
{
	SetClient(in_client);
	ResetTimeOutTimer();
	deletethis = false;

	my_handler = &g_clientReqSocketHandler;
	SetEventHandler(*my_handler, CLIENTREQSOCKET_HANDLER);
	SetNotify(
		wxSOCKET_CONNECTION_FLAG |
		wxSOCKET_INPUT_FLAG |
		wxSOCKET_OUTPUT_FLAG |
		wxSOCKET_LOST_FLAG);
	Notify(true);

	theApp.listensocket->AddSocket(this);
	theApp.listensocket->AddConnection();
}


CClientReqSocket::~CClientReqSocket()
{
	// remove event handler
	SetNotify(0);
	Notify(false);

	if (m_client) {
		m_client->SetSocket( NULL );
	}
	m_client = NULL;

	if (theApp.listensocket && !theApp.listensocket->OnShutdown()) {
		theApp.listensocket->RemoveSocket(this);
	}
}


void CClientReqSocket::ResetTimeOutTimer()
{
	timeout_timer = ::GetTickCount();
}


bool CClientReqSocket::CheckTimeOut()
{
	// 0.42x
	uint32 uTimeout = GetTimeOut();
	if (m_client) {

		if (m_client->GetKadState() == KS_CONNECTED_BUDDY) {
			//We originally ignored the timeout here for buddies.
			//This was a stupid idea on my part. There is now a ping/pong system
			//for buddies. This ping/pong system now prevents timeouts.
			//This release will allow lowID clients with KadVersion 0 to remain connected.
			//But a soon future version needs to allow these older clients to time out to prevent dead connections from continuing.
			//JOHNTODO: Don't forget to remove backward support in a future release.
			if ( m_client->GetKadVersion() == 0 ) {
				return false;
			}
			
			uTimeout += MIN2MS(15);		
		}
		
		if (m_client->GetChatState() != MS_NONE) {
			uTimeout += CONNECTION_TIMEOUT;		
		}
	}
	
	if (::GetTickCount() - timeout_timer > uTimeout){
		timeout_timer = ::GetTickCount();
		Disconnect(wxT("Timeout"));
		return true;
	}
	
	return false;	
}


void CClientReqSocket::SetClient(CUpDownClient* pClient)
{
	m_client = pClient;
	if (m_client) {
		m_client->SetSocket( this );
	}
}


void CClientReqSocket::OnClose(int nErrorCode)
{
	// 0.42x
	wxASSERT(theApp.listensocket->IsValidSocket(this));
	CEMSocket::OnClose(nErrorCode);
	if (nErrorCode) {
		Disconnect(wxString::Format(wxT("Closed: %u"), nErrorCode));
	} else {
		Disconnect(wxT("Close"));
	}
}


void CClientReqSocket::Disconnect(const wxString& strReason)
{
	byConnected = ES_DISCONNECTED;
	if (m_client) {
		if (m_client->Disconnected(strReason, true)) {
			// Somehow, Safe_Delete() is beeing called by Disconnected(),
			// or any other function that sets m_client to NULL,
			// so we must check m_client first.
			if (m_client) {
				m_client->SetSocket( NULL );
				m_client->Safe_Delete();
			}
		} 
		m_client = NULL;
	}
	
	Safe_Delete();
}


void CClientReqSocket::Safe_Delete()
{
	if ( !deletethis && !OnDestroy() ) {
		// Paranoia is back.
		SetNotify(0);
		Notify(false);
		// lfroen: first of all - stop handler
		deletethis = true;

		if (m_client) {
			m_client->SetSocket( NULL );
			m_client = NULL;
		}
		byConnected = ES_DISCONNECTED;
		Close(); // Destroy is suposed to call Close(), but.. it doesn't hurt.
		Destroy();
	}
}


bool CClientReqSocket::ProcessPacket(const char* packet, uint32 size, uint8 opcode)
{
	#ifdef __PACKET_RECV_DUMP__
	printf("Rec: OPCODE %x \n",opcode);
	DumpMem(packet,size);
	#endif
	if (!m_client && opcode != OP_HELLO) {
		throw wxString(wxT("Asks for something without saying hello"));
	} else if (m_client && opcode != OP_HELLO && opcode != OP_HELLOANSWER)
		m_client->CheckHandshakeFinished(OP_EDONKEYPROT, opcode);
	
	switch(opcode) {
		case OP_HELLOANSWER: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_HELLOANSWER") );
			theStats::AddDownOverheadOther(size);
			m_client->ProcessHelloAnswer(packet,size);

			// start secure identification, if
			//  - we have received OP_EMULEINFO and OP_HELLOANSWER (old eMule)
			//	- we have received eMule-OP_HELLOANSWER (new eMule)
			if (m_client->GetInfoPacketsReceived() == IP_BOTH) {
				m_client->InfoPacketsReceived();
			}
			
			// Socket might die because of sending in InfoPacketsReceived, so check
			if (m_client) {
				m_client->ConnectionEstablished();
			}
			
			// Socket might die on ConnectionEstablished somehow. Check it.
			if (m_client) {					
				Notify_UploadCtrlRefreshClient( m_client );
			}
			
			break;
		}
		case OP_HELLO: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_HELLO") );
							
			theStats::AddDownOverheadOther(size);
			bool bNewClient = !m_client;				
			if (bNewClient) {
				// create new client to save standart informations
				m_client = new CUpDownClient(this);
			}
			// client->ProcessHelloPacket(packet,size);
			bool bIsMuleHello = false;
			
			try{
				bIsMuleHello = m_client->ProcessHelloPacket(packet,size);
			} catch(...) {
				if (bNewClient && m_client) {
					// Don't let CUpDownClient::Disconnected be processed for a client which is not in the list of clients.
					m_client->Safe_Delete();
					m_client = NULL;
				}
				throw;
			}

			// if IP is filtered, dont reply but disconnect...
			if (theApp.ipfilter->IsFiltered(m_client->GetIP())) {
				if (bNewClient) {
					m_client->Safe_Delete();
					m_client = NULL;
				}
				Disconnect(wxT("IPFilter"));
				return false;
			}
					
			wxASSERT(m_client);
			
			// now we check if we now this client already. if yes this socket will
			// be attached to the known client, the new client will be deleted
			// and the var. "client" will point to the known client.
			// if not we keep our new-constructed client ;)
			if (theApp.clientlist->AttachToAlreadyKnown(&m_client,this)) {
				// update the old client informations
				bIsMuleHello = m_client->ProcessHelloPacket(packet,size);
			} else {
				theApp.clientlist->AddClient(m_client);
				m_client->SetCommentDirty();
			}
			Notify_UploadCtrlRefreshClient( m_client );
			// send a response packet with standart informations
			if ((m_client->GetHashType() == SO_EMULE) && !bIsMuleHello) {
				m_client->SendMuleInfoPacket(false);				
			}
			
			// Client might die from Sending in SendMuleInfoPacket, so check
			if ( m_client ) {
				m_client->SendHelloAnswer();
			}
						
			// Kry - If the other side supports it, send OS_INFO
			// Client might die from Sending in SendHelloAnswer, so check				
			if (m_client && m_client->m_fOsInfoSupport) {
				m_client->SendMuleInfoPacket(false,true); // Send the OS Info tag on the recycled Mule Info
			}				
			
			// Client might die from Sending in SendMuleInfoPacket, so check
			if ( m_client ) {
				m_client->ConnectionEstablished();
			}
			
			// start secure identification, if
			//	- we have received eMule-OP_HELLO (new eMule)				
			if (m_client && m_client->GetInfoPacketsReceived() == IP_BOTH) {
					m_client->InfoPacketsReceived();		
			}
			
			break;
		}
		case OP_REQUESTFILENAME: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_REQUESTFILENAME") );
			
			theStats::AddDownOverheadFileRequest(size);
			// IP banned, no answer for this request
			if (m_client->IsBanned()) {
				break;
			}
			if (size >= 16) {
				if (!m_client->GetWaitStartTime()) {
					m_client->SetWaitStartTime();
				}
				CMemFile data_in((byte*)packet,size);
				CMD4Hash reqfilehash = data_in.ReadHash();
				CKnownFile *reqfile = theApp.sharedfiles->GetFileByID(reqfilehash);
				if ( reqfile == NULL ) {
					reqfile = theApp.downloadqueue->GetFileByID(reqfilehash);
					if ( !( reqfile != NULL && reqfile->GetFileSize() > PARTSIZE ) ) {
						break;
					}
				}
				// if we are downloading this file, this could be a new source
				// no passive adding of files with only one part
				if (reqfile->IsPartFile() && reqfile->GetFileSize() > PARTSIZE) {
					if (thePrefs::GetMaxSourcePerFile() > 
						((CPartFile*)reqfile)->GetSourceCount()) {
						theApp.downloadqueue->CheckAndAddKnownSource((CPartFile*)reqfile, m_client);
					}
				}

				// check to see if this is a new file they are asking for
				if (m_client->GetUploadFileID() != reqfilehash) {
						m_client->SetCommentDirty();
				}

				m_client->SetUploadFileID(reqfile);
				m_client->ProcessExtendedInfo(&data_in, reqfile);
				
				// send filename etc
				CMemFile data_out(128);
				data_out.WriteHash(reqfile->GetFileHash());
				data_out.WriteString(reqfile->GetFileName());
				CPacket* packet = new CPacket(&data_out);
				packet->SetOpCode(OP_REQFILENAMEANSWER);
				
				theStats::AddUpOverheadFileRequest(packet->GetPacketSize());
				SendPacket(packet,true);

				// SendPacket might kill the socket, so check
				if (m_client)
					m_client->SendCommentInfo(reqfile);

				break;
			}
			throw wxString(wxT("Invalid OP_REQUESTFILENAME packet size"));
			break;  
		}
		case OP_SETREQFILEID: {	// 0.43b EXCEPT track of bad clients
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_SETREQFILEID") );
			
			theStats::AddDownOverheadFileRequest(size);
			
			if (m_client->IsBanned()) {
				break;
			}
			
			// DbT:FileRequest
			if (size == 16) {
				if (!m_client->GetWaitStartTime()) {
					m_client->SetWaitStartTime();
				}

				const CMD4Hash fileID((byte*)packet);
				CKnownFile *reqfile = theApp.sharedfiles->GetFileByID(fileID);
				if ( reqfile == NULL ) {
					reqfile = theApp.downloadqueue->GetFileByID(fileID);
					if ( !( reqfile  != NULL && reqfile->GetFileSize() > PARTSIZE ) ) {
						CPacket* replypacket = new CPacket(OP_FILEREQANSNOFIL, 16);
						replypacket->Copy16ToDataBuffer(fileID.GetHash());
						theStats::AddUpOverheadFileRequest(replypacket->GetPacketSize());
						SendPacket(replypacket, true);
						break;
					}
				}

				// check to see if this is a new file they are asking for
				if (m_client->GetUploadFileID() != fileID) {
					m_client->SetCommentDirty();
				}

				m_client->SetUploadFileID(reqfile);
				// send filestatus
				CMemFile data(16+16);
				data.WriteHash(reqfile->GetFileHash());
				if (reqfile->IsPartFile()) {
					((CPartFile*)reqfile)->WritePartStatus(&data);
				} else {
					data.WriteUInt16(0);
				}
				CPacket* packet = new CPacket(&data);
				packet->SetOpCode(OP_FILESTATUS);
				
				theStats::AddUpOverheadFileRequest(packet->GetPacketSize());
				SendPacket(packet, true);
				break;
			}
			throw wxString(wxT("Invalid OP_FILEREQUEST packet size"));
			break;
			// DbT:End
		}			
		
		case OP_FILEREQANSNOFIL: {	// 0.43b protocol, lacks ZZ's download manager on swap
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_FILEREQANSNOFIL") );
			
			theStats::AddDownOverheadFileRequest(size);
			if (size == 16) {
				// if that client does not have my file maybe has another different
				CPartFile* reqfile = theApp.downloadqueue->GetFileByID(CMD4Hash((byte*)packet));
				if ( reqfile) {
					reqfile->AddDeadSource( m_client );
				} else {
					break;
				}
					
				// we try to swap to another file ignoring no needed parts files
				switch (m_client->GetDownloadState()) {
					case DS_CONNECTED:
					case DS_ONQUEUE:
					case DS_NONEEDEDPARTS:
					if (!m_client->SwapToAnotherFile(true, true, true, NULL)) {
						theApp.downloadqueue->RemoveSource(m_client);
					}
					break;
				}
				break;
			}
			throw wxString(wxT("Invalid OP_FILEREQUEST packet size"));
			break;
		}
		
		case OP_REQFILENAMEANSWER: {	// 0.43b except check for bad clients
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_REQFILENAMEANSWER") );
			
			theStats::AddDownOverheadFileRequest(size);
			CMemFile data((byte*)packet,size);
			CMD4Hash hash = data.ReadHash();
			const CPartFile* file = theApp.downloadqueue->GetFileByID(hash);
			m_client->ProcessFileInfo(&data, file);
			break;
		}
		
		case OP_FILESTATUS: {		// 0.43b except check for bad clients
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_FILESTATUS") );
			
			theStats::AddDownOverheadFileRequest(size);
			CMemFile data((byte*)packet,size);
			CMD4Hash hash = data.ReadHash();
			const CPartFile* file = theApp.downloadqueue->GetFileByID(hash);
			m_client->ProcessFileStatus(false, &data, file);
			break;
		}
		
		case OP_STARTUPLOADREQ: {
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_STARTUPLOADREQ") );
			
			theStats::AddDownOverheadFileRequest(size);
		
			if (!m_client->CheckHandshakeFinished(OP_EDONKEYPROT, opcode)) {
				break;
			}
			
			m_client->CheckForAggressive();
			if ( m_client->IsBanned() ) {
				break;
			}
			
			if (size == 16) {
				const CMD4Hash fileID((byte*)packet);
				CKnownFile* reqfile = theApp.sharedfiles->GetFileByID(fileID);
				if (reqfile) {
					if (m_client->GetUploadFileID() != fileID) {
						m_client->SetCommentDirty();
					}
					m_client->SetUploadFileID(reqfile);
					m_client->SendCommentInfo(reqfile);

					// Socket might die because of SendCommentInfo, so check
					if (m_client)
						theApp.uploadqueue->AddClientToQueue(m_client);
				}
			}
			break;
		}
		
		case OP_QUEUERANK: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_QUEUERANK") );
			 
			theStats::AddDownOverheadFileRequest(size);
			CMemFile data((byte*)packet,size);
			uint32 rank = data.ReadUInt32();
			
			m_client->SetRemoteQueueRank(rank);
			break;
		}
		
		case OP_ACCEPTUPLOADREQ: {	// 0.42e (xcept khaos stats)
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ACCEPTUPLOADREQ") );
			
			theStats::AddDownOverheadFileRequest(size);
			if (m_client->GetRequestFile() && !m_client->GetRequestFile()->IsStopped() && (m_client->GetRequestFile()->GetStatus()==PS_READY || m_client->GetRequestFile()->GetStatus()==PS_EMPTY)) {
				if (m_client->GetDownloadState() == DS_ONQUEUE ) {
					m_client->SetDownloadState(DS_DOWNLOADING);
					m_client->m_lastPartAsked = 0xffff; // Reset current downloaded Chunk // Maella -Enhanced Chunk Selection- (based on jicxicmic)
					m_client->SendBlockRequests();
				}
			} else {
				if (!m_client->GetSentCancelTransfer()) {
					CPacket* packet = new CPacket(OP_CANCELTRANSFER,0);
					theStats::AddUpOverheadFileRequest(packet->GetPacketSize());
					m_client->SendPacket(packet,true,true);
					
					// SendPacket can cause the socket to die, so check
					if (m_client)
						m_client->SetSentCancelTransfer(1);
				}
				
				if (m_client)
					m_client->SetDownloadState((m_client->GetRequestFile()==NULL || m_client->GetRequestFile()->IsStopped()) ? DS_NONE : DS_ONQUEUE);
			}
			break;
		}
		
		case OP_REQUESTPARTS: {		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_REQUESTPARTS") );
			
			theStats::AddDownOverheadFileRequest(size);

			CMemFile data((byte*)packet,size);

			CMD4Hash reqfilehash = data.ReadHash();

			uint32 auStartOffsets[3];
			auStartOffsets[0] = data.ReadUInt32();
			auStartOffsets[1] = data.ReadUInt32();
			auStartOffsets[2] = data.ReadUInt32();

			uint32 auEndOffsets[3];
			auEndOffsets[0] = data.ReadUInt32();
			auEndOffsets[1] = data.ReadUInt32();
			auEndOffsets[2] = data.ReadUInt32();


			for (int i = 0; i < ARRSIZE(auStartOffsets); i++) {
				if (auEndOffsets[i] > auStartOffsets[i]) {
					Requested_Block_Struct* reqblock = new Requested_Block_Struct;
					reqblock->StartOffset = auStartOffsets[i];
					reqblock->EndOffset = auEndOffsets[i];
					md4cpy(reqblock->FileID, reqfilehash.GetHash());
					reqblock->transferred = 0;
					m_client->AddReqBlock(reqblock);
				} else {
					if ( CLogger::IsEnabled( logClient ) ) {
							if (auEndOffsets[i] != 0 || auStartOffsets[i] != 0) {
								wxString msg = wxString::Format(_("Client requests invalid %u "), i);
								msg += wxT(" ") + wxString::Format(_("File block %u-%u (%d bytes):"), auStartOffsets[i], auEndOffsets[i], auEndOffsets[i] - auStartOffsets[i]);
								msg += wxT(" ") + m_client->GetFullIP();
							//	AddLogLineM( false, CFormat(_("Client requests invalid %u. file block %u-%u (%d bytes): %s")) % i % auStartOffsets[i] % auEndOffsets[i] % auEndOffsets[i] - auStartOffsets[i] % m_client->GetFullIP());
								AddLogLineM(false, msg);
							}
						}
					}
				}
				break;
		}
		
		case OP_CANCELTRANSFER: {		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_CANCELTRANSFER") );
			
			theStats::AddDownOverheadFileRequest(size);
			theApp.uploadqueue->RemoveFromUploadQueue(m_client);
			if ( CLogger::IsEnabled( logClient ) ) {
				AddDebugLogLineM( false, logClient, m_client->GetUserName() + wxT(": Upload session ended due canceled transfer."));
			}
			break;
		}
		
		case OP_END_OF_DOWNLOAD: { // 0.43b except check for bad clients
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_END_OF_DOWNLOAD") );
			
			theStats::AddDownOverheadFileRequest(size);
			if (size>=16 && m_client->GetUploadFileID() == CMD4Hash((byte*)packet)) {
				theApp.uploadqueue->RemoveFromUploadQueue(m_client);
				if ( CLogger::IsEnabled( logClient ) ) {
					AddDebugLogLineM( false, logClient, m_client->GetUserName() + wxT(": Upload session ended due ended transfer."));
				}
			}
			break;
		}
		
		case OP_HASHSETREQUEST: {		// 0.43b except check for bad clients
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_HASHSETREQUEST") );
			
			
			theStats::AddDownOverheadFileRequest(size);
			if (size != 16) {
				throw wxString(wxT("Invalid OP_HASHSETREQUEST packet size"));
			}
			m_client->SendHashsetPacket(CMD4Hash((byte*)packet));
			break;
		}
		
		case OP_HASHSETANSWER: {		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_HASHSETANSWER") );
			
			theStats::AddDownOverheadFileRequest(size);
			m_client->ProcessHashSet(packet,size);
			break;
		}
		
		case OP_SENDINGPART: {			// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_SENDINGPART") );
			
			if (	 m_client->GetRequestFile() && 
				!m_client->GetRequestFile()->IsStopped() && 
				(m_client->GetRequestFile()->GetStatus() == PS_READY || m_client->GetRequestFile()->GetStatus()==PS_EMPTY)) {
				m_client->ProcessBlockPacket(packet,size);
				if ( 	m_client && 
					( m_client->GetRequestFile()->IsStopped() || 
					  m_client->GetRequestFile()->GetStatus() == PS_PAUSED || 
					  m_client->GetRequestFile()->GetStatus() == PS_ERROR) ) {
					if (!m_client->GetSentCancelTransfer()) {
						CPacket* packet = new CPacket(OP_CANCELTRANSFER,0);
						theStats::AddUpOverheadFileRequest(packet->GetPacketSize());
						m_client->SendPacket(packet,true,true);
						
						// Socket might die because of SendPacket, so check
						if (m_client)
							m_client->SetSentCancelTransfer(1);
					}

					if (m_client)
						m_client->SetDownloadState(m_client->GetRequestFile()->IsStopped() ? DS_NONE : DS_ONQUEUE);
				}
			} else {
				if (!m_client->GetSentCancelTransfer()) {
					CPacket* packet = new CPacket(OP_CANCELTRANSFER,0);
					theStats::AddUpOverheadFileRequest(packet->GetPacketSize());
					m_client->SendPacket(packet,true,true);
					
					// Socket might die because of SendPacket, so check
					m_client->SetSentCancelTransfer(1);
				}
				m_client->SetDownloadState((m_client->GetRequestFile()==NULL || m_client->GetRequestFile()->IsStopped()) ? DS_NONE : DS_ONQUEUE);
			}
			break;
		}
		
		case OP_OUTOFPARTREQS: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_OUTOFPARTREQS") );
			
			theStats::AddDownOverheadFileRequest(size);
			if (m_client->GetDownloadState() == DS_DOWNLOADING) {
				m_client->SetDownloadState(DS_ONQUEUE);
			}
			break;
		}
		
		case OP_CHANGE_CLIENT_ID: { 	// Kad reviewed
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_CHANGE_CLIENT_ID") );
			
			theStats::AddDownOverheadOther(size);
			CMemFile data((byte*)packet, size);
			uint32 nNewUserID = data.ReadUInt32();
			uint32 nNewServerIP = data.ReadUInt32();
			
			if (IsLowID(nNewUserID)) { // client changed server and gots a LowID
				CServer* pNewServer = theApp.serverlist->GetServerByIP(nNewServerIP);
				if (pNewServer != NULL){
					m_client->SetUserIDHybrid(nNewUserID); // update UserID only if we know the server
					m_client->SetServerIP(nNewServerIP);
					m_client->SetServerPort(pNewServer->GetPort());
				}
			} else if (nNewUserID == m_client->GetIP()) { // client changed server and gots a HighID(IP)
				m_client->SetUserIDHybrid(wxUINT32_SWAP_ALWAYS(nNewUserID));
				CServer* pNewServer = theApp.serverlist->GetServerByIP(nNewServerIP);
				if (pNewServer != NULL){
					m_client->SetServerIP(nNewServerIP);
					m_client->SetServerPort(pNewServer->GetPort());
				}
			} 
			
			break;
		}					
		
		case OP_CHANGE_SLOT:{	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_CHANGE_SLOT") );
			
			// sometimes sent by Hybrid
			theStats::AddDownOverheadOther(size);
			break;
		}			
		
		case OP_MESSAGE: {		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_MESSAGE") );
			
			AddLogLineM( true, CFormat(_("New message from '%s' (IP:%s)")) % m_client->GetUserName() % m_client->GetFullIP());
			theStats::AddDownOverheadOther(size);
			
			CMemFile message_file((byte*)packet,size);

			//filter me?
			wxString message = message_file.ReadString(m_client->GetUnicodeSupport());
			if (IsMessageFiltered(message, m_client)) {
				if (!m_client->m_bMsgFiltered) {
					AddLogLineM( true, CFormat(_("Message filtered from '%s' (IP:%s)")) % m_client->GetUserName() % m_client->GetFullIP());
				}
				m_client->m_bMsgFiltered=true;
			} else {
				Notify_ChatProcessMsg(GUI_ID(m_client->GetIP(),m_client->GetUserPort()), m_client->GetUserName() + wxT("|") + message);
			}
			break;
		}
		
		case OP_ASKSHAREDFILES:	{	// 0.43b (well, er, it does the same, but in our own way)
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ASKSHAREDFILES") );
			
			// client wants to know what we have in share, let's see if we allow him to know that
			theStats::AddDownOverheadOther(size);
			// IP banned, no answer for this request
			if (m_client->IsBanned()) {
				break;
			}
			std::vector<CKnownFile*> list;
			if (thePrefs::CanSeeShares()==vsfaEverybody || (thePrefs::CanSeeShares()==vsfaFriends && m_client->IsFriend())) {
				theApp.sharedfiles->CopyFileList(list);
				AddLogLineM( true, CFormat( _("User %s (%u) requested your sharedfiles-list -> Accepted"))
					% m_client->GetUserName() 
					% m_client->GetUserIDHybrid() );
			} else {
				AddLogLineM( true, CFormat( _("User %s (%u) requested your sharedfiles-list -> Denied"))
					% m_client->GetUserName() 
					% m_client->GetUserIDHybrid() );
			}
			// now create the memfile for the packet
			CMemFile tempfile(80);
			tempfile.WriteUInt32(list.size());
			for (unsigned i = 0; i < list.size(); ++i) {
				theApp.sharedfiles->CreateOfferedFilePacket(list[i], &tempfile, NULL, m_client);
			}
			
			// create a packet and send it
			CPacket* replypacket = new CPacket(&tempfile);
			replypacket->SetOpCode(OP_ASKSHAREDFILESANSWER);
			theStats::AddUpOverheadOther(replypacket->GetPacketSize());
			SendPacket(replypacket, true, true);
			break;
		}
		
		case OP_ASKSHAREDFILESANSWER: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ASKSHAREDFILESANSWER") );
			
			theStats::AddDownOverheadOther(size);
			wxString EmptyStr;
			m_client->ProcessSharedFileList(packet,size,EmptyStr);
			break;
		}
		
		case OP_ASKSHAREDDIRS: { 		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ASKSHAREDDIRS") );
			
			theStats::AddDownOverheadOther(size);
			wxASSERT( size == 0 );
			// IP banned, no answer for this request
			if (m_client->IsBanned()) {
				break;
			}
			if ((thePrefs::CanSeeShares()==vsfaEverybody) || ((thePrefs::CanSeeShares()==vsfaFriends) && m_client->IsFriend())) {
				AddLogLineM( true, CFormat( _("User %s (%u) requested your shareddirectories-list -> Accepted") )
					% m_client->GetUserName()
					% m_client->GetUserIDHybrid() );

				// Kry - This new code from eMule will avoid duplicated folders
				ArrayOfwxStrings folders_to_send;
				
				uint32 uDirs = theApp.glob_prefs->shareddir_list.GetCount();
			
				// the shared folders
				for (uint32 iDir=0; iDir < uDirs; iDir++) {
					folders_to_send.Add(wxString(theApp.glob_prefs->shareddir_list[iDir]));
				}			
				
				bool bFoundFolder = false;
				
				wxString char_ptrDir;
				// ... the categories folders ... (category 0 -> incoming)
				for (uint32 ix=0;ix< theApp.glob_prefs->GetCatCount();ix++) {
					char_ptrDir = theApp.glob_prefs->GetCategory(ix)->incomingpath;
					bFoundFolder = false;
					for (uint32 iDir=0; iDir < (uint32)folders_to_send.GetCount(); iDir++) {	
						if (folders_to_send[iDir].CmpNoCase(char_ptrDir) == 0) {
							bFoundFolder = true;
							break;
						}
					}			
					if (!bFoundFolder) {
						folders_to_send.Add(wxString(char_ptrDir));
					}							
				}
	
				// ... and the Magic thing from the eDonkey Hybrids...
				bFoundFolder = false;
				for (uint32 iDir = 0; iDir < (uint32) folders_to_send.GetCount(); iDir++) {
					if (folders_to_send[iDir].CmpNoCase(OP_INCOMPLETE_SHARED_FILES) == 0) {
						bFoundFolder = true;
						break;
					}
				}
				if (!bFoundFolder) {
					folders_to_send.Add(wxString(OP_INCOMPLETE_SHARED_FILES));
				}
				
				// Send packet.
				CMemFile tempfile(80);

				uDirs = folders_to_send.GetCount();
				tempfile.WriteUInt32(uDirs);
				for (uint32 iDir=0; iDir < uDirs; iDir++) {
					tempfile.WriteString(folders_to_send[iDir]);
				}

				CPacket* replypacket = new CPacket(&tempfile);
				replypacket->SetOpCode(OP_ASKSHAREDDIRSANS);
				theStats::AddUpOverheadOther(replypacket->GetPacketSize());
				SendPacket(replypacket, true, true);
			} else {
				AddLogLineM( true, CFormat( _("User %s (%u) requested your shareddirectories-list -> Denied") )
					% m_client->GetUserName()
					% m_client->GetUserIDHybrid() );

				CPacket* replypacket = new CPacket(OP_ASKSHAREDDENIEDANS, 0);
				theStats::AddUpOverheadOther(replypacket->GetPacketSize());
				SendPacket(replypacket, true, true);
			}

			break;
		}
		
		case OP_ASKSHAREDFILESDIR: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ASKSHAREDFILESDIR") );
			
			theStats::AddDownOverheadOther(size);
			// IP banned, no answer for this request
			if (m_client->IsBanned()) {
				break;
			}
			CMemFile data((byte*)packet, size);
										
			wxString strReqDir = data.ReadString(m_client->GetUnicodeSupport());
			if (thePrefs::CanSeeShares()==vsfaEverybody || (thePrefs::CanSeeShares()==vsfaFriends && m_client->IsFriend())) {
				AddLogLineM( true, CFormat(_("User %s (%u) requested your sharedfiles-list for directory %s -> accepted")) % m_client->GetUserName() % m_client->GetUserIDHybrid() % strReqDir);
				wxASSERT( data.GetPosition() == data.GetLength() );
				CTypedPtrList<CPtrList, CKnownFile*> list;
				
				if (strReqDir == OP_INCOMPLETE_SHARED_FILES) {
					// get all shared files from download queue
					int iQueuedFiles = theApp.downloadqueue->GetFileCount();
					for (int i = 0; i < iQueuedFiles; i++) {
						CPartFile* pFile = theApp.downloadqueue->GetFileByIndex(i);
						if (pFile == NULL || pFile->GetStatus(true) != PS_READY) {
							break;
						}
						list.AddTail(pFile);
					}
				} else {
					theApp.sharedfiles->GetSharedFilesByDirectory(strReqDir,list);
				}

				CMemFile tempfile(80);
				tempfile.WriteString(strReqDir);
				tempfile.WriteUInt32(list.GetCount());
				while (list.GetCount()) {
					theApp.sharedfiles->CreateOfferedFilePacket(list.GetHead(), &tempfile, NULL, m_client);
					list.RemoveHead();
				}
				
				CPacket* replypacket = new CPacket(&tempfile);
				replypacket->SetOpCode(OP_ASKSHAREDFILESDIRANS);
				theStats::AddUpOverheadOther(replypacket->GetPacketSize());
				SendPacket(replypacket, true, true);
			} else {
				AddLogLineM( true, CFormat(_("User %s (%u) requested your sharedfiles-list for directory %s -> denied")) % m_client->GetUserName() % m_client->GetUserIDHybrid() % strReqDir);
				
				CPacket* replypacket = new CPacket(OP_ASKSHAREDDENIEDANS, 0);
				theStats::AddUpOverheadOther(replypacket->GetPacketSize());
				SendPacket(replypacket, true, true);
			}
			break;
		}		
		
		case OP_ASKSHAREDDIRSANS:{		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ASKSHAREDDIRSANS") );
			
			theStats::AddDownOverheadOther(size);
			if (m_client->GetFileListRequested() == 1){
				CMemFile data((byte*)packet, size);
				uint32 uDirs = data.ReadUInt32();
				for (uint32 i = 0; i < uDirs; i++){
					wxString strDir = data.ReadString(m_client->GetUnicodeSupport());
					AddLogLineM( true, CFormat( _("User %s (%u) shares directory %s") )
						% m_client->GetUserName()
						% m_client->GetUserIDHybrid()
						% strDir );
			
					CMemFile tempfile(80);
					tempfile.WriteString(strDir);
					CPacket* replypacket = new CPacket(&tempfile);
					replypacket->SetOpCode(OP_ASKSHAREDFILESDIR);
					theStats::AddUpOverheadOther(replypacket->GetPacketSize());
					SendPacket(replypacket, true, true);
				}
				wxASSERT( data.GetPosition() == data.GetLength() );
				m_client->SetFileListRequested(uDirs);
			} else {
				AddLogLineM( true, CFormat( _("User %s (%u) sent unrequested shared dirs.") )
					% m_client->GetUserName() 
					% m_client->GetUserIDHybrid() );
			}
			break;
		}
  
		case OP_ASKSHAREDFILESDIRANS: {		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ASKSHAREDFILESDIRANS") );
			
			theStats::AddDownOverheadOther(size);
			CMemFile data((byte*)packet, size);
			wxString strDir = data.ReadString(m_client->GetUnicodeSupport());

			if (m_client->GetFileListRequested() > 0){
				AddLogLineM( true, CFormat( _("User %s (%u) sent sharedfiles-list for directory %s") )
					% m_client->GetUserName()
					% m_client->GetUserIDHybrid()
					% strDir );
				
				m_client->ProcessSharedFileList(packet + data.GetPosition(), size - data.GetPosition(), strDir);
				if (m_client->GetFileListRequested() == 0) {
					AddLogLineM( true, CFormat( _("User %s (%u) finished sending sharedfiles-list") )
						% m_client->GetUserName()
						% m_client->GetUserIDHybrid() );
				}
			} else {
				AddLogLineM( true, CFormat( _("User %s (%u) sent unwanted sharedfiles-list") )
					% m_client->GetUserName()
					% m_client->GetUserIDHybrid() );
			}
			break;
		}
		
		case OP_ASKSHAREDDENIEDANS:
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ASKSHAREDDENIEDANS" ) );
			
			theStats::AddDownOverheadOther(size);
			wxASSERT( size == 0 );
			AddLogLineM( true, CFormat( _("User %s (%u) denied access to shared directories/files list") )
				% m_client->GetUserName()
				% m_client->GetUserIDHybrid() );
					
			m_client->SetFileListRequested(0);			
			break;
		
		default:
			theStats::AddDownOverheadOther(size);
			AddDebugLogLineM( false, logRemoteClient, wxString::Format(wxT("Edonkey packet: unknown opcode: %i %x"), opcode, opcode) );
			return false;
	}
	
	return true;
}


bool CClientReqSocket::ProcessExtPacket(const char* packet, uint32 size, uint8 opcode)
{
	#ifdef __PACKET_RECV_DUMP__
	printf("Rec: OPCODE %x \n",opcode);
	DumpMem(packet,size);
	#endif
		
	// 0.42e - except the catchs on mem exception and file exception
	if (!m_client) {
		throw wxString(wxT("Unknown clients sends extended protocol packet"));
	}
	/*
	if (!client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
		// Here comes a extended packet without finishing the hanshake.
		// IMHO, we should disconnect the client.
		throw wxString(wxT("Client send extended packet before finishing handshake"));
	}
	*/
	switch(opcode) {
		case OP_MULTIPACKET: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_MULTIPACKET") );

			theStats::AddDownOverheadFileRequest(size);

			if (m_client->IsBanned()) {
				break;
			}

			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_MULTIPACKET before finishing handshake"));
			}

			CMemFile data_in((byte*)packet,size);
			CMD4Hash reqfilehash = data_in.ReadHash();
			CKnownFile* reqfile = theApp.sharedfiles->GetFileByID(reqfilehash);
			if ( reqfile == NULL ){
				reqfile = theApp.downloadqueue->GetFileByID(reqfilehash);
				if ( !( reqfile != NULL && reqfile->GetFileSize() > PARTSIZE ) ) {
					CPacket* replypacket = new CPacket(OP_FILEREQANSNOFIL, 16);
					replypacket->Copy16ToDataBuffer(reqfilehash.GetHash());
					theStats::AddUpOverheadFileRequest(replypacket->GetPacketSize());
					SendPacket(replypacket, true);
					break;
				}
			}

			if (!m_client->GetWaitStartTime()) {
				m_client->SetWaitStartTime();
			}
			// if we are downloading this file, this could be a new source
			// no passive adding of files with only one part
			if (reqfile->IsPartFile() && reqfile->GetFileSize() > PARTSIZE) {
				if (thePrefs::GetMaxSourcePerFile() > ((CPartFile*)reqfile)->GetSourceCount()) {
					theApp.downloadqueue->CheckAndAddKnownSource((CPartFile*)reqfile, m_client);
				}
			}
			// check to see if this is a new file they are asking for
			if (m_client->GetUploadFileID() != reqfilehash) {
				m_client->SetCommentDirty();
			}
			m_client->SetUploadFileID(reqfile);
			CMemFile data_out(128);
			data_out.WriteHash(reqfile->GetFileHash());
			while(data_in.GetLength()-data_in.GetPosition()) {
				if (!m_client) {
					throw wxString(wxT("Client suddenly disconnected"));
				}
				uint8 opcode_in = data_in.ReadUInt8();
				switch(opcode_in) {
					case OP_REQUESTFILENAME: {
						m_client->ProcessExtendedInfo(&data_in, reqfile);
						data_out.WriteUInt8(OP_REQFILENAMEANSWER);
						data_out.WriteString(reqfile->GetFileName());
						break;
					}
					case OP_AICHFILEHASHREQ: {
						if (m_client->IsSupportingAICH() && reqfile->GetAICHHashset()->GetStatus() == AICH_HASHSETCOMPLETE
							&& reqfile->GetAICHHashset()->HasValidMasterHash())
						{
							data_out.WriteUInt8(OP_AICHFILEHASHANS);
							reqfile->GetAICHHashset()->GetMasterHash().Write(&data_out);
						}
						break;
					}						
					case OP_SETREQFILEID: {
						data_out.WriteUInt8(OP_FILESTATUS);
						if (reqfile->IsPartFile()) {
							((CPartFile*)reqfile)->WritePartStatus(&data_out);
						} else {
							data_out.WriteUInt16(0);
						}
						break;
					}
					//We still send the source packet seperately.. 
					//We could send it within this packet.. If agreeded, I will fix it..
					case OP_REQUESTSOURCES: {
						//Although this shouldn't happen, it's a just in case to any Mods that mess with version numbers.
						if (m_client->GetSourceExchangeVersion() > 1) {
							//data_out.WriteUInt8(OP_ANSWERSOURCES);
							uint32 dwTimePassed = ::GetTickCount() - m_client->GetLastSrcReqTime() + CONNECTION_LATENCY;
							bool bNeverAskedBefore = m_client->GetLastSrcReqTime() == 0;
							if( 
									//if not complete and file is rare
									(    reqfile->IsPartFile()
									&& (bNeverAskedBefore || dwTimePassed > SOURCECLIENTREASKS)
									&& ((CPartFile*)reqfile)->GetSourceCount() <= RARE_FILE
									) ||
									//OR if file is not rare or if file is complete
									( (bNeverAskedBefore || dwTimePassed > SOURCECLIENTREASKS * MINCOMMONPENALTY) )
									) 
							{
								m_client->SetLastSrcReqTime();
								CPacket* tosend = reqfile->CreateSrcInfoPacket(m_client);
								if(tosend) {
									theStats::AddUpOverheadSourceExchange(tosend->GetPacketSize());
									SendPacket(tosend, true);
								}
							}
						}
						break;
					}
				}

			}
			if( data_out.GetLength() > 16 ) {
				CPacket* reply = new CPacket(&data_out, OP_EMULEPROT);
				reply->SetOpCode(OP_MULTIPACKETANSWER);
				theStats::AddUpOverheadFileRequest(reply->GetPacketSize());
				SendPacket(reply, true);
			}
			break;
		}

		case OP_MULTIPACKETANSWER: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_MULTIPACKETANSWER") );
			
			theStats::AddDownOverheadFileRequest(size);

			if (m_client->IsBanned()) {
				break;
			}
			
			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_MULTIPACKETANSWER before finishing handshake"));
			}
			
			CMemFile data_in((byte*)packet,size);
			CMD4Hash reqfilehash = data_in.ReadHash();
			const CPartFile *reqfile = theApp.downloadqueue->GetFileByID(reqfilehash);
			//Make sure we are downloading this file.
			if ( !reqfile ) {
				throw wxString(wxT(" Wrong File ID: (OP_MULTIPACKETANSWER; reqfile==NULL)"));
			}
			if ( !m_client->GetRequestFile() ) {
				throw wxString(wxT(" Wrong File ID: OP_MULTIPACKETANSWER; client->reqfile==NULL)"));
			}
			if (reqfile != m_client->GetRequestFile()) {
				throw wxString(wxT(" Wrong File ID: OP_MULTIPACKETANSWER; reqfile!=client->reqfile)"));
			}
			while (data_in.GetLength()-data_in.GetPosition()) {
				// Some of the cases down there can actually send a packet and lose the client
				if (!m_client) {
					throw wxString(wxT("Client suddenly disconnected"));
				}
				uint8 opcode_in = data_in.ReadUInt8();
				switch(opcode_in) {
					case OP_REQFILENAMEANSWER: {
						m_client->ProcessFileInfo(&data_in, reqfile);
						break;
					}
					case OP_FILESTATUS: {
						m_client->ProcessFileStatus(false, &data_in, reqfile);
						break;
					}
					case OP_AICHFILEHASHANS: {
						m_client->ProcessAICHFileHash(&data_in, reqfile);
						break;
					}					
				}
			}

			break;
		}
	
		case OP_EMULEINFO: {	// 0.43b
			theStats::AddDownOverheadOther(size);

			if (!m_client->ProcessMuleInfoPacket(packet,size)) { 
				AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_EMULEINFO") );
				
				// If it's not a OS Info packet, is an old client
				// start secure identification, if
				//  - we have received eD2K and eMule info (old eMule)
				if (m_client->GetInfoPacketsReceived() == IP_BOTH) {	
					m_client->InfoPacketsReceived();
				}
				m_client->SendMuleInfoPacket(true);
			} else {
				AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_EMULEINFO is an OS_INFO") );
			}
			break;
		}
		case OP_EMULEINFOANSWER: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_EMULEINFOANSWER") );
			theStats::AddDownOverheadOther(size);
			
			m_client->ProcessMuleInfoPacket(packet,size);
			// start secure identification, if
			//  - we have received eD2K and eMule info (old eMule)
			
			if (m_client->GetInfoPacketsReceived() == IP_BOTH) {
				m_client->InfoPacketsReceived();				
			}
			
			break;
		}
		
		case OP_SECIDENTSTATE:{		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_SECIDENTSTATE") );
			
			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_SECIDENTSTATE before finishing handshake"));
			}								
			m_client->ProcessSecIdentStatePacket((byte*)packet,size);
			// ProcessSecIdentStatePacket() might cause the socket to die, so check
			if (m_client) {
				int SecureIdentState = m_client->GetSecureIdentState();
				if (SecureIdentState == IS_SIGNATURENEEDED) {
					m_client->SendSignaturePacket();
				} else if (SecureIdentState == IS_KEYANDSIGNEEDED) {
					m_client->SendPublicKeyPacket();
					// SendPublicKeyPacket() might cause the socket to die, so check
					if ( m_client ) {
						m_client->SendSignaturePacket();
					}
				}
			}
			break;
		}
		
		case OP_PUBLICKEY: {		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_PUBLICKEY") );
			
			if (m_client->IsBanned() ){
				break;						
			}
			
			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_PUBLICKEY before finishing handshake"));
			}
											
			m_client->ProcessPublicKeyPacket((byte*)packet,size);
			break;
		}
		case OP_SIGNATURE:{			// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_SIGNATURE") );
			
			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_COMPRESSEDPART before finishing handshake"));
			}
											
			m_client->ProcessSignaturePacket((byte*)packet,size);
			break;
		}		
		case OP_COMPRESSEDPART: {	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_COMPRESSEDPART") );
			
			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_COMPRESSEDPART before finishing handshake"));
			}
											
			if (m_client->GetRequestFile() && !m_client->GetRequestFile()->IsStopped() && (m_client->GetRequestFile()->GetStatus()==PS_READY || m_client->GetRequestFile()->GetStatus()==PS_EMPTY)) {
				m_client->ProcessBlockPacket(packet,size,true);
				if (m_client && (
					m_client->GetRequestFile()->IsStopped() ||
					m_client->GetRequestFile()->GetStatus() == PS_PAUSED ||
					m_client->GetRequestFile()->GetStatus() == PS_ERROR)) {
					if (!m_client->GetSentCancelTransfer()) {
						CPacket* packet = new CPacket(OP_CANCELTRANSFER,0);
						theStats::AddUpOverheadFileRequest(packet->GetPacketSize());
						m_client->SendPacket(packet,true,true);					
						
						if (m_client) {
							m_client->SetSentCancelTransfer(1);
						}
					}

					if ( m_client ) {
						m_client->SetDownloadState(m_client->GetRequestFile()->IsStopped() ? DS_NONE : DS_ONQUEUE);	
					}
				}
			} else {
				if (!m_client->GetSentCancelTransfer()) {
					CPacket* packet = new CPacket(OP_CANCELTRANSFER,0);
					theStats::AddUpOverheadFileRequest(packet->GetPacketSize());
					m_client->SendPacket(packet,true,true);
					
					if ( m_client ) {
						m_client->SetSentCancelTransfer(1);
					}
				}
			
				if ( m_client ) {
					m_client->SetDownloadState((m_client->GetRequestFile()==NULL || m_client->GetRequestFile()->IsStopped()) ? DS_NONE : DS_ONQUEUE);
				}
			}
			break;
		}
		
		case OP_QUEUERANKING: {		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_QUEUERANKING") );
			
			theStats::AddDownOverheadOther(size);
			
			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_QUEUERANKING before finishing handshake"));
			}
			
			if (size != 12) {
				throw wxString(wxT("Invalid size (OP_QUEUERANKING)"));
			}

			uint16 newrank = PeekUInt16(packet);
			m_client->SetRemoteQueueFull(false);
			m_client->SetRemoteQueueRank(newrank);
			break;
		}
		
		case OP_REQUESTSOURCES:{	// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_REQUESTSOURCES") );
			
			theStats::AddDownOverheadSourceExchange(size);

			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_REQUESTSOURCES before finishing handshake"));
			}
			
			if (m_client->GetSourceExchangeVersion() >= 1) {
				if(size != 16) {
					throw wxString(wxT("Invalid size (OP_QUEUERANKING)"));
				}
				//first check shared file list, then download list
				const CMD4Hash fileID((byte*)packet);
				CKnownFile* file = theApp.sharedfiles->GetFileByID(fileID);
				if(!file) {
					file = theApp.downloadqueue->GetFileByID(fileID);
				}
				if(file) {
					uint32 dwTimePassed = ::GetTickCount() - m_client->GetLastSrcReqTime() + CONNECTION_LATENCY;
					bool bNeverAskedBefore = m_client->GetLastSrcReqTime() == 0;
					if( 
					//if not complete and file is rare, allow once every 40 minutes
					( file->IsPartFile() &&
					((CPartFile*)file)->GetSourceCount() <= RARE_FILE &&
					(bNeverAskedBefore || dwTimePassed > SOURCECLIENTREASKS)
					) ||
					//OR if file is not rare or if file is complete, allow every 90 minutes
					( (bNeverAskedBefore || dwTimePassed > SOURCECLIENTREASKS * MINCOMMONPENALTY) )
					)
					{
						m_client->SetLastSrcReqTime();
						CPacket* tosend = file->CreateSrcInfoPacket(m_client);
						if(tosend) {
							theStats::AddUpOverheadSourceExchange(tosend->GetPacketSize());
							SendPacket(tosend, true, true);
						}
					}
				}
			}
			break;
		}
		
		case OP_ANSWERSOURCES: {
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_ANSWERSOURCES") );
			
			theStats::AddDownOverheadSourceExchange(size);

			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_ANSWERSOURCES before finishing handshake"));
			}
			
			CMemFile data((byte*)packet,size);
			CMD4Hash hash = data.ReadHash();
			const CKnownFile* file = theApp.downloadqueue->GetFileByID(hash);
			if(file){
				if (file->IsPartFile()){
					//set the client's answer time
					m_client->SetLastSrcAnswerTime();
					//and set the file's last answer time
					((CPartFile*)file)->SetLastAnsweredTime();

					((CPartFile*)file)->AddClientSources(&data, m_client->GetSourceExchangeVersion(), SF_SOURCE_EXCHANGE);
				}
			}
			break;
		}
		case OP_FILEDESC: {		// 0.43b
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_FILEDESC") );
			
			theStats::AddDownOverheadFileRequest(size);

			if (!m_client->CheckHandshakeFinished(OP_EMULEPROT, opcode)) {
				// Here comes a extended packet without finishing the hanshake.
				// IMHO, we should disconnect the client.
				throw wxString(wxT("Client send OP_FILEDESC before finishing handshake"));
			}
			
			m_client->ProcessMuleCommentPacket(packet,size);
			break;
		}

		// Unsupported
		case OP_REQUESTPREVIEW: {
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_REQUESTPREVIEW") );
			break;
		}
		// Unsupported
		case OP_PREVIEWANSWER: {
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_PREVIEWANSWER") );
			break;
		}

		case OP_PUBLICIP_ANSWER: {
			theStats::AddDownOverheadOther(size);
			m_client->ProcessPublicIPAnswer((byte*)packet,size);
			break;
		}
		case OP_PUBLICIP_REQ: {
			theStats::AddDownOverheadOther(size);
			CPacket* pPacket = new CPacket(OP_PUBLICIP_ANSWER, 4, OP_EMULEPROT);
			pPacket->CopyUInt32ToDataBuffer(m_client->GetIP());
			theStats::AddUpOverheadOther(pPacket->GetPacketSize());
			SendPacket(pPacket);
			break;
		}			
		case OP_AICHANSWER: {
			theStats::AddDownOverheadOther(size);
			m_client->ProcessAICHAnswer(packet,size);
			break;
		}
		case OP_AICHREQUEST: {
			theStats::AddDownOverheadOther(size);
			m_client->ProcessAICHRequest(packet,size);
			break;
		}
		case OP_AICHFILEHASHANS: {
			// those should not be received normally, since we should only get those in MULTIPACKET
			theStats::AddDownOverheadOther(size);
			CMemFile data((byte*)packet, size);
			m_client->ProcessAICHFileHash(&data, NULL);
			break;
		}
		case OP_AICHFILEHASHREQ: {
			// those should not be received normally, since we should only get those in MULTIPACKET
			CMemFile data((byte*)packet, size);
			CMD4Hash hash = data.ReadHash();
			CKnownFile* pPartFile = theApp.sharedfiles->GetFileByID(hash);
			if (pPartFile == NULL){
				break;
			}
			
			if (m_client->IsSupportingAICH() && pPartFile->GetAICHHashset()->GetStatus() == AICH_HASHSETCOMPLETE
				&& pPartFile->GetAICHHashset()->HasValidMasterHash()) {
				CMemFile data_out;
				data_out.WriteHash(hash);
				pPartFile->GetAICHHashset()->GetMasterHash().Write(&data_out);
				CPacket* packet = new CPacket(&data_out, OP_EMULEPROT, OP_AICHFILEHASHANS);
				theStats::AddUpOverheadOther(packet->GetPacketSize());
				SendPacket(packet);
			}
			break;
		}
		case OP_CALLBACK: {
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_CALLBACK") );				
			theStats::AddDownOverheadFileRequest(size);
			if(!Kademlia::CKademlia::isRunning()) {
				break;
			}
			CMemFile data((byte*)packet, size);
			CUInt128 check = data.ReadUInt128();
			check.XOR(Kademlia::CUInt128(true));
			if( check.compareTo(Kademlia::CKademlia::getPrefs()->getKadID())) {
				break;
			}
			CUInt128 fileid = data.ReadUInt128();
			byte fileid2[16];
			fileid.toByteArray(fileid2);
			const CMD4Hash fileHash(fileid2);
			if (theApp.sharedfiles->GetFileByID(fileHash) == NULL) {
				if (theApp.downloadqueue->GetFileByID(fileHash) == NULL) {
					break;
				}
			}

			uint32 ip = data.ReadUInt32();
			uint16 tcp = data.ReadUInt16();
			CUpDownClient* callback;
			callback = theApp.clientlist->FindClientByIP(wxUINT32_SWAP_ALWAYS(ip), tcp);
			if( callback == NULL ) {
				#warning Do we actually have to check friend status here?
				callback = new CUpDownClient(tcp,ip,0,0,NULL,false, false);
				theApp.clientlist->AddClient(callback);
			}
			callback->TryToConnect(true);
			break;
		}
		
		case OP_BUDDYPING: {
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_BUDDYPING") );
			theStats::AddDownOverheadKad(size);

			CUpDownClient* buddy = theApp.clientlist->GetBuddy();
			if( buddy != m_client || m_client->GetKadVersion() == 0 || !m_client->AllowIncomeingBuddyPingPong() ) {
				//This ping was not from our buddy or wrong version or packet sent to fast. Ignore
				break;
			}
			
			AddDebugLogLineM( false, logLocalClient, wxT("Local Client: OP_BUDDYPING") );
			m_client->SetLastBuddyPingPongTime();
			CPacket* replypacket = new CPacket(OP_BUDDYPONG, 0, OP_EMULEPROT);
			theStats::AddUpOverheadKad(replypacket->GetPacketSize());
			SendPacket(replypacket);
			break;
		}
		case OP_BUDDYPONG: {
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_BUDDYPONG") );
			theStats::AddDownOverheadKad(size);

			CUpDownClient* buddy = theApp.clientlist->GetBuddy();
			if( buddy != m_client || m_client->GetKadVersion() == 0 ) {
				//This pong was not from our buddy or wrong version. Ignore
				break;
			}
			m_client->SetLastBuddyPingPongTime();
			//All this is for is to reset our socket timeout.
			break;
		}
		case OP_REASKCALLBACKTCP: {
			theStats::AddDownOverheadFileRequest(size);
			CUpDownClient* buddy = theApp.clientlist->GetBuddy();
			if (buddy != m_client) {
				AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_REASKCALLBACKTCP") );
				//This callback was not from our buddy.. Ignore.
				break;
			}
			AddDebugLogLineM( false, logRemoteClient, wxT("Remote Client: OP_REASKCALLBACKTCP") );				
			CMemFile data_in((byte*)packet, size);
			uint32 destip = data_in.ReadUInt32();
			uint16 destport = data_in.ReadUInt16();
			CMD4Hash hash = data_in.ReadHash();
			CKnownFile* reqfile = theApp.sharedfiles->GetFileByID(hash);
			if (!reqfile) {
				AddDebugLogLineM( false, logLocalClient, wxT("Local Client: OP_FILENOTFOUND") );
				CPacket* response = new CPacket(OP_FILENOTFOUND,0,OP_EMULEPROT);
				theStats::AddUpOverheadFileRequest(response->GetPacketSize());
				theApp.clientudp->SendPacket(response, destip, destport);
				break;
			}
			
			CUpDownClient* sender = theApp.uploadqueue->GetWaitingClientByIP_UDP(destip, destport);
			if (sender) {
				//Make sure we are still thinking about the same file
				if (hash == sender->GetUploadFileID()) {
					sender->AddAskedCount();
					sender->SetLastUpRequest();
					//I messed up when I first added extended info to UDP
					//I should have originally used the entire ProcessExtenedInfo the first time.
					//So now I am forced to check UDPVersion to see if we are sending all the extended info.
					//For now on, we should not have to change anything here if we change
					//anything to the extended info data as this will be taken care of in ProcessExtendedInfo()
					//Update extended info. 
					if (sender->GetUDPVersion() > 3) {
						sender->ProcessExtendedInfo(&data_in, reqfile);
					} else if (sender->GetUDPVersion() > 2) {
						//Update our complete source counts.			
						uint16 nCompleteCountLast= sender->GetUpCompleteSourcesCount();
						uint16 nCompleteCountNew = data_in.ReadUInt16();
						sender->SetUpCompleteSourcesCount(nCompleteCountNew);
						if (nCompleteCountLast != nCompleteCountNew) {
							reqfile->UpdatePartsInfo();
						}
					}
					
					CMemFile data_out(128);
					if(sender->GetUDPVersion() > 3) {
						if (reqfile->IsPartFile()) {
							((CPartFile*)reqfile)->WritePartStatus(&data_out);
						} else {
							data_out.WriteUInt16(0);
						}
					}
					
					data_out.WriteUInt16(theApp.uploadqueue->GetWaitingPosition(sender));
					AddDebugLogLineM( false, logLocalClient, wxT("Local Client: OP_REASKACK") );
					CPacket* response = new CPacket(&data_out, OP_EMULEPROT);
					response->SetOpCode(OP_REASKACK);
					theStats::AddUpOverheadFileRequest(response->GetPacketSize());
					theApp.clientudp->SendPacket(response, destip, destport);
				} else {
					AddDebugLogLineM(false, logListenSocket, wxT("Client UDP socket; OP_REASKCALLBACKTCP; reqfile does not match"));
				}
			} else {
				if ((theStats::GetWaitingUserCount() + 50) > thePrefs::GetQueueSize()) {
					AddDebugLogLineM( false, logLocalClient, wxT("Local Client: OP_QUEUEFULL") );
					CPacket* response = new CPacket(OP_QUEUEFULL,0,OP_EMULEPROT);
					theStats::AddUpOverheadFileRequest(response->GetPacketSize());
					theApp.clientudp->SendPacket(response, destip, destport);
				}
			}
			break;
		}
		default:
			theStats::AddDownOverheadOther(size);
			AddDebugLogLineM( false, logRemoteClient, wxString::Format(wxT("eMule packet : unknown opcode: %i %x"),opcode,opcode));
			break;
	}
	
	return true;
}


void CClientReqSocket::OnConnect(int nErrorCode)
{
	if (nErrorCode) {
		OnError(nErrorCode);
	} else if (!m_client) {
		// and now? Disconnect? not?			
		AddDebugLogLineM( false, logClient, wxT("Couldn't send hello packet (Client deleted!)") );
	} else if (!m_client->SendHelloPacket()) {	
		// and now? Disconnect? not?				
		AddDebugLogLineM( false, logClient, wxT("Couldn't send hello packet (Client deleted by SendHelloPacket!)") );
	} else {
		ResetTimeOutTimer();
	}
}


void CClientReqSocket::OnSend(int nErrorCode)
{
	ResetTimeOutTimer();
	CEMSocket::OnSend(nErrorCode);
}


void CClientReqSocket::OnReceive(int nErrorCode)
{
	ResetTimeOutTimer();
	CEMSocket::OnReceive(nErrorCode);
}


void CClientReqSocket::OnError(int nErrorCode)
{
	// 0.42e + Kry changes for handling of socket lost events
	wxString strError;
	
	if ((nErrorCode == 0) || (nErrorCode == 7) || (nErrorCode == 0xFEFF)) {	
		if (m_client) {
			if (!m_client->GetUserName().IsEmpty()) {
				strError = wxT("Client '") + m_client->GetUserName() + wxT("'");
			} else {
				strError = wxT("An unnamed client");
			}
			strError += wxT(" (IP:") + m_client->GetFullIP() + wxT(") ");
		} else {
			strError = wxT("A client ");
		}
		if (nErrorCode == 0) {
			strError += wxT("closed connection.");
		} else if (nErrorCode == 0xFEFF) {
			strError += wxT(" caused a wxSOCKET_LOST event.");
		}	else {
			strError += wxT("caused a socket blocking error.");
		}
	} else {
		if ( CLogger::IsEnabled( logClient ) && (nErrorCode != 107)) {
			// 0    -> No Error / Disconect
			// 107  -> Transport endpoint is not connected
			if (m_client) {
				if (m_client->GetUserName()) {
					strError = wxT("OnError: Client '") + m_client->GetUserName() +
						wxT("' (IP:") + m_client->GetFullIP() + 
						wxString::Format(wxT(") caused an error: %u. Disconnecting client!"), nErrorCode);
				} else {
					strError = wxT("OnError: Unknown client (IP:") + 
					m_client->GetFullIP() + 
					wxString::Format(wxT(") caused an error: %u. Disconnecting client!"), nErrorCode);
				}
			} else {
				strError = wxString::Format(wxT("OnError: A client caused an error or did something bad (error %u). Disconnecting client !"),
					nErrorCode);
			}
		} else {
			strError = wxT("Error 107 (Transport endpoint is not connected)");
		}	
	}
	
	Disconnect(strError);
}


bool CClientReqSocket::PacketReceived(CPacket* packet)
{
	// 0.42e
	bool bResult = false;
	uint32 uRawSize = packet->GetPacketSize();

	AddDebugLogLineM( false, logRemoteClient,
		CFormat(wxT("Packet with protocol %x, opcode %x, size %u received from %s"))
			% packet->GetProtocol()
			% packet->GetOpCode()
			% packet->GetPacketSize()
			% ( m_client ? m_client->GetFullIP() : wxT("Unknown Client") )
	);

	wxString exception;
	try {
		switch (packet->GetProtocol()) {
			case OP_EDONKEYPROT:		
				bResult = ProcessPacket(packet->GetDataBuffer(),uRawSize,packet->GetOpCode());
				break;		
			case OP_PACKEDPROT:
				if (!packet->UnPackPacket()) {
					AddDebugLogLineM( false, logZLib, wxT("Failed to decompress client TCP packet."));
					bResult = false;
					break;
				} else {
					AddDebugLogLineM(false, logRemoteClient, 
						wxString::Format(wxT("Packet unpacked, new protocol %x, opcode %x, size %u"), 
							packet->GetProtocol(),
							packet->GetOpCode(),
							packet->GetPacketSize())
					);
				}
			case OP_EMULEPROT:
				bResult = ProcessExtPacket(packet->GetDataBuffer(), packet->GetPacketSize(), packet->GetOpCode());
				break;
			default: {
				theStats::AddDownOverheadOther(uRawSize);
				if (m_client) {
					m_client->SetDownloadState(DS_ERROR);
				}
				Disconnect(wxT("Unknown protocol"));
				bResult = false;
			}
		}
	} catch (const CEOFException& err) {
		exception = wxT("EOF exception: ") + err.what();
	} catch (const CInvalidPacket& err) {
		exception = wxT("InvalidPacket exception: ") + err.what();
	} catch (const wxString& error) {
		exception = wxT("error: ") + (error.IsEmpty() ? wxT("Unknown error") : error);
	}

	if (!exception.IsEmpty()) {
		AddDebugLogLineM( false, logPacketErrors,
			CFormat(wxT("Caught %s\n"
						"On packet with protocol %x, opcode %x, size %u"
						"\tClientData: %s\n"))
				% exception
				% packet->GetProtocol()
				% packet->GetOpCode()
				% packet->GetPacketSize()
				% ( m_client ? m_client->GetClientFullInfo() : wxT("Unknown") )
		);
		
		if (m_client) {
			m_client->SetDownloadState(DS_ERROR);
		}
		
		AddDebugLogLineM( false, logClient, 
			CFormat( wxT("Client '%s' (IP: %s) caused an error (%s). Disconnecting client!" ) )
				% ( m_client ? m_client->GetUserName() : wxString(wxT("Unknown")) )
				% ( m_client ? m_client->GetFullIP() : wxString(wxT("Unknown")) )
				% exception
		);
		
		Disconnect(wxT("Caught exception on CClientReqSocket::ProcessPacket\n"));
	}

	return bResult;
}


bool CClientReqSocket::IsMessageFiltered(const wxString& Message, CUpDownClient* client) {
	
	bool filtered = false;
	// If we're chatting to the guy, we don't want to filter!
	if (client->GetChatState() != MS_CHATTING) {
		if (thePrefs::MsgOnlyFriends() && !client->IsFriend()) {
			filtered = true;
		} else if (thePrefs::MsgOnlySecure() && client->GetUserName().IsEmpty() ) {
			filtered = true;
		} else if (thePrefs::MustFilterMessages()) {
			if (thePrefs::MessageFilter().IsSameAs(wxT("*"))){  
				// Filter anything
				filtered = true;
			} else {
				wxStringTokenizer tokenizer( thePrefs::MessageFilter(), wxT(",") );
				while (tokenizer.HasMoreTokens() && !filtered) {
					if ( Message.Lower().Trim(false).Trim(true).Contains(
							tokenizer.GetNextToken().Lower().Trim(false).Trim(true))) {
						filtered = true;
					}
				}
			}
		}
	}
	return filtered;
}

SocketSentBytes CClientReqSocket::SendControlData(uint32 maxNumberOfBytesToSend, uint32 overchargeMaxBytesToSend)
{
    SocketSentBytes returnStatus = CEMSocket::SendControlData(maxNumberOfBytesToSend, overchargeMaxBytesToSend);

    if(returnStatus.success && (returnStatus.sentBytesControlPackets > 0 || returnStatus.sentBytesStandardPackets > 0)) {
        ResetTimeOutTimer();
    }

    return returnStatus;
}


SocketSentBytes CClientReqSocket::SendFileAndControlData(uint32 maxNumberOfBytesToSend, uint32 overchargeMaxBytesToSend)
{
	SocketSentBytes returnStatus = CEMSocket::SendFileAndControlData(maxNumberOfBytesToSend, overchargeMaxBytesToSend);

    if(returnStatus.success && (returnStatus.sentBytesControlPackets > 0 || returnStatus.sentBytesStandardPackets > 0)) {
        ResetTimeOutTimer();
    }

    return returnStatus;
}


void CClientReqSocket::SendPacket(CPacket* packet, bool delpacket, bool controlpacket, uint32 actualPayloadSize)
{
	ResetTimeOutTimer();
	CEMSocket::SendPacket(packet,delpacket,controlpacket, actualPayloadSize);
}


//-----------------------------------------------------------------------------
// CListenSocket
//-----------------------------------------------------------------------------
//
// This is the socket that listens to incomming connections in aMule's TCP port
// As soon as a connection is detected, it creates a new socket of type 
// CClientReqSocket to handle (accept) the connection.
// 

CListenSocket::CListenSocket(wxIPaddress &addr, const CProxyData *ProxyData)
:
// wxSOCKET_NOWAIT    - means non-blocking i/o
// wxSOCKET_REUSEADDR - means we can reuse the socket imediately (wx-2.5.3)
CSocketServerProxy(addr, wxSOCKET_NOWAIT|wxSOCKET_REUSEADDR, ProxyData)
{
	// 0.42e - vars not used by us
	bListening = false;
	shutdown = false;
	m_OpenSocketsInterval = 0;
	m_nPeningConnections = 0;
	totalconnectionchecks = 0;
	averageconnections = 0.0;
	// Set the listen socket event handler -- The handler is written in amule.cpp
	if (Ok()) {
 		SetEventHandler(theApp, LISTENSOCKET_HANDLER);
 		SetNotify(wxSOCKET_CONNECTION_FLAG);
 		Notify(true);

		printf("ListenSocket: Ok.\n");
	} else {
		AddLogLineM( true, _("Error: Could not listen to TCP port.") );
		printf("ListenSocket: Could not listen to TCP port.");
	}
}

CListenSocket::~CListenSocket()
{
	shutdown = true;
	Discard();
	Close();

	KillAllSockets();
}

bool CListenSocket::StartListening()
{
	// 0.42e
	bListening = true;

	return true;
}

void CListenSocket::ReStartListening()
{
	// 0.42e
	bListening = true;
	if (m_nPeningConnections) {
		m_nPeningConnections--;
		OnAccept(0);
	}
}

void CListenSocket::StopListening()
{
	// 0.42e
	bListening = false;
	theStats::AddMaxConnectionLimitReached();
}

void CListenSocket::OnAccept(int nErrorCode)
{
	// 0.42e
	if (!nErrorCode) {
		m_nPeningConnections++;
		if (m_nPeningConnections < 1) {
			wxASSERT(FALSE);
			m_nPeningConnections = 1;
		}
		if (TooManySockets(true) && !theApp.serverconnect->IsConnecting()) {
			StopListening();
			return;
		} else if (bListening == false) {
			// If the client is still at maxconnections,
			// this will allow it to go above it ...
			// But if you don't, you will get a lowID on all servers.
			ReStartListening();
		}
		// Deal with the pending connections, there might be more than one, due to
		// the StopListening() call above.
		while (m_nPeningConnections) {
			m_nPeningConnections--;
			// Create a new socket to deal with the connection
			CClientReqSocket* newclient = new CClientReqSocket();
			// Accept the connection and give it to the newly created socket
			if (!AcceptWith(*newclient, false)) {
				newclient->Safe_Delete();
			} else {
				#ifdef __DEBUG__
				amuleIPV4Address addr;
				newclient->GetPeer(addr);
				AddDebugLogLineM(false, logClient, wxT("Accepted connection from ") + addr.IPAddress());
				#endif
			}
		}
	}
}

void CListenSocket::AddConnection()
{
	m_OpenSocketsInterval++;
}

void CListenSocket::Process()
{
	// 042e + Kry changes for Destroy
	m_OpenSocketsInterval = 0;
	SocketSet::iterator it = socket_list.begin();
	while ( it != socket_list.end() ) {
		CClientReqSocket* cur_socket = *it++;
		if (!cur_socket->OnDestroy()) {
			if (cur_socket->deletethis) {
				cur_socket->Destroy();
			} else {
				cur_socket->CheckTimeOut();
			}
		}
	}
	
	if ((GetOpenSockets()+5 < thePrefs::GetMaxConnections() || theApp.serverconnect->IsConnecting()) && !bListening) {
		ReStartListening();
	}
}

void CListenSocket::RecalculateStats()
{
	// 0.42e
	memset(m_ConnectionStates,0,6);
	for (SocketSet::iterator it = socket_list.begin(); it != socket_list.end(); ) {
		CClientReqSocket* cur_socket = *it++;
		switch (cur_socket->GetConState()) {
			case ES_DISCONNECTED:
				m_ConnectionStates[0]++;
				break;
			case ES_NOTCONNECTED:
				m_ConnectionStates[1]++;
				break;
			case ES_CONNECTED:
				m_ConnectionStates[2]++;
				break;
		}
	}
}

void CListenSocket::AddSocket(CClientReqSocket* toadd)
{
	wxASSERT(toadd);
	socket_list.insert(toadd);
	theStats::AddActiveConnection();
}

void CListenSocket::RemoveSocket(CClientReqSocket* todel)
{
	wxASSERT(todel);
	socket_list.erase(todel);
	theStats::RemoveActiveConnection();
}

void CListenSocket::KillAllSockets()
{
	// 0.42e reviewed - they use delete, but our safer is Destroy...
	// But I bet it would be better to call Safe_Delete on the socket.
	// Update: no... Safe_Delete MARKS for deletion. We need to delete it.
	for (SocketSet::iterator it = socket_list.begin(); it != socket_list.end(); ) {
		CClientReqSocket* cur_socket = *it++;
		if (cur_socket->GetClient()) {
			cur_socket->GetClient()->Safe_Delete();
		} else {
			cur_socket->Safe_Delete();
			cur_socket->Destroy(); 
		}
	}
}

bool CListenSocket::TooManySockets(bool bIgnoreInterval)
{
	if (GetOpenSockets() > thePrefs::GetMaxConnections() || (m_OpenSocketsInterval > (thePrefs::GetMaxConperFive()*GetMaxConperFiveModifier()) && !bIgnoreInterval)) {
		return true;
	} else {
		return false;
	}
}

bool CListenSocket::IsValidSocket(CClientReqSocket* totest)
{
	// 0.42e
	return socket_list.find(totest) != socket_list.end();
}


void CListenSocket::UpdateConnectionsStatus()
{
	// 0.42e xcept for the khaos stats
	if( theApp.IsConnected() ) {
		totalconnectionchecks++;
		float percent;
		percent = (float)(totalconnectionchecks-1)/(float)totalconnectionchecks;
		if( percent > .99f ) {
			percent = .99f;
		}
		averageconnections = (averageconnections*percent) + (float)GetOpenSockets()*(1.0f-percent);
	}
}


float CListenSocket::GetMaxConperFiveModifier()
{
	float SpikeSize = GetOpenSockets() - averageconnections;
	if ( SpikeSize < 1 ) {
		return 1;
	}

	float SpikeTolerance = 2.5f*thePrefs::GetMaxConperFive();
	if ( SpikeSize > SpikeTolerance ) {
		return 0;
	}
	
	return 1.0f - (SpikeSize/SpikeTolerance);
}
