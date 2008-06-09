//
// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
// Copyright (C) 2002 Merkur ( merkur-@users.sourceforge.net / http://www.emule-project.net )
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include <unistd.h>           // Needed for close(2) and sleep(3)
#include <wx/defs.h>

#ifdef __WXMSW__
	#include <winsock.h>
	#include <wx/msw/winundef.h>
#else
	#ifdef __BSD__
     	  #include <sys/types.h>
	#endif /* __BSD__ */
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

#ifdef __WXGTK__

	#ifdef __BSD__
     	#include <sys/param.h>
       	#include <sys/mount.h>
	#else 
		#include <execinfo.h>
		#include <mntent.h>
	#endif /* __BSD__ */

	
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"		// Needed for HAVE_GETRLIMIT, HAVE_SETRLIMIT, HAVE_SYS_RESOURCE_H,
#endif				//   LOCALEDIR, PACKAGE, PACKAGE_STRING and VERSION

#ifdef __WXMAC__
	#warning Kry? Get rid of this!
	#include <wx/wx.h>
#endif
#include <wx/filefn.h>
#include <wx/ffile.h>
#include <wx/file.h>
#include <wx/filename.h>                // Needed for wxFileName::GetPathSeparator()
#include <wx/log.h>
#include <wx/timer.h>
#include <wx/config.h>
#include <wx/socket.h>			// Needed for wxSocket
#include <wx/utils.h>
#include <wx/ipc.h>
#include <wx/intl.h>			// Needed for i18n
#include <wx/mimetype.h>		// For launching default browser
#include <wx/textfile.h>		// Needed for wxTextFile
#include <wx/cmdline.h>			// Needed for wxCmdLineParser
#include <wx/tokenzr.h>			// Needed for wxStringTokenizer
#include <wx/url.h>

#include "amule.h"				// Interface declarations.
#include "GetTickCount.h"		// Needed for GetTickCount
#include "server.h"				// Needed for GetListName
#include "CFile.h"				// Needed for CFile
#include "otherfunctions.h"		// Needed for GetTickCount
#include "IPFilter.h"			// Needed for CIPFilter
#include "UploadQueue.h"		// Needed for CUploadQueue
#include "DownloadQueue.h"		// Needed for CDownloadQueue
#include "ClientCredits.h"		// Needed for CClientCreditsList
#include "ClientUDPSocket.h"	// Needed for CClientUDPSocket
#include "SharedFileList.h"		// Needed for CSharedFileList
#include "sockets.h"			// Needed for CServerConnect
#include "ServerList.h"			// Needed for CServerList
#include "KnownFileList.h"		// Needed for CKnownFileList
#include "SearchList.h"			// Needed for CSearchList
#include "ClientList.h"			// Needed for CClientList
#include "Preferences.h"		// Needed for CPreferences
#include "ListenSocket.h"		// Needed for CListenSocket
#include "ExternalConn.h"		// Needed for ExternalConn & MuleConnection
#include "ServerSocket.h"	// Needed for CServerSocket
#include "UDPSocket.h"		// Needed for CUDPSocket
#include "PartFile.h"		// Needed for CPartFile
#include "AddFileThread.h"	// Needed for CAddFileThread
#include "packets.h"
#include "PrefsUnifiedDlg.h"
#include "AICHSyncThread.h"

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef __GLIBC__
# define RLIMIT_RESOURCE __rlimit_resource
#else
# define RLIMIT_RESOURCE int
#endif

BEGIN_EVENT_TABLE(CamuleApp, wxAppConsole)

	// Socket timers (TCP + UDO)
	EVT_CUSTOM(wxEVT_AMULE_TIMER, TM_UDPSOCKET, CamuleApp::OnUDPTimer)
	EVT_CUSTOM(wxEVT_AMULE_TIMER, TM_TCPSOCKET, CamuleApp::OnTCPTimer)

	// Core timer is OnRun
	EVT_CUSTOM(wxEVT_NOTIFY_EVENT, -1, CamuleApp::OnNotifyEvent)

	// Async dns handling
        EVT_CUSTOM(wxEVT_CORE_DNS_DONE, -1, CamuleApp::OnDnsDone)
	
        EVT_CUSTOM(wxEVT_CORE_SOURCE_DNS_DONE, -1, CamuleApp::OnSourcesDnsDone)

	// Hash ended notifier
	EVT_CUSTOM(wxEVT_CORE_FILE_HASHING_FINISHED, -1, CamuleApp::OnFinishedHashing)

	// Hashing thread finished and dead
	EVT_CUSTOM(wxEVT_CORE_FILE_HASHING_SHUTDOWN, -1, CamuleApp::OnHashingShutdown)

	// File completion ended notifier
	EVT_CUSTOM(wxEVT_CORE_FINISHED_FILE_COMPLETION, -1, CamuleApp::OnFinishedCompletion)

	// HTTPDownload finished
	EVT_CUSTOM(wxEVT_CORE_FINISHED_HTTP_DOWNLOAD, -1, CamuleApp::OnFinishedHTTPDownload)
	
END_EVENT_TABLE()

IMPLEMENT_APP(CamuleApp)

//
// LOGGING
//
// lfroen: do it simple.
wxFile *applog = 0;
bool enable_stdout_log = false;
wxString server_msg;

int CamuleApp::OnRun()
{
	AddDebugLogLineM(true, _("CamuleApp::OnRun()"));
	// lfroen: this loop is instead core timer.
	// Actually, using such loop is worst design decision i ever
	// saw. Right way is to have thread per socket.
	while ( 1 ) {
		wxThread::Sleep(100);
		// lock data after sleep
		CALL_APP_DATA_LOCK;
		OnCoreTimer(*((wxEvent *)0));
		ProcessPendingEvents();
		
	}
	return 0;
}

static void UnlimitResource(RLIMIT_RESOURCE resType)
{
#if defined(HAVE_GETRLIMIT) && defined(HAVE_SETRLIMIT)
	struct rlimit rl;
	getrlimit(resType, &rl);
	rl.rlim_cur = rl.rlim_max;
	setrlimit(resType, &rl);
#endif
}


static void SetResourceLimits()
{
#ifdef HAVE_SYS_RESOURCE_H
	UnlimitResource(RLIMIT_DATA);
	UnlimitResource(RLIMIT_FSIZE);
	UnlimitResource(RLIMIT_NOFILE);
#ifdef RLIMIT_RSS
	UnlimitResource(RLIMIT_RSS);
#endif
#endif
}


int CamuleApp::OnExit()
{

	if (core_timer) {
		// Stop the Core Timer
		delete core_timer;
	}

	if (m_app_state!=APP_STATE_STARTING) {
		printf("Now, exiting main app...\n");
	}

	if (serverlist) {
		delete serverlist;
		serverlist = NULL;
	}
	
	if (searchlist) {
		delete searchlist;
		searchlist = NULL;
	}
	
	if (clientcredits) {
		delete clientcredits;
		clientcredits = NULL;
	}
	
	// Destroying CDownloadQueue calls destructor for CPartFile
	// calling CSharedFileList::SafeAddKFile occasally.
	
	if (sharedfiles) {
		delete sharedfiles;
		sharedfiles = NULL;
	}

	if (serverconnect) {
		delete serverconnect;
		serverconnect = NULL;
	}

	if (listensocket) {
		delete listensocket;
		listensocket = NULL;
	}

	if (knownfiles) {
		delete knownfiles;
		knownfiles = NULL;
	}

	if (clientlist) {
		delete clientlist;
		clientlist = NULL;
	}

	if (uploadqueue) {
		delete uploadqueue;
		uploadqueue = NULL;
	}

	if (downloadqueue) {
		delete downloadqueue;
		downloadqueue = NULL;
	}

	if (ipfilter) {
		delete ipfilter;
		ipfilter = NULL;
	}

	if (ECServerHandler) {
		delete ECServerHandler;
		ECServerHandler = NULL;
	}

	if (glob_prefs) {
		delete glob_prefs;
		glob_prefs = NULL;
	}

	if (localserver) {
		delete localserver;
		localserver = NULL;
	}
	
	if (m_app_state!=APP_STATE_STARTING) {
		printf("aMule shutdown completed.\n");
	}

	// Return 0 for succesful program termination
	return 0;
}


