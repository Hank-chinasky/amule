//
// This file is part of the aMule Project
//
// Copyright (c) 2004 aMule Project ( http://www.amule-project.net )
// Original author: Emilio Sandoz
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


#ifndef __PrefsUnifiedDlg_H__
#define __PrefsUnifiedDlg_H__


#include <wx/dialog.h>		// Needed for wxDialog


class Cfg_Base;
class CPreferences;
class CDirectoryTreeCtrl;

class wxWindow;
class wxChoice;
class wxButton;
class wxPanel;

class wxCommandEvent;
class wxListEvent;
class wxSpinEvent;
class wxScrollEvent;
class wxInitDialogEvent;


/**
 * This class represents a dialog used to display preferences.
 */
class PrefsUnifiedDlg : public wxDialog
{
public:
	/**
	 * This functions creates a new dialog if no other exists.
	 * 
	 * @param The parent of the new dialog.
	 *
	 * This function is needed since we dont want to have more than
	 * one preference-dialog at a time. Thus if a preference-dialog
	 * already exists, this function returns NULL;
	 */
	static PrefsUnifiedDlg* NewPrefsDialog(wxWindow* parent);

	/**
	 * Destructor.
	 */
	~PrefsUnifiedDlg();


	/**
	 * Updates the widgets with the values of the preference-variables.
	 */
	bool TransferFromWindow();
	/**
	 * Updates the prefernce-variables with the values of the widgets.
	 */
	bool TransferToWindow();


protected:
	/**
	 * Constructor.
	 *
	 * @param parent The parent window.
	 *
	 * This constructor is a much more simple version of the wxDialog one,
	 * which only needs to know the parent of the dialog. Please note that 
	 * it is private so that we can ensure that only one dialog has been 
	 * created at one time.
	 */
	PrefsUnifiedDlg(wxWindow* parent);

	
	//! Contains the ID of the current window or zero if no preferences window has been created.
	static int	s_ID;

	
	/**
	 * Helper functions which checks if a Cfg has has changed.
	 */
	bool			CfgChanged(int id);

	/**
	 * Helper functions which returns the Cfg assosiated with the specified id.
	 */
	Cfg_Base*		GetCfg(int id);


	//! Pointer to the shared-files list
	CDirectoryTreeCtrl* 	m_ShareSelector;

	//! Pointer to the color-selector
	wxChoice*		m_choiceColor;

	//! Pointer to the color-selection button
	wxButton*		m_buttonColor;

	//! Pointer to the currently shown preference-page
	wxPanel*		m_CurrentPanel;


	void OnOk(wxCommandEvent &event);
	void OnCancel(wxCommandEvent &event);

	void OnButtonBrowseWav(wxCommandEvent &event);
	void OnButtonBrowseSkin(wxCommandEvent &event);
	void OnButtonBrowseVideoplayer(wxCommandEvent &event);
	void OnButtonDir(wxCommandEvent& event);
	void OnButtonSystray(wxCommandEvent& event);
	void OnButtonEditAddr(wxCommandEvent& event);
	void OnButtonColorChange(wxCommandEvent &event);
	void OnButtonIPFilterReload(wxCommandEvent &event);
	void OnButtonIPFilterUpdate(wxCommandEvent &event);
	void OnColorCategorySelected(wxCommandEvent &event);
	void OnCheckBoxChange(wxCommandEvent &event);
	void OnFakeBrowserChange(wxCommandEvent &event);
	void OnPrefsPageChange(wxListEvent& event);
	void OnToolTipDelayChange(wxSpinEvent& event);
	void OnScrollBarChange( wxScrollEvent& event );
	void OnRateLimitChanged( wxSpinEvent& event );

	void OnInitDialog( wxInitDialogEvent& evt );

	DECLARE_EVENT_TABLE()
};

#endif
