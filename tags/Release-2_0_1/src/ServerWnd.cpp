//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2005 aMule Team ( admin@amule.org / http://www.amule.org )
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

//
// ServerWnd.cpp : implementation file
//


#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "ServerWnd.h"
#endif

#include <wx/settings.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>

#include "muuli_wdr.h"		// Needed for ID_ADDTOLIST
#include "ServerWnd.h"		// Interface declarations.
#include "GetTickCount.h"	// Needed for GetTickCount
#include "Server.h"		// Needed for CServer
#include "ServerList.h"		// Needed for CServerList
#include "ServerListCtrl.h"	// Needed for CServerListCtrl
#include "OtherFunctions.h"	// Needed for GetTickCount
#include "Preferences.h"	// Needed for CPreferences
#include "ServerConnect.h"
#include "NetworkFunctions.h" // Needed for Uint32_16toStringIP_Port
#include "amuleDlg.h"		// Needed for CamuleDlg
#include "amule.h"			// Needed for theApp
#include "StringFunctions.h" // Needed for StrToULong
#include "Logger.h"


BEGIN_EVENT_TABLE(CServerWnd,wxPanel)
	EVT_BUTTON(ID_ADDTOLIST,CServerWnd::OnBnClickedAddserver)
	EVT_BUTTON(ID_UPDATELIST,CServerWnd::OnBnClickedUpdateservermetfromurl)
	EVT_TEXT_ENTER(IDC_SERVERLISTURL,CServerWnd::OnBnClickedUpdateservermetfromurl)
	EVT_BUTTON(ID_BTN_RESET, CServerWnd::OnBnClickedResetLog)
	EVT_BUTTON(ID_BTN_RESET_SERVER, CServerWnd::OnBnClickedResetServerLog)
	EVT_SPLITTER_SASH_POS_CHANGED(ID_SRV_SPLITTER,CServerWnd::OnSashPositionChanged)
END_EVENT_TABLE()

	
CServerWnd::CServerWnd(wxWindow* pParent /*=NULL*/, int splitter_pos)
: wxPanel(pParent, -1)
{
	wxSizer* sizer = serverListDlg(this,TRUE);

	// init serverlist
	// no use now. too early.
	CServerListCtrl* list= CastChild( ID_SERVERLIST, CServerListCtrl );
	serverlistctrl=list;

	CastChild( ID_SRV_SPLITTER, wxSplitterWindow )->SetSashPosition(splitter_pos, true);
	
	// Insert two columns, currently without a header
	wxListCtrl* MyInfoList = CastChild( ID_MYSERVINFO, wxListCtrl );
	MyInfoList->InsertColumn(0, wxEmptyString);
	MyInfoList->InsertColumn(1, wxEmptyString);
	
	sizer->Show(this,TRUE);
}


CServerWnd::~CServerWnd()
{
}


void CServerWnd::UpdateServerMetFromURL(const wxString& strURL)
{
	theApp.serverlist->UpdateServerMetFromURL(strURL);
}


void CServerWnd::OnBnClickedAddserver(wxCommandEvent& WXUNUSED(evt))
{
	wxString servername = CastChild( IDC_SERVERNAME, wxTextCtrl )->GetValue();
	wxString serveraddr = CastChild( IDC_IPADDRESS, wxTextCtrl )->GetValue();
	long port = StrToULong( CastChild( IDC_SPORT, wxTextCtrl )->GetValue() );

	if ( serveraddr.IsEmpty() ) {
		AddLogLineM( true, _("Server not added: No IP or hostname specified."));
		return;
	}
	
	if ( port <= 0 || port > 65535 ) {
		AddLogLineM( true, _("Server not added: Invalid server-port specified."));
		return;
	}
  
	CServer* toadd = new CServer( port, serveraddr );
	toadd->SetListName( servername.IsEmpty() ? serveraddr : servername );
	
	if ( theApp.AddServer( toadd, true ) ) {
		CastChild( IDC_SERVERNAME, wxTextCtrl )->Clear();
		CastChild( IDC_IPADDRESS, wxTextCtrl )->Clear();
		CastChild( IDC_SPORT, wxTextCtrl )->Clear();
	} else {
		CServer* update = theApp.serverlist->GetServerByAddress(toadd->GetAddress(), toadd->GetPort());
		if ( update ) {
			update->SetListName(toadd->GetListName());
			serverlistctrl->RefreshServer(update);
		}
		delete toadd;
	}
	
	theApp.serverlist->SaveServerMet();
}


void CServerWnd::OnBnClickedUpdateservermetfromurl(wxCommandEvent& WXUNUSED(evt))
{
	wxString strURL = CastChild( IDC_SERVERLISTURL, wxTextCtrl )->GetValue();
	UpdateServerMetFromURL(strURL);
}


void CServerWnd::OnBnClickedResetLog(wxCommandEvent& WXUNUSED(evt))
{
	theApp.GetLog(true); // Reset it.
}


void CServerWnd::OnBnClickedResetServerLog(wxCommandEvent& WXUNUSED(evt))
{
	theApp.GetServerLog(true); // Reset it
}


void CServerWnd::UpdateMyInfo()
{
	wxListCtrl* MyInfoList = CastChild( ID_MYSERVINFO, wxListCtrl );
	
	MyInfoList->DeleteAllItems();
	MyInfoList->InsertItem(0, _("Status:"));

	if (theApp.serverconnect->IsConnected()) {
		MyInfoList->SetItem(0, 1, _("Connected"));

		// Connection data		
		
		MyInfoList->InsertItem(1, _("IP:Port"));
		MyInfoList->SetItem(1, 1, theApp.serverconnect->IsLowID() ? 
			 wxString(_("LowID")) : Uint32_16toStringIP_Port( theApp.serverconnect->GetClientID(), thePrefs::GetPort()));

		MyInfoList->InsertItem(2, _("ID"));
		// No need to test the server connect, it's already true
		MyInfoList->SetItem(2, 1, wxString::Format(wxT("%u"), theApp.serverconnect->GetClientID()));
		
		MyInfoList->InsertItem(3, wxEmptyString);		

		if (theApp.serverconnect->IsLowID()) {
			MyInfoList->SetItem(1, 1, _("Server")); // LowID, unknown ip
			MyInfoList->SetItem(3, 1, _("LowID"));
		} else {
			MyInfoList->SetItem(1, 1, Uint32_16toStringIP_Port(theApp.serverconnect->GetClientID(), thePrefs::GetPort()));
			MyInfoList->SetItem(3, 1, _("HighID"));
		}
		
	} else {
		// No data
		MyInfoList->SetItem(0, 1, _("Disconnected"));
	}

	// Fit the width of the columns
	MyInfoList->SetColumnWidth(0, -1);
	MyInfoList->SetColumnWidth(1, -1);
}


void CServerWnd::OnSashPositionChanged(wxSplitterEvent& WXUNUSED(evt))
{
	theApp.amuledlg->srv_split_pos = CastChild( wxT("SrvSplitterWnd"), wxSplitterWindow )->GetSashPosition();
}