bool CamuleApp::OnInit()
{
	wxPendingEventsLocker = new wxCriticalSection;

	m_app_state = APP_STATE_STARTING;
	ConfigDir = wxGetHomeDir() + wxFileName::GetPathSeparator() + wxT(".aMule") + wxFileName::GetPathSeparator();

	// open debug log
	applog = new wxFile();
	applog->Create(ConfigDir + wxFileName::GetPathSeparator() + wxT("logfile"),
			 wxFile::read_write);
	//applog = new wxFFile(ConfigDir + wxT("logfile"), "rw");
	if ( !applog->IsOpened() ) {
		printf("ERROR: unable to open log file\n");
		delete applog;
		applog = 0;
	}
	// Initialization
	IsReady			= false;
	amuledlg 		= NULL;
	clientlist			= NULL;
	searchlist		= NULL;
	knownfiles		= NULL;
	serverlist		= NULL;
	serverconnect	= NULL;
	sharedfiles		= NULL;
	listensocket		= NULL;
	clientudp			= NULL;
	clientcredits		= NULL;
	downloadqueue	= NULL;
	uploadqueue 	= NULL;
	ipfilter			= NULL;


// reset statistic values
	stat_sessionReceivedBytes = 0;
	stat_sessionSentBytes = 0;
	stat_reconnects = 0;
	stat_transferStarttime = 0;
	stat_serverConnectTime = 0;
	sTransferDelay = 0.0;


	Start_time = GetTickCount64();

#ifndef __WXMSW__
	// catch fatal exceptions
	wxHandleFatalExceptions(true);
#endif

	// Apprently needed for *BSD
	SetResourceLimits();


	// Parse cmdline arguments.
	wxCmdLineParser cmdline(wxAppConsole::argc, wxAppConsole::argv);

	// Handle these arguments.
	cmdline.AddSwitch(wxT("v"), wxT("version"), wxT("Displays the current version number."));
	cmdline.AddSwitch(wxT("h"), wxT("help"), wxT("Displays this information."));
	cmdline.AddSwitch(wxT("i"), wxT("enable-stdin"), wxT("Does not disable stdin."));
	cmdline.AddSwitch(wxT("o"), wxT("log-stdout"), wxT("Print log messages to stdout."));
	cmdline.Parse();

	if ( cmdline.Found(wxT("version")) ) {
		printf("aMule %s\n", VERSION);

		return false;
	}

	if ( cmdline.Found(wxT("help")) ) {
		cmdline.Usage();

		return false;
	}

	
	if ( cmdline.Found(wxT("log-stdout")) ) { 
                printf("Logging to stdout enabled\n"); 
		enable_stdout_log = true;
        } 


	printf("Initialising aMule\n");

	SetVendorName(wxT("TikuWarez"));

	// Do NOT change this string to aMule nor anything else, it WILL fuck you up.
	SetAppName(wxT("eMule"));


	// see if there is another instance running
	wxString server = ConfigDir + wxFileName::GetPathSeparator() + wxT("muleconn");
	wxString host = wxT("localhost");
	wxString IPC = wxT("aMule IPC TESTRUN");
	wxClient* client = new wxClient();
	//wxConnectionBase* conn = client->MakeConnection(host, server, IPC);
	wxConnectionBase* conn = 0;

	// If the connection failed, conn is NULL
	if ( conn ) {
		// An instance is already running!
		
		// This is very tricky. The most secure way to communicate is via ED2K links file
		FILE *ed2kfile;
		char filename[1024];

		/* Link seemed ok, add it to file. */
		sprintf(filename,"%s/.aMule/ED2KLinks",getenv("HOME"));
		ed2kfile = fopen(filename,"a");
		if (ed2kfile != NULL) {
			fprintf(ed2kfile,"RAISE_DIALOG");
			printf("Raised current running aMule\n");
			fclose(ed2kfile);
		} else {
			printf("Error opening file %s.Cannot raise active aMule\n", filename);
		}
		
		conn->Disconnect();
		delete conn;
		delete client;

		printf("aMule already running: exiting\n");
		return false;
	}
	delete client;

	// If there was no server, start one
	localserver = new wxServer();
	localserver->Create(server);


	// Close standard-input
	if ( !cmdline.Found(wxT("enable-stdin")) ) {
		close(0);
	}

	/* If no aMule configuration files exist, see if either lmule or xmule config
	   exists, so that we can use those. */
	wxString lMulePrefDir = wxGetHomeDir() + wxFileName::GetPathSeparator() + wxT(".lmule");
	wxString xMulePrefDir = wxGetHomeDir() + wxFileName::GetPathSeparator() + wxT(".xmule");

	if ( !wxDirExists( ConfigDir ) ) {
		if ( wxDirExists( lMulePrefDir ) ) {
			printf("Found lMule old settings, moving to new dir.\n");
			wxRenameFile(lMulePrefDir,ConfigDir);

		} else if ( wxDirExists(xMulePrefDir) ) {
			printf("Found xMule old settings, copying config & credits files.\n");
			wxMkdir( ConfigDir, CPreferences::GetDirPermissions() );

			// Copy .dat files to the aMule dir
			wxString file = wxFindFirstFile(xMulePrefDir + wxFileName::GetPathSeparator() + wxT("*.dat"), wxFILE);
  			while ( !file.IsEmpty() ) {
				wxCopyFile( file, ConfigDir + wxFileName::GetPathSeparator() + file.AfterLast(wxFileName::GetPathSeparator()));

				file = wxFindNextFile();
  			}

			// Copy .met files to the aMule dir
			file = wxFindFirstFile(xMulePrefDir + wxFileName::GetPathSeparator() + wxT("*.met"), wxFILE);
  			while ( !file.IsEmpty() ) {
				wxCopyFile( file, ConfigDir + wxFileName::GetPathSeparator() + file.AfterLast(wxFileName::GetPathSeparator()));

				file = wxFindNextFile();
  			}

			//wxMessageBox(_("Copied old ~/.xMule config and credit files to ~/.aMule\nHowever, be sure NOT to remove .xMule if your Incoming / Temp folders are still there ;)"), _("Info"), wxOK);
		} else {
			// No settings to import, new to build.
			wxMkdir( ConfigDir, CPreferences::GetDirPermissions() );
		}
	}


	// Load Preferences
	CPreferences::BuildItemList( theApp.ConfigDir);
	CPreferences::LoadAllItems( wxConfig::Get() );
	glob_prefs = new CPreferences();

	// Build the filenames for the two OS files
	SetOSFiles(glob_prefs->GetOSDir());


	// Display notification on new version or first run
	wxTextFile vfile( ConfigDir + wxFileName::GetPathSeparator() + wxT("lastversion") );
	wxString newMule(wxT(VERSION));
	if ( wxFileExists( vfile.GetName() ) && vfile.Open() && !vfile.Eof() ) {
		if ( vfile.GetFirstLine() != newMule ) {
			Trigger_New_version( newMule );

			// Remove prior version
			while ( vfile.GetLineCount() ) {
				vfile.RemoveLine(0);
			}

			vfile.AddLine(newMule);
			vfile.Write();
		}

		vfile.Close();
	} else {
		Trigger_New_version( newMule );

		// If we failed to open the file, create it
		if ( !vfile.IsOpened() )
			vfile.Create();

		vfile.AddLine(wxT(VERSION));
		vfile.Write();
		vfile.Close();
	}


	use_chmod = true;
#ifdef __WXGTK__
	/* Test to see if the Temp or the Incoming dir is on a vfat partition. If
	   that is the case, we need to avoid chmoding to avoid lots of warnings.
	   This is done by reading through fstab entries and comparing to the
	   folders used for incomming and temp files. */
#ifndef __BSD__
	FILE* mnt_tab = setmntent("/etc/mtab","r");
	if ( mnt_tab ) {
		wxString incomingdir = glob_prefs->GetIncomingDir();
		wxString tempdir = glob_prefs->GetTempDir();
		struct mntent* entries;

		entries = getmntent(mnt_tab);
		while ( entries ) {
			if ( strncmp(entries->mnt_type, "vfat",4) == 0 ) {
				if ( tempdir.StartsWith( char2unicode(entries->mnt_dir )) ) {
					// Kry - We cannot addlogline because there's no GUI yet!
					AddLogLineM(false,_("Temp dir is placed on a FAT32 partition. Disabling chmod to avoid useless warnings."));
					use_chmod = false;
				}
				if ( incomingdir.StartsWith( char2unicode(entries->mnt_dir )) ) {
					AddLogLineM(false,_("Incoming dir is placed on a FAT32 partition. Disabling chmod to avoid useless warnings."));
					use_chmod = false;
				}
				if (!use_chmod) {
					break;
				}
			}

			entries = getmntent(mnt_tab);
		}

		fclose(mnt_tab);
	}
#else
	wxString incomingdir = glob_prefs->GetIncomingDir();
	wxString tempdir = glob_prefs->GetTempDir();
	long size, i;
	struct statfs *mntbuf;

	size = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < size; i++) {
		if ( !strcmp(mntbuf[i].f_fstypename,"msdos")) {
			if ( tempdir.StartsWith( char2unicode( mntbuf[i].f_mntonname )) ) {
				// Kry - We cannot addlogline because there's no GUI yet!
      			AddLogLineM(false,_("Temp dir is placed on a FAT32 partition. Disabling chmod to avoid useless warnings."));
                    use_chmod = false;
			}
			if ( incomingdir.StartsWith( char2unicode( mntbuf[i].f_mntonname ) ) ) {
				AddLogLineM(false,_("Incoming dir is placed on a FAT32 partition. Disabling chmod to avoid useless warnings."));
				use_chmod = false;
			}
			if (!use_chmod) {
				break;
			}
		}
	}

