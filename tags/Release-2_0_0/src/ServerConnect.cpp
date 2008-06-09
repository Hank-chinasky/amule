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

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "ServerConnect.h"
#endif

#include "ServerConnect.h"		// Interface declarations.
#include "SearchList.h"		// Needed for CSearchList
#include "GetTickCount.h"	// Needed for GetTickCount
#include "UploadQueue.h"	// Needed for CUploadQueue
#include "ServerUDPSocket.h"		// Needed for CServerUDPSocket
#include "SharedFileList.h"	// Needed for CSharedFileList
#include "Packet.h"		// Needed for CTag
#include "OPCodes.h"		// Needed for CT_NAME
#include "SafeFile.h"		// Needed for CSafeMemFile
#include "OtherFunctions.h"	// Needed for GetTickCount
#include "ServerSocket.h"	// Needed for CServerSocket
#include "ListenSocket.h"	// Needed for CListenSocket
#include "Server.h"		// Needed for CServer
#include "amule.h"		// Needed for theApp
#include "ServerList.h"		// Needed for CServerList
#include "Preferences.h"	// Needed for CPreferences
#include "updownclient.h"	// for SO_AMULE
#include "Statistics.h"
#include "NetworkFunctions.h" // for StringHosttoUint32
#include "Logger.h"
#include "Format.h"

#ifndef AMULE_DAEMON
	#include "SearchDlg.h"		// Needed for CSearchDlg
#endif


//#define DEBUG_CLIENT_PROTOCOL


void CServerConnect::TryAnotherConnectionrequest()
{
	if ( connectionattemps.size() < (( thePrefs::IsSafeServerConnectEnabled()) ? 1 : 2) ) {
	
		CServer*  next_server = used_list->GetNextServer();

		if (!next_server)
		{
			if ( connectionattemps.empty() ) {
				AddLogLineM(true, _("Failed to connect to all servers listed. Making another pass."));
				
				ConnectToAnyServer( lastStartAt );
			}
			return;
		}

		// Barry - Only auto-connect to static server option
		if ( thePrefs::AutoConnectStaticOnly() ) {
			if ( next_server->IsStaticMember() )
                ConnectToServer(next_server, true);
		} else {
			ConnectToServer(next_server, true);
		}
	}
}


void CServerConnect::ConnectToAnyServer(uint32 startAt,bool prioSort,bool isAuto)
{
	lastStartAt=startAt;
	StopConnectionTry();
	Disconnect();
	connecting = true;
	singleconnecting = false;
	Notify_ShowConnState(false,wxEmptyString);


	// Barry - Only auto-connect to static server option
	if (thePrefs::AutoConnectStaticOnly() && isAuto)
	{
		bool anystatic = false;
		CServer *next_server; 
		used_list->SetServerPosition( startAt );
		while ((next_server = used_list->GetNextServer()) != NULL)
		{
			if (next_server->IsStaticMember())
			{
				anystatic = true;
				break;
			}
		}
		if (!anystatic)
		{
			connecting = false;
			AddLogLineM(true,_("No valid servers to connect in serverlist found"));
			return;
		}
	}

	used_list->SetServerPosition( startAt );
	if ( thePrefs::Score() && prioSort ) used_list->Sort();

	if (used_list->GetServerCount()==0 ){
		connecting = false;
		AddLogLineM(true,_("No valid servers to connect in serverlist found"));
		return;
	}
	theApp.listensocket->Process();

	TryAnotherConnectionrequest();
}


void CServerConnect::ConnectToServer(CServer* server, bool multiconnect)
{
	if (!multiconnect) {
		StopConnectionTry();
		Disconnect();
	}
	connecting = true;
	singleconnecting = !multiconnect;

//#ifdef TESTING_PROXY
	CServerSocket* newsocket = new CServerSocket(this, thePrefs::GetProxyData());
	m_lstOpenSockets.AddTail(newsocket);
	newsocket->ConnectToServer(server);

	connectionattemps[GetTickCount()] = newsocket;
}


