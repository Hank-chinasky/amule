// This file is part of the aMule Project.
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
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

#ifndef MULELISTCTRL_H
#define MULELISTCTRL_H


// Defines base class of CMuleListCtrl so that we only need this one ifdef
#ifdef __WXMSW__
	#include <wx/listctrl.h>
	typedef wxListCtrl CMuleListCtrl_Base;
#else
	#include "listctrl_gen.h"
	typedef wxODListCtrl CMuleListCtrl_Base;
#endif


//! This value will be added to the user-data of the sorter-funtion when sorting ascending.
const int SORT_OFFSET_ASC = 0;
//! This value will be added to the user-data of the sorter-funtion when sorting decending.
const int SORT_OFFSET_DEC = 1000;
//! This value will be added to the user-data of the sorter-funtion when sorting alt-ascending.
const int SORT_OFFSET_ALT_ASC = 2000;
//! This value will be added to the user-data of the sorter-funtion when sorting alt-decending.
const int SORT_OFFSET_ALT_DEC = 3000;


/**
 * Enhanced wxListCtrl provided custom-drawing among other things.
 *
 * This class provides these features which the original wxListCtrl lacks:
 *  - Automatic sort arrows upon clicks on the column headers
 *  - Custom drawing of items.
 *  - Hiding of columns through auto-generated popup-menu.
 *  - Helper function for inserting items pre-sorted.
 *  - Loading and saving of column properties.
 */
class CMuleListCtrl : public CMuleListCtrl_Base
{
public:

	/**
	 * Constructor needed for dynamic classes.
	 */
	CMuleListCtrl();

	/**
	 * Constructor.
	 * 
	 * @see wxListCtrl::wxListCtrl for documentation of parameters.
	 */
	 CMuleListCtrl(
	            wxWindow *parent,
                wxWindowID winid = -1,
                const wxPoint &pos = wxDefaultPosition,
                const wxSize &size = wxDefaultSize,
                long style = wxLC_ICON,
                const wxValidator& validator = wxDefaultValidator,
                const wxString &name = wxT("mulelistctrl") );
	
	/**
	 * Destructor.
	 *
	 * If a name for the table has been specified with SetTableName, then 
	 * column settings will be saved automatically.
	 */ 
	virtual ~CMuleListCtrl();


	/**
	 * Saves column settings.
	 *
	 * Currently saves the width of all columns, hidden columns, the column
	 * to sort by and in which direction to sort.
	 */
	virtual void SaveSettings();

	/**
	 * Loads column settings.
	 *
	 * Currently loads the width of all columns, hidden columns, the column
	 * to sort by and in which direction to sort. This function also ensures
	 * that the items are sorted after the settings have been read.
	 */
	virtual void LoadSettings();


	/**
	 * This function tries to locate the best place to insert an item.
	 *
	 * @param The userdata of the new item.
	 * 
	 * This function does a binary type search to locate the best place to
	 * insert the new item with the specified userdata. It then returns the 
	 * item after this position. To do this, the sorter-function must be set
	 * though the SetSortFunc function, otherwise it will just return the 
	 * position after the last item.
	 */
	long GetInsertPos( long data );


	/**
	 * Sorts the list.
	 *
	 * Before you can use this function, you will need to specify a sorter
	 * function using SetSortFunc. wxListCtrl needs such a function to
	 * perform the sort.
	 */
	virtual void SortList();
	
	
protected:

	/**
	 * Sets the internally used table-name.
	 *
	 * @param name The new name or an empty string to disable.
	 *
	 * You need to call this function with a unique name before you can 
	 * make use of the LoadSettings/SaveSettings functions. CMuleListCtrl
	 * uses the name specified in this command to create unique keynames.
	 */
	void SetTableName( const wxString& name );

	/**
	 * Returns the table name.
	 *
	 * @return Tablename or an empty string if none was set.
	 */
	const wxString& GetTableName();

	/**
	 * Sets the sorter function.
	 *
	 * @param func
	 *
	 * See the documentation on wxListCtrl::SortItems for more information
	 * about the expected function type.
	 */
	void SetSortFunc(wxListCtrlCompare func);


	/**
	 * Returns the current sorter-function.
	 *
	 * @return The current sorter function or NULL if none has been set.
	 */
	wxListCtrlCompare GetSortFunc();

	/**
	 * Returns the current sort order.
	 *
	 * @return True if the sort is ascending, false if it is decending.
	 */
	bool GetSortAsc();
	
	/**
	 * Sets the sort order.
	 *
	 * @param value Set to true if the search is to be ascending, false if decending.
	 */
	void SetSortAsc( bool value );

	
	/**
	 * Function that controls which columns have alternate sorting.
	 *
	 * @param column The column to be examined.
	 * @return True if the specified column can be alt-sorted, false otherwise.
	 *
	 * By default this function will always return false, so sub-clases that wish
	 * to make use of alternate sorting should overload this function and return 
	 * true for columns where it is feasible to alternate-sort.
	 */
	virtual bool AltSortAllowed( int column );


	/**
	 * This function specifies if alternate sorting is being used.
	 *
	 * @return True if the current column is being sorted using an alternate method, false otherwise.
	 */
	bool GetSortAlt();
	
	/**
	 * Enables or disables alternate sorting.
	 *
	 * @param value Specifies if alternate sorting should be used or not.
	 *
	 * Please note that setting this value will have no effect if the current
	 * column doesn't support alt-sorting. Also note that it is reset when the
	 * user clicks on another column.
	 */
	void SetSortAlt( bool value );


	/**
	 * Gets the sort column.
	 *
	 * @return The column which is currently used to sort the list.
	 */
	int  GetSortColumn();

	/**
	 * Set the sort column
	 *
	 * @param column The column with which the list should be sorted.
	 */
	void SetSortColumn( int column );


	/**
	 * Event handler for right-clicks on the column headers.
	 */
	void OnColumnRClick(wxListEvent& evt);
	/**
	 * Event handler for left-clicks on the column headers.
	 */
	void OnColumnLClick(wxListEvent& evt);
	/**
	 * Event handler for the hide/show menu items.
	 */
	void OnMenuSelected(wxCommandEvent& evt);
	/**
	 * Event handler for the mouse wheel.
	 */
	void OnMouseWheel(wxMouseEvent &event);


private:

	//! The name of the table. Used to load/save settings.
	wxString			m_name;
	//! The sort order. Ascending is the default.
	bool				m_sort_asc;
	//! Specifies if the sort should be sorted in the alternative fashion.
	bool				m_sort_alt;
	//! The column to sort by.
	int					m_sort_column;
	//! The sorter function needed by wxListCtrl.
	wxListCtrlCompare	m_sort_func;
	
	DECLARE_EVENT_TABLE()
	DECLARE_DYNAMIC_CLASS(CMuleListCtrl)
};

#endif // MULELISTCTRL_H