#endif // __BSD__
#endif

	// Load localization settings
	Localize_mule();

	// Create Hashing thread
	CAddFileThread::Start();


	clientlist		= new CClientList();
	searchlist		= new CSearchList();
	knownfiles		= new CKnownFileList();
	serverlist		= new CServerList(glob_prefs);
	serverconnect	= new CServerConnect(serverlist, glob_prefs);
	sharedfiles		= new CSharedFileList(glob_prefs, serverconnect, knownfiles);

	clientcredits	= new CClientCreditsList(glob_prefs);
	downloadqueue	= new CDownloadQueue(glob_prefs, sharedfiles);	// bugfix - do this before creating the uploadqueue
	uploadqueue		= new CUploadQueue(glob_prefs);
	ipfilter		= new CIPFilter();
	
	wxIPV4address myaddr;
	myaddr.AnyAddress();
	myaddr.Service(glob_prefs->GetUDPPort());
	printf("*** TCP socket at %d\n", glob_prefs->GetPort());
	clientudp		= new CClientUDPSocket(myaddr);


	// Create listen socket 
	myaddr.Service(glob_prefs->GetPort());
	listensocket = new CListenSocket(glob_prefs, myaddr);

	// Create main dialog
	// Should default/last-used position be overridden?

	// Get ready to handle connections from apps like amulecmd
	ECServerHandler = new ExternalConn();

	serverlist->Init();

	// init downloadqueue
	downloadqueue->Init();




	m_app_state = APP_STATE_RUNNING;

	// reload shared files
	sharedfiles->Reload(true, true);

	listensocket->StartListening();

	// If we wern't able to start listening, we need to warn the user

	if ( !listensocket->Ok() ) {
		AddLogLineM(true, 
			wxString::Format(_("Port %d is not available. You will be LOWID"),glob_prefs->GetPort()));
	}


	// Autoconnect if that option is enabled
	if (glob_prefs->DoAutoConnect()) {
		AddLogLineM(true, _("Connecting"));
		theApp.serverconnect->ConnectToAnyServer();
	}

	// Ensure that the up/down ratio is used
	CPreferences::CheckUlDlRatio();

	// The user may now click on buttons
	IsReady = true;

	// Kry - Load the sources seeds on app init
	if (glob_prefs->GetSrcSeedsOn()) {
		downloadqueue->LoadSourceSeeds();
	}

	// Everything should be loaded now, so sort the list widgets
	//amuledlg->InitSort();
	
	// Start the Core Timer

	// Note: wxTimer can be off by more than 10% !!!
	// In addition to the systematic error introduced by wxTimer, we are losing
	// timer cycles due to high CPU load.  I've observed about 0.5% random loss of cycles under
	// low load, and more than 6% lost cycles with heavy download traffic and/or other tasks
	// in the system, such as a video player or a VMware virtual machine.
	// The upload queue process loop has now been rewritten to compensate for timer errors.
	// When adding functionality, assume that the timer is only approximately correct;
	// for measurements, always use the system clock [::GetTickCount()].
	
	// lfroen: core_timer replaced by OnRun loop
	//core_timer->Start(100);	

	// Start the Gui Timer
	
	// Note: wxTimer can be off by more than 10% !!!
	// In addition to the systematic error introduced by wxTimer, we are losing
	// timer cycles due to high CPU load.  I've observed about 0.5% random loss of cycles under
	// low load, and more than 6% lost cycles with heavy download traffic and/or other tasks
	// in the system, such as a video player or a VMware virtual machine.
	// The upload queue process loop has now been rewritten to compensate for timer errors.
	// When adding functionality, assume that the timer is only approximately correct;
	// for measurements, always use the system clock [::GetTickCount()].
	//amuledlg->StartGuiTimer();
	
	return true;
}


// Updates the number of received bytes and marks when transfers first began
void CamuleApp::UpdateReceivedBytes(int32 bytesToAdd)
{
	SetTimeOnTransfer();
	stat_sessionReceivedBytes += bytesToAdd;
}


// Updates the number of received bytes and marks when transfers first began
void CamuleApp::UpdateSentBytes(int32 bytesToAdd)
{
	SetTimeOnTransfer();
	stat_sessionSentBytes += bytesToAdd;
}


