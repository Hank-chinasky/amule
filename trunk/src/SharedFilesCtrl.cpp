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


#include "muuli_wdr.h"			// Needed for ID_SHFILELIST
#include "SharedFilesCtrl.h"	// Interface declarations
#include "otherfunctions.h"		// Needed for CastItoXBytes
#include "SharedFilesWnd.h"		// Needed for CSharedFilesWnd
#include "amuleDlg.h"			// Needed for CamuleDlg
#include "CommentDialog.h"		// Needed for CCommentDialog
#include "PartFile.h"			// Needed for CPartFile
#include "SharedFileList.h"		// Needed for CKnownFileMap
#include "opcodes.h"			// Needed for MP_PRIOVERYLOW
#include "amule.h"				// Needed for theApp
#include "color.h"				// Needed for SYSCOLOR
#include "sockets.h"			// Needed for CServerConnect
#include "Preferences.h"
#include "BarShader.h"			// Needed for CBarShader


#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/menu.h>


BEGIN_EVENT_TABLE(CSharedFilesCtrl,CMuleListCtrl)
	EVT_LIST_ITEM_RIGHT_CLICK(ID_SHFILELIST, CSharedFilesCtrl::OnRightClick)

	EVT_MENU( MP_PRIOVERYLOW,	CSharedFilesCtrl::OnSetPriority )
	EVT_MENU( MP_PRIOLOW,		CSharedFilesCtrl::OnSetPriority )
	EVT_MENU( MP_PRIONORMAL,	CSharedFilesCtrl::OnSetPriority )
	EVT_MENU( MP_PRIOHIGH,		CSharedFilesCtrl::OnSetPriority )
	EVT_MENU( MP_PRIOVERYHIGH,	CSharedFilesCtrl::OnSetPriority )
	EVT_MENU( MP_POWERSHARE,	CSharedFilesCtrl::OnSetPriority )
	EVT_MENU( MP_PRIOAUTO,		CSharedFilesCtrl::OnSetPriorityAuto )

	EVT_MENU( MP_PERMNONE,		CSharedFilesCtrl::OnSetPermissions )
	EVT_MENU( MP_PERMFRIENDS,	CSharedFilesCtrl::OnSetPermissions )
	EVT_MENU( MP_PERMALL,		CSharedFilesCtrl::OnSetPermissions )
	
	EVT_MENU( MP_CMT,			CSharedFilesCtrl::OnEditComment )
	EVT_MENU( MP_GETCOMMENTS,   CSharedFilesCtrl::OnGetComment )  
	
	EVT_MENU( MP_GETED2KLINK,				CSharedFilesCtrl::OnCreateURI )
	EVT_MENU( MP_GETHTMLED2KLINK,			CSharedFilesCtrl::OnCreateURI )
	EVT_MENU( MP_GETSOURCEED2KLINK,			CSharedFilesCtrl::OnCreateURI )
	EVT_MENU( MP_GETHOSTNAMESOURCEED2KLINK,	CSharedFilesCtrl::OnCreateURI )
END_EVENT_TABLE()

using namespace otherfunctions;

CSharedFilesCtrl::CSharedFilesCtrl(wxWindow* parent, int id, const wxPoint& pos, wxSize size, int flags)
	: CMuleListCtrl(parent, id, pos, size, flags | wxLC_OWNERDRAW )
{
	// Setting the sorter function.
	SetSortFunc( SortProc );

	// Set the table-name (for loading and saving preferences).
	SetTableName( wxT("Shared") );

	m_menu=NULL;

	InsertColumn(0,  _("File Name"),			wxLIST_FORMAT_LEFT, 250);
	InsertColumn(1,  _("Size"),					wxLIST_FORMAT_LEFT, 100);
	InsertColumn(2,  _("Type"),					wxLIST_FORMAT_LEFT,  50);
	InsertColumn(3,  _("Priority"),				wxLIST_FORMAT_LEFT,  70);
	InsertColumn(4,  _("Permission"),			wxLIST_FORMAT_LEFT, 100);
	InsertColumn(5,  _("FileID"),				wxLIST_FORMAT_LEFT, 220);
	InsertColumn(6,  _("Requests"),				wxLIST_FORMAT_LEFT, 100);
	InsertColumn(7,  _("Accepted Requests"),	wxLIST_FORMAT_LEFT, 100);
	InsertColumn(8,  _("Transferred Data"),		wxLIST_FORMAT_LEFT, 120);
	InsertColumn(9,  _("Obtained Parts"),		wxLIST_FORMAT_LEFT, 120);
	InsertColumn(10, _("Complete Sources"),		wxLIST_FORMAT_LEFT, 120);
	InsertColumn(11, _("Directory Path"),		wxLIST_FORMAT_LEFT, 220);

	LoadSettings();
}


