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

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "ClientListCtrl.h"
#endif

#include "ClientListCtrl.h"
#include "ClientDetailDialog.h"
#include "ChatWnd.h"
#include "amuleDlg.h"
#include "color.h"

#include "ClientCredits.h"
#include "updownclient.h"
#include "amule.h"
#include "opcodes.h"
#include "KnownFile.h"
#include "UploadQueue.h"
#include "amule.h"
#include "ClientList.h"

#include <wx/menu.h>
#include <wx/textdlg.h>


BEGIN_EVENT_TABLE( CClientListCtrl, CMuleListCtrl )
	EVT_RIGHT_DOWN(CClientListCtrl::OnRightClick)

	EVT_MENU( MP_DETAIL,		CClientListCtrl::OnShowDetails	)
	EVT_MENU( MP_ADDFRIEND,		CClientListCtrl::OnAddFriend	)
	EVT_MENU( MP_SHOWLIST,		CClientListCtrl::OnViewFiles	)
	EVT_MENU( MP_SENDMESSAGE,	CClientListCtrl::OnSendMessage	)
	EVT_MENU( MP_UNBAN,			CClientListCtrl::OnUnbanClient	)
	EVT_MENU_RANGE( MP_SWITCHCTRL_0,	MP_SWITCHCTRL_9,	CClientListCtrl::OnChangeView	)
END_EVENT_TABLE()

using namespace otherfunctions;

#define imagelist theApp.amuledlg->imagelist


/**
 * This struct is used to keep track of the different view-types.
 *
 * Each view has a number of attributes, namely a title and serveral functions 
 * which are used by the CClientListCtrl class. This struct is used to store these
 * for easier access.
 *
 * Please note that none of the values are required, however for a fully functional
 * view, it is nescesarry to specificy all of them.
 */
struct ClientListView
{
	//! The name of the view, this is used to load and save column-preferences.
	wxString	m_title;
	
	//! Pointer to the initialization function.
	void		(*m_init)(CClientListCtrl*);

	//! Pointer to the drawing function 
	void		(*m_draw)(CUpDownClient*, int, wxDC*, const wxRect&);

	//! Pointer to the sorting function.
	int			(*m_sort)(long, long, long);
};


//! This is the list of currently usable views, in the same order as the ViewType enum.
ClientListView g_listViews[] = 
{
	//! None: This view does nothing at all.
	{ wxEmptyString,	NULL,							NULL,						NULL					},

	//! Uploading: The clients currently being uploaded to.
	{ wxT("Uploads"),	CUploadingView::Initialize,		CUploadingView::DrawCell,	CUploadingView::SortProc},

	//! Queued: The clients currently queued for uploading.
	{ wxT("Queue"),		CQueuedView::Initialize,		CQueuedView::DrawCell,		CQueuedView::SortProc	},

	//! Clients The complete list of all known clients.
	{ wxT("Clients"),	CClientsView::Initialize,		CClientsView::DrawCell,		CClientsView::SortProc	}
};



CClientListCtrl::CClientListCtrl( wxWindow *parent, wxWindowID winid, const wxPoint &pos, const wxSize &size, long style, const wxValidator& validator, const wxString& name )
	: CMuleListCtrl( parent, winid, pos, size, style | wxLC_OWNERDRAW, validator, name )
{
	m_viewType = vtNone;
	
	m_menu = NULL;
	
	wxColour col = SYSCOLOR( wxSYS_COLOUR_HIGHLIGHT );
	m_hilightBrush = new wxBrush( BLEND( col, 125), wxSOLID );

	col = SYSCOLOR( wxSYS_COLOUR_BTNSHADOW );
	m_hilightUnfocusBrush = new wxBrush( BLEND( col, 125), wxSOLID );


	// We show the uploading-list initially
	SetListView( vtUploading );
}


CClientListCtrl::~CClientListCtrl()
{
	delete m_hilightBrush;
	delete m_hilightUnfocusBrush;
}


ViewType CClientListCtrl::GetListView()
{
	return m_viewType;
}


void CClientListCtrl::SetListView( ViewType newView )
{
	if ( m_viewType != newView ) {
		SaveSettings();
		
		ClearAll();
		
		m_viewType = newView;

		const ClientListView& view = g_listViews[ (int)newView ];
		
		// Initialize the selected view
		if ( view.m_init ) {
			view.m_init( this );
		}
	
		SetTableName( view.m_title );
			
		SetSortFunc( view.m_sort );

		LoadSettings();

		SortList();
	}
}


