/*  This file is part of aMule project
 *  
 *  aMule Copyright (C)2003-2004 aMule Team ( http://www.amule-project.net )
 *  This file Copyright (C)2003 Kry (elkry@sourceforge.net  http://www.amule-project.net )
 *  This file Copyright (C)2004 shakraw <shakraw@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "WebInterface.h"
#endif

#ifdef HAVE_CONFIG_H
	#include "config.h"	// For VERSION
#endif

#if !defined( __WXMSW__ )
	#include <unistd.h>
#endif

#include <wx/filename.h>	// Needed for wxFileName

#if wxUSE_GUI
	#include <wx/statline.h>
#endif

#include <stdio.h>

#include "ECFileConfig.h"	// Needed for CECFileConfig
#include "MD5Sum.h"
#include "OtherFunctions.h"
#include "WebInterface.h"
#include "WebServer.h"

//-------------------------------------------------------------------

#define CMD_ID_HELP	1
//#define CMD_ID_STOP	2
//#define CMD_ID_START	3
//#define CMD_ID_RESTART	4

#define APP_INIT_SIZE_X 640
#define APP_INIT_SIZE_Y 480

#define theApp (*((CamulewebApp*)wxTheApp))

static CmdId commands[] = {
	{ wxT("quit"),		CMD_ID_QUIT },
	{ wxT("exit"),		CMD_ID_QUIT },
	{ wxT("help"),		CMD_ID_HELP },
//	{ wxT("stop"),		CMD_ID_STOP },
//	{ wxT("start"),		CMD_ID_START },
//	{ wxT("restart"),	CMD_ID_RESTART },
	{ wxEmptyString,	0 },
};

static CWebServer *webserver = NULL;

#if wxUSE_GUI && wxUSE_TIMER
	class MyTimer *mytimer;
#endif

//-------------------------------------------------------------------
IMPLEMENT_APP(CamulewebApp);
//-------------------------------------------------------------------

//-------------------------------------------------------------------
#if wxUSE_GUI
//-------------------------------------------------------------------
// IDs for the controls and the menu commands
enum {
    // menu items
    amuleweb_Quit = 1,

    // it is important for the id corresponding to the "About" command to have
    // this standard value as otherwise it won't be handled properly under Mac
    // (where it is special and put into the "Apple" menu)
    amuleweb_About = wxID_ABOUT,
    Event_Comand_ID = 32001,
    amuleFrame_ID = 32000,
    Timer_ID
};

BEGIN_EVENT_TABLE(CamulewebFrame, wxFrame)
	EVT_MENU(amuleweb_Quit,  CamulewebFrame::OnQuit)
	EVT_MENU(amuleweb_About, CamulewebFrame::OnAbout)
	EVT_TEXT_ENTER(Event_Comand_ID, CamulewebFrame::OnCommandEnter)
	EVT_IDLE(CamulewebFrame::OnIdle)
	EVT_TIMER(Timer_ID, CamulewebFrame::OnTimerEvent)
END_EVENT_TABLE()


CamulewebFrame::CamulewebFrame(const wxString& title, const wxPoint& pos, const wxSize& size, long style)
       : wxFrame(NULL, amuleFrame_ID, title, pos, size, style)
{
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(amuleweb_Quit, _("E&xit\tAlt-X"), _("Quit amuleweb"));

	wxMenu *helpMenu = new wxMenu;
	helpMenu->Append(amuleweb_About, _("&About...\tF1"), _("Show about dialog"));

	// now append the freshly created menu to the menu bar...
	wxMenuBar *menuBar = new wxMenuBar();
	menuBar->Append(menuFile, _("&File"));
	menuBar->Append(helpMenu, _("&Help"));

	// ... and attach this menu bar to the frame
	SetMenuBar(menuBar);

	// Text controls and sizer
	wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);
	log_text = new wxTextCtrl(this, -1, wxEmptyString,
		wxDefaultPosition, wxSize(APP_INIT_SIZE_X,APP_INIT_SIZE_Y),
		wxTE_MULTILINE|wxVSCROLL|wxTE_READONLY);
	log_text->SetBackgroundColour(wxT("wheat"));
	log_text->SetDefaultStyle(
		wxTextAttr(
			wxNullColour, 
			wxT("wheat"), 
			wxFont(10, wxMODERN, wxNORMAL, wxNORMAL)
		)
	);
	vsizer->Add( log_text, 1, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 0 );
	wxStaticLine *line = new wxStaticLine( this, -1,
		wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	vsizer->Add( line, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );
	cmd_control = new wxTextCtrl(this, Event_Comand_ID, wxEmptyString,
			wxDefaultPosition, wxSize(APP_INIT_SIZE_X,-1), wxTE_PROCESS_ENTER);
	cmd_control->SetBackgroundColour(wxT("wheat"));
	vsizer->Add(cmd_control, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 0);

	SetSizer(vsizer);
	vsizer->SetSizeHints(this);

	m_timer = new wxTimer(this, Timer_ID);
	m_timer->Start(5000);
}

void CamulewebFrame::OnQuit(wxCommandEvent& WXUNUSED(event))
{
	// true is to force the frame to close
	Show(_("\nOk, exiting Web Client...\n"));
	Close(true);
}

void CamulewebFrame::OnAbout(wxCommandEvent& WXUNUSED(event))
{
	wxString msg;
	msg = wxString::Format( 
		_("amuleweb [DLG version]\n"
		"Using %s\n(c) aMule Dev Team"),
		wxVERSION_STRING);
	wxMessageBox(msg, _("About amuleweb"), wxOK | wxICON_INFORMATION, this);
}

void CamulewebFrame::OnCommandEnter(wxCommandEvent& WXUNUSED(event)){
	if (cmd_control->GetLineLength(0) == 0) {
		return; 
	}
	wxString buffer = cmd_control->GetLineText(0);
	if (theApp.Parse_Command(buffer, commands)) {
		Close(true);
	}
	cmd_control->Clear();
}

void CamulewebFrame::OnIdle(wxIdleEvent &WXUNUSED(event))
{
	theApp.MainThreadIdleNow();
}

void CamulewebFrame::OnTimerEvent(wxTimerEvent &WXUNUSED(event))
{
	wxWakeUpIdle();
}

void CamulewebApp::LocalShow(const wxString &s)
{
	if (!frame) {
		return;
	}
	frame->log_text->AppendText(s);
}

int CamulewebApp::OnExit() {
	frame = NULL;
	if (webserver) {
		webserver->StopServer();
	}
	return 0;
}

bool CamulewebApp::OnInit() {
	CaMuleExternalConnector::OnInit();
	frame = new CamulewebFrame(_("amuleweb DLG"), wxPoint(50, 50), wxSize(APP_INIT_SIZE_X, APP_INIT_SIZE_Y));
	frame->Show(true);
	ConnectAndRun(wxT("aMuleweb"), wxT(VERSION), commands);

	return true;
}

#else

int CamulewebApp::OnRun() {
	ConnectAndRun(wxT("aMuleweb"), wxT(VERSION), commands);

	return true;
}

void CamulewebApp::Post_Shell() {
	webserver->StopServer();
}

#endif

bool CamulewebApp::CheckDirForTemplate(wxString& dir, const wxString& tmpl)
{
	DebugShow(wxT("checking for directory '") + dir + wxT("'..."));
	if (wxFileName::DirExists(dir)) {
		DebugShow(wxT(" yes\n"));
		dir += wxFileName::GetPathSeparator() + tmpl;
		DebugShow(wxT("checking for directory '") + dir + wxT("'..."));
		if (wxFileName::DirExists(dir)) {
			DebugShow(wxT(" yes\n"));
			wxString tmplPath(dir + wxFileName::GetPathSeparator() + wxT("aMule.tmpl"));
			DebugShow(wxT("checking for file '") + tmplPath + wxT("'..."));
			if (wxFileName::FileExists(tmplPath)) {
				DebugShow(wxT(" yes\n"));
				// dir is already set to the directory component of the template path
				return true;
			} else {
				DebugShow(wxT(" no\n"));
			}
		} else {
			DebugShow(wxT(" no\n"));
		}
	} else {
		DebugShow(wxT(" no\n"));
	}
	return false;
}

bool CamulewebApp::GetTemplateDir(const wxString& templateName, wxString& templateDir)
{
	wxString dir;

	DebugShow(wxT("looking for template: ") + templateName + wxT("\n"));
	dir = otherfunctions::GetConfigDir() + wxT("webserver");
	if (CheckDirForTemplate(dir, templateName)) {
		templateDir = dir;
		return true;
	}
	dir = wxT(WEBSERVERDIR);
	if (CheckDirForTemplate(dir, templateName)) {
		templateDir = dir;
		return true;
	}
	return false;
}

void CamulewebApp::OnInitCmdLine(wxCmdLineParser& amuleweb_parser)
{
	CaMuleExternalConnector::OnInitCmdLine(amuleweb_parser);
	amuleweb_parser.AddOption(wxT("t"), wxT("template"), 
		_("Loads template <str>"), 
		wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
		
	amuleweb_parser.AddOption(wxT("s"), wxT("server-port"), 
		_("Webserver HTTP port"),
		wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL);

	amuleweb_parser.AddSwitch(wxT("z"), wxT("enable-gzip"), 
		_("Use gzip compression"),
		wxCMD_LINE_PARAM_OPTIONAL);
	
	amuleweb_parser.AddSwitch(wxT("Z"), wxT("disable-gzip"), 
		wxT("Do not use gzip compression"),
		wxCMD_LINE_PARAM_OPTIONAL);
	
	amuleweb_parser.AddOption(wxT("A"), wxT("admin-pass"), 
		_("Full access password for webserver"), 
		wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);

	amuleweb_parser.AddOption(wxT("G"), wxT("guest-pass"), 
		_("Guest password for webserver"), 
		wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);

	amuleweb_parser.AddSwitch(wxT("a"), wxT("allow-guest"), 
		_("Allow guest access"),
		wxCMD_LINE_PARAM_OPTIONAL);

	amuleweb_parser.AddSwitch(wxT("d"), wxT("deny-guest"), 
		_("Deny guest access"),
		wxCMD_LINE_PARAM_OPTIONAL);

	amuleweb_parser.AddSwitch(wxT("L"), wxT("load-settings"), 
		_("Load/save webserver settings from/to remote aMule"),
		wxCMD_LINE_PARAM_OPTIONAL);

	amuleweb_parser.AddOption(wxEmptyString, wxT("amule-config-file"),
		_("aMule config file path. DO NOT USE DIRECTLY!"),
		wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
}

bool CamulewebApp::OnCmdLineParsed(wxCmdLineParser& parser)
{
	wxString aMuleConfigFile;
	if (parser.Found(wxT("amule-config-file"), &aMuleConfigFile)) {
		if (!::wxFileExists(aMuleConfigFile)) {
			fprintf(stderr, (const char *)unicode2char(wxT("FATAL ERROR: ") + aMuleConfigFile + wxT(" does not exist.\n")));
			exit(1);
		}
		CECFileConfig cfg(aMuleConfigFile);
		LoadAmuleConfig(cfg);
		// do not process any other command-line parameters, use defaults instead
		m_TemplateName = wxT("default");
		if (!GetTemplateDir(m_TemplateName, m_TemplateDir)) {
			// no reason to run webserver without a template
			fprintf(stderr, (const char *)unicode2char(wxT("FATAL ERROR: Cannot find template: ") + m_TemplateName + wxT("\n")));
			exit(1);
			// cmd-line versions exit after a return false; but DLG versions not - that's why the exit()
			//return false;
		}
		m_TemplateFileName = m_TemplateDir + wxFileName::GetPathSeparator() + wxT("aMule.tmpl");
		m_Verbose = false;
		m_KeepQuiet = true;
		m_LoadSettingsFromAmule = true;
		return true;
	}

	if (CaMuleExternalConnector::OnCmdLineParsed(parser)) {

		parser.Found(wxT("template"), &m_TemplateName);
		if (m_TemplateName.IsEmpty()) {
			m_TemplateName = wxT("default");
		}
		if (!GetTemplateDir(m_TemplateName, m_TemplateDir)) {
			// no reason to run webserver without a template
			fprintf(stderr, (const char *)unicode2char(_("FATAL ERROR: Cannot find template: ") + m_TemplateName + wxT("\n")));
			//std::cerr << _("You should have a look at http://www.amule.org/wiki/index.php/Webserver#Webserver_with_aMule_2.0.0_or_later_(starting_from_CVS_2005.02.27)\n");
			exit(1);
			//return false;
		}
		m_TemplateFileName = m_TemplateDir + wxFileName::GetPathSeparator() + wxT("aMule.tmpl");
		DebugShow(wxT("*** Using template: ") + m_TemplateFileName + wxT("\n"));

		long port;
		if (parser.Found(wxT("server-port"), &port)) {
			m_WebserverPort = port;
		}
		if (parser.Found(wxT("enable-gzip"))) {
			m_UseGzip = true;
		}
		if (parser.Found(wxT("disable-gzip"))) {
			m_UseGzip = false;
		}

		if (parser.Found(wxT("allow-guest"))) {
			m_AllowGuest = true;
		}
		if (parser.Found(wxT("deny-guest"))) {
			m_AllowGuest = false;
		}

		wxString tmp;
		if ( parser.Found(wxT("admin-pass"), &tmp) ) {
			if (tmp.IsEmpty()) {
				m_AdminPass.Clear();
			} else {
				m_AdminPass.Decode(MD5Sum(tmp).GetHash());
			}
		}
		if ( parser.Found(wxT("guest-pass"), &tmp) ) {
			if (tmp.IsEmpty()) {
				m_GuestPass.Clear();
			} else {
				m_GuestPass.Decode(MD5Sum(tmp).GetHash());
			}
		}

		m_LoadSettingsFromAmule = parser.Found(wxT("load-settings"));
		return true;
	} else {
		return false;
	}
}

int CamulewebApp::ProcessCommand(int ID) {
	switch (ID) {
		case CMD_ID_HELP:
			ShowHelp();
			break;
/*		case CMD_ID_STOP:
			//webserver->StopServer();
			break;
		case CMD_ID_START:
			//webserver->StartServer();
			break;
		case CMD_ID_RESTART:
			//webserver->RestartServer();
			break;	*/
		default:
			return -1;
			break;
	}

	return 0;
}

