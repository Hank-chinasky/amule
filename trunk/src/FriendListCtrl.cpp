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

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "FriendListCtrl.h"
#endif

#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/intl.h>		// Needed for _
#include <wx/menu.h>		// Needed for wxMenu

#include "amule.h"			// Needed for theApp: let it first or fail under win32
#include "amuleDlg.h"		// Needed for CamuleDlg
#include "FriendListCtrl.h"	// Interface declarations
#include "ClientDetailDialog.h"	// Needed for CClientDetailDialog
#include "AddFriend.h"		// Needed for CAddFriend
#include "ClientList.h"		// Needed for CClientList
#include "ChatWnd.h"		// Needed for CChatWnd
#include "OPCodes.h"		// Needed for MP_DETAIL
#include "updownclient.h"	// Needed for CUpDownClient
#include "Friend.h"		// Needed for CFriend
#include "ArchSpecific.h"
#include "OtherFunctions.h"
#include "muuli_wdr.h"
#include "SafeFile.h"
#include "Logger.h"

#warning REMOVE WHEN EC IS CODED!
#include "FriendList.h"		// Needed for the friends list

BEGIN_EVENT_TABLE(CFriendListCtrl, CMuleListCtrl)
	EVT_RIGHT_DOWN(CFriendListCtrl::OnNMRclick)
	EVT_LIST_ITEM_SELECTED(ID_FRIENDLIST, CFriendListCtrl::OnItemSelected)
	EVT_LIST_ITEM_ACTIVATED(ID_FRIENDLIST, CFriendListCtrl::OnItemActivated) 
	
	EVT_MENU(MP_MESSAGE, CFriendListCtrl::OnPopupMenu)
	EVT_MENU(MP_REMOVEFRIEND, CFriendListCtrl::OnPopupMenu)
	EVT_MENU(MP_ADDFRIEND, CFriendListCtrl::OnPopupMenu)
	EVT_MENU(MP_DETAIL, CFriendListCtrl::OnPopupMenu)
	EVT_MENU(MP_SHOWLIST, CFriendListCtrl::OnPopupMenu)
	EVT_MENU(MP_FRIENDSLOT, CFriendListCtrl::OnPopupMenu)
END_EVENT_TABLE()


CDlgFriend::CDlgFriend(const CMD4Hash& hash, const wxString& name, uint32 ip, uint16 port, bool IsLinked, bool HasFriendSlot) {
	m_hash = hash;
	m_name = name;
	m_ip = ip;
	m_port = port;
	islinked = IsLinked;
	hasfriendslot = HasFriendSlot;
}


CFriendListCtrl::CFriendListCtrl(wxWindow* parent, int id, const wxPoint& pos, wxSize siz, int flags)
: CMuleListCtrl(parent, id, pos, siz, flags)
{
  InsertColumn(0, _("Username"), wxLIST_FORMAT_LEFT, siz.GetWidth() - 4);

  LoadList();
}

CFriendListCtrl::~CFriendListCtrl()
{
	for ( int i = 0; i < GetItemCount(); i++ )  {
		delete (CDlgFriend*)GetItemData(i);
	}
}


void CFriendListCtrl::AddFriend(CDlgFriend* toadd, bool send_to_core)
{
	uint32 itemnr = InsertItem(GetItemCount(), toadd->m_name);
	SetItemData(itemnr, (long)toadd);
	
	#warning CORE/GUI
	if (send_to_core) {
	#ifndef CLIENT_GUI
		theApp.friendlist->AddFriend(toadd->m_hash, 0, toadd->m_ip, toadd->m_port, 0,toadd->m_name);
	#endif		
	}
}


void CFriendListCtrl::AddFriend(const CMD4Hash& userhash, const wxString& name, uint32 lastUsedIP, uint32 lastUsedPort, bool IsLinked, bool HasFriendSlot, bool send_to_core)
{
	CDlgFriend* NewFriend = new CDlgFriend( userhash, name, lastUsedIP, lastUsedPort, IsLinked, HasFriendSlot);

	AddFriend( NewFriend, send_to_core);
}


void CFriendListCtrl::AddFriend(CUpDownClient* toadd)
{
	if ( toadd->IsFriend() ) {
		return;
	}
	
	#warning CORE/GUI
	// This links the friend to the client also
	#ifndef CLIENT_GUI
	theApp.friendlist->AddFriend(toadd);
	#endif
	
	CDlgFriend* NewFriend = new CDlgFriend( toadd->GetUserHash(), toadd->GetUserName(), toadd->GetIP(), toadd->GetUserPort(), true, false);
	
	AddFriend( NewFriend, false/*already sent to core*/ );
}


void CFriendListCtrl::RemoveFriend(CDlgFriend* toremove)
{
	sint32 itemnr = FindItem(-1, (long)toremove);
	
	if ( itemnr == -1 )
		return;
	
	#warning CORE/GUI
	#ifndef CLIENT_GUI
	theApp.friendlist->RemoveFriend(toremove->m_hash, toremove->m_ip, toremove->m_port);
	#endif
	
	DeleteItem(itemnr);
}


void CFriendListCtrl::RefreshFriend(CDlgFriend* toupdate)
{
	sint32 itemnr = FindItem(-1, (long)toupdate);
	if (itemnr != -1) {
		SetItem(itemnr, 0, toupdate->m_name);
	}	
	#warning CORE/GUI
	#ifndef CLIENT_GUI
	theApp.friendlist->UpdateFriendName(toupdate->m_hash, toupdate->m_name, toupdate->m_ip, toupdate->m_port);
	#endif
}