void CServerConnect::StopConnectionTry()
{
	connectionattemps.clear();
	connecting = false;
	singleconnecting = false;

	if (m_idRetryTimer.IsRunning()) 
	{ 
	  m_idRetryTimer.Stop();
	} 

	// close all currenty opened sockets except the one which is connected to our current server
	for( POSITION pos = m_lstOpenSockets.GetHeadPosition(); pos != NULL; ) {
		CServerSocket* pSck = m_lstOpenSockets.GetNext(pos);
		if (pSck == connectedsocket)		// don't destroy socket which is connected to server
			continue;
		if (pSck->m_bIsDeleting == false)	// don't destroy socket if it is going to destroy itself later on
			DestroySocket(pSck);
	}
}


#define CAPABLE_ZLIB 0x01
#define CAPABLE_IP_IN_LOGIN_FRAME 0x02
#define CAPABLE_AUXPORT 0x04
#define CAPABLE_NEWTAGS 0x08
#define CAPABLE_UNICODE 0x10

void CServerConnect::ConnectionEstablished(CServerSocket* sender)
{
	if (connecting == false)
	{
		// we are already connected to another server
		DestroySocket(sender);
		return;
	}	
	InitLocalIP();
	
	if (sender->GetConnectionState() == CS_WAITFORLOGIN) {
		AddLogLineM(false, CFormat( _("Connected to %s (%s:%i)") )
			% sender->cur_server->GetListName()
			% sender->cur_server->GetFullIP()
			% sender->cur_server->GetPort() );

		//send loginpacket
		CServer* update = theApp.serverlist->GetServerByAddress( sender->cur_server->GetAddress(), sender->cur_server->GetPort() );
		if (update){
			update->ResetFailedCount();
			Notify_ServerRefresh( update );
		}
		
		CSafeMemFile data(256);
		data.WriteHash16(thePrefs::GetUserHash());
		// Why pass an ID, if we are loggin in?
		data.WriteUInt32(GetClientID());
		data.WriteUInt16(thePrefs::GetPort());
		data.WriteUInt32(5); // tagcount

		CTag tagname(CT_NAME,thePrefs::GetUserNick());
		tagname.WriteTagToFile(&data);

		CTag tagversion(CT_VERSION,EDONKEYVERSION);
		tagversion.WriteTagToFile(&data);
		
		CTag tagport(CT_PORT,thePrefs::GetPort());
		tagport.WriteTagToFile(&data);
		
		// FLAGS for server connection
		CTag tagflags(CT_SERVER_FLAGS, CAPABLE_ZLIB 
													| CAPABLE_AUXPORT 
													| CAPABLE_NEWTAGS 
#if wxUSE_UNICODE													
													| CAPABLE_UNICODE
#endif													
													); 
		
		tagflags.WriteTagToFile(&data);

		// eMule Version (14-Mar-2004: requested by lugdunummaster (need for LowID clients which have no chance 
		// to send an Hello packet to the server during the callback test))
		CTag tagMuleVersion(CT_EMULE_VERSION, 
			(SO_AMULE	<< 24) |
			(VERSION_MJR	<< 17) |
			(VERSION_MIN	<< 10) |
			(VERSION_UPDATE	<<  7) );
		tagMuleVersion.WriteTagToFile(&data);

		CPacket* packet = new CPacket(&data);
		packet->SetOpCode(OP_LOGINREQUEST);
		#ifdef DEBUG_CLIENT_PROTOCOL
		AddLogLineM(true,wxT("Client: OP_LOGINREQUEST"));
		AddLogLineM(true,wxString(wxT("        Hash     : ")) << thePrefs::GetUserHash().Encode());
		AddLogLineM(true,wxString(wxT("        ClientID : ")) << GetClientID());
		AddLogLineM(true,wxString(wxT("        Port     : ")) << thePrefs::GetPort());
		AddLogLineM(true,wxString(wxT("        User Nick: ")) << thePrefs::GetUserNick());
		AddLogLineM(true,wxString(wxT("        Edonkey  : ")) << EDONKEYVERSION);
		#endif
		theApp.statistics->AddUpDataOverheadServer(packet->GetPacketSize());
		SendPacket(packet, true, sender);
	} else if (sender->GetConnectionState() == CS_CONNECTED){
		theApp.statistics->AddReconnect();
		theApp.statistics->SetServerConnectTime(GetTickCount64());
		connected = true;
		AddLogLineM(true, CFormat( _("Connection established on: %s") ) % sender->cur_server->GetListName());
		connectedsocket = sender;
		Notify_ShowConnState(true,connectedsocket->cur_server->GetListName());
		
		StopConnectionTry();
		
		CServer* update = theApp.serverlist->GetServerByAddress(connectedsocket->cur_server->GetAddress(),sender->cur_server->GetPort());
		if ( update )
			Notify_ServerHighlight(update, true);
		
		theApp.sharedfiles->ClearED2KPublishInfo();

		Notify_ServerRemoveDead();
		
		// tecxx 1609 2002 - serverlist update
		if (thePrefs::AddServersFromServer())
		{
			CPacket* packet = new CPacket(OP_GETSERVERLIST,0);
			theApp.statistics->AddUpDataOverheadServer(packet->GetPacketSize());
			SendPacket(packet, true);
			#ifdef DEBUG_CLIENT_PROTOCOL
			AddLogLineM(true,wxT("Client: OP_GETSERVERLIST"));
			#endif
		}
	}
}


