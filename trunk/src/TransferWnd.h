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

#ifndef TRANSFERWND_H
#define TRANSFERWND_H


#include <wx/panel.h>	// Needed for wxPanel
#include "types.h"		// Needed for uint32


class Category_Struct;
class CUploadListCtrl;
class CDownloadListCtrl;
class CQueueListCtrl;
class CMuleNotebook;
class wxListCtrl;
class wxSplitterEvent;
class wxNotebookEvent;
class wxCommandEvent;
class wxMouseEvent;
class wxEvent;
class wxMenu;



/**
 * This class takes care of managing the lists and other controls contained 
 * in the transfer-window. It's primary function is to manage the user-defined
 * categories.
 */
class CTransferWnd : public wxPanel
{
public:
	/**
	 * Constructor.
	 */
	CTransferWnd(wxWindow* pParent = NULL);
	
	/**
	 * Destructor.
	 */
	~CTransferWnd();
	

	/**
	 * Adds the specified category to the end of the list.
	 *
	 * @param category A pointer to the new category.
	 *
	 * This function should be called after a category has been 
	 * added to the lists of categories. The new category is assumed
	 * to be the last, and thus will be appended to the end of the tabs
	 * on the category-notebook.
	 */
	void AddCategory( Category_Struct* category );
	
	/**
	 * Updates the title of the specified category.
	 *
	 * @param index The index of the category on the notebook.
	 * @param titleChanged Set to true if the actual title has changed.
	 *
	 * The second paramerter will make the UpdateCategory function signal
	 * to the searchdlg page that the lists of categories has been changed,
	 * and force it to refresh its list of categories. 
	 *
	 * This however should only be done when the title has actually changed, 
	 * since it will cause the user-selection to be reset.
	 */
	void UpdateCategory( int index, bool titleChanged = false );

	
	/**
	 * This functions updates the displayed client-numbers*
	 *
	 * @param number The number of clients on the upload queue.
	 *
	 * This function updates both the number of clients on the queue,
	 * as well as the number of banned clients.
	 */
	void	ShowQueueCount(uint32 number);
	
	/**
	 * This function switches between showing the upload-queue and the list of
	 * client who are currently recieving files.
	 */
	void	SwitchUploadList(wxCommandEvent& evt);
	
	/**
	 * Helper-function which updates the displayed titles of all existing categories.
	 */
	void	UpdateCatTabTitles();


	//! Pointer to the list of recieving files.
	CUploadListCtrl*	uploadlistctrl;
	//! Pointer to the download-queue.
	CDownloadListCtrl*	downloadlistctrl;
	//! Pointer to the list of clients waiting to recieve files.
	CQueueListCtrl*		queuelistctrl;
	
	//! Used to signify which of the two upload lists are showed.
	bool				windowtransferstate;

private:
	/**
	 * Event-handler for the set status by category menu-item.
	 */
	void OnSetCatStatus( wxCommandEvent& event );

	/**
	 * Event-handler for the set priority by category menu-item.
	 */
	void OnSetCatPriority( wxCommandEvent& event );

	/**
	 * Event-handler for the "Add Category" menu-item.
	 */
	void OnAddCategory( wxCommandEvent& event );

	/**
	 * Event-handler for the "Delete Category" menu-item.
	 */
	void OnDelCategory( wxCommandEvent& event );

	/**
	 * Event-handler for the "Edit Category" menu-item.
	 */
	void OnEditCategory( wxCommandEvent& event );

	/**
	 * Event-handler for manipulating the default category.
	 */
	void OnSetDefaultCat( wxCommandEvent& event );

	/**
	 * Event-handler for the "Clear Completed" button.
	 */
	void OnBtnClearDownloads(wxCommandEvent &evt);

	/**
	 * Event-handler for changing categories.
	 */
	void OnCategoryChanged(wxNotebookEvent& evt);
	
	/**
	 * Event-handler for displaying the category-popup menu.
	 */
	void OnNMRclickDLtab(wxMouseEvent& evt);

	/**
	 * Event-handler for saving the position of the splitter-widget.
	 */    
	void OnSashPositionChanged(wxSplitterEvent& evt);

	//! Variable used to ensure that the category menu doesn't get displayed twice.
	wxMenu* m_menu;

	//! Pointer to the category tabs.
	CMuleNotebook* m_dlTab;


	DECLARE_EVENT_TABLE()
};

#endif

