// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
// Copyright (C) 2002 Merkur ( merkur-@users.sourceforge.net / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

// FileDetailDialog.cpp : implementation file
//


#include "muuli_wdr.h"		// Needed for ID_CLOSEWNDFD
#include "FileDetailListCtrl.h"	// Interface declarations

#define LVCFMT_LEFT wxLIST_FORMAT_LEFT
#define wxLIST_STATE_DESELECTED 0x0000

BEGIN_EVENT_TABLE(CFileDetailListCtrl, CMuleListCtrl)
	EVT_LIST_COL_CLICK(IDC_LISTCTRLFILENAMES, CFileDetailListCtrl::OnColumnClick) // Change sorting
	EVT_LIST_ITEM_SELECTED(IDC_LISTCTRLFILENAMES, CFileDetailListCtrl::OnSelect) // Care for single selection
END_EVENT_TABLE()


CFileDetailListCtrl::CFileDetailListCtrl(wxWindow * &parent, int id, const wxPoint & pos, wxSize siz, int flags):CMuleListCtrl(parent, id, pos, siz, flags)
{
	// Initial sorting: Sources descending
	sortItem = 1;
	m_SortAscending[0] =true;
	m_SortAscending[1] =false; 
 
	InsertColumn(0, _("File Name"), LVCFMT_LEFT, 370);
	InsertColumn(1, _("Sources"), LVCFMT_LEFT, 70);
	SetSortArrow(1, 1);
	SortItems(CompareListNameItems, 11);
}

int CFileDetailListCtrl::CompareListNameItems(long lParam1, long lParam2, long lParamSort)
{ 
	// Comparison for different sortings
	SourcenameItem *item1 = (SourcenameItem *) lParam1; 
	SourcenameItem *item2 = (SourcenameItem *) lParam2; 
	switch (lParamSort){
		case 1: return (item1->count - item2->count); break;       // Sources descending
		case 11: return (item2->count - item1->count); break;      // Sources ascending
		case 0: return item1->name.CmpNoCase(item2->name); break;  // Name descending
		case 10: return item2->name.CmpNoCase(item1->name); break; // Name ascending
		default: return 0;
	}
} 


void CFileDetailListCtrl::UpdateSort()
{
	SortItems(CompareListNameItems,sortItem+((m_SortAscending[sortItem])? 0:10));
}


void CFileDetailListCtrl::OnColumnClick(wxListEvent & evt)
{
	int newSortItem = evt.GetColumn();
	if (newSortItem == sortItem) {
		// Sort by same column -> only change the order
		m_SortAscending[sortItem] = !m_SortAscending[sortItem];
	} else {
		// Sort by other column 
		sortItem = newSortItem;
	}
	SetSortArrow(sortItem, m_SortAscending[sortItem]);
	SortItems(CompareListNameItems,sortItem+((m_SortAscending[sortItem])? 0:10));
}

void CFileDetailListCtrl::OnSelect(wxListEvent& event)
{
	// Damn wxLC_SINGLE_SEL does not work! So we have to care for single selection ourselfs:
	long realpos = event.m_itemIndex;
	long pos=-1;
	for(;;) {
		// Loop through all selected items
		pos=GetNextItem(pos,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);
		if(pos==-1) {
			// No more selected items left
			break;
		} else if (pos != realpos) {
			// Deselect all items except the one we have just clicked
			SetItemState(pos, wxLIST_STATE_DESELECTED, wxLIST_STATE_SELECTED);
		}
	}
}
