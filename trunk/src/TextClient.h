/*  This file is part of aMule project
 *  
 *  aMule Copyright (C) 2003-2004 aMule Team (http://www.amule.org)
 *  This fle Copyright (C) 2003 Kry (elkry@users.sourceforge.net)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef TEXTCLIENT_H
#define TEXTCLIENT_H

#ifndef WIN32
	#include "config.h"
#endif

//-------------------------------------------------------------------
enum {
	CMD_ID_HELP	= 1,
	CMD_ID_STATS,
	CMD_ID_SHOW	,
	CMD_ID_RESUME,
	CMD_ID_PAUSE,
	CMD_ID_SRVSTAT,
	CMD_ID_CONN,
	CMD_ID_DISCONN,
	CMD_ID_CONN_TO_SRV,
	CMD_ID_RELOAD_IPFILTER,
	CMD_ID_SET_IPFILTER,
	CMD_ID_GET_IPLEVEL,
	CMD_ID_SET_IPLEVEL,
	CMD_ID_IPLEVEL,
	CMD_ID_DLOAD_QUEUE,
	CMD_ID_SHUTDOWN,
	CMD_ID_SERVERLIST,
};
//-------------------------------------------------------------------
//
// wxUSE_GUI will only be defined after this include
// 
#include "ExternalConnector.h"
//-------------------------------------------------------------------

wxString ECv2_Response2String(CECPacket *response);

#if wxUSE_GUI
#include <wx/textctrl.h>	// For wxTextCtrl
#include <wx/timer.h>		// For wxTimer
class CamulecmdFrame : public wxFrame
{
public:
	CamulecmdFrame(const wxString& title, const wxPoint& pos,
		const wxSize& size, long style = wxDEFAULT_FRAME_STYLE);

	// event handlers (these functions should _not_ be virtual)
	void OnQuit(wxCommandEvent &event);
	void OnAbout(wxCommandEvent &event);
	void OnComandEnter(wxCommandEvent &event);
	void OnIdle(wxIdleEvent &e);
	void OnTimerEvent(wxTimerEvent &WXUNUSED(event));
	wxTextCtrl *log_text;
	wxTextCtrl *cmd_control;
	wxTimer *m_timer;
	
private:
	wxLog *logTargetOld;
	// any class wishing to process wxWindows events must use this macro
	DECLARE_EVENT_TABLE()
};
#endif

class CamulecmdApp : public CaMuleExternalConnector
{
public:
	void ShowHelp();
	void ShowGreet();
	int ProcessCommand(int ID);
	void Process_Answer_v2(CECPacket *reply);

#if wxUSE_GUI
public:
	void LocalShow(const wxString &s);
	CamulecmdFrame *frame;
private:
	// GUI Version
	virtual bool OnInit();
#else
private:
	// Command line version
	virtual int OnRun();
	// other command line switches
	void OnInitCmdLine(wxCmdLineParser& amuleweb_parser);
	bool OnCmdLineParsed(wxCmdLineParser& parser);
	void TextShell(const wxString& prompt, CmdId *commands);
	bool m_HasCmdOnCmdLine;
	wxString m_CmdString;
#endif
};

#endif // TEXTCLIENT_H