void CFriendListCtrl::OnItemSelected(wxListEvent& WXUNUSED(evt))
{
	// Force the list to refresh. For some reason, Refresh() wont work on its own...
	Freeze();
	Refresh(true);
	Thaw(); 
}


void CFriendListCtrl::OnItemActivated(wxListEvent& WXUNUSED(evt))
{
	int cursel = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	
	CDlgFriend* cur_friend = (CDlgFriend*)GetItemData(cursel);
	
	/* ignore this one, it is not activated anymore :) */
	if (cur_friend == NULL) {
		return;
	}

	theApp.amuledlg->chatwnd->StartSession(cur_friend);
	
}


void CFriendListCtrl::LoadList()
{
	#warning EC: ASK THE LIST TO CORE!
	
	#ifndef CLIENT_GUI
	for(FriendList::iterator it = theApp.friendlist->m_FriendList.begin(); it != theApp.friendlist->m_FriendList.end(); ++it) {
		CFriend* core_friend = *it;
		AddFriend(core_friend->GetUserHash(), core_friend->GetName(), core_friend->GetIP(), core_friend->GetPort(), core_friend->GetLinkedClient(), core_friend->HasFriendSlot(), false);
	}
	#endif
	
}

CDlgFriend* CFriendListCtrl::FindFriend(const CMD4Hash& userhash, uint32 dwIP, uint16 nPort)
{
	for ( int i = 0; i < GetItemCount(); i++ ) {
		CDlgFriend* cur_friend = (CDlgFriend*)GetItemData(i);
		
		// to avoid that unwanted clients become a friend, we have to distinguish between friends with
		// a userhash and of friends which are identified by IP+port only.
		if ( !userhash.IsEmpty() && !cur_friend->m_hash.IsEmpty() ) {
			// check for a friend which has the same userhash as the specified one
			if (cur_friend->m_hash == userhash) {
				return cur_friend;
			}
		}
		else if (cur_friend->m_ip == dwIP && cur_friend->m_port == nPort) {
				return cur_friend;
		}
	}
	
	return NULL;
}

bool CFriendListCtrl::IsAlreadyFriend( uint32 dwLastUsedIP, uint32 nLastUsedPort )
{
	return FindFriend( NULL, dwLastUsedIP, nLastUsedPort );
}

void CFriendListCtrl::OnNMRclick(wxMouseEvent& evt)
{
	CDlgFriend* cur_friend = NULL;
	
	wxMenu* menu = new wxMenu(_("Friends"));
	int cursel = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	
	if ( cursel != -1 ) {
		cur_friend = (CDlgFriend*)GetItemData(cursel);
		menu->Append(MP_DETAIL, _("Show &Details"));
		menu->Enable(MP_DETAIL, cur_friend->islinked);
	}
	
	menu->Append(MP_ADDFRIEND, _("Add a friend"));
	
	if (cursel != (-1)) {
		menu->Append(MP_REMOVEFRIEND, _("Remove Friend"));
		menu->Append(MP_MESSAGE, _("Send &Message"));
		menu->Append(MP_SHOWLIST, _("View Files"));
		menu->AppendCheckItem(MP_FRIENDSLOT, _("Establish Friend Slot"));
		
		if (cur_friend->islinked) {
			menu->Enable(MP_FRIENDSLOT, true);
			menu->Check(MP_FRIENDSLOT, cur_friend->hasfriendslot);
		} else {
			menu->Enable(MP_FRIENDSLOT, false);
		}
	}
	
	PopupMenu(menu, evt.GetPosition());
}


void CFriendListCtrl::OnPopupMenu(wxCommandEvent& evt)
{
	int cursel = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	
	CDlgFriend* cur_friend = NULL;
	
	if (cursel != -1) {
		cur_friend = (CDlgFriend*)GetItemData(cursel);
	}
	
	switch (evt.GetId()) {
		case MP_MESSAGE: {
			theApp.amuledlg->chatwnd->StartSession(cur_friend);			
			#warning CORE/GUI!			
			#ifndef CLIENT_GUI
			theApp.friendlist->StartChatSession(cur_friend->m_hash, cur_friend->m_ip, cur_friend->m_port);
			#endif
			break;
		}
		
		case MP_REMOVEFRIEND: {
			RemoveFriend(cur_friend);
			break;
		}
		
		case MP_ADDFRIEND: {
			CAddFriend* dialog2 = new CAddFriend(this);
			dialog2->ShowModal();			
			delete dialog2;			
			break;
		}
		
		case MP_DETAIL: {
			if (cur_friend->islinked) {
				#warning EC: We need a reply packet with a full CUpDownClient
				#ifndef CLIENT_GUI
				CClientDetailDialog* dialog = new CClientDetailDialog(this, theApp.friendlist->FindFriend(cur_friend->m_hash, cur_friend->m_ip, cur_friend->m_port)->GetLinkedClient());
				dialog->ShowModal();
				delete dialog;
				#endif
			}
			break;
		}
		
		case MP_SHOWLIST: {
			#warning CORE/GUI!
			#ifndef CLIENT_GUI
			theApp.friendlist->RequestSharedFileList(cur_friend->m_hash, cur_friend->m_ip, cur_friend->m_port);
			#endif
			break;
		}
		
		case MP_FRIENDSLOT: {
			#warning CORE/GUI!
			#ifndef CLIENT_GUI
			theApp.friendlist->ToogleFriendSlot(cur_friend->m_hash, cur_friend->m_ip, cur_friend->m_port);
			#endif
			break;
		}
	}
}
