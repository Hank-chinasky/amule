// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
// Copyright (C) 2002 Tiku & Patrizio Bassi aka Hetfield <hetfield@email.it>
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

// this file will implement GNOME/KDE compatible system tray icon

#ifndef __WXMSW__
	#ifdef __BSD__
		#include <sys/types.h>
	#endif /* __BSD__ */

	#include <cstddef>			// Needed for NULL. Must be BEFORE gtk/gdk headers!
	#include <sys/socket.h>		//
	#include <netinet/in.h>		// Needed for inet_ntoa
	#include <net/if.h>			// Needed for struct ifreq
	#include <arpa/inet.h>		//
	#include <gtk/gtk.h>		// Needed for gtk_object_get_data
	#include <X11/Xatom.h>		// Needed for XA_WINDOW
#endif //end of msw

#include "pixmaps/mule_Tr_grey.ico.xpm"
#include "amuleDlg.h"		// Needed for CamuleDlg
#include "UploadListCtrl.h"	// Needed for CUploadListCtrl
#include "SysTray.h"		// Interface declarations.
#include "GetTickCount.h"	// Needed for GetTickCount
#include "DownloadQueue.h"	// Needed for GetKBps()
#include "UploadQueue.h"	// Needed for CUploadQueue
#include "SharedFileList.h"	// Needed for CSharedFileList
#include "server.h"		// Needed for GetListName
#include "otherfunctions.h"	// Needed for EncodeBase16
#include "sockets.h"		// Needed for CServerConnect
#include "SharedFilesCtrl.h"	// Needed for CSharedFilesCtrl
#include "SharedFilesWnd.h"	// Needed for CSharedFilesWnd
#include "ServerListCtrl.h"	// Needed for CServerListCtrl
#include "ServerWnd.h"		// Needed for CServerWnd
#include "DownloadListCtrl.h"	// Needed for CDownloadListCtrl
#include "TransferWnd.h"	// Needed for CTransferWnd
#include "opcodes.h"		// Needed for UNLIMITED
#include "Preferences.h"	// Needed for CPreferences
#include "amule.h"		// Needed for theApp

#ifdef __WXGTK__
	#include "eggtrayicon.h"	// Needed for egg_tray_icon_new
#endif

#ifdef __WXMSW__


CSysTray::CSysTray(wxWindow* WXUNUSED(parent), int WXUNUSED(desktopmode), const wxString& WXUNUSED(title))
{
}


void CSysTray::SetTrayToolTip(const wxString& tip)
{
	SetIcon(c, tip);
}


void CSysTray::SetTrayIcon(char** data, int* WXUNUSED(pVals))
{
	SetIcon(wxIcon(data));
	c=wxIcon(data);
}


#endif // __WXMSW__

#ifdef __WXGTK__ // Only use this code in wxGTK since it uses GTK code.

// Terminate aMule
void close_amule()
{
	wxCloseEvent SendCloseEvent;
	theApp.amuledlg->OnClose(SendCloseEvent);
}


// Hide aMule
void do_hide()
{
	theApp.amuledlg->Hide_aMule();
}


// Show aMule and bring it to front
void do_show()
{
	theApp.amuledlg->Show_aMule();
}


// Connect to a server
void connect_any_server()
{
	if ( theApp.serverconnect->IsConnected() ) {
		theApp.serverconnect->Disconnect();
	} else if ( !theApp.serverconnect->IsConnecting() ) {
		AddLogLineM(true, _("Connecting"));
	
		theApp.serverconnect->ConnectToAnyServer();
	
		theApp.amuledlg->ShowConnectionState(false);
	}
}


// Disconnect
void disconnect()
{
	if ( theApp.serverconnect->IsConnected() )
		theApp.serverconnect->Disconnect();
}


// Set download speed
void set_dl_speed(GtkWidget* widget, GdkEventButton* WXUNUSED(event), gpointer WXUNUSED(data))
{
	unsigned int temp = (unsigned int)gtk_object_get_data (GTK_OBJECT(widget), "label");
	
	theApp.glob_prefs->SetMaxDownload(temp);
}


// Set upload speed
void set_ul_speed(GtkWidget* widget, GdkEventButton* WXUNUSED(event), gpointer WXUNUSED(data))
{
	unsigned int temp = (unsigned int)gtk_object_get_data (GTK_OBJECT(widget), "label");

	theApp.glob_prefs->SetMaxUpload(temp);
}


