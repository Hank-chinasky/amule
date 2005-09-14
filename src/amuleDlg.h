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

#ifndef AMULEDLG_H
#define AMULEDLG_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "amuleDlg.h"
#endif

#ifdef __WXMSW__
	#include <wx/msw/winundef.h> // Needed to be able to include wx headers
#endif

#include <wx/defs.h>		// Needed before any other wx/*.h.
#include <wx/frame.h>		// Needed for wxFrame
#include <wx/timer.h>
#include <wx/imaglist.h>

#include "Types.h"			// Needed for uint32

class CTransferWnd;
class CServerWnd;
class CSharedFilesWnd;
class CSearchDlg;
class CChatWnd;
class CStatisticsDlg;
class CKadDlg;
class PrefsUnifiedDlg;	

class wxTimerEvent;
class wxTextCtrl;

#ifndef __SYSTRAY_DISABLED__
	#ifdef USE_WX_TRAY
		class CMuleTrayIcon;		
	#else
		class CSysTray;
	#endif
#endif

#define MP_RESTORE	4001
#define MP_CONNECT	4002
#define MP_DISCONNECT	4003
#define MP_EXIT		4004

#define DEFAULT_SIZE_X  800
#define DEFAULT_SIZE_Y  600
		
// CamuleDlg Dialogfeld
class CamuleDlg : public wxFrame 
{
public:
	CamuleDlg(wxWindow* pParent = NULL, const wxString &title = wxEmptyString,
		wxPoint where = wxDefaultPosition,
		wxSize dlg_size = wxSize(DEFAULT_SIZE_X,DEFAULT_SIZE_Y));
	~CamuleDlg();

	void AddLogLine(bool addtostatusbar, const wxString& line);
	void AddServerMessageLine(wxString& message);
	void ResetLog(uint32 whichone);
	
	void ShowUserCount(const wxString& info = wxEmptyString);
	void ShowConnectionState(uint32 connection_state);

	void ShowTransferRate();
	
	bool StatisticsWindowActive()	{return (activewnd == (wxWindow*)statisticswnd);}
	
	/* Returns the active dialog. Needed to check what to redraw. */
	enum DialogType { TransferWnd, NetworksWnd, SearchWnd, SharedWnd, ChatWnd, StatsWnd, KadWnd };
	DialogType GetActiveDialog()	{return m_nActiveDialog;}
	void SetActiveDialog(DialogType type, wxWindow* dlg);

	/**
	 * Helper function for deciding if a certian dlg is visible.
	 *
	 * @return True if the dialog is visible to the user, false otherwise.
	 */
	bool IsDialogVisible( DialogType dlg )
	{
		return ( m_nActiveDialog == dlg ) && ( is_safe_state ) /* && ( !IsIconized() ) */; 
	}

	void ShowED2KLinksHandler( bool show );

	void DlgShutDown();
	void OnClose(wxCloseEvent& evt);
	void OnBnConnect(wxCommandEvent& evt);

	void ShowNotifier(wxString Text, int MsgType, bool ForceSoundOFF = false); // Should be removed or implemented!
	void Hide_aMule(bool iconize = true);
	void Show_aMule(bool uniconize = true);
	
	bool SafeState() { return is_safe_state; }

	void LaunchUrl(const wxString &url);
	
	//! These are the currently known web-search providers
	enum WebSearch { wsFileHash, wsJugle };
	// websearch function
	wxString GenWebSearchUrl( const wxString &filename, WebSearch provider );


#ifndef __SYSTRAY_DISABLED__ 
	#ifndef USE_WX_TRAY
		// Has to be done in own method
		void changeDesktopMode();
	#endif
	void CreateSystray();
	void RemoveSystray();	
#endif

	CTransferWnd*		transferwnd;
	CServerWnd*		serverwnd;
	CSharedFilesWnd*	sharedfileswnd;
	CSearchDlg*		searchwnd;
	CChatWnd*		chatwnd;
	wxWindow*		activewnd;
	CStatisticsDlg*		statisticswnd;
	CKadDlg*		kademliawnd;

	int			srv_split_pos;
	
	wxImageList imagelist;
	
	void StartGuiTimer() { gui_timer->Start(100); }
	void StopGuiTimer() { gui_timer->Stop(); }
	
	PrefsUnifiedDlg* prefs_dialog;

	/**
	 * This function ensures that _all_ list widgets are properly sorted.
	 */
	void InitSort();
	
	void SetMessageBlink(bool state) { m_BlinkMessages = state; }
	
protected:
	
	void OnToolBarButton(wxCommandEvent& ev);
	void OnAboutButton(wxCommandEvent& ev);
	void OnPrefButton(wxCommandEvent& ev);
#ifndef CLIENT_GUI	
	void OnImportButton(wxCommandEvent& ev);
#endif

	void OnMinimize(wxIconizeEvent& evt);

	void OnBnClickedFast(wxCommandEvent& evt);
	void OnBnStatusText(wxCommandEvent& evt);

	void OnGUITimer(wxTimerEvent& evt);

	void OnMainGUISizeChange(wxSizeEvent& evt);

private:

	wxToolBar*	m_wndToolbar;
	bool		LoadGUIPrefs(bool override_pos, bool override_size); 
	bool		SaveGUIPrefs();

	wxTimer* gui_timer;

// Systray functions
#ifndef __SYSTRAY_DISABLED__
	void UpdateTrayIcon(int percent);
	#ifdef USE_WX_TRAY
		CMuleTrayIcon* m_wndTaskbarNotifier;
	#else
		CSysTray *m_wndTaskbarNotifier;
	#endif
#endif

	DialogType m_nActiveDialog;

	bool is_safe_state;

	bool m_BlinkMessages;
	
	int m_CurrentBlinkBitmap;

	//bool is_hidden;

	uint32 last_iconizing;

	void Apply_Clients_Skin(wxString file);
	
	void Create_Toolbar(wxString skinfile);
	
	void ToogleED2KLinksHandler();

	void SetMessagesTool();

	DECLARE_EVENT_TABLE()
};

#endif