CSharedFilesCtrl::~CSharedFilesCtrl()
{

}


void CSharedFilesCtrl::OnRightClick(wxListEvent& evt)
{
	// Check if clicked item is selected. If not, unselect all and select it.
	if ( !GetItemState(evt.GetIndex(), wxLIST_STATE_SELECTED) ) {
		long index = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

		while ( index > -1 ) {
			SetItemState( index, 0, wxLIST_STATE_SELECTED );

			index = GetNextItem(index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		}

		SetItemState(evt.GetIndex(), wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}


	if ( m_menu == NULL ) {
		wxMenu* m_menu = new wxMenu(_("Shared Files"));
		wxMenu* prioMenu = new wxMenu();
		prioMenu->Append(MP_PRIOVERYLOW, _("Very low"));
		prioMenu->Append(MP_PRIOLOW, _("Low"));
		prioMenu->Append(MP_PRIONORMAL, _("Normal"));
		prioMenu->Append(MP_PRIOHIGH, _("High"));
		prioMenu->Append(MP_PRIOVERYHIGH, _("Very High"));
		prioMenu->Append(MP_POWERSHARE, _("Release"));
		prioMenu->Append(MP_PRIOAUTO, _("Auto"));

		wxMenu* permMenu = new wxMenu();
		permMenu->Append(MP_PERMALL, _("Public"));
		permMenu->Append(MP_PERMFRIENDS, _("Friends only"));
		permMenu->Append(MP_PERMNONE, _("Locked"));

		m_menu->Append(0,_("Priority"),prioMenu);
		m_menu->Append(0,_("Permissions"),permMenu);
		m_menu->AppendSeparator();
		m_menu->Append(MP_CMT, _("Change this file's comment..."));
		m_menu->Append(MP_GETCOMMENTS,_("Show all comments"));
		m_menu->AppendSeparator();

		m_menu->Append(MP_GETED2KLINK,_("Copy ED2k &link to clipboard"));
		m_menu->Append(MP_GETSOURCEED2KLINK,_("Copy ED2k link to clipboard (&Source)"));
		m_menu->Append(MP_GETHOSTNAMESOURCEED2KLINK,_("Copy ED2k link to clipboard (Hostname)"));
		m_menu->Append(MP_GETHTMLED2KLINK,_("Copy ED2k link to clipboard (&HTML)"));
		

		PopupMenu( m_menu, evt.GetPoint() );

		delete m_menu;

		m_menu = NULL;
	}
}


void CSharedFilesCtrl::ShowFileList(CSharedFileList* list)
{
	DeleteAllItems();

	CKnownFileMap::iterator it = list->m_Files_map.begin();
	for ( ; it != list->m_Files_map.end(); ++it ) {
		ShowFile( it->second );
	}
}


void CSharedFilesCtrl::RemoveFile(CKnownFile *toRemove)
{
	long index = FindItem( -1, (long)toRemove );
	
	if ( index > -1 ) {
		DeleteItem( index );
		
		ShowFilesCount();
	}
}


void CSharedFilesCtrl::UpdateFile( CKnownFile* file, long itemnr )
{
	wxString buffer;
	
	SetItemData( itemnr, (long)file );
	
	SetItem(itemnr, 1, CastItoXBytes(file->GetFileSize()) );
	SetItem(itemnr, 2, GetFiletypeByName(file->GetFileName()) );
	
	if ( file->IsAutoUpPriority() ) {
		switch ( file->GetUpPriority() ) {
			case PR_LOW:		SetItem( itemnr, 3, _("Auto [Lo]") ); break;
			case PR_NORMAL:		SetItem( itemnr, 3, _("Auto [No]") ); break;
			case PR_HIGH:		SetItem( itemnr, 3, _("Auto [Hi]") ); break;
			case PR_VERYHIGH:	SetItem( itemnr, 3, _("Auto [Re]") ); break;
			default:
				SetItem( itemnr, 3, _("Auto [UNK]") );
		}
	} else {
		switch ( file->GetUpPriority() ) {
		case PR_VERYLOW:	SetItem( itemnr, 3, _("Very low") );	break;
		case PR_LOW:		SetItem( itemnr, 3, _("Low") );			break;
		case PR_NORMAL:		SetItem( itemnr, 3, _("Normal") );		break;
		case PR_HIGH:		SetItem( itemnr, 3, _("High") );		break;
		case PR_VERYHIGH:	SetItem( itemnr, 3, _("Very High") );	break;
		case PR_POWERSHARE:	SetItem( itemnr, 3, _("Release") );		break;
		default:
			SetItem( itemnr, 3, _("Unknown") );
		}
	}

	switch ( file->GetPermissions() ) {
		case PERM_NOONE:	SetItem(itemnr, 4, _("Hidden") );		break;
		case PERM_FRIENDS:	SetItem(itemnr, 4, _("Friends only") );	break;
		case PERM_ALL:		SetItem(itemnr, 4, _("Public") );		break;
	}

	SetItem( itemnr, 5, file->GetFileHash().Encode() );

	buffer.Printf( wxT("%u (%u)"), file->statistic.GetRequests(), file->statistic.GetAllTimeRequests() );
	SetItem( itemnr, 6, buffer );
	
	buffer.Printf( wxT("%u (%u)"), file->statistic.GetAccepts(), file->statistic.GetAllTimeAccepts() );
	SetItem( itemnr, 7, buffer );
	
	buffer = CastItoXBytes(file->statistic.GetTransfered()) + wxT(" (") + CastItoXBytes(file->statistic.GetAllTimeTransfered()) + wxT(")");
	SetItem( itemnr, 8, buffer );

	if ( file->m_nCompleteSourcesCountLo == 0 ) {
		if ( file->m_nCompleteSourcesCountHi ) {
			buffer.Printf(wxT("< %u"), file->m_nCompleteSourcesCountHi );
		} else {
			buffer = wxT("0");
		}
	} else if (file->m_nCompleteSourcesCountLo == file->m_nCompleteSourcesCountHi) {
		buffer.Printf(wxT("%u"), file->m_nCompleteSourcesCountLo);
	} else {
		buffer.Printf(wxT("%u - %u"), file->m_nCompleteSourcesCountLo, file->m_nCompleteSourcesCountHi);
	}

	SetItem( itemnr, 10, buffer );

	if ( file->IsPartFile() ) {
		SetItem( itemnr, 11, _("[PartFile]") );
	} else {
		SetItem( itemnr, 11, file->GetFilePath() );
	}
}


void CSharedFilesCtrl::ShowFile(CKnownFile* file)
{
	long newitem = InsertItem( GetInsertPos((long)file), file->GetFileName() );
	SetItemData( newitem, (long)file );
	
	// set background...
	wxListItem myitem;
	myitem.m_itemId = newitem;
	myitem.SetBackgroundColour(SYSCOLOR(wxSYS_COLOUR_LISTBOX));
	
	SetItem( myitem );
	
	UpdateFile( file, newitem );
	
	ShowFilesCount();
}


void CSharedFilesCtrl::OnSetPermissions( wxCommandEvent& event )
{
	int permission = 0;
	bool warned = false;

	// Sainity checks
	switch ( event.GetId() ) {
		case MP_PERMNONE: permission = PERM_NOONE; 			break;
		case MP_PERMFRIENDS:	permission = PERM_FRIENDS;	break;
		case MP_PERMALL:		permission = PERM_ALL;		break;
	}


	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	while( index > -1 ) {
		CKnownFile* file = (CKnownFile*)GetItemData( index );

		if ( file->IsPartFile() ) {
			if ( !warned ) { 
				// Only show the message once
				warned = true;

				wxMessageBox( _("You cannot change permissions while a file is still downloading!"),_("Unable to change permissions"),wxOK,(wxWindow*)theApp.amuledlg);
			}
		} else {
			CoreNotify_KnownFile_Perm_Set( file, permission );

			UpdateFile( file, index );
		}
		
		index = GetNextItem( index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}
}


void CSharedFilesCtrl::OnSetPriority( wxCommandEvent& event )
{
	int priority = 0;

	switch ( event.GetId() ) {
		case MP_PRIOVERYLOW:	priority = PR_VERYLOW;	break;
		case MP_PRIOLOW:		priority = PR_LOW;		break;
		case MP_PRIONORMAL:		priority = PR_NORMAL;	break;
		case MP_PRIOHIGH:		priority = PR_HIGH;		break;
		case MP_PRIOVERYHIGH:	priority = PR_VERYHIGH;	break;
		case MP_POWERSHARE:		priority = PR_POWERSHARE; break;
	}

	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	while( index > -1 ) {
		CKnownFile* file = (CKnownFile*)GetItemData( index );
		CoreNotify_KnownFile_Up_Prio_Set( file, priority );

		UpdateFile( file, index );

		index = GetNextItem( index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}
}


void CSharedFilesCtrl::OnSetPriorityAuto( wxCommandEvent& WXUNUSED(event) )
{
	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	while( index > -1 ) {
		CKnownFile* file = (CKnownFile*)GetItemData( index );
		CoreNotify_KnownFile_Up_Prio_Auto(file);

		UpdateFile( file, index );

		index = GetNextItem( index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}
}


void CSharedFilesCtrl::OnCreateURI( wxCommandEvent& event )
{
	wxString URIs;	

	if ( event.GetId() == MP_GETSOURCEED2KLINK ) {
		if ( !theApp.serverconnect->IsConnected() || theApp.serverconnect->IsLowID() ) {
			wxMessageBox(_("You need a HighID to create a valid sourcelink"),_("Unable to create ED2k link"),wxOK, (wxWindow*)theApp.amuledlg);

			return;
		}
	}

	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	while( index > -1 ) {
		CKnownFile* file = (CKnownFile*)GetItemData( index );

		switch ( event.GetId() ) {
			case MP_GETED2KLINK:				URIs += theApp.CreateED2kLink( file ) + wxT("\n");					break;
			case MP_GETHTMLED2KLINK:			URIs += theApp.CreateHTMLED2kLink( file ) + wxT("\n");				break;
			case MP_GETSOURCEED2KLINK:			URIs += theApp.CreateED2kSourceLink( file ) + wxT("\n");			break;
			case MP_GETHOSTNAMESOURCEED2KLINK:	URIs += theApp.CreateED2kHostnameSourceLink( file ) + wxT("\n");	break;
		}

		index = GetNextItem( index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	}
	
	if ( !URIs.IsEmpty() ) {	
		theApp.CopyTextToClipboard( URIs.RemoveLast() );
	}
}


void CSharedFilesCtrl::OnEditComment( wxCommandEvent& WXUNUSED(event) )
{
	long index = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	
	if ( index > -1 ) {
		CKnownFile* file = (CKnownFile*)GetItemData( index );

		CCommentDialog dialog( (wxWindow*)theApp.amuledlg, file );
	
		dialog.ShowModal();
	}
}

void CSharedFilesCtrl::OnGetComment ( wxCommandEvent& WXUNUSED(event) )
	{
			int item = GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
	        if ( item == -1 )
		    return;
	
	CKnownFile* file = (CKnownFile*)GetItemData( item );
	
	theApp.amuledlg->LaunchUrl(wxT("http://jugle.net/?comments=") + file->GetFileHash().Encode());
}
	
	
int CSharedFilesCtrl::SortProc( long item1, long item2, long sortData )
{
	CKnownFile* file1 = (CKnownFile*)item1;
	CKnownFile* file2 = (CKnownFile*)item2;

	int mod = 1;
	if ( IsOffsetDec( sortData ) ) {
		// Sorting is decending
		mod = -1;

		// We only handle the ascending cases
		sortData -= 1000;
	}


	switch ( sortData ) {
		// Sort by filename. Ascending.
		case  0: return mod * file1->GetFileName().CmpNoCase( file2->GetFileName() );
		
		// Sort by filesize. Ascending.
		case  1: return mod * CmpAny( file1->GetFileSize(), file2->GetFileSize() );

		// Sort by filetype. Ascending.
		case  2: return mod * GetFiletypeByName(file1->GetFileName()).CmpNoCase(GetFiletypeByName( file2->GetFileName()) );

		// Sort by priority. Ascending.
		case  3: {
			int8 prioA = file1->GetUpPriority();
			int8 prioB = file2->GetUpPriority();

			// Work-around for PR_VERYLOW which has value 4. See KnownFile.h for that stupidity ...
			return mod * CmpAny( ( prioB != PR_VERYLOW ? prioB : -1 ), ( prioA != PR_VERYLOW ? prioA : -1 ) );
		}

		// Sort by permission. Ascending.
		case  4: return mod * CmpAny( file2->GetPermissions(), file1->GetPermissions() );

		// Sort by fileID. Ascending.
		case  5: return mod * file1->GetFileHash().Encode().Cmp( file2->GetFileHash().Encode() );

		// Sort by Requests this session. Ascending.
		case  6: return mod * CmpAny( file1->statistic.GetRequests(), file2->statistic.GetRequests() );

		// Sort by accepted requests. Ascending.
		case  7: return mod * CmpAny( file1->statistic.GetAccepts(), file2->statistic.GetAccepts() );

		// Sort by transferred. Ascending.
		case  8: return mod * CmpAny( file1->statistic.GetTransfered(), file2->statistic.GetTransfered() );

		// Complete sources asc
		case 10: return mod * CmpAny( file1->m_nCompleteSourcesCount, file2->m_nCompleteSourcesCount );

		// Folders ascending
		case 11: {
			if ( file1->IsPartFile() && file2->IsPartFile() )
				return mod *  0;
			if ( file1->IsPartFile() )
				return mod * -1;
			if ( file2->IsPartFile() )
				return mod *  1;

			return mod * file1->GetFilePath().Cmp( file2->GetFilePath() );
		}
		
		// Sort by requests (All). Ascending.
		case 2006: return mod * CmpAny( file1->statistic.GetAllTimeRequests(), file2->statistic.GetAllTimeRequests() );

		// Sort by accepted requests (All). Ascending.
		case 2007: return mod * CmpAny( file1->statistic.GetAllTimeAccepts(), file2->statistic.GetAllTimeAccepts() );

		// Sort by transferred (All). Ascending.
		case 2008: return mod * CmpAny( file1->statistic.GetAllTimeTransfered(), file2->statistic.GetAllTimeTransfered() );
	
		default:
			return 0;
	}
}


void CSharedFilesCtrl::UpdateItem(CKnownFile* toupdate)
{
	long result = FindItem( -1, (long)toupdate );
	
	if ( result > -1 ) {
		UpdateFile(toupdate, result);

		if ( GetItemState( result, wxLIST_STATE_SELECTED ) ) {
			theApp.amuledlg->sharedfileswnd->SelectionUpdated();
		}
	}
}


void CSharedFilesCtrl::ShowFilesCount()
{
	wxString str = wxString::Format(_("Shared Files (%i)"), GetItemCount());
	CastByName( wxT("sharedFilesLabel"), GetParent(), wxStaticText )->SetLabel( str );
}


void CSharedFilesCtrl::OnDrawItem( int item, wxDC* dc, const wxRect& rect, const wxRect& rectHL, bool highlighted )
{
	CKnownFile *file = (CKnownFile*)GetItemData(item);
	wxASSERT( file );

	if ( highlighted ) {
		wxColour newcol;
		wxBrush hilBrush;

		if (GetFocus()) {
			newcol = SYSCOLOR(wxSYS_COLOUR_HIGHLIGHT);
			newcol = wxColour(G_BLEND(newcol.Red(),125),
			                  G_BLEND(newcol.Green(),125),
			                  G_BLEND(newcol.Blue(),125));
			hilBrush = wxBrush(newcol, wxSOLID);
			dc->SetBackground(hilBrush);
		} else {
			newcol = SYSCOLOR(wxSYS_COLOUR_BTNSHADOW);
			newcol = wxColour(G_BLEND(newcol.Red(),125),
			                  G_BLEND(newcol.Green(),125),
			                  G_BLEND(newcol.Blue(),125));
			hilBrush = wxBrush(newcol, wxSOLID);
			dc->SetBackground(hilBrush);
		}
		
		dc->SetTextForeground( SYSCOLOR(wxSYS_COLOUR_HIGHLIGHTTEXT));

		newcol = wxColour( G_BLEND(newcol.Red(), 65),
		                   G_BLEND(newcol.Green(), 65),
		                   G_BLEND(newcol.Blue(), 65) );
		dc->SetPen(wxPen(newcol,1,wxSOLID));
	} else {
		dc->SetBackground( wxBrush(SYSCOLOR(wxSYS_COLOUR_LISTBOX), wxSOLID) );
		dc->SetTextForeground(SYSCOLOR(wxSYS_COLOUR_WINDOWTEXT));
		dc->SetPen(*wxTRANSPARENT_PEN);
	}
	
	dc->SetBrush(dc->GetBackground());
	dc->DrawRectangle(rectHL);
	dc->SetPen(*wxTRANSPARENT_PEN);

	wxRect columnRect = rect;
	
	const int SPARE_PIXELS_HORZ	= 4;

	// Offset based on the height of the fonts
	const int textVOffset = ( rect.GetHeight() - GetFont().GetPointSize() ) / 2;

	columnRect.SetLeft( columnRect.GetLeft() + SPARE_PIXELS_HORZ );
	columnRect.SetWidth( columnRect.GetWidth()-2*SPARE_PIXELS_HORZ);
	for ( int i = 0; i < GetColumnCount(); ++i ) {
		wxListItem columnItem;
		GetColumn(i, columnItem);
		int width = columnItem.GetWidth();
		columnRect.SetWidth(width-2*SPARE_PIXELS_HORZ);

		if ( width ) {
			wxDCClipper clipper(*dc, columnRect);
					
			switch ( i ) {
				case 9: {
					if ( file->GetPartCount() ) {
						--columnRect.height;
						++columnRect.y;
						DrawAvailabilityBar(file, dc, columnRect );
						++columnRect.height;
						--columnRect.y;
					}
					break;
				}

				default: {
					columnItem.m_col = i;
					columnItem.m_itemId = item;
					GetItem(columnItem);
					dc->DrawText(columnItem.m_text, columnRect.GetLeft(), columnRect.GetTop() + textVOffset );
					break;
				}
			}
		}

		columnRect.SetLeft(columnRect.GetLeft()+width);
	}
}


bool CSharedFilesCtrl::AltSortAllowed( int column )
{
	switch ( column ) {
		case 6:
		case 7:
		case 8:
			return true;

		default:
			return false;
	}
}


void CSharedFilesCtrl::DrawAvailabilityBar(CKnownFile* file, wxDC* dc, wxRect rect ) const
{
	// Reference to the availability list
	const ArrayOfUInts16& list = ( file->IsPartFile() ? ((CPartFile*)file)->m_SrcpartFrequency : file->m_AvailPartFrequency );
	
	static CBarShader s_ChunkBar;

	s_ChunkBar.SetFileSize( file->GetFileSize() );
	s_ChunkBar.SetHeight( rect.GetHeight() );
	s_ChunkBar.SetWidth( rect.GetWidth() );
	s_ChunkBar.Set3dDepth( CPreferences::Get3DDepth() );
	s_ChunkBar.Fill( RGB(255, 0, 0) );

	for ( unsigned int i = 0; i < list.GetCount(); ++i ) {
		if ( list[i] ) {
			COLORREF color = RGB(0, (210-(22*( list[i] - 1 ) ) < 0) ? 0 : 210-(22*( list[i] - 1 ) ), 255);
			s_ChunkBar.FillRange(PARTSIZE*(i),PARTSIZE*(i+1),color);
		}
	}

   	s_ChunkBar.Draw(dc, rect.GetLeft(), rect.GetTop(), CPreferences::UseFlatBar() ); 
}