// Saves the time where transfers were started and calucated the time before
void CamuleApp::SetTimeOnTransfer()
{
	if (stat_transferStarttime)
		return;

	stat_transferStarttime = GetTickCount64();
	sTransferDelay = (stat_transferStarttime - Start_time)/1000.0;
}


// Returns the uptime in millie-seconds
uint64 CamuleApp::GetUptimeMsecs()
{
	return GetTickCount64() - Start_time;
}


// Returns the uptime in seconds
uint32 CamuleApp::GetUptimeSecs()
{
	return GetUptimeMsecs() / 1000;
}


// Returns the amount of time where transfers have been going on
uint32 CamuleApp::GetTransferSecs()
{
	return ( GetTickCount64() - stat_transferStarttime ) / 1000;
}


// Returns the amount of time where we've been connected to a server
uint32 CamuleApp::GetServerSecs()
{
	return ( GetTickCount64() - stat_serverConnectTime) / 1000;
}


// Strips specific chars to ensure legal filenames
wxString CamuleApp::StripInvalidFilenameChars(const wxString& strText, bool bKeepSpaces)
{
	wxString result;

	for ( unsigned int i = 0; i < strText.Length(); i++ ) {
		switch ( strText.GetChar(i) ) {
			case wxT('\"'):
			case wxT('*'):
			case wxT('<'):
			case wxT('>'):
			case wxT('?'):
			case wxT('|'):
			case wxT('\\'):
			case wxT('/'):
			case wxT(':'):
				continue;
			default:
				// Many illegal for filenames in windows below the 32th char (which is space).
				if ( (wxUChar) strText[i] > 31 ) {
						result += strText[i];
				}
		}
	}

	// Should we replace spaces?
	if ( !bKeepSpaces ) {
		result.Replace(wxT(" "), wxT("_"), TRUE);
	}

	return result;
}


// Returns a ed2k file URL
wxString CamuleApp::CreateED2kLink(const CAbstractFile* f)
{
	wxString strURL;

	// Construct URL like this: ed2k://|file|<filename>|<size>|<hash>|/
	strURL << wxT("ed2k://|file|")
	       << StripInvalidFilenameChars(f->GetFileName(), true)
		   << wxT("|")
	       << f->GetFileSize()
		   << wxT("|")
		   << EncodeBase16( f->GetFileHash(), 16 )
		   << wxT("|/");

	return strURL;
}


// Returns a ed2k source URL
wxString CamuleApp::CreateED2kSourceLink(const CAbstractFile* f)
{
	if ( !serverconnect->IsConnected() || serverconnect->IsLowID() ) {
		//wxMessageBox(_("You need a HighID to create a valid sourcelink"));
		return wxEmptyString;
	}

	uint32 clientID = serverconnect->GetClientID();

	// Create the first part of the URL
	wxString strURL = CreateED2kLink( f );

	// And append the source information: "|sources,<ip>:<port>|/"
	strURL << wxT("|sources,")
	       << (uint8) clientID << wxT(".")
		   << (uint8) (clientID >> 8) << wxT(".")
		   << (uint8) (clientID >> 16) << wxT(".")
		   << (uint8) (clientID >> 24) << wxT(":")
		   << glob_prefs->GetPort() << wxT("|/");

	// Result is "ed2k://|file|<filename>|<size>|<hash>|/|sources,<ip>:<port>|/"
	return strURL;
}


// Returns a ed2k source URL using a hostname rather than IP. Currently, the
// hostname doesn't appear to be set, thus this function wont work as intended.
wxString CamuleApp::CreateED2kHostnameSourceLink(const CAbstractFile* f)
{
	wxString strURL;

	// Create the first part of the URL
	strURL = CreateED2kLink( f );

	// Append the source information: "|sources,<host>:port|/"
	strURL << wxT("|sources,")
	       << glob_prefs->GetYourHostname() << wxT(":")
		   << glob_prefs->GetPort() << wxT("|/");

	// Result is "ed2k://|file|<filename>|<size>|<hash>|/|sources,<host>:<port>|/"
	return strURL;
}


// Creates a ED2k hyperlink
wxString CamuleApp::CreateHTMLED2kLink(const CAbstractFile* f)
{
	wxString strCode = wxT("<a href=\"") + CreateED2kLink(f) + wxT("\">") + StripInvalidFilenameChars(f->GetFileName(), true) + wxT("</a>");
	return strCode;
}


// Generates an URL for checking if a file is "fake"
wxString CamuleApp::GenFakeCheckUrl(const CAbstractFile *f)
{
	wxString strURL = wxT("http://donkeyfakes.gambri.net/index.php?action=search&ed2k=");

	strURL = wxURL::ConvertToValidURI( strURL +  CreateED2kLink( f ) );

	// The following cause problems, so we escape them
	strURL.Replace(wxT("\""), wxT("%22"));
	strURL.Replace(wxT("'"),  wxT("%27"));
	strURL.Replace(wxT("`"),  wxT("%60"));

	return strURL;
}

// jugle.net fake check
wxString CamuleApp::GenFakeCheckUrl2(const CAbstractFile *f)
{
	
    wxString strURL = wxT("http://www.jugle.net/?fakecheck=%s");
	
	strURL = wxURL::ConvertToValidURI( strURL +  CreateED2kLink( f ) );

	strURL.Replace(wxT("\""), wxT("%22"));
	strURL.Replace(wxT("'"),  wxT("%27"));
	strURL.Replace(wxT("`"),  wxT("%60"));

	return strURL;
}