// Create menu linked to the tray icon
static gboolean tray_menu (GtkWidget* WXUNUSED(widget), GdkEventButton* event, gpointer WXUNUSED(data))
{
	GtkWidget* item = NULL;
	GtkWidget* info_item = NULL;

	
	// Main menu
	GtkWidget* status_menu = gtk_menu_new();
	gtk_menu_set_title(GTK_MENU(status_menu), StringToSystray(_("aMule Tray Menu")));


	// A few stats: Version, Limits and current speeds
	{
		wxString label = MOD_VERSION_LONG wxT(":\n");
		label += wxString::Format(_("Download Speed: %.1f\n"), theApp.downloadqueue->GetKBps());
		label += wxString::Format(_("Upload Speed: %.1f\n"), theApp.uploadqueue->GetKBps());
		label += _("\nSpeed Limits:\n");
	
		// Check for upload limits
		int max_upload = theApp.glob_prefs->GetMaxUpload();
		if ( max_upload == UNLIMITED ) {
			label += wxString::Format( _("UL: None, "));
		} else {
			label += wxString::Format( _("UL: %d, "), max_upload);
		}
	
		// Check for download limits
		int max_download = theApp.glob_prefs->GetMaxDownload();
		if ( max_download == UNLIMITED ) {
			label += wxString::Format( _("DL: None"));
		} else {
			label += wxString::Format( _("DL: %d"), max_download);
		}

		item = gtk_menu_item_new_with_label( StringToSystray( label ) );
		gtk_container_add (GTK_CONTAINER (status_menu), item);
	}
	

	// Client-Info menu
	GtkWidget* info_menu = gtk_menu_new();
	gtk_menu_set_title(GTK_MENU(info_menu),StringToSystray(_("aMule Tray Menu Info")));


	//separator
	item = gtk_menu_item_new();
	gtk_container_add (GTK_CONTAINER (status_menu), item);


	//personal infos item, not linked, only to show them
	item=gtk_menu_item_new_with_label(StringToSystray(_("Client Information")));
	gtk_container_add (GTK_CONTAINER (status_menu), item);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),info_menu);


	// User nick-name
	{
		wxString temp = _("Nickname: ");
			
		if ( theApp.glob_prefs->GetUserNick().IsEmpty() )
			temp += _("No Nickname Selected!");
		else
			temp += theApp.glob_prefs->GetUserNick();
		
		info_item = gtk_menu_item_new_with_label( StringToSystray( temp ) );
		gtk_container_add(GTK_CONTAINER(info_menu), info_item);
	}

	
	// Client ID
	{
		wxString temp = wxT("ClientID: ");
		
		if (theApp.serverconnect->IsConnected()) {
			unsigned long id = theApp.serverconnect->GetClientID();
					
			temp += wxString::Format(wxT("%lu"), id);
		} else {
			temp += _("Not Connected");
		}

		info_item=gtk_menu_item_new_with_label( StringToSystray( temp ) );
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}


	// Current Server and Server IP
	{
		wxString temp_name = _("ServerName: ");
		wxString temp_ip   = _("ServerIP: ");
		
		if ( theApp.serverconnect->GetCurrentServer() ) {
			temp_name += theApp.serverconnect->GetCurrentServer()->GetListName();
			temp_ip   += theApp.serverconnect->GetCurrentServer()->GetFullIP();
		} else {
			temp_name += _("Not Connected");
			temp_ip   += _("Not Connected");
		}

		info_item = gtk_menu_item_new_with_label( StringToSystray( temp_name ) );
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
		
		info_item = gtk_menu_item_new_with_label( StringToSystray( temp_ip) );
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}
	

	// IP Address
	{
		wxString temp = wxT("IP: ");
		if ( theApp.GetPublicIP() ) {
			in_addr IP;
			IP.s_addr = theApp.GetPublicIP();

			temp += char2unicode(inet_ntoa(IP)); 
		} else {
			temp += _("Unknown");
		}
	
		info_item = gtk_menu_item_new_with_label( StringToSystray( temp ) );
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}


	// TCP PORT
	{
		if (theApp.glob_prefs->GetPort()) {
			wxString temp = wxString::Format(wxT("%s%d"), _("TCP Port: "), theApp.glob_prefs->GetPort());
			info_item=gtk_menu_item_new_with_label( StringToSystray( temp ) );
		} else
			info_item=gtk_menu_item_new_with_label(StringToSystray(_("TCP Port: Not Ready")));
		
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}
	

	// UDP PORT
	{
		if (theApp.glob_prefs->GetUDPPort()) {
			wxString temp = wxString::Format(wxT("%s%d"), _("UDP Port: "), theApp.glob_prefs->GetUDPPort());	
			info_item=gtk_menu_item_new_with_label( StringToSystray( temp ) );
		} else
			info_item=gtk_menu_item_new_with_label(StringToSystray(_("UDP Port: Not Ready")));
	
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}


	// Online Signature
	{
		if (theApp.glob_prefs->IsOnlineSignatureEnabled())
			info_item=gtk_menu_item_new_with_label(StringToSystray(_("Online Signature: Enabled")));
		else
			info_item=gtk_menu_item_new_with_label(StringToSystray(_("Online Signature: Disabled")));
		
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}


	// Uptime
	{
		wxString temp = wxString::Format(wxT("%s%s"), _("Uptime: "), CastSecondsToHM(theApp.GetUptimeSecs()).c_str());
								   
		info_item=gtk_menu_item_new_with_label( StringToSystray(temp));
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}


	// Number of shared files
	{
		wxString temp = wxString::Format(wxT("%s%d"), _("Shared Files: "), theApp.sharedfiles->GetCount());
		info_item=gtk_menu_item_new_with_label( StringToSystray( temp ) );
		gtk_container_add(GTK_CONTAINER (info_menu), info_item);
	}


	// Number of queued clients
	{
		wxString temp = wxString::Format(wxT("%s%d"), _("Queued Clients: "), theApp.uploadqueue->GetWaitingUserCount() );
		info_item=gtk_menu_item_new_with_label( StringToSystray(temp));
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}
	

	// Total Downloaded
	{
		wxString temp = CastItoXBytes( theApp.stat_sessionReceivedBytes + theApp.glob_prefs->GetTotalDownloaded() );
		temp = wxString(_("Total DL: ")) + temp;
		info_item=gtk_menu_item_new_with_label( StringToSystray( temp ) );
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}
	

	// Total Uploaded
	{
		wxString temp = CastItoXBytes( theApp.stat_sessionSentBytes + theApp.glob_prefs->GetTotalUploaded() );
		temp = wxString(_("Total UL: ")) + temp;
		info_item=gtk_menu_item_new_with_label( StringToSystray( temp ) );
		gtk_container_add (GTK_CONTAINER (info_menu), info_item);
	}


	// Separator
	item=gtk_menu_item_new();
	gtk_container_add (GTK_CONTAINER (status_menu), item);


	// Upload Speed sub-menu
	{
		GtkWidget* up_speed = gtk_menu_new();
		
		item=gtk_menu_item_new_with_label(StringToSystray(_("Upload Limit")));
		gtk_container_add (GTK_CONTAINER (status_menu), item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),up_speed);
	
		GtkWidget* up_item = gtk_menu_item_new_with_label(StringToSystray(_("Unlimited")));
		gtk_object_set_data_full(GTK_OBJECT(up_item), "label", 0, NULL);
		gtk_container_add(GTK_CONTAINER(up_speed), up_item);
		gtk_signal_connect(GTK_OBJECT(up_item), "activate", GTK_SIGNAL_FUNC(set_ul_speed), up_item);

	
		uint32 max_ul_speed = theApp.glob_prefs->GetMaxGraphUploadRate();
		// If we dont have a frame of reference, then we'll just have to pull some numbers out our asses
		if ( max_ul_speed == UNLIMITED )
			max_ul_speed = 100;
		else if ( max_ul_speed < 10 )
			max_ul_speed = 10;
	
		
		for ( int i = 0; i < 5; i++ ) {
			unsigned int tempspeed = (unsigned int)((double)max_ul_speed / 5) * (5 - i);

			wxString temp = wxString::Format(wxT("%u%s"), tempspeed, _(" kb/s"));

			up_item=gtk_menu_item_new_with_label( StringToSystray( temp ));
			gtk_container_add(GTK_CONTAINER(up_speed), up_item);
			gtk_object_set_data_full(GTK_OBJECT(up_item), "label", (void*)tempspeed, NULL);
			gtk_signal_connect(GTK_OBJECT(up_item), "activate", GTK_SIGNAL_FUNC(set_ul_speed), up_item);
		}
	}

	
	// Download Speed sub-menu
	{ 
		GtkWidget* down_speed = gtk_menu_new();
		
		item=gtk_menu_item_new_with_label(StringToSystray(_("Download Limit")));
		gtk_container_add (GTK_CONTAINER (status_menu), item);
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), down_speed);
		
		GtkWidget* dl_item = gtk_menu_item_new_with_label(StringToSystray(_("Unlimited")));
		gtk_object_set_data_full(GTK_OBJECT(dl_item), "label", 0 , NULL);
		gtk_container_add (GTK_CONTAINER (down_speed), dl_item);
		gtk_signal_connect (GTK_OBJECT(dl_item), "activate",GTK_SIGNAL_FUNC (set_dl_speed),dl_item);

		uint32 max_dl_speed = theApp.glob_prefs->GetMaxGraphDownloadRate();
		// If we dont have a frame of reference, then we'll just have to pull some numbers out our asses
		if ( max_dl_speed == UNLIMITED )
			max_dl_speed = 100;
		else if ( max_dl_speed < 10 )
			max_dl_speed = 10;

	
		for ( int i = 0; i < 5; i++ ) {
			unsigned int tempspeed = (unsigned int)((double)max_dl_speed / 5) * (5 - i);

			wxString temp = wxString::Format(wxT("%d%s"), tempspeed, _(" kb/s"));
		
			dl_item=gtk_menu_item_new_with_label( StringToSystray( temp ) );
			gtk_container_add (GTK_CONTAINER (down_speed), dl_item);
			gtk_object_set_data_full(GTK_OBJECT(dl_item), "label", (void*)tempspeed, NULL);
			gtk_signal_connect (GTK_OBJECT(dl_item), "activate",GTK_SIGNAL_FUNC (set_dl_speed),dl_item);	
		}
	}


	// Separator
	item=gtk_menu_item_new();
	gtk_container_add (GTK_CONTAINER (status_menu), item);


	if (theApp.serverconnect->IsConnected()) {
		//Disconnection Speed item
		item=gtk_menu_item_new_with_label(StringToSystray(_("Disconnect from server")));
		gtk_container_add (GTK_CONTAINER (status_menu), item);
		gtk_signal_connect (GTK_OBJECT (item), "activate",GTK_SIGNAL_FUNC (disconnect),NULL);
	} else {
		//Connect item
		item=gtk_menu_item_new_with_label(StringToSystray(_("Connect to any server")));
		gtk_container_add (GTK_CONTAINER (status_menu), item);
		gtk_signal_connect (GTK_OBJECT (item), "activate",GTK_SIGNAL_FUNC (connect_any_server),NULL);
	}


	//separator
	item=gtk_menu_item_new();
	gtk_container_add (GTK_CONTAINER (status_menu), item);


	if (theApp.amuledlg->IsShown()) {
		//hide item
		item = gtk_menu_item_new_with_label(StringToSystray(_("Hide aMule")));
		gtk_container_add(GTK_CONTAINER(status_menu), item);
		gtk_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(do_hide), NULL);
	} else {
		//show item
		item = gtk_menu_item_new_with_label(StringToSystray(_("Show aMule")));
		gtk_container_add(GTK_CONTAINER(status_menu), item);
		gtk_signal_connect(GTK_OBJECT(item), "activate", GTK_SIGNAL_FUNC(do_show), NULL);
	}


	//separator
	item=gtk_menu_item_new();
	gtk_container_add (GTK_CONTAINER (status_menu), item);


	// Exit item
	item=gtk_menu_item_new_with_label(StringToSystray(_("Exit")));
	gtk_container_add (GTK_CONTAINER (status_menu), item);
	gtk_signal_connect (GTK_OBJECT (item), "activate",GTK_SIGNAL_FUNC (close_amule),NULL);


	// When the menu is popped-down, you need to destroy it
	gtk_signal_connect (GTK_OBJECT(status_menu), "selection-done",
	                    GTK_SIGNAL_FUNC(gtk_widget_destroy), &status_menu);


	// Show the pop-up menu
	gtk_widget_show_all (status_menu);
	gtk_menu_popup (GTK_MENU(status_menu), NULL, NULL,NULL, NULL,event->button, event->time);

	return TRUE;
}