void CamulewebApp::ShowHelp() {
//                                  1         2         3         4         5         6         7         8
//                         12345678901234567890123456789012345678901234567890123456789012345678901234567890
	Show(         _("\n----------------> Help: Available commands (case insensitive): <----------------\n\n"));	
	Show(wxString(wxT("Help:\t\t\t")) + wxString(_("Shows this help.\n")));
	//Show(wxString(wxT("Start:\t\t\t)) + wxString(_("Start web server.\n")));
	//Show(wxString(wxT("Stop:\t\t\t))  + wxString(_("Stop web server.\n")));
	//Show(wxString(wxT("Restart:\t\t\t)) + wxString(_("Restart web server.\n")));
	Show(wxString(wxT("Quit, Exit:\t\t")) + wxString(_("Exits aMuleWeb.\n")));
	Show(         _("\n----------------------------> End of listing <----------------------------------\n"));
}

void CamulewebApp::ShowGreet() {
	Show(wxT("\n---------------------------------\n"));
	Show(wxString(wxT("|       ")) + wxString(_("aMule Web Server")) + wxString(wxT("        |\n")));
	Show(wxT("---------------------------------\n\n"));
	Show(_("\nUse 'Help' for command list\n\n"));
}

void CamulewebApp::Pre_Shell() {
	//Creating the web server
	webserver = new CWebServer(this, m_TemplateDir);
	webserver->StartServer();
}

