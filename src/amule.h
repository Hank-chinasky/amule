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

#ifndef AMULE_H
#define AMULE_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "amule.h"
#endif

#ifdef __WXMSW__
#include <wx/msw/winundef.h>
#endif

#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/app.h>			// Needed for wxApp
#include <wx/intl.h>		// Needed for wxLocale
#include <wx/textfile.h>		// Needed for wxTextFile
#include <wx/file.h>
#include <wx/string.h>

#include "Types.h"			// Needed for int32, uint16 and uint64
#include "GuiEvents.h"

#include <deque>
#include <list>

#include "CTypedPtrList.h"	// Needed for CLis

// If wx version is less than 2.5.2, we need this defined. This new flag 
// is needed to ensure the old behaviour of sizers.
#if !wxCHECK_VERSION(2,5,2)
	#define wxFIXED_MINSIZE 0
#endif

class CAbstractFile;
class ExternalConn;
class CamuleDlg;
class CPreferences;
class CDownloadQueue;
class CUploadQueue;
class CServerConnect;
class CSharedFileList;
class CServer;
class CFriend;
class CMD4Hash;
class CServerList;
class CListenSocket;
class CClientList;
class CKnownFileList;
class CSearchList;
class CClientCreditsList;
class CFriendList;
class CClientUDPSocket;
class CIPFilter;
class UploadBandwidthThrottler;
class CStatistics;
class wxServer;
class wxString;
class wxSocketEvent;
class wxTimer;
class wxTimerEvent;
class wxCommandEvent;
class wxFFileOutputStream;
class CUpDownClient;

#define theApp wxGetApp()

enum APPState {
	APP_STATE_RUNNING = 0,
	APP_STATE_SHUTINGDOWN,
	APP_STATE_STARTING
};	


#include <wx/event.h>
#include <list>

DECLARE_EVENT_TYPE(wxEVT_CORE_FILE_HASHING_FINISHED, wxEVT_USER_FIRST+FILE_HASHING_FINISHED)
DECLARE_EVENT_TYPE(wxEVT_CORE_FILE_HASHING_SHUTDOWN, wxEVT_USER_FIRST+FILE_HASHING_SHUTDOWN)
DECLARE_EVENT_TYPE(wxEVT_CORE_FINISHED_FILE_COMPLETION, wxEVT_USER_FIRST+FILE_COMPLETION_FINISHED)
DECLARE_EVENT_TYPE(wxEVT_CORE_FINISHED_HTTP_DOWNLOAD, wxEVT_USER_FIRST+HTTP_DOWNLOAD_FINISHED)

DECLARE_EVENT_TYPE(wxEVT_CORE_SOURCE_DNS_DONE, wxEVT_USER_FIRST+SOURCE_DNS_DONE)
DECLARE_EVENT_TYPE(wxEVT_CORE_UDP_DNS_DONE, wxEVT_USER_FIRST+UDP_DNS_DONE)
DECLARE_EVENT_TYPE(wxEVT_CORE_SERVER_DNS_DONE, wxEVT_USER_FIRST+SERVER_DNS_DONE)

DECLARE_EVENT_TYPE(wxEVT_AMULE_TIMER, wxEVT_USER_FIRST+EVENT_TIMER)

class wxMuleInternalEvent : public wxEvent {
	void *m_ptr;
	long m_value;
	int  m_commandInt;
	public:
	wxMuleInternalEvent(int id, int event_id) : wxEvent(event_id, id)
	{
	}
	wxMuleInternalEvent(int id) : wxEvent(-1, id)
	{
	}
	wxMuleInternalEvent(int id, void *ptr, long value) : wxEvent(-1, id)
	{
		m_ptr = ptr;
		m_value = value;
	}
	wxEvent *Clone(void) const
	{
		return new wxMuleInternalEvent(*this);
	}
	void SetExtraLong(long value)
	{
		m_value = value;
	}
	long GetExtraLong()
	{
		return m_value;
	}
	void SetInt(int i)
	{
		m_commandInt = i;
	}
	long GetInt() const
	{
		return m_commandInt; 
	}