/* Original implementation by Bouc7 of the eMule Project.
   aMule Signature idea was designed by BigBob and implemented
   by Un-Thesis, with design inputs and suggestions from bothie.
*/
void CamuleApp::OnlineSig(bool zero /* reset stats (used on shutdown) */)
{
	// Do not do anything if online signature is disabled in Preferences
	if (!glob_prefs->IsOnlineSignatureEnabled() || emulesig_path.IsEmpty()) {
		// We do not need to check amulesig_path because if emulesig_path is empty,
		// that means amulesig_path is empty too.
		return;
	}

	// Open both files for writing
	CFile amulesig_out, emulesig_out;
	if (!emulesig_out.Open(emulesig_path, CFile::write)) {
		AddLogLineM(true, wxString(_("Failed to save"))+wxString(_(" OnlineSig File")));
	}
	if (!amulesig_out.Open(amulesig_path, CFile::write)) {
		AddLogLineM(true, wxString(_("Failed to save"))+wxString(_(" aMule OnlineSig File")));
	}

	char buffer[256];

	if (zero) {
		sprintf(buffer, "0\xA");
		strcat(buffer, "0.0|0.0|0");
		emulesig_out.Write(buffer, strlen(buffer));
		amulesig_out.Write("0\n0\n0\n0\n0\n0.0\n0.0\n0\n0\n", 22);
	} else {
		if (serverconnect->IsConnected()) {
			// We are online
			emulesig_out.Write("1",1);
			emulesig_out.Write("|",1);
			// Name of server (Do not use GetRealName()!)
			emulesig_out.Write(unicode2char(serverconnect->GetCurrentServer()->GetListName()),strlen(unicode2char(serverconnect->GetCurrentServer()->GetListName())));
			emulesig_out.Write("|",1);
			// IP and port of the server
			emulesig_out.Write(unicode2char(serverconnect->GetCurrentServer()->GetFullIP()),strlen(unicode2char(serverconnect->GetCurrentServer()->GetFullIP())));
			emulesig_out.Write("|",1);
			sprintf(buffer,"%d",serverconnect->GetCurrentServer()->GetPort());
			emulesig_out.Write(buffer,strlen(buffer));

			// Now for amule sig
			amulesig_out.Write("1",1);
			amulesig_out.Write("\n",1);
			amulesig_out.Write(unicode2char(serverconnect->GetCurrentServer()->GetListName()),strlen(unicode2char(serverconnect->GetCurrentServer()->GetListName())));
			amulesig_out.Write("\n",1);
			amulesig_out.Write(unicode2char(serverconnect->GetCurrentServer()->GetFullIP()),strlen(unicode2char(serverconnect->GetCurrentServer()->GetFullIP())));
			amulesig_out.Write("\n",1);
			amulesig_out.Write(buffer,strlen(buffer));
			amulesig_out.Write("\n",1);

			// Low- or High-ID (only in amule sig)
			if (serverconnect->IsLowID()) {
				amulesig_out.Write("L\n",2);
			} else {
				amulesig_out.Write("H\n",2);
			}
		} else if (serverconnect->IsConnecting()) {
			emulesig_out.Write("0",1);    // shouldn't be modified, to mantain eMule compatibility

                	amulesig_out.Write("2",1);
                	amulesig_out.Write("\n",1);
                	amulesig_out.Write("0",1);
                	amulesig_out.Write("\n",1);
                	amulesig_out.Write("0",1);
                	amulesig_out.Write("\n",1);
                	amulesig_out.Write("0",1);
                	amulesig_out.Write("\n",1);
                	amulesig_out.Write("0\n",2);
		} else {	// Not connected to a server
			emulesig_out.Write("0",1);
			amulesig_out.Write("0\n0\n0\n0\n0\n",10);
		}
		emulesig_out.Write("\xA",1);

		// Datarate for downloads
		sprintf(buffer,"%.1f",downloadqueue->GetKBps());
		emulesig_out.Write(buffer,strlen(buffer));
		emulesig_out.Write("|",1);
		amulesig_out.Write(buffer,strlen(buffer));
		amulesig_out.Write("\n",1);

		// Datarate for uploads
		sprintf(buffer,"%.1f",uploadqueue->GetKBps());
		emulesig_out.Write(buffer,strlen(buffer));
		emulesig_out.Write("|",1);
		amulesig_out.Write(buffer,strlen(buffer));
		amulesig_out.Write("\n",1);

		// Number of users waiting for upload
		sprintf(buffer,"%d",uploadqueue->GetWaitingUserCount());
		emulesig_out.Write(buffer,strlen(buffer));
		amulesig_out.Write(buffer,strlen(buffer));
		amulesig_out.Write("\n",1);

		// Number of shared files
		sprintf(buffer,"%d", sharedfiles->GetCount());
		amulesig_out.Write(buffer, strlen(buffer));
		amulesig_out.Write("\n",1);
	}	/* if (!zero) */

	// Nick on the network
	sprintf(buffer, "%s", unicode2char(glob_prefs->GetUserNick()));
	amulesig_out.Write(buffer, strlen(buffer));
	amulesig_out.Write("\n",1);

	// Total received in bytes
	sprintf(buffer, "%llu", (stat_sessionReceivedBytes+glob_prefs->GetTotalDownloaded()));
	amulesig_out.Write(buffer, strlen(buffer));
	amulesig_out.Write("\n",1);

	// Total sent in bytes
	sprintf(buffer, "%llu", (stat_sessionSentBytes+glob_prefs->GetTotalUploaded()));
	amulesig_out.Write(buffer, strlen(buffer));
	amulesig_out.Write("\n",1);

	// amule version
	sprintf(buffer,"%s",VERSION);
	amulesig_out.Write(buffer, strlen(buffer));
	amulesig_out.Write("\n",1);

        // Total received bytes in session
	if (zero) {
		amulesig_out.Write("0",1);
		amulesig_out.Write("\n",1);
	} else {
		sprintf(buffer, "%llu", stat_sessionReceivedBytes);
        	amulesig_out.Write(buffer, strlen(buffer));
        	amulesig_out.Write("\n",1);
	}

        // Total sent bytes in session
	if (zero) {
		amulesig_out.Write("0",1);
		amulesig_out.Write("\n",1);
	} else {
		sprintf(buffer, "%llu", stat_sessionSentBytes);
        	amulesig_out.Write(buffer, strlen(buffer));
        	amulesig_out.Write("\n",1);
	}

	// Uptime
	if (zero) {
		sprintf(buffer,"%s",unicode2char(CastSecondsToHM(0).GetData()));
		amulesig_out.Write(buffer, strlen(buffer));
		amulesig_out.Write("\n",1);
	} else {
		sprintf(buffer,"%s",unicode2char(CastSecondsToHM(GetUptimeSecs())));
		amulesig_out.Write(buffer, strlen(buffer));
		amulesig_out.Write("\n",1);
	}

	// Close the files
	emulesig_out.Close();
	amulesig_out.Close();

} //End Added By Bouc7

// Gracefully handle fatal exceptions and print backtrace if possible
void CamuleApp::OnFatalException()
{
#ifndef __WXMSW__
	// Close sockets first.
	if ( listensocket )
		listensocket->Destroy();
	if ( clientudp )
		clientudp->Destroy();

	// (stkn) create backtrace
#ifdef __WXGTK__
#ifndef __BSD__
	void *bt_array[100];	// 100 should be enough ?!?
	char **bt_strings;
	int num_entries;

	if ((num_entries = backtrace(bt_array, 100)) < 0) {
		fprintf(stderr, "* Could not generate backtrace\n");
		return;
	}

	if ((bt_strings = backtrace_symbols(bt_array, num_entries)) == NULL) {
		fprintf(stderr, "* Could not get symbol names for backtrace\n");
		return;
	}

	fprintf(stderr, "\nOOPS! - Seems like aMule crashed\n--== BACKTRACE FOLLOWS: ==--\n\n");
	for (int i = 0; i < num_entries; i++) {
		fprintf(stderr, "[%d] %s\n", i, bt_strings[i]);
	}
	free(bt_strings);
#endif

#ifdef __BSD__
	fprintf(stderr, "\nOOPS! - Seems like aMule crashed\n--== no BACKTRACE yet \n\n");
#endif // __BSD__
	
#endif
#endif
}

#define wxGTK_WINDOW 1
#define SHIFT (8 * (sizeof(short int) - sizeof(char)))

// static bool GetColourWidget(int &, int &, int &, int )
// {
// 	  return FALSE;
// }
//
//
// // external helper function
// wxColour GetColour(wxSystemColour what)
// {
// 	static wxColour *_systemWindowColour = NULL;
// 
// 	switch (what) {
// 		case wxSYS_COLOUR_WINDOW:
// 			if (!_systemWindowColour) {
// 				int red, green, blue;
// 				if (!GetColourWidget(red, green, blue, wxGTK_WINDOW)) {
// 					red = green = blue = 0x9c40;
// 				}
// 				_systemWindowColour = new wxColour(red >> SHIFT, green >> SHIFT, blue >> SHIFT);
// 			}
// 			return *_systemWindowColour;
// 			break;
// 		default:
// 			break;
// 	}
// 	return true;
// }