bool CServerConnect::SendPacket(CPacket* packet,bool delpacket, CServerSocket* to)
{
	if (!to) {
		if (connected) {
			return connectedsocket->SendPacket(packet, delpacket, true);
		} else {
			if ( delpacket ) {
				delete packet;
			}
			
			return false;
		}
	} else {
		return to->SendPacket(packet, delpacket, true);
	}
}


bool CServerConnect::SendUDPPacket(CPacket* packet, CServer* host, bool delpacket)
{
	if (connected) {
		serverudpsocket->SendPacket(packet, host);
	}

	if (delpacket)
		delete packet;

	return true;
}


void CServerConnect::ConnectionFailed(CServerSocket* sender){
	if (connecting == false && sender != connectedsocket)
	{
		// just return, cleanup is done by the socket itself
		return;
	}
	//messages
	CServer* update = NULL;
	switch (sender->GetConnectionState()){
		case CS_FATALERROR:
			AddLogLineM(true, _("Fatal Error while trying to connect. Internet connection might be down"));
			break;
		case CS_DISCONNECTED:
			theApp.sharedfiles->ClearED2KPublishInfo();
			AddLogLineM(false,CFormat( _("Lost connection to %s (%s:%i)") )
				% sender->cur_server->GetListName()
				% sender->cur_server->GetFullIP()
				% sender->cur_server->GetPort() );

			update = theApp.serverlist->GetServerByAddress( sender->cur_server->GetAddress(), sender->cur_server->GetPort() );
			if (update){
				Notify_ServerHighlight(update, false);
			}
			break;
		case CS_SERVERDEAD:
			AddLogLineM(false, CFormat( _("%s (%s:%i) appears to be dead.") )
				% sender->cur_server->GetListName()
				% sender->cur_server->GetFullIP()
				% sender->cur_server->GetPort() );

			update = theApp.serverlist->GetServerByAddress( sender->cur_server->GetAddress(), sender->cur_server->GetPort() );
			if (update) {
				update->AddFailedCount();
				Notify_ServerRefresh( update );
			}
			break;
		case CS_ERROR:
			break;
		case CS_SERVERFULL:
			AddLogLineM(false, CFormat( _("%s (%s:%i) appears to be full.") )
				% sender->cur_server->GetListName()
				% sender->cur_server->GetFullIP()
				% sender->cur_server->GetPort() );
			
			break;
		case CS_NOTCONNECTED:; 
			break; 
	}

	// IMPORTANT: mark this socket not to be deleted in StopConnectionTry(), 
	// because it will delete itself after this function!
	sender->m_bIsDeleting = true;

	switch (sender->GetConnectionState()) {
		case CS_FATALERROR:{
			bool autoretry= !singleconnecting;
			StopConnectionTry();
			if ((thePrefs::Reconnect()) && (autoretry) && (!m_idRetryTimer.IsRunning())){ 
				AddLogLineM(false, wxString::Format(_("Automatic connection to server will retry in %d seconds"), CS_RETRYCONNECTTIME)); 
				
				m_idRetryTimer.SetOwner(&theApp,TM_TCPSOCKET);
				m_idRetryTimer.Start(1000*CS_RETRYCONNECTTIME);
			}
			Notify_ShowConnState(false,wxEmptyString);
			break;
		}
		case CS_DISCONNECTED:{
			theApp.sharedfiles->ClearED2KPublishInfo();		
			connected = false;
			Notify_ServerHighlight(sender->cur_server,false);
			if (connectedsocket) 
				connectedsocket->Close();
			connectedsocket = NULL;
			theApp.searchlist->StopGlobalSearch();			
			Notify_SearchCancel();
			theApp.statistics->SetServerConnectTime(0);
			if (thePrefs::Reconnect() && !connecting){
				ConnectToAnyServer();		
			}
			
			AddLogLineM( true, _("Connection lost") );
			Notify_ShowConnState(false,wxEmptyString);
			break;
		}
		case CS_ERROR:
		case CS_NOTCONNECTED:{
			if (!connecting)
				break;
			AddLogLineM(false, CFormat( _("Connecting to %s (%s:%i) failed.") )
				% sender->info
				% sender->cur_server->GetFullIP()
				% sender->cur_server->GetPort() );
		}
		case CS_SERVERDEAD:
		case CS_SERVERFULL:{
			if (!connecting)
				break;
			if (singleconnecting){
				StopConnectionTry();
				break;
			}

			std::map<uint32, CServerSocket*>::iterator it = connectionattemps.begin();
			while ( it != connectionattemps.end() ){
				if ( it->second == sender ) {
					connectionattemps.erase( it );
					break;
				} 
				++it;
			}			
			TryAnotherConnectionrequest();
		}
	}
	Notify_ShowConnState(false,wxEmptyString);
}