	void SetClientData(void *ptr)
	{
		m_ptr = ptr;
	}
	void *GetClientData()
	{
		return m_ptr;
	}
};

#ifdef AMULE_DAEMON
#define AMULE_APP_BASE wxAppConsole
#else
#define AMULE_APP_BASE wxApp
#endif

class CamuleApp : public AMULE_APP_BASE
{
public:
	CamuleApp();
	virtual	 ~CamuleApp();
	
	virtual bool	OnInit();
	int		OnExit();
	void		OnFatalException();
	bool		ReinitializeNetwork(wxString *msg);

	// derived classes may override those
	virtual int InitGui(bool geometry_enable, wxString &geometry_string);

	virtual void NotifyEvent(const GUIEvent& event) = 0;
	virtual void ShowAlert(wxString msg, wxString title, int flags) = 0;
	
	// Barry - To find out if app is running or shutting/shut down
	const bool IsRunning() const { return (m_app_state == APP_STATE_RUNNING); }
	const bool IsOnShutDown() const { return (m_app_state == APP_STATE_SHUTINGDOWN); }
	
	// Check ED2K and Kademlia state
	bool IsFirewalled();
	// Check if we should callback this client
	bool DoCallback( CUpDownClient *client );
	
	// ed2k URL functions
	wxString	CreateED2kLink(const CAbstractFile* f);
	wxString	CreateHTMLED2kLink(const CAbstractFile* f);
	wxString	CreateED2kSourceLink(const CAbstractFile* f);
	wxString	CreateED2kHostnameSourceLink(const CAbstractFile* f);
	wxString	GenFakeCheckUrl(const CAbstractFile *f);
	wxString        GenFakeCheckUrl2(const CAbstractFile *f);
	
	void RunAICHThread();
	
	// Misc functions
	void		OnlineSig(bool zero = false); 
	void		Localize_mule();
	void Trigger_New_version(wxString newMule);

	// Used to detect a previous running instance of aMule
	wxServer*	localserver;
	
	// shakraw - new EC code using wxSocketBase
	ExternalConn*	ECServerHandler;

	// Kry - avoid chmod on win32
	bool		use_chmod;
	
	uint32	GetPublicIP() const;	// return current (valid) public IP or 0 if unknown
	void		SetPublicIP(const uint32 dwIP);

	// Other parts of the interface and such
	CPreferences*		glob_prefs;
	CDownloadQueue*		downloadqueue;
	CUploadQueue*		uploadqueue;
	CServerConnect*		serverconnect;
	CSharedFileList*	sharedfiles;
	CServerList*		serverlist;
	CListenSocket*		listensocket;
	CClientList*		clientlist;
	CKnownFileList*		knownfiles;
	CSearchList*		searchlist;
	CClientCreditsList*	clientcredits;
	CFriendList*		friendlist;
	CClientUDPSocket*	clientudp;
	CStatistics*		statistics;
	CIPFilter*		ipfilter;
	UploadBandwidthThrottler* uploadBandwidthThrottler;
	
	void ShutDown();
	
	wxString GetLog(bool reset = false);
	wxString GetServerLog(bool reset = false);
	wxString GetDebugLog(bool reset = false);
	
	bool AddServer(CServer *srv, bool fromUser = false);
	void AddServerMessageLine(wxString &msg);
#ifdef __DEBUG__
	void AddSocketDeleteDebug(uint32 socket_pointer, uint32 creation_time);
#endif
	void SetOSFiles(const wxString new_path); 
	
	wxString ConfigDir;

	void AddLogLine(const wxString &msg);
	
	const wxString& GetOSType() const { return OSType; }
	
	uint32 sent;

protected:
#ifdef __WXDEBUG__
	/**
	 * Handles asserts in a thread-safe manner.
	 */
	virtual void OnAssert(const wxChar *file, int line, const wxChar *cond, const wxChar *msg);
#endif
	
