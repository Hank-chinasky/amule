// This file is part of the aMule project.
//
// Copyright (c) 2004, aMule Team ( http://www.amule-project.net )
//
// Copyright (c) Angel Vidal Veiga (kry@users.sourceforge.net)
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



#include <wx/event.h>
#include <wx/app.h>
#include <wx/imaglist.h>
#include <wx/menu.h>
#include <wx/intl.h>

#include "MuleNotebook.h"	// Interface declarations
#include "opcodes.h"		// Needed for MP_CLOSE_ IDs

DEFINE_EVENT_TYPE(wxEVT_COMMAND_MULENOTEBOOK_PAGE_CLOSED)

BEGIN_EVENT_TABLE(CMuleNotebook, wxNotebook)
	EVT_RIGHT_DOWN(CMuleNotebook::OnRMButton)

	EVT_MENU(MP_CLOSE_TAB,			CMuleNotebook::OnPopupClose)
	EVT_MENU(MP_CLOSE_ALL_TABS,		CMuleNotebook::OnPopupCloseAll)
	EVT_MENU(MP_CLOSE_OTHER_TABS,	CMuleNotebook::OnPopupCloseOthers)	
	
	// Madcat - tab closing engine
	EVT_LEFT_DOWN(CMuleNotebook::MouseClick)
	EVT_LEFT_DCLICK(CMuleNotebook::MouseClick)
	EVT_MOTION(CMuleNotebook::MouseMotion)
END_EVENT_TABLE()

CMuleNotebook::CMuleNotebook( wxWindow *parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style, const wxString& name )
	: wxNotebook(parent, id, pos, size, style, name)
{
	m_popup_enable = true;
	m_popup_widget = NULL;
};


CMuleNotebook::~CMuleNotebook()
{
	// Ensure that all notifications gets sent
	DeleteAllPages();
}


bool CMuleNotebook::DeletePage(int nPage)
{
	// Send out close event
	wxNotebookEvent evt( wxEVT_COMMAND_MULENOTEBOOK_PAGE_CLOSED, GetId(), nPage );
	evt.SetEventObject(this);
	ProcessEvent( evt );

	// and finally remove the actual page
	if ( wxNotebook::DeletePage( nPage ) ) {
		// Ensure a valid selection
		if ( nPage >= GetPageCount() )
			nPage = GetPageCount() - 1;

		if ( nPage != -1 )
			SetSelection( nPage );

		return true;
	} 

	return false;
}


bool CMuleNotebook::DeleteAllPages()
{
	Freeze();
	
	bool result = false;
	for ( int i = GetPageCount(); i > 0; i-- )
		result |= DeletePage(i - 1);
		
	Thaw();
		
	return result;
}


void CMuleNotebook::EnablePopup( bool enable )
{
	m_popup_enable = enable;
}


void CMuleNotebook::SetPopupHandler( wxWindow* widget )
{
	m_popup_widget = widget;
}


void CMuleNotebook::OnRMButton(wxMouseEvent& event)
{
	// Allow the event to propagate further. That way, tabs can be selected with right-click
	event.Skip();

	// Cases where we shouldn't be showing a popup-menu
	if ( !GetPageCount() || !m_popup_enable )
		return;


	// Should we send the event to a specific widget?
	if ( m_popup_widget ) {
		wxMouseEvent evt = event;

		evt.SetEventObject( this );
						
		// Map the coordinates onto the parent
		wxPoint point = evt.GetPosition();
		point = ClientToScreen( point );
		point = m_popup_widget->ScreenToClient( point );
			
		evt.m_x = point.x;
		evt.m_y = point.y;
			
		m_popup_widget->ProcessEvent( evt );

	} else {
		// The usual case, where we show the popup
		wxMenu* menu = new wxMenu(wxString(_("Close")));
		menu->Append(MP_CLOSE_TAB, wxString(_("Close tab")));
		menu->Append(MP_CLOSE_ALL_TABS, wxString(_("Close all tabs")));
		menu->Append(MP_CLOSE_OTHER_TABS, wxString(_("Close other tabs")));

		PopupMenu( menu, event.GetPosition() );
	
		delete menu;
	}
}


void CMuleNotebook::OnPopupClose(wxCommandEvent& WXUNUSED(evt))
{
	DeletePage( GetSelection() );
}


void CMuleNotebook::OnPopupCloseAll(wxCommandEvent& WXUNUSED(evt))
{
	DeleteAllPages();
}


void CMuleNotebook::OnPopupCloseOthers(wxCommandEvent& WXUNUSED(evt))
{
	wxNotebookPage* current = GetPage( GetSelection() );
	
	for ( int i = GetPageCount() - 1; i >= 0; i-- ) {
		if ( current != GetPage( i ) )
			DeletePage( i );
	}
}