// Sets the localization of aMule
void CamuleApp::Localize_mule()
{
	int language;

	#define  wxLANGUAGE_CUSTOM 				wxLANGUAGE_USER_DEFINED+1
	#define  wxLANGUAGE_ITALIAN_NAPOLITAN 	wxLANGUAGE_USER_DEFINED+2

	wxLanguageInfo CustomLanguage;
	CustomLanguage.Language = wxLANGUAGE_ITALIAN_NAPOLITAN;
	CustomLanguage.CanonicalName = wxT("it_NA");
	CustomLanguage.Description = wxT("sNeo's Custom Napolitan Language");
	wxLocale::AddLanguage(CustomLanguage);

	CustomLanguage.Language = wxLANGUAGE_CUSTOM;
	CustomLanguage.CanonicalName = wxT("aMule_custom");
	CustomLanguage.Description = wxT("aMule's custom language");

	switch (glob_prefs->GetLanguageID()) {
		case 0:
			language = wxLANGUAGE_DEFAULT;
			break;
		case 1:
			//strcpy(newlang,"ar");
			language = wxLANGUAGE_ARABIC;
			break;
		case 2:
			//strcpy(newlang,"eu");
			language = wxLANGUAGE_BASQUE;
			break;
		case 3:
			//strcpy(newlang,"bg_BG");
			language = wxLANGUAGE_BULGARIAN;
			break;
		case 4:
			//strcpy(newlang,"zh_CN");
			language = wxLANGUAGE_CHINESE_SIMPLIFIED;
			break;
		case 5:
			//strcpy(newlang,"da_DK");
			language = wxLANGUAGE_DANISH;
			break;
		case 6:
			//strcpy(newlang,"nl_NL");
			language = wxLANGUAGE_DUTCH;
			break;
		case 7:
			//strcpy(newlang,"en_GB");
			language = wxLANGUAGE_ENGLISH;
			break;
		case 8:
			//strcpy(newlang,"et_EE");
			language = wxLANGUAGE_ESTONIAN;
			break;
		case 9:
			//strcpy(newlang,"fi");
			language = wxLANGUAGE_FINNISH;
			break;
		case 10:
			//strcpy(newlang,"fr_FR");
			language = wxLANGUAGE_FRENCH;
			break;
		case 11:
			//strcpy(newlang,"gl_ES");
			language = wxLANGUAGE_GALICIAN;
			break;
		case 12:
			//strcpy(newlang,"de_DE");
			language = wxLANGUAGE_GERMAN;
			break;
		case 13:
			//strcpy(newlang,"it_IT");
			language = wxLANGUAGE_ITALIAN;
			break;
		case 14:
			//strcpy(newlang,"ko_KR");
			language = wxLANGUAGE_KOREAN;
			break;
		case 15:
			//strcpy(newlang,"lt_LT");
			language = wxLANGUAGE_LITHUANIAN;
			break;
		case 16:
			//strcpy(newlang,"pl_PL");
			language = wxLANGUAGE_POLISH;
			break;
		case 17:
			//strcpy(newlang,"pt_PT");
			language = wxLANGUAGE_PORTUGUESE;
			break;
		case 18:
			//strcpy(newlang,"pt_BR");
			language = wxLANGUAGE_PORTUGUESE_BRAZILIAN;
			break;
		case 19:
			//strcpy(newlang,"ru_RU");
			language = wxLANGUAGE_RUSSIAN;
			break;
		case 20:
			//strcpy(newlang,"es_ES");
			language = wxLANGUAGE_SPANISH;
			break;
		case 21:
			//strcpy(newlang,"es_CH");
			language = wxLANGUAGE_SPANISH_CHILE;
			break;
		case 22:
			//strcpy(newlang,"es_MX");
			language = wxLANGUAGE_SPANISH_MEXICAN;
			break;
		case 23:
			//Turkish makes weird things with .eMule file!!! why?
			//language = wxLANGUAGE_TURKISH;
			language = wxLANGUAGE_DEFAULT;
			break;
		case 24:
			//strcpy(newlang,"hu");
			language = wxLANGUAGE_HUNGARIAN;
			break;
		case 25:
			//strcpy(newlang,"ca_ES");
			language = wxLANGUAGE_CATALAN;
			break;
		case 26:
			//strcpy(newlang,"hr");
			language = wxLANGUAGE_CROATIAN;
			break;
		case 27:
			//strcpy(newlang,"it_CH");
			language = wxLANGUAGE_ITALIAN_SWISS;
			break;
		case 28:
			//strcpy(newlang,"custom");
			language = wxLANGUAGE_CUSTOM;
			break;
		case 29:
			//strcpy(newlang,"zh_TW");
		    language = wxLANGUAGE_CHINESE_TRADITIONAL;
		    break;
		default:
			language = wxLANGUAGE_DEFAULT;
			break;

	}

	int language_flags = 0;
	
	if ((language != wxLANGUAGE_CUSTOM) && (language != wxLANGUAGE_ITALIAN_NAPOLITAN)) {
		language_flags = wxLOCALE_LOAD_DEFAULT | wxLOCALE_CONV_ENCODING;
	}
	
	if ((!m_locale.Init(language,language_flags)) && (language != wxLANGUAGE_DEFAULT) && (language != wxLANGUAGE_CUSTOM)) {
		AddLogLineM(false,_("The selected locale seems not to be installed on your box. (Note: I'll try to set it anyway)"));
	}
	
	if (language != wxLANGUAGE_CUSTOM) {
		m_locale.AddCatalogLookupPathPrefix(wxT(LOCALEDIR));
		m_locale.AddCatalog(wxT(PACKAGE));
	} else {
		m_locale.AddCatalogLookupPathPrefix(wxString::Format(wxT("%s/.aMule"), getenv("HOME")));
		m_locale.AddCatalog(wxT("custom"));
	}

}


// Displays information related to important changes in aMule.
// Is called when the user runs a new version of aMule
void CamuleApp::Trigger_New_version(wxString )
{
	// lfroen: daemon will only log message
}

void CamuleApp::QueueLogLine(bool addtostatusbar, const wxString& line)
{
	m_LogQueueLock.Enter();

	QueuedLogLine new_line_to_log;
	
	new_line_to_log.line = line;
	new_line_to_log.addtostatus = addtostatusbar;

	QueuedAddLogLines.push_back(new_line_to_log);

	m_LogQueueLock.Leave();
}


void CamuleApp::FlushQueuedLogLines() {

	QueuedLogLine line_to_add;

	m_LogQueueLock.Enter();

	//wxASSERT(amuledlg);

	while (!QueuedAddLogLines.empty()) {
		line_to_add = QueuedAddLogLines.front();
		QueuedAddLogLines.pop_front();
		AddLogLineM(line_to_add.addtostatus, line_to_add.line);
	}

	m_LogQueueLock.Leave();
}

void CamuleApp::SetOSFiles(const wxString new_path) {
	if (::wxDirExists(new_path)) {
		emulesig_path = new_path + wxFileName::GetPathSeparator() + wxT("onlinesig.dat");
		amulesig_path = new_path + wxFileName::GetPathSeparator() + wxT("amulesig.dat");
	} else {
		//::wxMessageBox(_("The folder for Online Signature files you specified is INVALID!\n OnlineSignature will be DISABLED until you fix it on preferences."), _("Error"), wxOK | wxICON_ERROR);
		emulesig_path = wxEmptyString;
		amulesig_path = wxEmptyString;
	}
}