void CServerConnect::CheckForTimeout()
{
	uint32 dwCurTick = GetTickCount();

	std::map<uint32, CServerSocket*>::iterator it = connectionattemps.begin();
	while ( it != connectionattemps.end() ){
		if ( !it->second ) {
			AddLogLineM(false, _("Error: Socket invalid at timeoutcheck"));
			connectionattemps.erase( it );
			return;
		}

		if ( dwCurTick - it->first > CONSERVTIMEOUT) {
			uint32 key = it->first;
			CServerSocket* value = it->second;
			++it;
			if (!value->IsSolving()) {
				AddLogLineM(false, CFormat( _("Connection attempt to %s (%s:%i) timed out.") )
					% value->info
					% value->cur_server->GetFullIP()
					% value->cur_server->GetPort() );
			
				connectionattemps.erase( key );
	
				TryAnotherConnectionrequest();
				DestroySocket( value );
			}				
		} else {
			++it;
		}
	}
}


bool CServerConnect::Disconnect()
{
	if (connected && connectedsocket) {
		theApp.sharedfiles->ClearED2KPublishInfo();

		connected = false;

		CServer* update = theApp.serverlist->GetServerByAddress(
			connectedsocket->cur_server->GetAddress(),
			connectedsocket->cur_server->GetPort());
		Notify_ServerHighlight(update, false);
		theApp.SetPublicIP(0);
		DestroySocket(connectedsocket);
		connectedsocket = NULL;
		Notify_ShowConnState(false,wxEmptyString);
		theApp.statistics->SetServerConnectTime(0);
		return true;
	}
	else
		return false;
}