void CClientListCtrl::ToggleView()
{
	// Disallow toggling if the list is disabled
	if ( m_viewType == vtNone ) {
		return;
	}
	
	unsigned int view = (int)m_viewType + 1;
	
	if ( view < itemsof(g_listViews) ) {
		SetListView( (ViewType)(view) );
	} else {
		SetListView( (ViewType)(1) );
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////
void CClientListCtrl::OnRightClick(wxMouseEvent& event)
{
	// Check if clicked item is selected. If not, unselect all and select it.
	int tmp = 0;
	long index = HitTest( event.GetPosition(), tmp );

	if (index != -1) {
		if ( !GetItemState(index, wxLIST_STATE_SELECTED) ) {
			long item = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
			
			while ( item > -1 ) {
				SetItemState( item, 0, wxLIST_STATE_SELECTED );

				item = GetNextItem( item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
			}
			
			SetItemState( index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
		}
	}
	
	if ( m_menu == NULL ) {
		m_menu = new wxMenu(_("Clients"));
		m_menu->Append( MP_DETAIL,		_("Show &Details") );
		m_menu->Append( MP_ADDFRIEND,	_("Add to Friends") );
		m_menu->Append( MP_SHOWLIST,	_("View Files") );
		m_menu->Append( MP_SENDMESSAGE,	_("Send message") );
		m_menu->Append( MP_UNBAN,		_("Unban") );
		
		m_menu->AppendSeparator();
	
		wxMenu* view = new wxMenu();
		view->Append( MP_SWITCHCTRL_0 + 1, _("Show Uploads") );
		view->Append( MP_SWITCHCTRL_0 + 2, _("Show Queue") );
		view->Append( MP_SWITCHCTRL_0 + 3, _("Show Clients") );
	
		view->Enable( MP_SWITCHCTRL_0 + (int)m_viewType, false );
		
		m_menu->Append( 0, _("Select View"), view );
		
		m_menu->Enable( MP_DETAIL,		index > -1 );
		m_menu->Enable( MP_ADDFRIEND,	index > -1 );
		m_menu->Enable( MP_SHOWLIST,	index > -1 );
		m_menu->Enable( MP_SENDMESSAGE,	index > -1 );
		
		bool banned = false;

		// Check if the client is banned
		if ( index > -1 ) {
			CUpDownClient* client = (CUpDownClient*)GetItemData( index );

			banned = client->IsBanned();
		}
		m_menu->Enable( MP_UNBAN, 		banned );		

		PopupMenu( m_menu, event.GetPosition() );
		
		delete m_menu;
		
		m_menu = NULL;
	}
}


void CClientListCtrl::OnChangeView( wxCommandEvent& event )
{
	int view = event.GetId() - MP_SWITCHCTRL_0;

	SetListView( (ViewType)view );
}

	
void CClientListCtrl::OnAddFriend( wxCommandEvent& WXUNUSED(event) )
{
#ifndef CLIENT_GUI
	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	if ( index > -1 ) {
		CUpDownClient* client = (CUpDownClient*)GetItemData( index );
		
		theApp.amuledlg->chatwnd->AddFriend( client );
	}
#endif
}


void CClientListCtrl::OnShowDetails( wxCommandEvent& WXUNUSED(event) )
{
	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	if ( index > -1 ) {
		CUpDownClient* client = (CUpDownClient*)GetItemData( index );

		CClientDetailDialog dialog(this, client);
		
		dialog.ShowModal();
	}
}


void CClientListCtrl::OnViewFiles( wxCommandEvent& WXUNUSED(event) )
{
	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	if ( index > -1 ) {
		CUpDownClient* client = (CUpDownClient*)GetItemData( index );

		client->RequestSharedFileList();
	}
}


void CClientListCtrl::OnSendMessage( wxCommandEvent& WXUNUSED(event) )
{
#ifndef CLIENT_GUI
	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	if ( index > -1 ) {
		CUpDownClient* client = (CUpDownClient*)GetItemData( index );

		wxString message = ::wxGetTextFromUser( _("Send message to user"), _("Message to send:") );
		
		if ( !message.IsEmpty() ) {
			theApp.amuledlg->chatwnd->StartSession(client, false);
			theApp.amuledlg->chatwnd->SendMessage(message);
		}
	}
#endif
}

		
void CClientListCtrl::OnUnbanClient( wxCommandEvent& WXUNUSED(event) )
{
	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	if ( index > -1 ) {
		CUpDownClient* client = (CUpDownClient*)GetItemData( index );

		if ( client->IsBanned() ) {
			client->UnBan();
		}		
	}
}


void CClientListCtrl::InsertClient( CUpDownClient* client, ViewType view )
{
	if ( ( m_viewType != view ) || ( view == vtNone ) ) {
		return;
	}
	
	long index = InsertItem( GetItemCount(), wxT("") );
	SetItemData( index, (long)client );

	wxListItem myitem;
	myitem.SetId( index );
	myitem.SetBackgroundColour( SYSCOLOR(wxSYS_COLOUR_LISTBOX) );
	
	SetItem(myitem);

	RefreshItem( index );
}


void CClientListCtrl::RemoveClient( CUpDownClient* client, ViewType view )
{
	if ( ( m_viewType != view ) || ( view == vtNone ) ) {
		return;
	}
	
	long index = FindItem( -1, (long)client );
	
	if ( index > -1 ) {
		DeleteItem( index );
	}
}


void CClientListCtrl::UpdateClient( CUpDownClient* client, ViewType view )
{
	if ( ( m_viewType != view ) || ( view == vtNone ) ) {
		return;
	}
	
	if ( theApp.amuledlg->IsDialogVisible( CamuleDlg::TransferWnd ) ) {
		// Visible lines, default to all because not all platforms support the GetVisibleLines function
		long first = 0, last = GetItemCount();
		
		long result = FindItem( -1, (long)client );
	
		if ( result > -1 ) {
			#ifndef __WXMSW__
				GetVisibleLines( &first, &last );
			#endif
			
			if ( result >= first && result <= last) {
				RefreshItem(result);
			}
		}
	}
}


void CClientListCtrl::OnDrawItem( int item, wxDC* dc, const wxRect& rect, const wxRect& rectHL, bool highlighted )
{
	// Don't do any drawing if we not being watched.
	if ( !theApp.amuledlg->IsDialogVisible( CamuleDlg::TransferWnd ) ) {
		return;
	}

	
	if ( highlighted ) {
		if ( GetFocus() ) {
			dc->SetBackground(*m_hilightBrush);
			dc->SetTextForeground( SYSCOLOR(wxSYS_COLOUR_HIGHLIGHTTEXT) );
		} else {
			dc->SetBackground(*m_hilightUnfocusBrush);
			dc->SetTextForeground( SYSCOLOR(wxSYS_COLOUR_HIGHLIGHTTEXT));
		}
	

		wxColour colour = GetFocus() ? m_hilightBrush->GetColour() : m_hilightUnfocusBrush->GetColour();
		dc->SetPen( wxPen( BLEND(colour, 65), 1, wxSOLID) );
	} else {
		dc->SetBackground( wxBrush( SYSCOLOR(wxSYS_COLOUR_LISTBOX), wxSOLID ) );
		dc->SetTextForeground( SYSCOLOR(wxSYS_COLOUR_WINDOWTEXT) );


		dc->SetPen(*wxTRANSPARENT_PEN);
	}
	
	
	dc->SetBrush(dc->GetBackground());
	dc->DrawRectangle(rectHL);
	dc->SetPen(*wxTRANSPARENT_PEN);

	
	CUpDownClient* client = (CUpDownClient*)GetItemData(item);
	
	wxRect cur_rect = rect;
	cur_rect.x += 4;

	const ClientListView& view = g_listViews[ (int)m_viewType ];

	if ( view.m_draw ) {
		for ( int i = 0; i < GetColumnCount(); i++ ) {
			int width = GetColumnWidth( i );
	
			if ( width ) {
				cur_rect.width = width - 8;
		
				wxDCClipper clipper( *dc, cur_rect );
		
				view.m_draw( client, i, dc, cur_rect );
			}
		
			cur_rect.x += width;
		}
	}
}




/////////////////////////////////////////////////////////////////////////////////////////////
void CUploadingView::Initialize( CClientListCtrl* list )
{
	list->InsertColumn( 0,	_("Username"),			wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 1,	_("File"),				wxLIST_FORMAT_LEFT, 275 );
	list->InsertColumn( 2,	_("Client Software"),	wxLIST_FORMAT_LEFT, 100 );
	list->InsertColumn( 3,	_("Speed"),				wxLIST_FORMAT_LEFT,  60 );
	list->InsertColumn( 4,	_("Transferred"),		wxLIST_FORMAT_LEFT,  65 );
	list->InsertColumn( 5,	_("Waited"),			wxLIST_FORMAT_LEFT,  60 );
	list->InsertColumn( 6,	_("Upload Time"),		wxLIST_FORMAT_LEFT,  60 );
	list->InsertColumn( 7,	_("Status"),			wxLIST_FORMAT_LEFT, 110 );
	list->InsertColumn( 8,	_("Obtained Parts"),	wxLIST_FORMAT_LEFT, 100 );
	list->InsertColumn( 9,	_("Upload/Download"),	wxLIST_FORMAT_LEFT, 100 );
	list->InsertColumn( 10,	_("Remote Status"),		wxLIST_FORMAT_LEFT, 100 );			
			
			
	// Insert any existing items on the list
	POSITION pos = theApp.uploadqueue->GetFirstFromUploadList();
	while ( pos ) {
		list->InsertClient( theApp.uploadqueue->GetNextFromUploadList( pos ), list->GetListView() );
	}
}


void CUploadingView::DrawCell( CUpDownClient* client, int column, wxDC* dc, const wxRect& rect )
{
	wxString buffer;	

	switch ( column ) {
		case 0: {
			uint8 clientImage;
	
			if ( client->IsFriend() ) {
				clientImage = 13;
			} else {
				switch (client->GetClientSoft()) {
					case SO_AMULE:
						clientImage = 17;
						break;
					case SO_MLDONKEY:
					case SO_NEW_MLDONKEY:
					case SO_NEW2_MLDONKEY:
						clientImage = 15;
						break;
					case SO_EDONKEY:
					case SO_EDONKEYHYBRID:
						// Maybe we would like to make different icons?
						clientImage = 16;
						break;
					case SO_EMULE:
						clientImage = 14;
					break;
						case SO_LPHANT:
						clientImage = 18;
						break;
					case SO_SHAREAZA:
						clientImage = 19;
						break;
					case SO_LXMULE:
						clientImage = 20;
						break;
					default:
						// cDonkey, Compat Unk
						// No icon for those yet. Using the eMule one + '?'
						clientImage = 21;
						break;
				}
			}

			imagelist.Draw( clientImage, *dc, rect.x, rect.y + 1, wxIMAGELIST_DRAW_TRANSPARENT );

			if (client->credits && client->credits->GetScoreRatio(client->GetIP()) > 1) {
				// Has credits, draw the gold star
				imagelist.Draw( 11, *dc, rect.x, rect.y + 1, wxIMAGELIST_DRAW_TRANSPARENT );
			} else if (client->ExtProtocolAvailable()) {
				// Ext protocol -> Draw the '+'
				imagelist.Draw(  7, *dc, rect.x, rect.y + 1, wxIMAGELIST_DRAW_TRANSPARENT );
			}

			dc->DrawText( client->GetUserName(), rect.x + 20, rect.y + 3 );
			
			return;
		}
	
		case 1:
			if ( client->GetUploadFile() ) {
				buffer = client->GetUploadFile()->GetFileName();
			} else {
				buffer = wxT("N/A");
			}
			break;
	
		case 2:
			buffer = client->GetClientVerString();
			break;
		
		case 3:
			buffer.Printf( wxT("%.1f"), client->GetKBpsUp() );
				
			if ( client->GetDownloadState() == DS_DOWNLOADING ) {
				buffer += wxString::Format( wxT("/%.1f"), client->GetKBpsDown() );
			} 

			buffer += wxT(" ");
			buffer += _("kB/s");
		break;
			
		case 4:
			buffer = CastItoXBytes(client->GetSessionUp());
			break;
		
		case 5:
			buffer = CastSecondsToHM((client->GetWaitTime())/1000);
			break;
		
		case 6:
			buffer = CastSecondsToHM((client->GetUpStartTimeDelay())/1000);
			break;
		
		case 7:
			switch ( client->GetUploadState() ) {
				case US_CONNECTING:
					buffer = _("Connecting");
					break;
					
				case US_WAITCALLBACK:
					buffer = _("Connecting via server");
					break;
					
				case US_UPLOADING:
					buffer = wxT("<-- ");
					buffer.Append(_("Transferring"));
					
					if (client->GetDownloadState() == DS_DOWNLOADING) {
						buffer.Append(wxT(" -->"));
					}
					break;
					
				default:
					buffer = _("Unknown");
			}
			break;
			
		case 8:
			if ( client->GetUpPartCount() ) {
				CUploadingView::DrawStatusBar( client, dc, rect );
			}
			return;
		
		case 9:
			if ( client->Credits() ) {
				buffer = CastItoXBytes( client->Credits()->GetUploadedTotal() ) + wxT(" / ") + CastItoXBytes(client->Credits()->GetDownloadedTotal());
			} else {
				buffer = wxT("? / ?");
			}
			break;
		
		case 10:
			if ( client->GetDownloadState() == DS_ONQUEUE ) {
				if ( client->IsRemoteQueueFull() ) {
					buffer = _("Queue Full");
				} else {
					if (client->GetRemoteQueueRank()) {
						buffer.Printf(_("QR: %u"), client->GetRemoteQueueRank());
					} else {
						buffer = _("Unknown");
					}
				}
			} else {
				buffer = _("Unknown");
			}
			break;
	}

			
	dc->DrawText( buffer, rect.x, rect.y + 3 );
}


int CUploadingView::SortProc( long item1, long item2, long sortData )
{
	CUpDownClient* client1 = (CUpDownClient*)item1;
	CUpDownClient* client2 = (CUpDownClient*)item2;

	// Sorting ascending or decending
	int mode = 1;
	if ( sortData >= 1000 ) {
		mode = -1;
		sortData -= 1000;
	}


	switch ( sortData) {
		// Sort by username
		case 0:	return mode * client1->GetUserName().CmpNoCase( client2->GetUserName() );

		
		// Sort by requested file
		case 1: {
			CKnownFile* file1 = client1->GetUploadFile();
			CKnownFile* file2 = client2->GetUploadFile();

			if ( file1 && file2  ) {
				return mode * file1->GetFileName().CmpNoCase( file2->GetFileName() );
			} 
			
			return mode * CmpAny( file1, file2 );
		}
		
		// Sort by client software
		case 2: {
			if ( client1->GetClientSoft() != client2->GetClientSoft() )
				return mode * CmpAny( client1->GetClientSoft(), client2->GetClientSoft() );

			if (client1->GetVersion() != client2->GetVersion())
				return mode * CmpAny( client1->GetVersion(), client2->GetVersion() );

			return mode * client1->GetClientModString().CmpNoCase( client2->GetClientModString() );
		}
		
		// Sort by speed
		case 3: return mode * CmpAny( client1->GetKBpsUp(), client2->GetKBpsUp() );
		
		// Sort by transfered
		case 4: return mode * CmpAny( client1->GetTransferedUp(), client2->GetTransferedUp() );
		
		// Sort by wait-time
		case 5: return mode * CmpAny( client1->GetWaitTime(), client2->GetWaitTime() );
		
		// Sort by upload time
		case 6: return mode * CmpAny( client1->GetUpStartTimeDelay(), client2->GetUpStartTimeDelay() );
		
		// Sort by state
		case 7: return mode * CmpAny( client1->GetUploadState(), client2->GetUploadState() );
		
		// Sort by partcount
		case 8: return mode * CmpAny( client1->GetUpPartCount(), client2->GetUpPartCount() );
		
		// Sort by U/D ratio
		case 9: return mode * CmpAny( client2->Credits()->GetDownloadedTotal(), client1->Credits()->GetDownloadedTotal() );
		
		// Sort by remote rank
		case 10: return mode * CmpAny( client2->GetRemoteQueueRank(), client1->GetRemoteQueueRank() );

		default:
			return 0;
	}
}


void CUploadingView::DrawStatusBar( CUpDownClient* client, wxDC* dc, const wxRect& rect1 )
{
	wxRect rect = rect1;
	rect.y		+= 2;
	rect.height	-= 2;

	wxPen   old_pen   = dc->GetPen();
	wxBrush old_brush = dc->GetBrush();

	dc->SetPen(*wxTRANSPARENT_PEN);
	dc->SetBrush( wxBrush( wxColour(220,220,220), wxSOLID ) );
	
	dc->DrawRectangle( rect );
	dc->SetBrush(*wxBLACK_BRUSH);

	uint32 partCount = client->GetUpPartCount();

	float blockpixel = (float)(rect.width)/((float)(PARTSIZE*partCount)/1024);
	for ( uint32 i = 0; i < partCount; i++ ) {
		if ( client->IsUpPartAvailable( i ) ) { 
			int right = rect.x + (uint32)(((float)PARTSIZE*i/1024)*blockpixel);
			int left  = rect.x + (uint32)((float)((float)PARTSIZE*(i+1)/1024)*blockpixel);

			dc->DrawRectangle( (int)left, rect.y, right - left, rect.height );					
		}
	}

	dc->SetPen( old_pen );
	dc->SetBrush( old_brush );
}




/////////////////////////////////////////////////////////////////////////////////////////////
void CQueuedView::Initialize( CClientListCtrl* list )
{
	list->InsertColumn( 0,	_("Username"),			wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 1,	_("File"),				wxLIST_FORMAT_LEFT,	275 );
	list->InsertColumn( 2,	_("Client Software"),	wxLIST_FORMAT_LEFT,	100 );
	list->InsertColumn( 3,	_("File Priority"),		wxLIST_FORMAT_LEFT,	110 );
	list->InsertColumn( 4,	_("Rating"),			wxLIST_FORMAT_LEFT,	 60 );
	list->InsertColumn( 5,	_("Score"),				wxLIST_FORMAT_LEFT,	 60 );
	list->InsertColumn( 6,	_("Asked"),				wxLIST_FORMAT_LEFT,	 60 );
	list->InsertColumn( 7,	_("Last Seen"),			wxLIST_FORMAT_LEFT,	110 );
	list->InsertColumn( 8,	_("Entered Queue"),		wxLIST_FORMAT_LEFT,	110 );
	list->InsertColumn( 9,	_("Banned"),			wxLIST_FORMAT_LEFT,	 60 );
	list->InsertColumn( 10,	_("Obtained Parts"),	wxLIST_FORMAT_LEFT,	100 );


	// Insert any existing items on the list
	POSITION pos = theApp.uploadqueue->GetFirstFromWaitingList();

	while ( pos ) {
		list->InsertClient( theApp.uploadqueue->GetNextFromWaitingList( pos ), list->GetListView() );
	}
}


void CQueuedView::DrawCell( CUpDownClient* client, int column, wxDC* dc, const wxRect& rect )
{
	wxString buffer;
	
	switch ( column ) {
		// These 3 are the same for both lists
		case 0:
		case 1:
		case 2:
			CUploadingView::DrawCell( client, column, dc, rect );
			return;

		case 3:
			if ( client->GetUploadFile() ) {
				switch( client->GetUploadFile()->GetUpPriority() ) {
					case PR_POWERSHARE:		buffer = _("Release");		break;
					case PR_VERYHIGH:		buffer = _("Very High");	break;
					case PR_HIGH:			buffer = _("High");			break;
					case PR_LOW:			buffer = _("Low");			break;
					case PR_VERYLOW:		buffer = _("Very low");		break;
					default:				buffer = _("Normal");		break;
				}
			} else {
				buffer = _("Unknown");
			}

			break;
			
		case 4:
			buffer.Printf( wxT("%.1f"), (float)client->GetScore(false,false,true) );
			break;
		
		case 5:
			if ( client->HasLowID() ) {
				buffer.Printf( wxT("%i %s"), client->GetScore(false), _("LowID") );
			} else {
				buffer.Printf(wxT("%i"),client->GetScore(false));
			}
			break;
			
		case 6:
			buffer.Printf( wxT("%i"), client->GetAskedCount() );
			break;
		
		case 7:
			buffer = CastSecondsToHM((::GetTickCount() - client->GetLastUpRequest())/1000);
			break;
		
		case 8:
			buffer = CastSecondsToHM((::GetTickCount() - client->GetWaitStartTime())/1000);
			break;
		
		case 9:
			if ( client->IsBanned() ) {
				buffer = _("Yes");
			} else {
				buffer = _("No");
			}
			
			break;
			
		case 10:
			if ( client->GetUpPartCount() ) {
				CUploadingView::DrawStatusBar( client, dc, rect );
			}
			
			return;
	}
			
	dc->DrawText( buffer, rect.x, rect.y + 3 );
}


int CQueuedView::SortProc( long item1, long item2, long sortData )
{
	CUpDownClient* client1 = (CUpDownClient*)item1;
	CUpDownClient* client2 = (CUpDownClient*)item2;

	// Ascending or decending?
	int mode = 1;
	if ( sortData >= 1000 ) {
		mode = -1;
		sortData -= 1000;
	}

	switch ( sortData ) {
		// Sort by username
		case 0: return mode * client1->GetUserName().CmpNoCase( client2->GetUserName() );
		
		// Sort by filename
		case 1: {
			CKnownFile* file1 = client1->GetUploadFile();
			CKnownFile* file2 = client2->GetUploadFile();

			if ( file1 && file2 ) {
				return mode * file1->GetFileName().CmpNoCase( file2->GetFileName() );
			}

			// Place files with filenames on top
			return -mode * CmpAny( file1, file2 );
		}
		
		// Sort by client software
		case 2: {
			if (client1->GetClientSoft() != client2->GetClientSoft())
				return mode * CmpAny( client1->GetClientSoft(), client2->GetClientSoft() );

			if (client1->GetVersion() != client2->GetVersion())
				return mode * CmpAny( client1->GetVersion(), client2->GetVersion() );

			return mode * client1->GetClientModString().CmpNoCase( client2->GetClientModString() );
		}
		
		// Sort by file upload-priority
		case 3: {
			CKnownFile* file1 = client1->GetUploadFile();
			CKnownFile* file2 = client2->GetUploadFile();

			if ( file1 && file2  ) {
				int8 prioA = file1->GetUpPriority();
				int8 prioB = file2->GetUpPriority();

				// Work-around for PR_VERYLOW which has value 4. See KnownFile.h for that stupidity ...
				return mode * CmpAny( ( prioA != PR_VERYLOW ? prioA : -1 ), ( prioB != PR_VERYLOW ? prioB : -1 ) );
			} 
			
			// Place files with priorities on top
			return -mode * CmpAny( file1, file2 );
		}
		
		// Sort by rating
		case 4: return mode * CmpAny( client1->GetScore(false,false,true), client2->GetScore(false,false,true) );

		// Sort by score
		case 5: return mode * CmpAny( client1->GetScore(false), client2->GetScore(false) );
		
		// Sort by Asked count
		case 6:	return mode * CmpAny( client1->GetAskedCount(), client2->GetAskedCount() );
		
		// Sort by Last seen
		case 7: return mode * CmpAny( client1->GetLastUpRequest(), client2->GetLastUpRequest() );
		
		// Sort by entered time
		case 8: return mode * CmpAny( client1->GetWaitStartTime(), client2->GetWaitStartTime() );

		// Sort by banned
		case 9: return mode * CmpAny( client1->IsBanned(), client2->IsBanned() );
	
		default: return 0;
	}
}



/////////////////////////////////////////////////////////////////////////////////////////////
void CClientsView::Initialize( CClientListCtrl* list )
{
	list->InsertColumn( 0, _("Username"),			wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 1, _("Upload Status"),	wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 2, _("Transferred Up"),	wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 3, _("Download Status"),	wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 4, _("Transferred Down"),	wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 5, _("Client Software"),	wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 6, _("Connected"),		wxLIST_FORMAT_LEFT,	150 );
	list->InsertColumn( 7, _("Userhash"),			wxLIST_FORMAT_LEFT,	150 );


	const CClientList::IDMap& clist = theApp.clientlist->GetClientList();
	CClientList::IDMap::const_iterator it = clist.begin();
	
	for ( ; it != clist.end(); ++it ) {
		list->InsertClient( it->second, list->GetListView() );
	}
}


void CClientsView::DrawCell( CUpDownClient* client, int column, wxDC* dc, const wxRect& rect )
{
	wxString buffer;
	
	switch ( column ) {
		case 0:
			CUploadingView::DrawCell( client, column, dc, rect );
			return;
		
		case 1:
			CUploadingView::DrawCell( client, 7, dc, rect );
			return;

		
		case 2:
			if ( client->credits ) {
				buffer = CastItoXBytes( client->credits->GetUploadedTotal() );
			} else {
				buffer = CastItoXBytes( 0 );
			}
			
			break;
			
		case 3:
			switch ( client->GetDownloadState() ) {
				case DS_CONNECTING:		buffer = _("Connecting");				break;
				case DS_CONNECTED:		buffer = _("Asking");					break;
				case DS_WAITCALLBACK:	buffer = _("Connecting via server");	break;
				case DS_ONQUEUE:
					if ( client->IsRemoteQueueFull() ) {
						buffer = _("Queue Full");
					} else {
						buffer = _("On Queue");
					}
					
					break;
					
				case DS_DOWNLOADING:	buffer = _("Transferring");				break;
				case DS_REQHASHSET:		buffer = _("Receiving hashset");		break;
				case DS_NONEEDEDPARTS:	buffer = _("No needed parts");			break;
				case DS_LOWTOLOWIP:		buffer = _("Cannot connect LowID to LowID");	break;
				case DS_TOOMANYCONNS:	buffer = _("Too many connections");		break;
				default:
					buffer = _("Unknown");
			}
			
			break;
	
		case 4:
			if ( client->credits ) {
				buffer = CastItoXBytes( client->credits->GetDownloadedTotal() );
			} else {
				buffer = CastItoXBytes( 0 );
			}
			
			break;
			
		case 5:
			buffer = client->GetClientVerString();
			break;
			
		case 6:
			if ( client->IsConnected() ) {
				buffer = _("Yes");
			} else {
				buffer = _("No");
			}
			
			break;
			
		case 7:
			buffer = client->GetUserHash().Encode();
			break;
		
	}
	
	dc->DrawText( buffer, rect.x, rect.y + 3 );
}


int CClientsView::SortProc( long item1, long item2, long sortData )
{
	CUpDownClient* client1 = (CUpDownClient*)item1;
	CUpDownClient* client2 = (CUpDownClient*)item2;

	// Ascending or decending?
	int mode = 1;
	if ( sortData >= 1000 ) {
		mode = -1;
		sortData -= 1000;
	}

	switch ( sortData ) {
		// Sort by Username
		case 0: return mode * client1->GetUserName().CmpNoCase( client2->GetUserName() );
		
		// Sort by Uploading-state
		case 1: return mode * CmpAny( client1->GetUploadState(), client2->GetUploadState() );
		
		// Sort by data-uploaded
		case 2:
			if ( client1->credits && client2->credits ) {
				return mode * CmpAny( client1->credits->GetUploadedTotal(), client2->credits->GetUploadedTotal() );
			}

			return -mode * CmpAny( client1->credits, client2->credits );
		
		// Sort by Downloading-state
		case 3:
		    if( client1->GetDownloadState() == client2->GetDownloadState() ){
			    if( client1->IsRemoteQueueFull() && client2->IsRemoteQueueFull() ) {
				    return mode *  0;
			    } else if( client1->IsRemoteQueueFull() ) {
				    return mode *  1;
			    } else if( client2->IsRemoteQueueFull() ) {
				    return mode * -1;
			    } else {
				    return mode *  0;
				}
		    }
			return mode * CmpAny( client1->GetDownloadState(), client2->GetDownloadState() );
		
		// Sort by data downloaded
		case 4:
			if ( client1->credits && client2->credits ) {
				return CmpAny( client1->credits->GetDownloadedTotal(), client2->credits->GetDownloadedTotal() );
			}

			return -mode * CmpAny( client1->credits, client2->credits );
		
		
		// Sort by client-software
		case 5:
			if (client1->GetClientSoft() != client2->GetClientSoft())
				return mode * CmpAny( client1->GetClientSoft(), client2->GetClientSoft() );

			if (client1->GetVersion() != client2->GetVersion())
				return mode * CmpAny( client1->GetVersion(), client2->GetVersion() );

			return mode * client1->GetClientModString().CmpNoCase( client2->GetClientModString() );
		
		// Sort by connection
		case 6: return mode * CmpAny( client1->IsConnected(), client2->IsConnected() );

		
		case 7:
			return mode * CmpAny( client1->GetUserHash(), client2->GetUserHash() );
		
		default:
			return 0;
	}

}
