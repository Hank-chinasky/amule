// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
// Copyright (C) 2002 Merkur ( merkur-@users.sourceforge.net / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

// handling incoming connections (up or downloadrequests)

#ifndef LISTENSOCKET_H
#define LISTENSOCKET_H


#include "types.h"		// Needed for uint8, uint16, uint32 and uint64
#include "EMSocket.h"		// Needed for CEMSocket
#include "gsocket-fix.h"	// Needed for wxSOCKET_REUSEADDR

#include <wx/dynarray.h>

#include <map> 
#include <set> 
#include <list>

WX_DECLARE_OBJARRAY(wxString, ArrayOfwxStrings);

class CUpDownClient;
class CPacket;
class CTimerWnd;
class CPreferences;

class CClientReqSocketHandler;

class CClientReqSocket : public CEMSocket
{
friend class CClientReqSocketHandler;

	DECLARE_DYNAMIC_CLASS(CClientReqSocket)

	CClientReqSocket();

public:
	CClientReqSocket(CPreferences* in_prefs, CUpDownClient* in_client = 0);	
	virtual ~CClientReqSocket();
	virtual	void 	OnInit();
	virtual	bool 	Close(); /*	{return wxSocketBase::Close();}*/
	bool		Create();
	void		Disconnect(const wxString& strReason);

	void		ResetTimeOutTimer();
	bool		CheckTimeOut();
	void		Safe_Delete();

	bool		deletethis; // 0.30c (Creteil), set as bool

	void		OnClose(int nErrorCode);
	void		OnSend(int nErrorCode);
	void		OnReceive(int nErrorCode);
	void		OnError(int nErrorCode);
	void		OnConnect(int nErrorCode);
	
	uint32		timeout_timer;
	bool		hotrank;
	CPreferences* 	app_prefs;

	void		SetClient(CUpDownClient* client) { m_client = client; }
	CUpDownClient* GetClient() { return m_client; }
protected:
	bool	 PacketReceived(Packet* packet);

private:
	CUpDownClient*	m_client;
	
//	void	Delete_Timed();
	bool	ProcessPacket(const char *packet, uint32 size, uint8 opcode);
	bool	ProcessExtPacket(const char *packet, uint32 size, uint8 opcode);

	CClientReqSocketHandler* my_handler;
#ifdef AMULE_DAEMON
	wxMutex handler_exit;
#endif
};

#ifdef AMULE_DAEMON
#define CLIENT_REQ_SOCK_HANDLER_BASE wxThread
#else
#define CLIENT_REQ_SOCK_HANDLER_BASE wxEvtHandler
#endif

class CClientReqSocketHandler: public CLIENT_REQ_SOCK_HANDLER_BASE
{
public:
	CClientReqSocketHandler(CClientReqSocket* parent);

private:
	void ClientReqSocketHandler(wxSocketEvent& event);
	CClientReqSocket* socket;
	
#ifdef AMULE_DAEMON
public:
	// lfroen: for some reason wx can't Wait for detached threads
	~CClientReqSocketHandler();

private:
	void *Entry();
#else
	DECLARE_EVENT_TABLE();
#endif
};

class CSocketGlobalThread : public wxThread {
	void *Entry();
	
	std::set<CClientReqSocket *> socket_list;
public:
	CSocketGlobalThread(/*CListenSocket *socket*/);
	void AddSocket(CClientReqSocket* sock);
	void RemoveSocket(CClientReqSocket* sock);
};

// CListenSocket command target
class CListenSocket : public wxSocketServer
#ifdef AMULE_DAEMON
, public wxThread
#endif
{
	DECLARE_DYNAMIC_CLASS(CListenSocket)

	// The default constructor is here only because of IMPLEMENT_DYNAMIC_CLASS
	// in ListenSocket.cpp. Do we really need that?
	CListenSocket() : wxSocketServer(happyCompiler, wxSOCKET_REUSEADDR){};
	void *Entry();
#ifdef AMULE_DAEMON
	CSocketGlobalThread global_sock_thread;
#endif
public:
	CListenSocket(CPreferences* in_prefs,wxSockAddress& addr);
	~CListenSocket();
	bool	StartListening();
	void	StopListening();
	virtual void OnAccept(int nErrorCode);
	void	Process();
	void	RemoveSocket(CClientReqSocket* todel);
	void	AddSocket(CClientReqSocket* toadd);
	uint16	GetOpenSockets()		{return socket_list.size();}
	void	KillAllSockets();
	bool	TooManySockets(bool bIgnoreInterval = false);
	uint32	GetMaxConnectionReached()	{return maxconnectionreached;}
	bool    IsValidSocket(CClientReqSocket* totest);
	void	AddConnection();
	void	RecalculateStats();
	void	ReStartListening();
	void	UpdateConnectionsStatus();
	
	float	GetMaxConperFiveModifier();
	uint32	GetPeakConnections()		{ return peakconnections; }
	uint32	GetTotalConnectionChecks()	{ return totalconnectionchecks; }
	float	GetAverageConnections()		{ return averageconnections; }
	uint32	GetActiveConnections()		{ return activeconnections; }
	
private:
	typedef std::set<CClientReqSocket *> SocketSet;
	SocketSet socket_list;
	
	bool bListening;
	CPreferences* app_prefs;
	
	uint16 opensockets;
	uint16 m_OpenSocketsInterval;
	uint32 maxconnectionreached;
	wxIPV4address happyCompiler;
	uint16	m_ConnectionStates[3];
	uint16	m_nPeningConnections;
	uint32	peakconnections;
	uint32	totalconnectionchecks;
	float	averageconnections;
	uint32	activeconnections;

public:
	std::map<uint64, uint32> offensecounter;
	std::map<uint64, uint64> hashbase;  
};


#endif // LISTENSOCKET_H