void CamuleApp::OnDnsDone(wxEvent& e)
{
	wxMuleInternalEvent& evt = *((wxMuleInternalEvent*)&e);
	CUDPSocket* socket=(CUDPSocket*)evt.GetClientData();
	struct sockaddr_in *si=(struct sockaddr_in*)evt.GetExtraLong();
	socket->DnsLookupDone(si);
}

void CamuleApp::OnNotifyEvent(wxEvent& e)
{
	GUIEvent& evt = *((GUIEvent*)&e);
	NotifyEvent(evt);
}

void CamuleApp::OnSourcesDnsDone(wxEvent& e)
{
	wxMuleInternalEvent& evt = *((wxMuleInternalEvent*)&e);
	struct sockaddr_in *si=(struct sockaddr_in*)evt.GetExtraLong();
	downloadqueue->OnHostnameResolved(si);
}

void CamuleApp::OnUDPTimer(AMULE_TIMER_EVENT_CLASS& WXUNUSED(evt))
{
	if (IsReady) {
		serverlist->SendNextPacket();
	}
}


void CamuleApp::OnTCPTimer(AMULE_TIMER_EVENT_CLASS& WXUNUSED(evt))
{
	if(!IsReady) {
		return;
	}

	serverconnect->StopConnectionTry();
	if (serverconnect->IsConnected() ) {
		return;
	}

	serverconnect->ConnectToAnyServer();
}


void CamuleApp::OnCoreTimer(AMULE_TIMER_EVENT_CLASS& WXUNUSED(evt))
{
	// Former TimerProc section

	static uint32	msPrev1, msPrev5, msPrevSave;

	uint32 			msCur = GetUptimeMsecs();

	// can this actually happen under wxwin ?
	if (!IsRunning() || !IsReady) {
		return;
	}

	uploadqueue->Process();
	downloadqueue->Process();
	//theApp.clientcredits->Process();
	uploadqueue->CompUpDatarateOverhead();
	downloadqueue->CompDownDatarateOverhead();

	if (msCur-msPrev1 > 950) {  // approximately every second
		msPrev1 = msCur;
		clientcredits->Process();
		serverlist->Process();
		clientlist->Process();
		if( serverconnect->IsConnecting() && !serverconnect->IsSingleConnect() ) {
			serverconnect->TryAnotherConnectionrequest();
		}
		if (serverconnect->IsConnecting()) {
			serverconnect->CheckForTimeout();
		}
	}

	if (msCur-msPrev5 > 5000) {  // every 5 seconds
		msPrev5 = msCur;
		listensocket->Process();
		OnlineSig(); // Added By Bouc7
		// Kry - Log lines flush
		FlushQueuedLogLines();
	}

	if (msCur-msPrevSave >= 60000) {
		msPrevSave = msCur;
		wxString buffer;
		
		wxConfigBase* cfg = wxConfig::Get();
		buffer.Printf(wxT("%llu"),stat_sessionReceivedBytes+glob_prefs->GetTotalDownloaded());
		cfg->Write(wxT("/Statistics/TotalDownloadedBytes"), buffer);

		buffer.Printf(wxT("%llu"),stat_sessionSentBytes+glob_prefs->GetTotalUploaded());
		cfg->Write(wxT("/Statistics/TotalUploadedBytes"), buffer);
	}

	// Recomended by lugdunummaster himself - from emule 0.30c
	serverconnect->KeepConnectionAlive();

}

void CamuleApp::OnHashingShutdown(wxEvent& WXUNUSED(evt))
{
	if ( m_app_state != APP_STATE_SHUTINGDOWN ) {
		printf("Hashing thread ended\n");
		// Save the known.met file
		knownfiles->Save();

		// Known.met changed, AICH sync thread start
		RunAICHThread();
		
	} else {
		printf("Hashing thread terminated, ready to shutdown\n");
	}
}


void CamuleApp::OnFinishedHashing(wxEvent& e)
{
	wxMuleInternalEvent& evt = *((wxMuleInternalEvent*)&e);
	static int filecount = 0;
	static int bytecount = 0;

	CKnownFile* result = (CKnownFile*)evt.GetClientData();
	printf("Finished Hashing %s\n",unicode2char(result->GetFileName()));
	if (evt.GetExtraLong()) {
		CPartFile* requester = (CPartFile*)evt.GetExtraLong();
		if (downloadqueue->IsPartFile(requester)) {
			requester->PartFileHashFinished(result);
		}
	} else {
		if (knownfiles->SafeAddKFile(result)) {
			sharedfiles->SafeAddKFile(result);

			filecount++;
			bytecount += result->GetFileSize();
			// If we have added 30 files or files with a total size of ~300mb
			if ( ( filecount == 30 ) || ( bytecount >= 314572800 ) ) {
				if ( m_app_state != APP_STATE_SHUTINGDOWN ) {
					knownfiles->Save();
					filecount = 0;
					bytecount = 0;
				}
			}
		} else {
			printf("Not added\n");
			delete result;
		}
	}

	return;
}

void CamuleApp::OnFinishedCompletion(wxEvent& e)
{
	wxMuleInternalEvent& evt = *((wxMuleInternalEvent*)&e);
	CPartFile* completed = (CPartFile*)evt.GetClientData();

	wxASSERT(completed);

	completed->CompleteFileEnded(evt.GetInt(), (wxString*)evt.GetExtraLong());

	return;
}


void CamuleApp::ShutDown() {
	// Signal the hashing thread to terminate
	m_app_state = APP_STATE_SHUTINGDOWN;
	IsReady =  false;
	//amuledlg->Destroy();
	if (CAddFileThread::IsRunning()) {
		CAddFileThread::Stop();
	}
	if (CAICHSyncThread::IsRunning()) {
		CAICHSyncThread::Stop();
	}
}


#if defined(__DEBUG__) 
	#if defined(__LINUX__)
	
		void CamuleApp::AddSocketDeleteDebug(uint32 socket_pointer, uint32 creation_time) {
	
			socket_deletion_log_item current_socket;
			socket_deletion_log_item temp_socket;
	
			current_socket.socket_n = socket_pointer;
			current_socket.creation_time = creation_time;
			current_socket.backtrace = wxEmptyString;
	
			void *bt_array[6];	// 6 should be enough ?!?
			char **bt_strings;
			int num_entries;
	
			if ((num_entries = backtrace(bt_array, 6)) < 0) {
				current_socket.backtrace += wxT("* Could not generate backtrace\n");
			} else {
				if ((bt_strings = backtrace_symbols(bt_array, num_entries)) == NULL) {
					current_socket.backtrace += wxT("* Could not get symbol names for backtrace\n");
				}  else {
					int n;
					if (num_entries < 5) {
						n = num_entries;
					} else {
						n = 5;
					}
					for (int i = n - 1; i >= 0; i--) {
						current_socket.backtrace += wxString::Format(wxT("[%d] %s | "), i, bt_strings[i]);
					}
					current_socket.backtrace += wxT("END");
				}
			}
	
			uint32 size = SocketDeletionList.size();
			for ( uint32 i = 0; i < size; ++i ) {
				if (( SocketDeletionList[i].socket_n == socket_pointer) && ( SocketDeletionList[i].creation_time == creation_time)) {
	
					printf("\n-----------------------RSB FOUND!!!!!!!!!!!!!!!!!!!!!!!!!------------\n");
					printf("First deletion  (ptr: %u time: %u) BT:\n",temp_socket.socket_n, temp_socket.creation_time);
					printf("-> %s\n\n",unicode2char(temp_socket.backtrace));
	
					printf("Second deletion (ptr: %u time: %u) BT:\n",current_socket.socket_n,current_socket.creation_time);
					printf("-> %s\n\n",unicode2char(current_socket.backtrace));
	
					printf("--------------------------- Get Ready for RC4---------------------------\n");
	
					//wxASSERT(0);
				}
			}
	
			SocketDeletionList.push_back(current_socket);
	
		}

	#else
		void CamuleApp::AddSocketDeleteDebug(uint32 socket_pointer, uint32 creation_time) {
		// No backtrace on this platform.
		}
	#endif // __LINUX__
		