void CamulewebApp::LoadAmuleConfig(CECFileConfig& cfg)
{
	CaMuleExternalConnector::LoadAmuleConfig(cfg);
	m_UseGzip = (cfg.Read(wxT("/Webserver/UseGzip"), 0l) == 1l);
	m_AllowGuest = (cfg.Read(wxT("/Webserver/UseLowRightsUser"), 0l) == 1l);
	cfg.ReadHash(wxT("/Webserver/Password"), &m_AdminPass);
	cfg.ReadHash(wxT("/Webserver/PasswordLow"), &m_GuestPass);
	m_WebserverPort = cfg.Read(wxT("/Webserver/Port"), -1l);
	m_PageRefresh = cfg.Read(wxT("/Webserver/PageRefreshTime"), 120l);
}

void CamulewebApp::LoadConfigFile()
{
	CaMuleExternalConnector::LoadConfigFile();
	if (m_configFile) {
		wxString tmp;
		m_WebserverPort = m_configFile->Read(wxT("/Webserver/Port"), -1l);
		m_TemplateName = m_configFile->Read(wxT("/Webserver/Template"), wxT("default"));
		m_configFile->Read(wxT("/Webserver/UseGzip"), &m_UseGzip, false);
		m_configFile->Read(wxT("/Webserver/AllowGuest"), &m_AllowGuest, false);
		m_configFile->ReadHash(wxT("/Webserver/AdminPassword"), &m_AdminPass);
		m_configFile->ReadHash(wxT("/Webserver/GuestPassword"), &m_GuestPass);
	}
}

void CamulewebApp::SaveConfigFile()
{
	CaMuleExternalConnector::SaveConfigFile();
	if (m_configFile) {
		m_configFile->Write(wxT("/Webserver/Port"), m_WebserverPort);
		m_configFile->Write(wxT("/Webserver/Template"), m_TemplateName);
		m_configFile->Write(wxT("/Webserver/UseGzip"), m_UseGzip);
		m_configFile->Write(wxT("/Webserver/AllowGuest"), m_AllowGuest);
		m_configFile->WriteHash(wxT("/Webserver/AdminPassword"), m_AdminPass);
		m_configFile->WriteHash(wxT("/Webserver/GuestPassword"), m_GuestPass);
	}
}