static gboolean tray_clicked(GtkWidget* event_box, GdkEventButton* event, gpointer data)
{
	// Left or middle-click hides or shows the gui
	if ( ( event->button == 1 || event->button == 2 ) && event->type == GDK_BUTTON_PRESS ) {
		if ( theApp.amuledlg->IsShown() ) {
			do_hide();
		} else {
			do_show();
		}
	
		return true;
	}

	//mouse right click
	if (event->button == 3) {
		return tray_menu (event_box, event, data);
	}

	// Tell calling code that we have not handled this event; pass it on.
	return false;
}

CSysTray::CSysTray(wxWindow* parent, DesktopMode desktopmode, const wxString& title)
{

	m_DesktopMode = desktopmode;

	m_parent = parent;

	m_status_docklet = NULL;
	m_status_image = NULL;
	m_status_tooltips = NULL;

	// not wanted, so don't show it.
	if ( m_DesktopMode == dmDisabled )
		return;

	bool use_legacy=false;

	// case 2 and 3 are KDE/other legacy system
	if( m_DesktopMode == dmKDE3 || m_DesktopMode == dmKDE2)
		use_legacy=true;

	static GtkWidget *eventbox = NULL;
	gdk_rgb_init();

	if(use_legacy) {
		m_status_docklet=gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title(GTK_WINDOW(m_status_docklet), StringToSystray(title));
		gtk_window_set_wmclass(GTK_WINDOW(m_status_docklet),"amule_StatusDocklet","aMule");
		gtk_widget_set_usize(m_status_docklet,22,22);
	} else {
		m_status_docklet=GTK_WIDGET(egg_tray_icon_new(StringToSystray(_("aMule for Linux"))));
		if(m_status_docklet==NULL) {
			printf("**** WARNING: Can't create status docklet. Systray will not be created.\n");
			m_DesktopMode = dmDisabled;
			return;
		}
	}
	gtk_widget_realize(m_status_docklet);
	gtk_signal_connect(GTK_OBJECT(m_status_docklet),"destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed),&m_status_docklet);

	// set image
	GtkStyle *style;
	style = gtk_widget_get_style(m_status_docklet);
	GdkBitmap* mask=NULL;
	GdkPixmap* img=gdk_pixmap_create_from_xpm_d(m_status_docklet->window,&mask,&style->bg[GTK_STATE_NORMAL],mule_Tr_grey_ico);
	m_status_image=gtk_pixmap_new(img,mask);

	eventbox = gtk_event_box_new ();
	gtk_widget_show (eventbox);
	gtk_container_add (GTK_CONTAINER (eventbox), m_status_image);
	gtk_container_add (GTK_CONTAINER (m_status_docklet), eventbox);
	gtk_signal_connect (GTK_OBJECT (eventbox), "button_press_event",GTK_SIGNAL_FUNC (tray_clicked), NULL );
	gtk_signal_connect (GTK_OBJECT(m_status_image),"destroy",  GTK_SIGNAL_FUNC(gtk_widget_destroyed),&m_status_image);

	// set tooltips
	m_status_tooltips=gtk_tooltips_new();
	gtk_tooltips_enable(m_status_tooltips);
	gtk_tooltips_set_tip(m_status_tooltips,m_status_docklet,StringToSystray(_("aMule for Linux")),"blind text");

	// finalization
	gtk_widget_show(m_status_image);
	if(use_legacy) {
		setupProperties();
	}
	gtk_widget_show(GTK_WIDGET(m_status_docklet));
	gtk_widget_show_all (GTK_WIDGET (m_status_docklet));

}

void CSysTray::setupProperties()
{
	GdkWindow* window=m_status_docklet->window;

	glong data[1];

	GdkAtom kwm_dockwindow_atom;
	GdkAtom kde_net_system_tray_window_for_atom;

	kwm_dockwindow_atom = gdk_atom_intern("KWM_DOCKWINDOW", FALSE);
	kde_net_system_tray_window_for_atom = gdk_atom_intern("_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", FALSE);

	/* This is the old KDE 1.0 and GNOME 1.2 way... */
	data[0] = TRUE;
	gdk_property_change(window, kwm_dockwindow_atom,
	                    kwm_dockwindow_atom, 32,
	                    GDK_PROP_MODE_REPLACE, (guchar *)&data, 1);

	/* This is needed to support KDE 2.0 */
	/* can be set to zero or the root win I think */
	data[0] = 0;
	gdk_property_change(window, kde_net_system_tray_window_for_atom,
	                    (GdkAtom)XA_WINDOW, 32,
	                    GDK_PROP_MODE_REPLACE, (guchar *)&data, 1);
}

void CSysTray::SetTrayToolTip(const wxString& tip)
{
	if ( m_DesktopMode == dmDisabled || m_status_docklet == NULL )
		return;

	gtk_tooltips_set_tip(m_status_tooltips, m_status_docklet, StringToSystray(tip), NULL);
}

void CSysTray::DrawIconMeter(GdkPixmap* pix,GdkBitmap* mask,int nLevel,int nPos)
{
	GdkGC* gc=gdk_gc_new(pix);

	gdk_rgb_gc_set_background(gc,0);
	// border color is black :)
	gdk_rgb_gc_set_foreground(gc,0);
	gdk_draw_rectangle(pix,gc,1,((SysTrayWidth-1)/SysTrayBarCount)*nPos+SysTraySpacing,SysTrayHeight-((nLevel*(SysTrayHeight-1)/SysTrayMaxValue)+1),((SysTrayWidth-1)/SysTrayBarCount)*(nPos+1)+1,SysTrayHeight);
	// then draw to mask (look! it must be initialised even if it is not used!)
	GdkGC* newgc=gdk_gc_new(mask);
	gdk_rgb_gc_set_foreground(newgc,0x0);
	gdk_draw_rectangle(mask,newgc,TRUE,0,0,22,22);

	if(nLevel>0) {
		gdk_rgb_gc_set_foreground(newgc,0xffffff);
		gdk_draw_rectangle(mask,newgc,1,SysTrayWidth-2,SysTrayHeight-((nLevel*(SysTrayHeight-1)/SysTrayMaxValue)+1),
		                   SysTrayWidth,SysTrayHeight);
	}
	gdk_gc_unref(newgc);
	gdk_gc_unref(gc);
}

void CSysTray::drawMeters(GdkPixmap* pix,GdkBitmap* mask,int* pBarData)
{
	if(pBarData==NULL)
		return;

	for(int i=0;i<SysTrayBarCount;i++) {
		DrawIconMeter(pix,mask,pBarData[i],i);
	}
}

void CSysTray::SetTrayIcon(char** data, int* pVals)
{
	if(m_DesktopMode==dmDisabled)
		return;

	if(m_status_image==NULL)
		return; // nothing you can do..

	GdkPixmap* oldpix,*oldbit;
	GdkPixmap* newpix,*newbit;

	/* set new */
	gtk_pixmap_get(GTK_PIXMAP(m_status_image),&oldpix,&oldbit);
	newpix=gdk_pixmap_create_from_xpm_d(m_status_docklet->window,&newbit,NULL,data);
	/* create pixmap for meters */
	GdkPixmap *meterpix=gdk_pixmap_new(m_status_docklet->window,22,22,-1);
	GdkBitmap* meterbit=gdk_pixmap_new(m_status_docklet->window,22,22,1);
	/* draw meters */
	drawMeters(meterpix,meterbit,pVals);
	/* then draw meters onto main pix */
	GdkGC* mygc=gdk_gc_new(newpix);
	gdk_gc_set_clip_mask(mygc,meterbit);
	gdk_draw_pixmap(newpix,mygc,meterpix,0,0,0,0,22,22);
	gdk_gc_set_clip_mask(mygc,NULL);
	gdk_gc_unref(mygc);
	/* finally combine masks */
	mygc=gdk_gc_new(newbit);
	gdk_gc_set_function(mygc,GDK_OR);
	gdk_draw_pixmap(newbit,mygc,meterbit,0,0,0,0,22,22);
	gdk_gc_unref(mygc);
	gdk_pixmap_unref(meterpix);
	gdk_bitmap_unref(meterbit);
	gtk_pixmap_set(GTK_PIXMAP(m_status_image),newpix,newbit);

	/* free old */
	gdk_pixmap_unref(oldpix);
	gdk_bitmap_unref(oldbit);
	/* and force repaint */
	gtk_widget_draw(m_status_docklet,NULL);
}

CSysTray::~CSysTray()
{

}

#endif // __WXGTK__