#endif // __DEBUG__
	
//
// lfroen: logging is not unicode-aware, and it should not be !
// Phoenix: You must be joking. :)
void AddLogLine(const wxString &msg)
{
	wxString curr_date = wxDateTime::Now().FormatDate() + wxT(" ") + 
		wxDateTime::Now().FormatTime() + wxT(": ");
	const wxCharBuffer date_str_buf = unicode2charbuf(curr_date);
	const char *date_str = (const char *)date_str_buf;
	applog->Write(date_str, strlen(date_str));
	if ( enable_stdout_log ) {
		fputs(date_str, stdout);
	}

 	const wxCharBuffer c_msg_buf = unicode2charbuf(msg);
	const char *c_msg = (const char *)c_msg_buf;
	applog->Write(c_msg, strlen(c_msg));
	applog->Write("\n", 1);
        if ( enable_stdout_log ) { 
		puts(c_msg);
	}

	applog->Flush();
}

void CamuleApp::NotifyEvent(GUIEvent event)
{
	switch (event.ID) {
		// GUI->CORE events
		// it's daemon, so gui isn't here, but macros can be used as function calls
		case SHOW_CONN_STATE:
			if ( event.byte_value ) {
				const wxString id = theApp.serverconnect->IsLowID() ? wxT("LOW") : wxT("HIGH");
				AddLogLine(wxT("Connected to ") + event.string_value + wxT(" with ") + id + wxT(" ID"));
			} else {
				if ( theApp.serverconnect->IsConnecting() ) {
					AddLogLine(wxT("connecting to ") + event.string_value);
				} else {
					AddLogLine(wxT("disconnected\n"));
				}
			}
			break;
		// search
	        case SEARCH_REQ:
			uploadqueue->AddUpDataOverheadServer(((Packet *)event.ptr_value)->GetPacketSize());
			serverconnect->SendPacket( (Packet *)event.ptr_value, 0 );
			if ( event.byte_value ) {
				searchlist->m_searchpacket = (Packet *)event.ptr_value;
			} else {
				searchlist->m_searchpacket = 0;
			}
			break;
	        case SEARCH_ADD_TO_DLOAD:
			downloadqueue->AddSearchToDownload((CSearchFile *)event.ptr_value, event.byte_value);
			break;

		case SHAREDFILES_SHOW_ITEM:
			//printf("SHAREDFILES_SHOW_ITEM: %p\n", event.ptr_value);
			break;
			
		case DOWNLOAD_CTRL_ADD_SOURCE:
		/*
		printf("ADD_SOURCE: adding source %p to partfile %s\n",
		       event.ptr_aux_value, ((CPartFile*)event.ptr_value)->GetFullName().c_str());
		*/
			break;

		case ADDLOGLINE:
			AddLogLine(event.string_value);
			break;
		case ADDDEBUGLOGLINE:
			//printf("DEBUGLOG: %s\n", event.string_value.c_str());
			break;
		default:
			//printf("WARNING: event %d in daemon should not happen\n", event.ID);
			break;
	}
}


bool CamuleApp::AddServer(CServer *srv)
{
	return serverlist->AddServer(srv) ? true : false;
}

CFriend *CamuleApp::FindFriend(CMD4Hash *WXUNUSED(hash), uint32 WXUNUSED(ip), uint16 WXUNUSED(port))
{
// 	if ( amuledlg && amuledlg->chatwnd ) {
// 		return 	amuledlg->chatwnd->FindFriend(*hash, ip, port);
// 	}
	return NULL;
}

uint32 CamuleApp::GetPublicIP() const {
	/*
	if (m_dwPublicIP == 0 && Kademlia::CKademlia::isConnected() && !Kademlia::CKademlia::isFirewalled() )
		return ntohl(Kademlia::CKademlia::getIPAddress());
	*/
	return m_dwPublicIP;
}

void CamuleApp::SetPublicIP(const uint32 dwIP){
	if (dwIP != 0){
		wxASSERT ( !IsLowIDED2K(dwIP));
		//wxASSERT ( m_pPeerCache );
//		if ( GetPublicIP() == 0)
			//AddDebugLogLineM(false, wxString::Format(wxT("My public IP Address is: %s"),ipstr(dwIP)));
//		else if (Kademlia::CKademlia::isConnected() && !Kademlia::CKademlia::isFirewalled() && ntohl(Kademlia::CKademlia::getIPAddress()) != dwIP)
//			AddDebugLogLine(DLP_DEFAULT, false,  _T("Public IP Address reported from Kademlia (%s) differs from new found (%s)"),ipstr(ntohl(Kademlia::CKademlia::getIPAddress())),ipstr(dwIP));
//		m_pPeerCache->FoundMyPublicIPAddress(dwIP);	
	}
//	else
//		AddDebugLogLine(DLP_VERYLOW, false, _T("Deleted public IP"));
	
	m_dwPublicIP = dwIP;

}

wxString CamuleApp::GetLog(bool WXUNUSED(reset))
{
	wxString log = wxT("FIXME");
	return log;
}

wxString CamuleApp::GetServerLog(bool reset)
{
	wxString ret = server_msg;
	if ( reset ) {
		server_msg = wxT("");
	}
	return ret;
}

wxString CamuleApp::GetDebugLog(bool reset)
{
	return GetLog(reset);
}

void CamuleApp::AddServerMessageLine(wxString &msg)
{
	server_msg += msg + wxT("\n");
	AddLogLine(wxT("ServerMessage: ") + msg);
}

void CamuleApp::RunAICHThread()
{
	if ( !CAICHSyncThread::IsRunning() )
		CAICHSyncThread::Start();	
}

void CamuleApp::OnFinishedHTTPDownload(wxEvent& evt)
{
	wxMuleInternalEvent& event = *((wxMuleInternalEvent*)&evt);
	switch (event.GetInt()) {
		case HTTP_IPFilter:
			ipfilter->DownloadFinished(event.GetExtraLong());
			break;
		case HTTP_ServerMet:
			// not implemented
			break;
	}
}

DEFINE_EVENT_TYPE(wxEVT_NOTIFY_EVENT)

DEFINE_EVENT_TYPE(wxEVT_CORE_FILE_HASHING_FINISHED)
DEFINE_EVENT_TYPE(wxEVT_CORE_FILE_HASHING_SHUTDOWN)
DEFINE_EVENT_TYPE(wxEVT_CORE_FINISHED_FILE_COMPLETION)
DEFINE_EVENT_TYPE(wxEVT_CORE_FINISHED_HTTP_DOWNLOAD)
DEFINE_EVENT_TYPE(wxEVT_CORE_SOURCE_DNS_DONE)
DEFINE_EVENT_TYPE(wxEVT_CORE_DNS_DONE)

DEFINE_EVENT_TYPE(wxEVT_AMULE_TIMER)