/**
 * Copyright (c) 2004 Alo Sarv <madcat_@users.sourceforge.net>
 * Most important function in this class. Here we do some serious math to figure
 * out where pages are located, where close-buttons are located etc.
 * @widths array contains the width in pixels of each page
 * @begins array contains the tab beginnings locations relative to window
 *         left border
 * @ends array contains the tab ends locations relative to window left border.
 *
 * First we clear all 3 arrays, and then fill with zeroes. The latter is being
 * done because we need to do +='s in loops and we want to start out at zeros.
 * Then we loop through pages list, measure the text label and image label
 * sizes, add the space the underlying platform adds (needs #defining for other
 * platforms), and fill the arrays with the data. Important notice: The FIRST
 * notebook tab is 3 pixels wider than the rest (at least on GTK)!
 */
void CMuleNotebook::CalculatePositions()
{
	int imagesizex, imagesizey;  // Notebookpage image size
	int textsizex, textsizey;    // Notebookpage text size

	if (GetImageList() == NULL) {
		return; // No images
	}

	// Reset the arrays
	widths.Clear();
	begins.Clear();
	ends.Clear();
	widths.Alloc(GetPageCount());
	begins.Alloc(GetPageCount());
	ends.Alloc(GetPageCount());

	// Fill the arrays with zeros
	for (int i=0;i<GetPageCount();++i) {
		widths.Add(0);
		begins.Add(0);
		ends.Add(0);
	}

	// Loop through all pages and calculate their widths.
	// Store all page begins, ends and widths in the arrays.
	for (int i=0;i<GetPageCount();++i) {
		GetImageList()->GetSize(
			GetPageImage(i), imagesizex, imagesizey
		);
		GetTextExtent(GetPageText(i), &textsizex, &textsizey);
		widths[i] = 17+imagesizex+textsizex;
		if (i==0) {                              // first page
			begins[i]=0;
			ends[i]=widths[i];
		} else {                                 // other pages
			// Pages after first one are 3 pixels shorter
			widths[i]-=3;
			// Start 1 pixel after previous one
			begins[i]=ends[i-1]+1;
			// End is beginning + width
			ends[i]=begins[i]+widths[i];
		}
	}
}

/**
 * Copyright (c) 2004 Alo Sarv <madcat_@users.sourceforge.net>
 * This method handles mouse clicks on tabs. We need to detect here if the
 * click happened to be on our close button, thus we first request positions
 * recalculation, and then compare the event position to our known close
 * buttons locations. If found, close the neccesery tab.
 */
void CMuleNotebook::MouseClick(wxMouseEvent &event)
{
	long posx, posy;             // Mouse position at the time of the event

	if (GetImageList() == NULL) {
		event.Skip();
		return; // No images
	}

	CalculatePositions();

	event.GetPosition(&posx, &posy);

	// Determine which page was under the mouse
	for (int i=0;i<GetPageCount();++i) {
		if (posx >= begins[i] && posx <= ends[i]) {
			// Found it, check if image was hit
			// Notice: (GTK) First tab is 3 pixels wider, thus the
			//         inline ifs.
			// TODO: Use #defines instead of hardcoded constants and
			//       correct values for GTK2, Mac and MSW.
			if (
				// Horizontal positioning
				posx >= begins[i]+(i?6:9) &&
				posx <= begins[i]+(i?6:9)+12 &&
				// Vertical positioning
				posy >= 11 &&
				posy <= 22
			) {
				// Image was hit, close the page
				// and return w/o passing event to wx
				DeletePage(i);
				return;
			}
		}
	}
	event.Skip();
}

/**
 * Copyright (c) 2004 Alo Sarv <madcat_@users.sourceforge.net>
 * This method handles mouse moving events. Since we can't recalculate positions
 * in EVT_MOUSE_ENTER (for some reason, wxNotebook doesn't receive those events)
 * we have to request recalculation here also, which is rather CPU-heavy.
 * Nonetheless, after we have updated positions in arrays, we can compare the
 * event position to our known close button locations, and if found, highlight
 * the neccesery button.
 */
void CMuleNotebook::MouseMotion(wxMouseEvent &event)
{
	long posx, posy;                        // Event X and Y positions
	if (GetImageList() == NULL) {
		event.Skip();
		return; // No images
	}

	CalculatePositions();

	posx = event.m_x;
	posy = event.m_y;

	// Determine which page was under the mouse
	for (int i=0;i<GetPageCount();++i) {
		SetPageImage(i, 0);
		if (posx >= begins[i] && posx <= ends[i]) {
			// Found it, check if image was hit
			// Notice: First tab is 3 pixels wider, thus the inline ifs
			if (
				// Horizontal positioning
				posx >= begins[i]+(i?6:9) &&
				posx <= begins[i]+(i?6:9)+12 &&
				// Vertical positioning
				posy >= 11 &&
				posy <= 22
			) {
				// Image is under mouse, change to highlight
				SetPageImage(i, 1);
			}
		}
	}
	event.Skip();
}