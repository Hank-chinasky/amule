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

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "ChatWnd.h"
#endif

#include <wx/defs.h>		// Needed before any other wx/*.h

#include <wx/settings.h>	// Needed for wxSYS_COLOUR_WINDOW
#include <wx/sizer.h>
#include <wx/menu.h>
#include <wx/accel.h>
#include "ChatWnd.h"		// Interface declarations.
#include "amule.h"			// Needed for theApp
#include "amuleDlg.h"		// Needed for CamuleDlg
#include "FriendListCtrl.h"	// Needed for CFriendListCtrl
#include "updownclient.h"	// Needed for CUpDownClient
#include "ChatSelector.h"	// Needed for CChatSelector
#include "muuli_wdr.h"		// Needed for messagePage
#include "Color.h"			// Needed for GetColour
#include "OPCodes.h"
#include "OtherFunctions.h"

BEGIN_EVENT_TABLE(CChatWnd, wxPanel)
	EVT_BUTTON(IDC_CSEND, CChatWnd::OnBnClickedCsend)
	EVT_BUTTON(IDC_CCLOSE, CChatWnd::OnBnClickedCclose)
	EVT_TEXT_ENTER(IDC_CMESSAGE, CChatWnd::OnBnClickedCsend)
END_EVENT_TABLE()


CChatWnd::CChatWnd(wxWindow* pParent)
: wxPanel(pParent, -1)
{
	wxSizer* content = messagePage(this, true);
	content->Show(this, true);

	chatselector = CastChild( IDC_CHATSELECTOR, CChatSelector );
	friendlist   = CastChild( ID_FRIENDLIST, CFriendListCtrl );
}

#ifdef CLIENT_GUI
/*
 * This GUI is not functional (yet, if ever). So functions are replaced with their stubs
 * for compile/link to work
 */
void CChatWnd::OnBnClickedCsend(wxCommandEvent& WXUNUSED(evt))
{
}

void CChatWnd::OnBnClickedCclose(wxCommandEvent& WXUNUSED(evt))
{
}

#else

void CChatWnd::StartSession(CUpDownClient* client, bool setfocus)
{
	if ( !client->GetUserName().IsEmpty() ) {
		if (setfocus) {
			theApp.amuledlg->SetActiveDialog(CamuleDlg::ChatWnd, this);
		}
		chatselector->StartSession(client, true);
	}
}


void CChatWnd::OnBnClickedCsend(wxCommandEvent& WXUNUSED(evt))
{
	wxString message = CastChild(IDC_CMESSAGE, wxTextCtrl)->GetValue();
	SendMessage(message);
}


void CChatWnd::OnBnClickedCclose(wxCommandEvent& WXUNUSED(evt))
{
	chatselector->EndSession();
}


CFriend* CChatWnd::FindFriend(const CMD4Hash& userhash, uint32 dwIP, uint16 nPort)
{
	return friendlist->FindFriend(userhash, dwIP, nPort);
}


void CChatWnd::AddFriend(CUpDownClient* toadd)
{
	friendlist->AddFriend(toadd);
}


void CChatWnd::AddFriend(const CMD4Hash& userhash, uint32 lastSeen, uint32 lastUsedIP, uint32 lastUsedPort, uint32 lastChatted, wxString name, uint32 hasHash)
{
	friendlist->AddFriend( userhash, lastSeen, lastUsedIP, lastUsedPort, lastChatted, name, hasHash);
}


void CChatWnd::RefreshFriend(CFriend* toupdate)
{
	friendlist->RefreshFriend(toupdate);
	chatselector->RefreshFriend(toupdate);
}

void CChatWnd::ProcessMessage(CUpDownClient* sender, const wxString& message)
{
	chatselector->ProcessMessage(sender, message);
}

void CChatWnd::ConnectionResult(CUpDownClient* sender, bool success)
{
	chatselector->ConnectionResult(sender, success);
}

void CChatWnd::SendMessage(const wxString& message)
{
	if (chatselector->SendMessage( message )) {
		CastChild(IDC_CMESSAGE, wxTextCtrl)->Clear();
	}

	CastChild(IDC_CMESSAGE, wxTextCtrl)->SetFocus();
}
#endif