	/**
	 * This class is used to contain log messages that are to be displayed
	 * on the GUI, when it is currently impossible to do so. This is in order 
	 * to allows us to queue messages till after the dialog has been created.
	 */
	struct QueuedLogLine
	{
		//! The text line to be displayed
		wxString 	line;
		//! True if the line should be shown on the status bar, false otherwise.
		bool		show;
	};

	void OnUDPDnsDone(wxEvent& evt);
	void OnSourceDnsDone(wxEvent& evt);
	void OnServerDnsDone(wxEvent& evt);

	void OnTCPTimer(AMULE_TIMER_EVENT_CLASS& evt);

	void OnCoreTimer(AMULE_TIMER_EVENT_CLASS& evt);

	void OnFinishedHashing(wxEvent& evt);
	void OnFinishedCompletion(wxEvent& evt);
	void OnFinishedHTTPDownload(wxEvent& evt);
	void OnHashingShutdown(wxEvent&);
	
	void OnNotifyEvent(wxEvent& evt);
	
	void SetTimeOnTransfer();
			
	std::list<QueuedLogLine> m_logLines;
	
	wxLocale m_locale;

	APPState m_app_state;	

	wxString m_emulesig_path;
	wxString m_amulesig_path;
	
	wxString OSType;
	
	uint32 m_dwPublicIP;
	
	long webserver_pid;
	
#if wxCHECK_VERSION(2,5,3)
	wxFFileOutputStream* applog;
#else
	wxFile *applog;
#endif
	bool enable_stdout_log;
	bool enable_daemon_fork;
	wxString server_msg;
	
private:
	void CheckNewVersion(uint32 result);
	
};

#ifndef AMULE_DAEMON

class CamuleGuiBase {
public:
	CamuleGuiBase();
	virtual	 ~CamuleGuiBase();

	wxString	m_FrameTitle;
	CamuleDlg*	amuledlg;
	
	bool CopyTextToClipboard( wxString strText );

	virtual void NotifyEvent(const GUIEvent& event);
	virtual int InitGui(bool geometry_enable, wxString &geometry_string);
	virtual void ShowAlert(wxString msg, wxString title, int flags);
};

#ifndef CLIENT_GUI

class CamuleGuiApp : public CamuleApp, public CamuleGuiBase {
	AMULE_TIMER_CLASS* core_timer;

    virtual int InitGui(bool geometry_enable, wxString &geometry_string);
	
	// Socket handlers
	void ListenSocketHandler(wxSocketEvent& event);
	void ServerUDPSocketHandler(wxSocketEvent& event);
	void ServerSocketHandler(wxSocketEvent& event);
	void ClientUDPSocketHandler(wxSocketEvent& event);

	int OnExit();
	bool OnInit();
	
public:

    virtual void ShowAlert(wxString msg, wxString title, int flags);
	
	void ShutDown();
	virtual void NotifyEvent(const GUIEvent& event);
	
	wxString GetLog(bool reset = false);
	wxString GetServerLog(bool reset = false);
	void AddServerMessageLine(wxString &msg);
	DECLARE_EVENT_TABLE()
};

DECLARE_APP(CamuleGuiApp)

#else /* !CLIENT_GUI */

#include "amule-remote-gui.h"

class CamuleRemoteGuiApp : public wxApp, public CamuleGuiBase {
	AMULE_TIMER_CLASS* core_timer;

	virtual int InitGui(bool geometry_enable, wxString &geometry_string);

	int OnExit();
	bool OnInit();
	
	void OnCoreTimer(AMULE_TIMER_EVENT_CLASS& evt);

	class CRemoteConnect *connect;

public:
	bool CopyTextToClipboard(wxString strText);

	virtual void ShowAlert(wxString msg, wxString title, int flags);

	void ShutDown();

	uint32 GetUptimeMsecs();

	CPreferencesRem *glob_prefs;
	wxString ConfigDir;
	