CServerConnect::CServerConnect(CServerList* in_serverlist, amuleIPV4Address &address)
{
	connectedsocket = NULL;
	used_list = in_serverlist;
	max_simcons = (thePrefs::IsSafeServerConnectEnabled()) ? 1 : 2;
	connecting = false;
	connected = false;
	clientid = 0;
	singleconnecting = false;

	// initalize socket for udp packets
//#ifdef TESTING_PROXY
	serverudpsocket = new CServerUDPSocket(this, address, thePrefs::GetProxyData());
	m_idRetryTimer.SetOwner(&theApp,TM_TCPSOCKET);
	lastStartAt=0;	
	InitLocalIP();	
}


CServerConnect::~CServerConnect()
{
	m_idRetryTimer.Stop();
	// stop all connections
	StopConnectionTry();
	// close connected socket, if any
	DestroySocket(connectedsocket);
	connectedsocket = NULL;
	// close udp socket
#ifdef AMULE_DAEMON
	// daemon have thread there
	serverudpsocket->Delete();
#else
	serverudpsocket->Close();
	delete serverudpsocket;
#endif
}


CServer* CServerConnect::GetCurrentServer()
{
	if (IsConnected() && connectedsocket) {
		return connectedsocket->cur_server;
	}
	return NULL;
}


void CServerConnect::SetClientID(uint32 newid)
{
	clientid = newid;
	
	if (!IsLowIDED2K(newid)) {
		theApp.SetPublicIP(newid);
	}

	Notify_ShowConnState(IsConnected(),GetCurrentServer()->GetListName());
}


void CServerConnect::DestroySocket(CServerSocket* pSck){
	if (pSck == NULL)
		return;
	// remove socket from list of opened sockets
	POSITION pos = m_lstOpenSockets.Find( pSck );
	if ( pos != NULL ) {
		m_lstOpenSockets.RemoveAt( pos );
	}

	pSck->Destroy();
}


bool CServerConnect::IsLocalServer(uint32 dwIP, uint16 nPort)
{
	if (IsConnected()){
		if (connectedsocket->cur_server->GetIP() == dwIP && connectedsocket->cur_server->GetPort() == nPort)
			return true;
	}
	return false;
}


void CServerConnect::KeepConnectionAlive()
{
	uint32 dwServerKeepAliveTimeout = thePrefs::GetServerKeepAliveTimeout();
	if (dwServerKeepAliveTimeout && connected && connectedsocket &&
	connectedsocket->connectionstate == CS_CONNECTED &&
	GetTickCount() - connectedsocket->GetLastTransmission() >= dwServerKeepAliveTimeout) {
		// "Ping" the server if the TCP connection was not used for the specified interval with
		// an empty publish files packet -> recommended by lugdunummaster himself!
		
		CSafeMemFile* files = new CSafeMemFile(4);
		files->WriteUInt32(0); //nFiles
	
		CPacket* packet = new CPacket(files);
		packet->SetOpCode(OP_OFFERFILES);
		#ifdef DEBUG_CLIENT_PROTOCOL
		AddLogLineM(true,wxT("Client: OP_OFFERFILES"));
		#endif
		// compress packet
		//   - this kind of data is highly compressable (N * (1 MD4 and at least 3 string meta data tags and 1 integer meta data tag))
		//   - the min. amount of data needed for one published file is ~100 bytes
		//   - this function is called once when connecting to a server and when a file becomes shareable - so, it's called rarely.
		//   - if the compressed size is still >= the original size, we send the uncompressed packet
		// therefor we always try to compress the packet
		theApp.statistics->AddUpDataOverheadServer(packet->GetPacketSize());
		connectedsocket->SendPacket(packet,true);
		
		AddDebugLogLineM(false, logServer, wxT("Refreshing server connection"));
		delete files;
 	}
}


void CServerConnect::InitLocalIP()
{
	m_nLocalIP = 0;

	try{
		m_nLocalIP = StringHosttoUint32(GetLocalHost());
	}
	catch(...){
		// at least two ppl reported crashs when using 'gethostbyname' with third party winsock DLLs
		AddDebugLogLineM(false, logGeneral, wxT("Unknown exception in CServerConnect::InitLocalIP"));
	}
}