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

#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/app.h>		// Needed for wxApp
#include <wx/intl.h>		// Needed for wxLocale
#include <wx/string.h>		// Needed for wxString

#include "Timer.h"		// Needed for AMULE_TIMER_CLASS and AMULE_TIMER_EVENT_CLASS
#include "Types.h"		// Needed for int32, uint16 and uint64
#include "GuiEvents.h"		// Needed for GUIEvent

#include <list>			// Needed for std::list

// If wx version is less than 2.5.2, we need this defined. This new flag 
// is needed to ensure the old behaviour of sizers.
#if !wxCHECK_VERSION(2,5,2)
	#define wxFIXED_MINSIZE 0
#endif

class CAbstractFile;
class CKnownFile;
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
class wxSocketEvent;
class wxCommandEvent;
class wxFFileOutputStream;
class wxFile;
class CUpDownClient;

#define theApp wxGetApp()

enum APPState {
	APP_STATE_RUNNING = 0,
	APP_STATE_SHUTINGDOWN,
	APP_STATE_STARTING
};	


#ifdef AMULE_DAEMON
#define AMULE_APP_BASE wxAppConsole
#else
#define AMULE_APP_BASE wxApp
#endif

#define CONNECTED_ED2K 1<<0
#define CONNECTED_KAD_OK 1<<1
#define CONNECTED_KAD_FIREWALLED 1<<2

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

	// Socket handlers
	void ListenSocketHandler(wxSocketEvent& event);
	void ServerSocketHandler(wxSocketEvent& event);
	void UDPSocketHandler(wxSocketEvent& event);

	virtual void NotifyEvent(const GUIEvent& event) = 0;
	virtual void ShowAlert(wxString msg, wxString title, int flags) = 0;
	
	// Barry - To find out if app is running or shutting/shut down
	const bool IsRunning() const { return (m_app_state == APP_STATE_RUNNING); }
	const bool IsOnShutDown() const { return (m_app_state == APP_STATE_SHUTINGDOWN); }
	
	// Check ED2K and Kademlia state
	bool IsFirewalled();
	// Check if we should callback this client
	bool DoCallback( CUpDownClient *client );
	
	// Connection to ED2K
	bool IsConnectedED2K();
	// Connection to Kad
	bool IsConnectedKad();
	// Are we connected to at least one network?
	bool IsConnected();
	
	
	// ed2k URL functions
	wxString	CreateED2kLink(const CAbstractFile* f);
	wxString	CreateHTMLED2kLink(const CAbstractFile* f);
	wxString	CreateED2kSourceLink(const CAbstractFile* f);
	wxString	CreateED2kAICHLink(const CKnownFile* f);
	wxString	CreateED2kHostnameSourceLink(const CAbstractFile* f);
	
	void RunAICHThread();
	
	// Misc functions
	void		OnlineSig(bool zero = false); 
	void		Localize_mule();
	void		Trigger_New_version(wxString newMule);

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
	
	void ShowUserCount();
	
	void ShowConnectionState();

	void StartKad();
	void StopKad();
	
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

	static const wxString FullMuleVersion;
	static const wxString OSDescription;	
	static char *strFullMuleVersion;
	static char *strOSDescription;
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

	AMULE_TIMER_CLASS* core_timer;

private:
	virtual void OnUnhandledException();
	
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

    virtual int InitGui(bool geometry_enable, wxString &geometry_string);
	
	int OnExit();
	bool OnInit();
	
public:

    virtual void ShowAlert(wxString msg, wxString title, int flags);
	
	void ShutDown(wxCloseEvent &evt);
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

	bool OnInit();
	
	int OnExit();
	
	void OnCoreTimer(AMULE_TIMER_EVENT_CLASS& evt);

public:

	void Startup();
	
	bool ShowConnectionDialog();

	class CRemoteConnect *connect;
		
	CEConnectDlg *dialog;

	bool CopyTextToClipboard(wxString strText);

	virtual void ShowAlert(wxString msg, wxString title, int flags);

	void ShutDown(wxCloseEvent &evt);

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
	
	bool AddServer(CServer *srv, bool fromUser = false);
	
	uint32 GetPublicIP();
	wxString CreateED2kLink(const CAbstractFile* f);
	wxString CreateHTMLED2kLink(const CAbstractFile* f);
	wxString CreateED2kSourceLink(const CAbstractFile* f);
	wxString CreateED2kAICHLink(const CKnownFile* f);
	wxString CreateED2kHostnameSourceLink(const CAbstractFile* f);
	
	virtual void NotifyEvent(const GUIEvent& event);

	wxString GetLog(bool reset = false);
	wxString GetServerLog(bool reset = false);

	void AddServerMessageLine(wxString &msg);
	
	void SetOSFiles(wxString ) { /* onlinesig is created on remote side */ }
	
	bool IsConnectedED2K();
	bool IsConnectedKad();
	
	void StartKad();
	void StopKad();
	
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

class CSocketSet;

class CAmuledGSocketFuncTable : public GSocketGUIFunctionsTable {
		CSocketSet *m_in_set, *m_out_set;
		
		wxMutex m_lock;
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

class CDaemonAppTraits : public wxConsoleAppTraits {
		CAmuledGSocketFuncTable *m_table;

		wxMutex m_lock;
		std::list<wxObject *> m_sched_delete;
	public:
		CDaemonAppTraits(CAmuledGSocketFuncTable *table);
	    virtual GSocketGUIFunctionsTable* GetSocketGUIFunctionsTable();
	    virtual void ScheduleForDestroy(wxObject *object);
	    virtual void RemoveFromPendingDelete(wxObject *object);
	    
	    void DeletePending();
};

class CamuleDaemonApp : public CamuleApp {
	bool m_Exit;

	bool OnInit();
	int OnRun();
	int OnExit();
	
	virtual int InitGui(bool geometry_enable, wxString &geometry_string);
	
	CAmuledGSocketFuncTable *m_table;
public:
	CamuleDaemonApp();
	
	void ExitMainLoop() { m_Exit = true; }

	virtual void NotifyEvent(const GUIEvent& event);

	bool CopyTextToClipboard(wxString strText);

	virtual void ShowAlert(wxString msg, wxString title, int flags);

	wxMutex data_mutex;
	
	DECLARE_EVENT_TABLE()
	
	wxAppTraits *CreateTraits();

};

//#define CALL_APP_DATA_LOCK wxMutexLocker locker(theApp.data_mutex)
#define CALL_APP_DATA_LOCK

DECLARE_APP(CamuleDaemonApp)

#endif /* ! AMULE_DAEMON */

#endif // AMULE_H