	//
	// Provide access to core data thru EC
	CServerConnectRem *serverconnect;
	CServerListRem *serverlist;
	CUpQueueRem *uploadqueue;
	CDownQueueRem *downloadqueue;
	CSharedFilesRem *sharedfiles;
	CKnownFilesRem *knownfiles;
	CClientCreditsRem *clientcredits;
	CClientListRem *clientlist;
	CIPFilterRem *ipfilter;
	CSearchListRem *searchlist;
	CListenSocketRem *listensocket;

	CStatistics *statistics;
	CStatisticsRem *rem_stat_updater;
	
	bool AddServer(CServer *srv, bool fromUser = false);
	
	uint32 GetPublicIP();
	wxString CreateED2kLink(const CAbstractFile* f);
	wxString CreateHTMLED2kLink(const CAbstractFile* f);
	wxString CreateED2kSourceLink(const CAbstractFile* f);
	wxString CreateED2kHostnameSourceLink(const CAbstractFile* f);
	wxString GenFakeCheckUrl(const CAbstractFile *f);
	wxString GenFakeCheckUrl2(const CAbstractFile *f);
	
	virtual void NotifyEvent(const GUIEvent& event);

	wxString GetLog(bool reset = false);
	wxString GetServerLog(bool reset = false);

	void AddServerMessageLine(wxString &msg);
	
	void SetOSFiles(wxString ) { /* onlinesig is created on remote side */ }
	
	DECLARE_EVENT_TABLE()

protected:
	wxLocale	m_locale;
};

DECLARE_APP(CamuleRemoteGuiApp)

#endif // CLIENT_GUI

#define CALL_APP_DATA_LOCK

#else /* ! AMULE_DAEMON */

#include <wx/apptrait.h>
#include <wx/socket.h>

class CDaemonAppTraits : public wxConsoleAppTraits {
	public:
	    virtual GSocketGUIFunctionsTable* GetSocketGUIFunctionsTable();
	    virtual void ScheduleForDestroy(wxObject *object);
	    virtual void RemoveFromPendingDelete(wxObject *object);
};

class CAmuledGSocketFuncTable : public GSocketGUIFunctionsTable {
		int m_in_fds[1024], m_out_fds[1024];
		GSocket * m_in_gsocks[1024];
		GSocket * m_out_gsocks[1024];
		
		int m_in_fds_count, m_out_fds_count;

		fd_set m_readset, m_writeset;
	public:
		CAmuledGSocketFuncTable();

		void AddSocket(GSocket *socket, GSocketEvent event);
		void RemoveSocket(GSocket *socket, GSocketEvent event);
		void RunSelect();

		virtual bool OnInit();
		virtual void OnExit();
		virtual bool CanUseEventLoop();
		virtual bool Init_Socket(GSocket *socket);
		virtual void Destroy_Socket(GSocket *socket);
		virtual void Install_Callback(GSocket *socket, GSocketEvent event);
		virtual void Uninstall_Callback(GSocket *socket, GSocketEvent event);
		virtual void Enable_Events(GSocket *socket);
		virtual void Disable_Events(GSocket *socket);
};

class CamuleDaemonApp : public CamuleApp {
	bool m_Exit;
	int OnRun();
	int OnExit();
	
	virtual int InitGui(bool geometry_enable, wxString &geometry_string);
public:
	CamuleDaemonApp();
	
	void ExitMainLoop() { m_Exit = true; }

	virtual void NotifyEvent(const GUIEvent& event);

	bool CopyTextToClipboard(wxString strText);

	virtual void ShowAlert(wxString msg, wxString title, int flags);

	wxMutex data_mutex;
	
	DECLARE_EVENT_TABLE()
	
#warning Uncomment to enable new socket code
	// lfroen:
	// Still in comment, so existing code will not break
	// untill I commit all implementation
	//
	//wxAppTraits *CreateTraits();

};


class CamuleLocker : public wxMutexLocker {
	uint32 msStart;
public:
	CamuleLocker();
	~CamuleLocker();
};

#define CALL_APP_DATA_LOCK wxMutexLocker locker(theApp.data_mutex)

DECLARE_APP(CamuleDaemonApp)

#endif /* ! AMULE_DAEMON */

#endif // AMULE_H
