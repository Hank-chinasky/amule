//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2005 aMule Team ( http://www.amule.org )
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

#ifndef SEARCHLISTCTRL_H
#define SEARCHLISTCTRL_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "SearchListCtrl.h"
#endif

#include <list>				// Needed for std::list

#include "MuleListCtrl.h"	// Needed for CMuleListCtrl

class CSearchList;
class CSearchFile;


/**
 * This class is used to display search results.
 * 
 * Results on added to the list will be colored according to 
 * the number of sources and other parameters (see UpdateColor).
 *
 * To display results, first use the ShowResults function, which will display
 * all current results with the specified id and afterwards you can use the 
 * AddResult function to add new results or the UpdateResult function to update
 * already present results. Please note that it is not possible to add results
 * with the AddResult function before calling ShowResults.
 */
class CSearchListCtrl : public CMuleListCtrl
{
public:
	/**
	 * Constructor.
	 * 
	 * @see CMuleListCtrl::CMuleListCtrl for documentation of parameters.
	 */
	 CSearchListCtrl(
	            wxWindow *parent,
                wxWindowID winid = -1,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = wxLC_ICON,
                const wxValidator& validator = wxDefaultValidator,
                const wxString &name = wxT("mulelistctrl") );
			
	/**
	 * Destructor.
	 */
	virtual ~CSearchListCtrl();


	/**
	 * Adds ths specified file to the list.
	 *
	 * @param The new result to be shown.
	 *
	 * Please note that no duplicates checking is done, so the pointer should 
	 * point to a new file in order to avoid problems. Also note that the result
	 * will be inserted sorted according to current sort-type, so there is no
	 * need to resort the list after adding new items.
	 */
	void	AddResult(CSearchFile* toshow);

	/**
	 * Updates the specified source.
	 *
	 * @param The search result to be updated.
	 */
	void	UpdateResult(CSearchFile* toupdate);

	/**
	 * Clears the list and inserts all results with the specified Id instead.
	 *
	 * @param nResult The ID of the results or Zero to simply reset the list.
	 */
	void	ShowResults( long ResultsId );


	/**
	 * Updates the colors of item at the specified index. 
	 *
	 * @param index The zero-based index of the item. 
	 *
	 * This function sets the color of the item based on the following:
	 *  - Downloading files are marked in red.
	 *  - Known (shared/completed) files are marked in green.
	 *  - New files are marked in blue depending on the number of sources.
	 */
	void	UpdateColor( long index );


	/**
	 * Returns the current Search Id. 
	 *
	 * @return The Search Id of the displayed results (set through ShowResults()).
	 */
	long	GetSearchId();
	
protected:

	/**
	 * Sorter function used by wxListCtrl::SortItems function.
	 *
	 * @see CMuleListCtrl::SetSortFunc
	 * @see wxListCtrl::SortItems
	 */
	static int wxCALLBACK SortProc( long item1, long item2, long sortData );

	/**
	 * Override default AltSortAllowed method . See CMuleListCtrl.cpp.
	 */
	virtual bool AltSortAllowed( int column );


	/**
	 * Helper function which syncs two lists.
	 *
	 * @param src The source list.
	 * @param dst The list to be synced with the source list.
	 *
	 * This function syncronises the following settings of two lists:
	 *  - Sort column
	 *  - Sort direction
	 *  - Column widths
	 *
	 * If either sort column or direction is changed, then the dst list will
	 * be resorted. This function is used to ensure that all results list act
	 * as one, while still allowing individual selection.
	 */
	static void SyncLists( CSearchListCtrl* src, CSearchListCtrl* dst );

	/**
	 * Helper function which syncs all other lists against the specified one.
	 *
	 * @param src The list which all other lists should be synced against.
	 *
	 * This function just calls SyncLists() on all lists in s_lists, using
	 * the src argument as the src argument of the SyncLists function.
	 */
	static void SyncOtherLists( CSearchListCtrl* src );

	
	//! This list contains pointers to all current instances of CSearchListCtrl.
	static std::list<CSearchListCtrl*> s_lists;
	
	//! The ID of the search-results which the list is displaying or zero if unset. 
	long	m_nResultsID;


	/**
	 * Event handler for right mouse clicks.
	 */
	void OnNMRclick( wxMouseEvent& evt );

	/**
	 * Event handler for double-clicks or enter.
	 */
	void OnItemActivated( wxListEvent& evt );
	
	/**
	 * Event handler for left-clicks on the column headers.
	 * 
	 * This eventhandler takes care of sync'ing all the other lists with this one.
	 */
	void OnColumnLClick( wxListEvent& evt );

	/**
	 * Event handler for resizing of the columns.
	 *
	 * This eventhandler takes care of sync'ing all the other lists with this one.
	 */
	void OnColumnResize( wxListEvent& evt );

	
	/**
	 * Event handler for get-url menu items.
	 */
	void OnPopupGetUrl( wxCommandEvent& evt );

	/**
	 * Event handler for fake-check menu items.
	 */
	void OnPopupFakeCheck( wxCommandEvent& evt );

	/**
	 * Event handler for Razorback 2 stats menu items.
	 */
	void OnRazorStatsCheck( wxCommandEvent& evt );

	/**
	 * Event handler for download-file(s) menu item.
	 */
	void OnPopupDownload( wxCommandEvent& evt );

	DECLARE_EVENT_TABLE()
};

#endif // SEARCHLISTCTRL_H
