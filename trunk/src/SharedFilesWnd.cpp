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
#pragma implementation "SharedFilesWnd.h"
#endif

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/gauge.h>

#include "SharedFilesWnd.h"	// Interface declarations
#include "SharedFilesCtrl.h"
#include "muuli_wdr.h"		// Needed for ID_SHFILELIST
#include "otherfunctions.h"	// Needed for CastItoXBytes
#include "KnownFileList.h"	// Needed for CKnownFileList
#include "KnownFile.h"		// Needed for CKnownFile
#include "SharedFileList.h"	// Needed for CSharedFileList
#include "amule.h"			// Needed for theApp



BEGIN_EVENT_TABLE(CSharedFilesWnd, wxPanel)
	EVT_LIST_ITEM_SELECTED( ID_SHFILELIST,	CSharedFilesWnd::OnItemActivated )
	EVT_BUTTON( ID_BTNRELSHARED,			CSharedFilesWnd::OnBtnReloadShared )
END_EVENT_TABLE()



CSharedFilesWnd::CSharedFilesWnd( wxWindow* pParent )
	: wxPanel(pParent, -1)
{
	wxSizer* content = sharedfilesDlg(this, true);
	content->Show(this, true);

	m_bar_requests	= CastChild( wxT("popbar"), wxGauge );
	m_bar_accepted	= CastChild( wxT("popbarAccept"), wxGauge );
	m_bar_transfer	= CastChild( wxT("popbarTrans"), wxGauge );
	sharedfilesctrl = CastChild( wxT("sharedFilesCt"), CSharedFilesCtrl );
}


CSharedFilesWnd::~CSharedFilesWnd()
{
}


void CSharedFilesWnd::SelectionUpdated()
{
	m_bar_requests->SetRange( theApp.knownfiles->requested );
	m_bar_accepted->SetRange( theApp.knownfiles->accepted );
	m_bar_transfer->SetRange( theApp.knownfiles->transfered / 1024 );
	
	if ( !sharedfilesctrl->GetSelectedItemCount() ) {
		// Requests
		m_bar_requests->SetValue( 0 );
	
		CastChild(IDC_SREQUESTED, wxStaticText)->SetLabel( wxT("-") );
		CastChild(IDC_SREQUESTED2, wxStaticText)->SetLabel( wxT("-") );

		// Accepted requets
		m_bar_accepted->SetValue( 0 );
	
		CastChild(IDC_SACCEPTED, wxStaticText)->SetLabel( wxT("-") );
		CastChild(IDC_SACCEPTED2, wxStaticText)->SetLabel( wxT("-") );


		// Transferred
		m_bar_transfer->SetValue( 0 );
	
		CastChild(IDC_STRANSFERED, wxStaticText)->SetLabel( wxT("-") );
		CastChild(IDC_STRANSFERED2, wxStaticText)->SetLabel( wxT("-") );
	} else {
		// Create a total statistic for the selected item(s)
		uint32 session_requests = 0;
		uint32 session_accepted = 0;
		uint64 session_transfered = 0;
		
		uint32 all_requests = 0;
		uint32 all_accepted = 0;
		uint64 all_transfered = 0;
		
		long index = sharedfilesctrl->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );

		while ( index > -1 ) {
			if ( sharedfilesctrl->GetItemState( index, wxLIST_STATE_SELECTED ) ) {
				CKnownFile* file = (CKnownFile*)sharedfilesctrl->GetItemData( index );
			
				session_requests	+= file->statistic.GetRequests();
				session_accepted	+= file->statistic.GetAccepts();
				session_transfered	+= file->statistic.GetTransfered();
		
				all_requests	+= file->statistic.GetAllTimeRequests();
				all_accepted	+= file->statistic.GetAllTimeAccepts();
				all_transfered	+= file->statistic.GetAllTimeTransfered();
			}
			
			index = sharedfilesctrl->GetNextItem( index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		};
		
		
		// Requests
		m_bar_requests->SetValue( session_requests );
	
		CastChild(IDC_SREQUESTED, wxStaticText)->SetLabel( wxString::Format(wxT("%u"), session_requests ) );
		CastChild(IDC_SREQUESTED2, wxStaticText)->SetLabel( wxString::Format(wxT("%u"), all_requests ) );

	
		// Accepted requets
		m_bar_accepted->SetValue( session_accepted );
	
		CastChild(IDC_SACCEPTED, wxStaticText)->SetLabel( wxString::Format(wxT("%u"), session_accepted ) );
		CastChild(IDC_SACCEPTED2, wxStaticText)->SetLabel( wxString::Format(wxT("%u"), all_accepted ) );


		// Transferred
		m_bar_transfer->SetValue( session_transfered / 1024 );
	
		CastChild(IDC_STRANSFERED, wxStaticText)->SetLabel( otherfunctions::CastItoXBytes( session_transfered ) );
		CastChild(IDC_STRANSFERED2, wxStaticText)->SetLabel( otherfunctions::CastItoXBytes( all_transfered ) );
	}
	
	Layout();
}


void CSharedFilesWnd::OnBtnReloadShared( wxCommandEvent& WXUNUSED(evt) )
{
	theApp.sharedfiles->Reload(true, false);
	SelectionUpdated();
}


void CSharedFilesWnd::OnItemActivated(wxListEvent& WXUNUSED(evt))
{
	SelectionUpdated();
}
