//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2005 Kry ( elkry@sourceforge.net / http://www.amule.org )
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

#ifdef HAVE_CONFIG_H
	#include "config.h"		// Needed for VERSION
#endif


#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <stdlib.h>
#include <math.h> // Needed for cos, M_PI

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "WebServer.h"
// needs GetTickCount implementation too
#pragma implementation "GetTickCount.h"
#endif
#include "WebServer.h"

//-------------------------------------------------------------------

#include <wx/arrimpl.cpp>	// this is a magic incantation which must be done!
#include <wx/tokenzr.h>		// for wxTokenizer
#include <wx/txtstrm.h>
#include <wx/wfstream.h>
#include <wx/filename.h>

//-------------------------------------------------------------------

#include "ECFileConfig.h"	// Needed for CECFileConfig
#include "ECSocket.h"
#include "ECSpecialTags.h"
#include "GetTickCount.h"	// Needed for GetTickCount
#include "MD5Sum.h"
#include "OtherStructs.h"	// Needed for TransferredData
#include "OtherFunctions.h"	// Needed for atoll, ED2KFT_*
#include "NetworkFunctions.h"	// Needed for StringIPtoUint32
#include "Types.h"
#include "WebSocket.h"		// Needed for StopSockets()
#include "ECcodes.h"

#ifndef __WXMSW__
	#include "netinet/in.h"	// Needed for ntohl()
#else
	#include "winsock.h"
#endif
//-------------------------------------------------------------------

// Initialization of the static MyTimer member variables.
#if wxUSE_GUI && wxUSE_TIMER && !defined(AMULE_DAEMON)
uint32 MyTimer::tic32 = 0;
uint64 MyTimer::tic64 = 0;
#endif

//-------------------------------------------------------------------

WX_DEFINE_OBJARRAY(ArrayOfUpDown);
WX_DEFINE_OBJARRAY(ArrayOfSession);
WX_DEFINE_OBJARRAY(ArrayOfTransferredData);

#define HTTPInit "Server: aMule\r\nPragma: no-cache\r\nExpires: 0\r\nCache-Control: no-cache, no-store, must-revalidate\r\nConnection: close\r\nContent-Type: text/html\r\n"
#define HTTPInitGZ "Server: aMule\r\nPragma: no-cache\r\nExpires: 0\r\nCache-Control: no-cache, no-store, must-revalidate\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Encoding: gzip\r\n"

#define WEB_SERVER_TEMPLATES_VERSION	4


#if wxUSE_UNICODE
	#define	WEBCHARSET	wxT("UTF-8")
#else
	// Western (Latin) includes Catalan, Danish, Dutch, English, Faeroese, Finnish, French,
	// German, Galician, Irish, Icelandic, Italian, Norwegian, Portuguese, Spanish and Swedish
	#define	WEBCHARSET	wxT("ISO-8859-1")
#endif


using namespace otherfunctions;

inline void set_rgb_color_val(unsigned char *start, uint32 val, unsigned char mod)
{
	unsigned char r = val, g = val >> 8, b = val >> 16;
	start[0] = ( r > mod ) ? (r - mod) : 1;
	start[1] = ( g > mod ) ? (g - mod) : 1;
	start[2] = ( b > mod ) ? (b - mod) : 1;
}

wxString _SpecialChars(wxString str) {
	str.Replace(wxT("&"),wxT("&amp;"));
	str.Replace(wxT("<"),wxT("&lt;"));
	str.Replace(wxT(">"),wxT("&gt;"));
	str.Replace(wxT("\""),wxT("&quot;"));
	return str;
}

uint32 GetHigherPrio(uint32 prio, bool autoprio)
{
	if (autoprio) {
		return PR_LOW;
	} else {
		switch (prio) {
			case PR_LOW: return PR_NORMAL;
			case PR_NORMAL: return PR_HIGH;
			case PR_HIGH: return PR_AUTO;
			case PR_AUTO: return PR_LOW;
			default: return PR_AUTO;
		}
	}
}

uint32 GetHigherPrioShared(uint32 prio, bool autoprio)
{
	if (autoprio) {
		return PR_VERYLOW;
	} else {
		switch (prio) {
			case PR_VERYLOW: return PR_LOW;
			case PR_LOW: return PR_NORMAL;
			case PR_NORMAL: return PR_HIGH;
			case PR_HIGH: return PR_VERYHIGH;
			case PR_VERYHIGH: return PR_POWERSHARE;
			case PR_POWERSHARE: return PR_AUTO;
			case PR_AUTO: return PR_VERYLOW;
			default: return PR_AUTO;
		}
	}
}

wxString Prio2Str(uint32 nFilePriority, bool bFileAutoPriority)
{
	wxString sFilePriority;
	switch (nFilePriority) {
		case PR_VERYLOW:
			sFilePriority = _("Very Low"); break;
		case PR_LOW:
			sFilePriority = _("Low"); break;
		case PR_NORMAL:
			sFilePriority = _("Normal"); break;
		case PR_HIGH:
			sFilePriority = _("High"); break;
		case PR_VERYHIGH:
			sFilePriority = _("Very High"); break;
		case PR_POWERSHARE:
			sFilePriority = _("Release"); break;
		default:
			sFilePriority = wxT("-"); break;
	}
	if ( bFileAutoPriority ) {
		sFilePriority += _(" Auto");
	}
	return sFilePriority;
}

uint32 GetLowerPrio(uint32 prio, bool autoprio)
{
	if (autoprio) {
		return PR_HIGH;
	} else {
		switch (prio) {
			case PR_LOW: return PR_AUTO;
			case PR_NORMAL: return PR_LOW;
			case PR_HIGH: return PR_NORMAL;
			case PR_AUTO: return PR_HIGH;
			default: return PR_AUTO;
		}
	}
}

uint32 GetLowerPrioShared(uint32 prio, bool autoprio)
{
	if (autoprio) {
		return PR_POWERSHARE;
	} else {
		switch (prio) {
			case PR_VERYLOW: return PR_AUTO;
			case PR_LOW: return PR_VERYLOW;
			case PR_NORMAL: return PR_LOW;
			case PR_HIGH: return PR_NORMAL;
			case PR_VERYHIGH: return PR_HIGH;
			case PR_POWERSHARE: return PR_VERYHIGH;
			case PR_AUTO: return PR_POWERSHARE;
			default: return PR_AUTO;
		}
	}
}

CWebServer::CWebServer(CamulewebApp *webApp, const wxString& templateDir):
	m_ServersInfo(webApp), m_SharedFilesInfo(webApp), m_DownloadFilesInfo(webApp, &m_ImageLib),
	m_UploadsInfo(webApp), m_SearchInfo(webApp),
	m_ImageLib(templateDir)
{
	webInterface = webApp;
	m_mutexChildren = new wxMutex();

	m_Params.bShowUploadQueue = false;

	m_iSearchSortby = 3;
	m_bSearchAsc = 0;

	m_bServerWorking = false; // not running (listening) yet

	// Graph defaults
	m_nGraphHeight = 149;
	m_nGraphWidth = 500;
	m_nGraphScale = 3;
}

CWebServer::~CWebServer(void) {
	//stop web socket thread
	if (wsThread) wsThread->Delete();

	delete m_mutexChildren;
}

//start web socket and reload templates
void CWebServer::StartServer(void) {
	if (!m_bServerWorking) {
		if (!webInterface->m_LoadSettingsFromAmule) {
			if (webInterface->m_configFile) {
				webInterface->m_PageRefresh = webInterface->m_configFile->Read(wxT("/Webserver/PageRefreshTime"), 120l);
				m_nGraphHeight = webInterface->m_configFile->Read(wxT("/Webserver/GraphHeight"), 149l);
				m_nGraphWidth = webInterface->m_configFile->Read(wxT("/Webserver/GraphWidth"), 500l);
				m_nGraphScale = webInterface->m_configFile->Read(wxT("/Webserver/GraphScale"), 3l);
			}
		}
		ReloadTemplates();

		//create the thread...
		wsThread = new CWSThread(this);
		if ( wsThread->Create() != wxTHREAD_NO_ERROR ) {
			webInterface->Show(_("Can't create web socket thread\n"));
		} else {
			//...and run it
			wsThread->Run();
 
			m_bServerWorking = true;
			webInterface->Show(_("Web Server: Started\n"));
		}
	} else
		webInterface->Show(_("Web Server: running\n"));
}

//restart web socket and reload templates
void CWebServer::RestartServer(void) {
	if (m_bServerWorking) {
		if (wsThread) wsThread->Delete();
	}
	
	//create the thread...
	wsThread = new CWSThread(this);
	if ( wsThread->Create() != wxTHREAD_NO_ERROR ) {
		webInterface->Show(_("Can't create web socket thread\n"));
	} else {
		//...and run it
		wsThread->Run();
		webInterface->Show(_("Web Server: Restarted\n"));
	}
}


//stop web socket
void CWebServer::StopServer(void) {
	if (m_bServerWorking) {
		m_bServerWorking = false;
		if (wsThread) wsThread->Delete();
		webInterface->Show(_("Web Server: Stopped\n"));
	} else
		webInterface->Show(_("Web Server: not running\n"));
}


//returns web server listening port
long CWebServer::GetWSPrefs(void)
{
	CECPacket req(EC_OP_GET_PREFERENCES);
	req.AddTag(CECTag(EC_TAG_SELECT_PREFS, (uint32)EC_PREFS_REMOTECONTROLS));
	CECPacket *reply = webInterface->SendRecvMsg_v2(&req);
	if (!reply) {
		return -1;
	}
	// we have selected only the webserver preferences
	const CECTag *wsprefs = reply->GetTagByIndexSafe(0);
	unsigned int wsport = wsprefs->GetTagByNameSafe(EC_TAG_WEBSERVER_PORT)->GetInt16Data();

	if (webInterface->m_LoadSettingsFromAmule) {
		webInterface->m_AdminPass = wsprefs->GetTagByNameSafe(EC_TAG_PASSWD_HASH)->GetMD4Data();

		const CECTag *webserverGuest = wsprefs->GetTagByName(EC_TAG_WEBSERVER_GUEST);
		if (webserverGuest) {
			webInterface->m_AllowGuest = true;
			webInterface->m_GuestPass = webserverGuest->GetTagByNameSafe(EC_TAG_PASSWD_HASH)->GetMD4Data();
		} else {
			webInterface->m_AllowGuest = false;
		}

		// we only need to check the presence of this tag
		webInterface->m_UseGzip = wsprefs->GetTagByName(EC_TAG_WEBSERVER_USEGZIP) != NULL;
	
		const CECTag *webserverRefresh = wsprefs->GetTagByName(EC_TAG_WEBSERVER_REFRESH);
		if (webserverRefresh) {
			webInterface->m_PageRefresh = webserverRefresh->GetInt32Data();
		} else {
			webInterface->m_PageRefresh = 120;
		}
	}

	delete reply;

	return wsport;
}

//sends output to web interface
void CWebServer::Print(const wxString &s) {
	webInterface->Show(s);
}

// send EC request and discard output
void CWebServer::Send_Discard_V2_Request(CECPacket *request)
{
	CECPacket *reply = webInterface->SendRecvMsg_v2(request);
	CECTag *tag = NULL;
	if (reply) {
		if ( reply->GetOpCode() == EC_OP_STRINGS ) {
			for(int i = 0; i < reply->GetTagCount(); ++i) {
				if (	(tag = reply->GetTagByIndex(i)) &&
					(tag->GetTagName() == EC_TAG_STRING)) {
					webInterface->Show(tag->GetStringData());
				}
			}
		} else if (reply->GetOpCode() == EC_OP_FAILED) {
			if (	reply->GetTagCount() &&
				(tag = reply->GetTagByIndex(0)) ) {
				webInterface->Show(
					wxString(_("Request failed with the following error: ")) +
					wxString(wxGetTranslation(tag->GetStringData())) + wxT("."));
			} else {
				webInterface->Show(_("Request failed with an unknown error."));
			}
		}
		delete reply;
	}
}

//reload template file
void CWebServer::ReloadTemplates(void) {	
	wxString sFile(webInterface->m_TemplateFileName);
	// Left here just for sanity, if template is removed while running amuleweb
	if (!wxFileName::FileExists(sFile)) {
		// no file. do nothing.
		webInterface->Show(wxString(_("Can't load templates: Can't open file ")) + sFile);
		return;
	}

	wxFileInputStream input(sFile);
	if (input.Ok()) {
		wxTextInputStream file(input);
		wxString sAll;
		while (!input.Eof()) {
			wxString sLine = file.ReadString();
			sAll += sLine + wxT("\n");
		}
		wxString sVersion = _LoadTemplate(sAll,wxT("TMPL_VERSION"));
		long lVersion = StrToLong(sVersion);
		if (lVersion < WEB_SERVER_TEMPLATES_VERSION) {
			webInterface->Show(wxString(_("Can't load templates: Can't open file ")) + sFile);
		} else {
			m_Templates.sHeader = _LoadTemplate(sAll,wxT("TMPL_HEADER"));
			m_Templates.sHeaderMetaRefresh = _LoadTemplate(sAll,wxT("TMPL_HEADER_META_REFRESH"));
			m_Templates.sHeaderStylesheet = _LoadTemplate(sAll,wxT("TMPL_HEADER_STYLESHEET"));
			m_Templates.sFooter = _LoadTemplate(sAll,wxT("TMPL_FOOTER"));
			m_Templates.sServerList = _LoadTemplate(sAll,wxT("TMPL_SERVER_LIST"));
			m_Templates.sServerLine = _LoadTemplate(sAll,wxT("TMPL_SERVER_LINE"));
			m_Templates.sTransferImages = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_IMAGES"));
			m_Templates.sTransferList = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_LIST"));
			m_Templates.sTransferDownHeader = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_DOWN_HEADER"));
			m_Templates.sTransferDownFooter = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_DOWN_FOOTER"));
			m_Templates.sTransferDownLine = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_DOWN_LINE"));
			m_Templates.sTransferDownLineGood = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_DOWN_LINE_GOOD"));
			m_Templates.sTransferUpHeader = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_UP_HEADER"));
			m_Templates.sTransferUpFooter = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_UP_FOOTER"));
			m_Templates.sTransferUpLine = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_UP_LINE"));
			m_Templates.sTransferUpQueueShow = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_UP_QUEUE_SHOW"));
			m_Templates.sTransferUpQueueHide = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_UP_QUEUE_HIDE"));
			m_Templates.sTransferUpQueueLine = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_UP_QUEUE_LINE"));
			m_Templates.sTransferBadLink = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_BAD_LINK"));
			m_Templates.sDownloadLink = _LoadTemplate(sAll,wxT("TMPL_DOWNLOAD_LINK"));
			m_Templates.sSharedList = _LoadTemplate(sAll,wxT("TMPL_SHARED_LIST"));
			m_Templates.sSharedLine = _LoadTemplate(sAll,wxT("TMPL_SHARED_LINE"));
			m_Templates.sSharedLineChanged = _LoadTemplate(sAll,wxT("TMPL_SHARED_LINE_CHANGED"));
			m_Templates.sGraphs = _LoadTemplate(sAll,wxT("TMPL_GRAPHS"));
			m_Templates.sLog = _LoadTemplate(sAll,wxT("TMPL_LOG"));
			m_Templates.sServerInfo = _LoadTemplate(sAll,wxT("TMPL_SERVERINFO"));
			m_Templates.sDebugLog = _LoadTemplate(sAll,wxT("TMPL_DEBUGLOG"));
			m_Templates.sStats = _LoadTemplate(sAll,wxT("TMPL_STATS"));
			m_Templates.sPreferences = _LoadTemplate(sAll,wxT("TMPL_PREFERENCES"));
			m_Templates.sLogin = _LoadTemplate(sAll,wxT("TMPL_LOGIN"));
			m_Templates.sConnectedServer = _LoadTemplate(sAll,wxT("TMPL_CONNECTED_SERVER"));
			m_Templates.sAddServerBox = _LoadTemplate(sAll,wxT("TMPL_ADDSERVERBOX"));
			m_Templates.sWebSearch = _LoadTemplate(sAll,wxT("TMPL_WEBSEARCH"));
			m_Templates.sSearch = _LoadTemplate(sAll,wxT("TMPL_SEARCH"));
			m_Templates.iProgressbarWidth=StrToLong(_LoadTemplate(sAll,wxT("PROGRESSBARWIDTH")));
			m_Templates.sSearchHeader = _LoadTemplate(sAll,wxT("TMPL_SEARCH_RESULT_HEADER"));
			m_Templates.sSearchResultLine = _LoadTemplate(sAll,wxT("TMPL_SEARCH_RESULT_LINE"));
			m_Templates.sProgressbarImgs = _LoadTemplate(sAll,wxT("PROGRESSBARIMGS"));
			m_Templates.sProgressbarImgsPercent = _LoadTemplate(sAll,wxT("PROGRESSBARPERCENTIMG"));
			m_Templates.sClearCompleted = _LoadTemplate(sAll,wxT("TMPL_TRANSFER_DOWN_CLEARBUTTON"));
			m_Templates.sCatArrow= _LoadTemplate(sAll,wxT("TMPL_CATARROW"));

			m_Templates.sProgressbarImgsPercent.Replace(wxT("[PROGRESSGIFNAME]"),wxT("%s"));
			m_Templates.sProgressbarImgsPercent.Replace(wxT("[PROGRESSGIFINTERNAL]"),wxT("%i"));
			m_Templates.sProgressbarImgs.Replace(wxT("[PROGRESSGIFNAME]"),wxT("%s"));
			m_Templates.sProgressbarImgs.Replace(wxT("[PROGRESSGIFINTERNAL]"),wxT("%i"));
			
			m_DownloadFilesInfo.LoadImageParams(m_Templates.sProgressbarImgs,
				m_Templates.iProgressbarWidth, 20);
				
		}
	} else {
		webInterface->Show(wxString(_("Can't load templates: Can't open file ")) + sFile);
	}
}


wxString CWebServer::_LoadTemplate(wxString sAll, wxString sTemplateName) {
	wxString sRet;
	int nStart = sAll.Find(wxT("<--") + sTemplateName + wxT("-->"));
	int nEnd = sAll.Find(wxT("<--") + sTemplateName + wxT("_END-->"));
	
	if (nStart != -1 && nEnd != -1)	{
		nStart += sTemplateName.Length() + 7;
		sRet = sAll.Mid(nStart, nEnd - nStart - 1);
	}
	
	if (sRet.IsEmpty()) {
		if (sTemplateName==wxT("TMPL_VERSION"))
			webInterface->Show(_("Can't find template version number!\nPlease replace aMule.tmpl with a newer version!"));
		webInterface->Show(_("Failed to load template ") + sTemplateName + wxT("\n"));
	}
	return sRet;
}

void CWebServer::ProcessImgFileReq(ThreadData Data)
{
	webInterface->DebugShow(wxT("**** imgrequest: ") + Data.sURL + wxT("\n"));

	CAnyImage *img = m_ImageLib.GetImage(Data.sURL);
	if ( img ) {
		int img_size;
		unsigned char* img_data = img->RequestData(img_size);
		// This unicode2char is ok.
		Data.pSocket->SendContent(unicode2char(img->GetHTTP()), img_data, img_size);
	} else {
		webInterface->DebugShow(wxT("**** imgrequest: failed\n"));
	}	
}

void CWebServer::ProcessURL(ThreadData Data) {
	/* This method is called by different threads, and it
	   seems dangerous. For now we just serialize it */
	wxMutexLocker lock(*m_mutexChildren);

	bool isUseGzip = webInterface->m_UseGzip;
	wxString Out;
	// List Entry Templates
	wxString OutE;
	wxString OutE2;
	// ServerStatus Templates
	wxString OutS;
	TCHAR *gzipOut = NULL;

	long gzipLen = 0;
	wxString HTTPProcessData;
	wxString HTTPTemp;
	srand ( time(NULL) );
	long lSession = 0;
	wxString sSes = _ParseURL(Data, wxT("ses"));
	if (!sSes.IsEmpty()) {
		sSes.ToLong(&lSession);
	}
	wxString sSession = sSession.Format(wxT("%ld"), lSession);
	wxString sW = _ParseURL(Data, wxT("w"));
	if (sW == wxT("password")) {
		wxString PwStr = _ParseURL(Data, wxT("p"));
		CMD4Hash PwHash(MD5Sum(PwStr).GetHash());
		bool login = false;
		wxString ip = char2unicode(inet_ntoa(Data.inadr));
		if ( (PwHash == webInterface->m_AdminPass) || (PwStr.IsEmpty() && webInterface->m_AdminPass.IsEmpty()) ) {
			Session* ses = new Session();
			ses->admin = true;
			ses->startTime = time(NULL);
			ses->lSession = lSession = rand() * 10000L + rand();
			m_Params.Sessions.Add(ses);
			login = true;
			webInterface->DebugShow(wxT("*** logged in as admin\n"));
		} else if ( (webInterface->m_AllowGuest) &&
				( (PwHash == webInterface->m_GuestPass) || (PwStr.IsEmpty() && webInterface->m_GuestPass.IsEmpty()) )) {
			Session* ses = new Session();
			ses->admin = false;
			ses->startTime = time(NULL);
			ses->lSession = lSession = rand() * 10000L + rand();
			m_Params.Sessions.Add(ses);
			login = true;
			webInterface->DebugShow(wxT("*** logged in as guest\n"));
		} else {
			// This call to ::GetTickCount has segfaulted once with this == 0x0, because
			// wxUSE_GUI was set to 1 in a console only application. This may happen due
			// to wrong wxWidgets configuration.
			//
			// save failed attempt (ip,time)
			TransferredData newban = {StringIPtoUint32(ip), ::GetTickCount()}; 
			m_Params.badlogins.Add(&newban);
			login = false;
			webInterface->DebugShow(wxT("*** login failed\n"));
		}
		isUseGzip = false;
		if (login) {
			uint32 ipn = StringIPtoUint32(ip);
			for (size_t i = 0; i < m_Params.badlogins.GetCount();) {
				if (ipn == m_Params.badlogins[i]->datalen) {
					m_Params.badlogins.RemoveAt(i);
				} else {
					++i;
				}
			}
		}
	}
	if (sW == wxT("logout")) {
		_RemoveSession(Data, lSession);
	}
	if (_IsLoggedIn(Data, lSession)) {
		Out += _GetHeader(Data, lSession);		
		wxString sPage = sW;
		webInterface->DebugShow(wxT("***** logged in, getting page ") + sPage + wxT("\n"));
		webInterface->DebugShow(wxT("***** session is ") + sSession + wxT("\n"));		
		if (sPage == wxT("server")) {
			Out += _GetServerList(Data);
		} else if (sPage == wxT("download")) {
			Out += _GetDownloadLink(Data);
		} else if (sPage == wxT("shared")) { 
			Out += _GetSharedFilesList(Data);
		} else if (sPage == wxT("transfer")) {
			Out += _GetTransferList(Data);
		} else if (sPage == wxT("websearch")) {
			Out += _GetWebSearch(Data);
		} else if (sPage == wxT("search")) {
			Out += _GetSearch(Data);
		} else if (sPage == wxT("graphs")) {
			Out += _GetGraphs(Data);
		} else if (sPage == wxT("log")) {
			Out += _GetLog(Data);
		} else if (sPage == wxT("sinfo")) {
			Out += _GetServerInfo(Data);
		} else if (sPage == wxT("debuglog")) {
			Out += _GetDebugLog(Data);
		} else if (sPage == wxT("stats")) {
			Out += _GetStats(Data);
		} else if (sPage == wxT("options")) {
			Out += _GetPreferences(Data);
		}		
		Out += _GetFooter(Data);
		if (sPage.IsEmpty()) {
			isUseGzip = false;
		}
		if (isUseGzip) {
#if wxUSE_UNICODE
			const wxCharBuffer buf = wxConvUTF8.cWC2MB(Out.wc_str(aMuleConv));
			const char *httpOut = (const char *)buf;
#else
			const char *httpOut = (const char *)Out;
#endif
			bool bOk = false;
			try {
				uLongf destLen = strlen(httpOut) + 1024;
				gzipOut = new TCHAR[destLen];
				if( _GzipCompress((Bytef*)gzipOut, &destLen, 
				   (const Bytef*)httpOut, strlen(httpOut), Z_DEFAULT_COMPRESSION) == Z_OK) {
					bOk = true;
					gzipLen = destLen;
				}
			} catch(...) {
			}			
			if (!bOk) {
				isUseGzip = false;
				if (gzipOut != NULL) {
					delete[] gzipOut;
					gzipOut = NULL;
				}
			}
		}
	} else {
		isUseGzip = false;		
		uint32 ip = inet_addr(inet_ntoa( Data.inadr ));
		uint32 faults = 0;
		// check for bans
		for (size_t i = 0; i < m_Params.badlogins.GetCount(); ++i) {
			if (m_Params.badlogins[i]->datalen==ip) ++faults;
		}
		if (faults>4) {
			Out += _("Access denied!");
			// set 15 mins ban by using the badlist
			TransferredData preventive={ip, ::GetTickCount() + (15*60*1000) };
			for (int i = 0; i <= 5; ++i) {
				m_Params.badlogins.Add(&preventive);
			}
		} else {
			Out += _GetLoginScreen(Data);
		}
	}
	//
	// send answer ...
	//
#if wxUSE_UNICODE
	const wxCharBuffer buf = wxConvUTF8.cWC2MB(Out.wc_str(aMuleConv));
	const char *httpOut = (const char *)buf;
#else
	const char *httpOut = (const char *)Out;
#endif

	if (!isUseGzip)	{
		Data.pSocket->SendContent(HTTPInit, httpOut, strlen(httpOut));
	} else {
		Data.pSocket->SendContent(HTTPInitGZ, gzipOut, gzipLen);
	}
	if (gzipOut != NULL) {
		delete[] gzipOut;
	}
}


wxString CWebServer::_ParseURLArray(ThreadData Data, wxString fieldname) {

	wxString URL = Data.sURL;
	wxString res,temp;
	while (URL.Length()>0) {
		int pos=URL.MakeLower().Find(fieldname.MakeLower() + wxT("="));
		if (pos>-1) {
			temp=_ParseURL(Data,fieldname);
			if (temp.IsEmpty()) break;
			res.Append(temp+wxT("|"));
			Data.sURL.Remove(pos, 10);
			URL=Data.sURL;
			//URL.Remove(pos,10);
		} else break;
	}
	return res;
}


wxString CWebServer::_ParseURL(ThreadData Data, wxString fieldname) {

	wxString URL = Data.sURL;	
	wxString value;
	wxString Parameter;
	char fromReplace[4] = "";	// decode URL
	char toReplace[2] = "";		// decode URL
	int i = 0;
	int findPos = -1;
	int findLength = 0;
	webInterface->DebugShow(wxT("*** parsing url ") + URL + wxT(" :: field ") + fieldname + wxT("\n"));
	if (URL.Find(wxT("?")) > -1) {
		Parameter = URL.Mid(URL.Find(wxT("?"))+1, URL.Length()-URL.Find(wxT("?"))-1);
		// search the fieldname beginning / middle and strip the rest...
		if (Parameter.Find(fieldname + wxT("=")) == 0) {
			findPos = 0;
			findLength = fieldname.Length() + 1;
		}
		if (Parameter.Find(wxT("&") + fieldname + wxT("=")) > -1) {
			findPos = Parameter.Find(wxT("&") + fieldname + wxT("="));
			findLength = fieldname.Length() + 2;
		}
		if (findPos > -1) {
			Parameter = Parameter.Mid(findPos + findLength, Parameter.Length());
			if (Parameter.Find(wxT("&")) > -1) {
				Parameter = Parameter.Mid(0, Parameter.Find(wxT("&")));
			}	
			value = Parameter;
			// decode value ...
			value.Replace(wxT("+"), wxT(" "));
			for (i = 0 ; i <= 255 ; ++i) {
				sprintf(fromReplace, "%%%02x", i);
				toReplace[0] = (char)i;
				toReplace[1] = 0;
				value.Replace(char2unicode(fromReplace), char2unicode(toReplace));
				sprintf(fromReplace, "%%%02X", i);
				value.Replace(char2unicode(fromReplace), char2unicode(toReplace));
			}
		}
	}
	return value;
}


wxString CWebServer::_GetHeader(ThreadData Data, long lSession) {

	wxString sSession = sSession.Format(wxT("%ld"), lSession);

	wxString Out = m_Templates.sHeader;

	Out.Replace(wxT("[CharSet]"), WEBCHARSET);

	if (webInterface->m_PageRefresh) {
		wxString sPage = _ParseURL(Data, wxT("w"));
		if ((sPage == wxT("transfer")) || (sPage == wxT("server")) ||
			(sPage == wxT("graphs")) || (sPage == wxT("log")) ||
			(sPage == wxT("sinfo")) || (sPage == wxT("debuglog")) ||
			(sPage == wxT("stats"))) {
			wxString sT = m_Templates.sHeaderMetaRefresh;
			wxString sRefresh = sRefresh.Format(wxT("%d"), webInterface->m_PageRefresh);
			sT.Replace(wxT("[RefreshVal]"), sRefresh);
			
			wxString catadd;
			if (sPage == wxT("transfer"))
				catadd=wxT("&cat=") + _ParseURL(Data, wxT("cat"));
			sT.Replace(wxT("[wCommand]"), sPage+catadd);
			
			Out.Replace(wxT("[HeaderMeta]"), sT);
		}
	}
	
	Out.Replace(wxT("[Session]"), sSession);
	webInterface->DebugShow(wxT("*** replaced session with ") + sSession + wxT("\n"));
	Out.Replace(wxT("[HeaderMeta]"), wxEmptyString); // In case there are no meta
	Out.Replace(wxT("[aMuleAppName]"), wxT("aMule"));
	Out.Replace(wxT("[version]"), wxT(VERSION));
	Out.Replace(wxT("[StyleSheet]"), m_Templates.sHeaderStylesheet);
	Out.Replace(wxT("[WebControl]"), _("Web Control Panel"));
	Out.Replace(wxT("[Transfer]"), _("Transfer"));
	Out.Replace(wxT("[Server]"), _("Server list"));
	Out.Replace(wxT("[Shared]"), _("Shared Files"));
	Out.Replace(wxT("[Download]"), _("ED2K Link(s)"));
	Out.Replace(wxT("[Graphs]"), _("Graphs"));
	Out.Replace(wxT("[Log]"), _("Log"));
	Out.Replace(wxT("[ServerInfo]"), _("Serverinfo"));
	Out.Replace(wxT("[DebugLog]"), _("Debug Log"));
	Out.Replace(wxT("[Stats]"), _("Statistics"));
	Out.Replace(wxT("[Options]"), _("Preferences"));
	Out.Replace(wxT("[Logout]"), _("Logout"));
	Out.Replace(wxT("[Search]"), _("Search"));

	wxString sConnected;

	CECPacket stat_req(EC_OP_STAT_REQ, EC_DETAIL_CMD);

	CECPacket *stats = webInterface->SendRecvMsg_v2(&stat_req);
	if (!stats) {
		return wxEmptyString;
	}
	CEC_ConnState_Tag *tag = (CEC_ConnState_Tag *)stats->GetTagByName(EC_TAG_CONNSTATE);
	if (!tag) {
		return wxEmptyString;
	}
	
	switch (tag->ClientID()) {
	case 0:
		sConnected = _("Not connected");
		if (IsSessionAdmin(Data,sSession)) {
			sConnected += wxT(" (<small><a href=\"?ses=") + sSession +
				wxT("&w=server&c=connect\">Connect to any server</a></small>)");
		}
		break;
	case 0xffffffff:
		sConnected = _("Now connecting");
		break;
	default:
		CECTag *server = tag->GetTagByIndex(0);
		CECTag *sname  = server ? server->GetTagByName(EC_TAG_SERVER_NAME) : NULL;
		if (server && sname) {
			sConnected = _("Connected to ");
			sConnected += sname->GetStringData() + wxT(" ");
			sConnected += server->GetIPv4Data().StringIP() + wxT(" ");
			sConnected += tag->HaveLowID() ? _("with LowID") : _("with HighID");
		} else {
			return wxEmptyString;
		}
		break;
	}

	Out.Replace(wxT("[Connected]"), wxT("<b>Connection:</b> ") + sConnected);
	CECTag *ulSpeed = stats->GetTagByName(EC_TAG_STATS_UL_SPEED);
	CECTag *dlSpeed = stats->GetTagByName(EC_TAG_STATS_DL_SPEED);
	CECTag *ulSpeedLimit = stats->GetTagByName(EC_TAG_STATS_UL_SPEED_LIMIT);
	CECTag *dlSpeedLimit = stats->GetTagByName(EC_TAG_STATS_DL_SPEED_LIMIT);
	if (ulSpeed && dlSpeed && ulSpeedLimit && dlSpeedLimit) {
		Out.Replace(wxT("[Speed]"),
			wxString::Format(wxT("<b>Speed:</b> Up: %.1f | Down: %.1f <small> (Limits: %.1f/%.1f)</small>"),
			(double)ulSpeed->GetInt32Data() / 1024.0,
			(double)dlSpeed->GetInt32Data() / 1024.0,
			(double)ulSpeedLimit->GetInt32Data() / 1024.0,
			(double)dlSpeedLimit->GetInt32Data() / 1024.0));
	} else {
		return wxEmptyString;
	}
	
	return Out;
}


wxString CWebServer::_GetFooter(ThreadData WXUNUSED(Data)) {

	return m_Templates.sFooter;
}


wxString CWebServer::_GetServerList(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));

	wxString sAddServerBox;

	wxString sCmd = _ParseURL(Data, wxT("c"));
	if (sCmd == wxT("connect") && IsSessionAdmin(Data,sSession) ) {
		wxString sIP = _ParseURL(Data, wxT("ip"));
		wxString sPort = _ParseURL(Data, wxT("port"));
		uint32 ip;
		uint32 port;
		CECPacket req(EC_OP_SERVER_CONNECT);
		if ( sIP.ToULong((unsigned long *)&ip, 16) && sPort.ToULong((unsigned long *)&port, 10) ) {
			req.AddTag(CECTag(EC_TAG_SERVER, EC_IPv4_t(ip, port)));
		}
		Send_Discard_V2_Request(&req);
	} else if (sCmd == wxT("disconnect") && IsSessionAdmin(Data,sSession)) {
		CECPacket req(EC_OP_SERVER_DISCONNECT);
		Send_Discard_V2_Request(&req);
	} else if (sCmd == wxT("remove") && IsSessionAdmin(Data,sSession)) {
		wxString sIP = _ParseURL(Data, wxT("ip"));
		wxString sPort = _ParseURL(Data, wxT("port"));
		uint32 ip;
		uint32 port;
		if ( sIP.ToULong((unsigned long *)&ip, 16) && sPort.ToULong((unsigned long *)&port, 10) ) {
			CECPacket req(EC_OP_SERVER_REMOVE);
			req.AddTag(CECTag(EC_TAG_SERVER, EC_IPv4_t(ip, port)));
			Send_Discard_V2_Request(&req);
		}
	} else if (sCmd == wxT("options")) {
		sAddServerBox = _GetAddServerBox(Data);
	}
	
	wxString sSortRev = _ParseURL(Data, wxT("sortreverse"));
	wxString sSort = _ParseURL(Data, wxT("sort"));

	// reverse sort direction in link
	m_ServersInfo.SetSortOrder(sSort, sSortRev);

	wxString Out = m_Templates.sServerList;
	Out.Replace(wxT("[ConnectedServerData]"), _GetConnectedServer(Data));
	Out.Replace(wxT("[AddServerBox]"), sAddServerBox);
	Out.Replace(wxT("[Session]"), sSession);

	m_ServersInfo.ProcessHeadersLine(Out);
	
	Out.Replace(wxT("[ServerList]"), _("Server list"));
	Out.Replace(wxT("[Servername]"), _("Server name"));
	Out.Replace(wxT("[Description]"), _("Description"));
	Out.Replace(wxT("[Address]"), _("IP"));
	Out.Replace(wxT("[Connect]"), _("Connect"));
	Out.Replace(wxT("[Users]"), _("users"));
	Out.Replace(wxT("[Files]"), _("files"));
	Out.Replace(wxT("[Actions]"), _("Actions"));
		
	wxString OutE = m_Templates.sServerLine;
	OutE.Replace(wxT("[Connect]"), _("Connect"));
	OutE.Replace(wxT("[RemoveServer]"), _("Remove selected server"));
	OutE.Replace(wxT("[ConfirmRemove]"), _("Are you sure to remove this server from list?"));


	// Populating array - query core and sort
	m_ServersInfo.ReQuery();

	// Displaying
	wxString sList;
	ServersInfo::ItemIterator i = m_ServersInfo.GetBeginIterator();
	while (i != m_ServersInfo.GetEndIterator()) {
		wxString HTTPProcessData = OutE; // Copy Entry Line to Temp
		HTTPProcessData.Replace(wxT("[1]"), i->sServerName);
		HTTPProcessData.Replace(wxT("[2]"), i->sServerDescription);
		HTTPProcessData.Replace(wxT("[3]"), i->sServerIP );
		
		wxString sT;
		if (i->nServerUsers > 0) {
			if (i->nServerMaxUsers > 0) {
				sT = wxString::Format(wxT("%d (%d)"), i->nServerUsers, i->nServerMaxUsers);
			} else {
				sT = wxString::Format(wxT("%d"), i->nServerUsers);
			}
		} else {
			sT = wxT("0");
		}
		
		HTTPProcessData.Replace(wxT("[4]"), sT);
		sT = wxString::Format(wxT("%d"), i->nServerFiles);
		
		HTTPProcessData.Replace(wxT("[5]"), sT);
		if ( IsSessionAdmin(Data,sSession) ) {
			HTTPProcessData.Replace(wxT("[6]"),
						wxT("?ses=") + sSession + 
							wxString::Format(wxT("&w=server&c=connect&ip=%08x&port=%d"),
								 i->nServerIP, i->nServerPort));
			HTTPProcessData.Replace(wxT("[LinkRemove]"),
						wxT("?ses=") + sSession + 
						wxString::Format(wxT("&w=server&c=remove&ip=%08x&port=%d"),
								  i->nServerIP, i->nServerPort));
		} else {
			HTTPProcessData.Replace(wxT("[6]"), GetPermissionDenied());
			HTTPProcessData.Replace(wxT("[LinkRemove]"), GetPermissionDenied());
		}

		sList += HTTPProcessData;
		i++;
	}
	Out.Replace(wxT("[ServersList]"), sList);

	return Out;
}

wxString CWebServer::_GetTransferList(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));
	wxString sCat = _ParseURL(Data, wxT("cat"));

	wxString sOp = _ParseURL(Data, wxT("op"));
	wxString sFileHash = _ParseURL(Data, wxT("file"));
	wxString sSort = _ParseURL(Data, wxT("sort"));
	wxString sDownloadSortRev = _ParseURL(Data, wxT("sortreverse"));

	wxString Out;
	wxString HTTPTemp = _ParseURL(Data, wxT("c"));
	if (!HTTPTemp.IsEmpty() && IsSessionAdmin(Data, sSession)) {
		if (HTTPTemp.Right(1) != wxT("/")) {
			HTTPTemp += wxT("/");
		}
		CECPacket req(EC_OP_ED2K_LINK);
		req.AddTag(CECTag(EC_TAG_STRING, HTTPTemp));
		CECPacket *response = webInterface->SendRecvMsg_v2(&req);
		if (response) {
			if ( response->GetOpCode() == EC_OP_FAILED) {
				wxString HTTPTempC = _("This ed2k link is invalid (") + HTTPTemp + wxT(")");
				Out = m_Templates.sTransferBadLink;
				Out.Replace(wxT("[InvalidLink]"), HTTPTempC);
				Out.Replace(wxT("[Link]"), HTTPTemp);
			}
			delete response;
		} else {
			Out = _("Got no response from aMule.");
		}
	}
	//
	// Commands
	//
	if (!sOp.IsEmpty() && !sFileHash.IsEmpty()) {
		CECPacket *file_cmd = 0;
		CECTag hashtag(EC_TAG_PARTFILE, CMD4Hash(sFileHash));
		if (sOp == wxT("pause")) {
			file_cmd = new CECPacket(EC_OP_PARTFILE_PAUSE);
		} else if (sOp == wxT("resume")) {
			file_cmd = new CECPacket(EC_OP_PARTFILE_RESUME);
		} else if (sOp == wxT("cancel")) {
			file_cmd = new CECPacket(EC_OP_PARTFILE_DELETE);
		} else if (sOp == wxT("prioup")) {
			DownloadFiles *file = m_DownloadFilesInfo.GetByID(sFileHash);
			if ( file ) {
				file_cmd = new CECPacket(EC_OP_PARTFILE_PRIO_SET);
				hashtag.AddTag(CECTag(EC_TAG_PARTFILE_PRIO,
					GetHigherPrio(file->lFilePrio, file->bFileAutoPriority)));
			}
		} else if (sOp == wxT("priodown")) {
			DownloadFiles *file = m_DownloadFilesInfo.GetByID(sFileHash);
			if ( file ) {
				file_cmd = new CECPacket(EC_OP_PARTFILE_PRIO_SET);
				hashtag.AddTag(CECTag(EC_TAG_PARTFILE_PRIO,
					GetLowerPrio(file->lFilePrio, file->bFileAutoPriority)));
			}
		}
		wxASSERT(file_cmd);
		if ( file_cmd ) {
			file_cmd->AddTag(hashtag);
			Send_Discard_V2_Request(file_cmd);
			delete file_cmd;
		}
	}

	m_DownloadFilesInfo.SetSortOrder(sSort, sDownloadSortRev);
	m_Params.bShowUploadQueue = _ParseURL(Data, wxT("showuploadqueue")) == wxT("true");

	Out += m_Templates.sTransferImages;
	Out += m_Templates.sTransferList;
	Out.Replace(wxT("[DownloadHeader]"), m_Templates.sTransferDownHeader);
	Out.Replace(wxT("[DownloadFooter]"), m_Templates.sTransferDownFooter);
	Out.Replace(wxT("[UploadHeader]"), m_Templates.sTransferUpHeader);
	Out.Replace(wxT("[UploadFooter]"), m_Templates.sTransferUpFooter);
	Out.Replace(wxT("[Session]"), sSession);


	Out.Replace(wxT("[CATBOX]"), GetStatusBox(sCat));
	//InsertCatBox(pThis, Out, cat, wxEmptyString, true, true);
	
	m_DownloadFilesInfo.ProcessHeadersLine(Out);
	
	Out.Replace(wxT("[Filename]"), _("File Name"));
	Out.Replace(wxT("[Size]"), _("Size"));
	Out.Replace(wxT("[Completed]"), _("Complete"));
	Out.Replace(wxT("[Transferred]"), _("Transferred"));
	Out.Replace(wxT("[Progress]"), _("Progress"));
	Out.Replace(wxT("[Speed]"), _("Speed"));
	Out.Replace(wxT("[Sources]"), _("Sources"));
	Out.Replace(wxT("[Actions]"), _("Actions"));
	Out.Replace(wxT("[User]"), _("Username"));
	Out.Replace(wxT("[TotalDown]"), _("Downloaded total"));
	Out.Replace(wxT("[TotalUp]"), _("Uploaded total"));
	Out.Replace(wxT("[Prio]"), _("Priority"));

	Out.Replace(wxT("[CatSel]"), sCat.Length() ? (wxT("&cat=") + sCat) : wxString());

	wxString OutE = m_Templates.sTransferDownLine;
	wxString OutE2 = m_Templates.sTransferDownLineGood;

	uint64 fTotalSize = 0, fTotalTransferred = 0, fTotalCompleted = 0, fTotalSpeed = 0;
	
	m_DownloadFilesInfo.ReQuery();

	//
	// Displaying
	//
	wxString sDownList;
	DownloadFilesInfo::ItemIterator i = m_DownloadFilesInfo.GetBeginIterator();
	while (i != m_DownloadFilesInfo.GetEndIterator()) {

		if ( sCat.Length() && (sCat != i->sFileStatus)) {
			i++;
			continue;
		}
		
		wxString JSfileinfo = i->sFileName + wxT("-") + i->sFileStatus;

		JSfileinfo.Replace(wxT("|"),wxT("\\n"));
		wxString sActions = 
			wxT("<acronym title=\"") +
			i->sFileStatus +
			wxT("\"><a ref=\"javascript:alert('") +
			JSfileinfo +
			wxT("')\"><img src=\"l_info.gif\" alt=\"") +
			i->sFileStatus +
			wxT("\"></a></acronym>");
		wxString sED2kLink =
			wxT("<acronym title=\"[Ed2klink]\"><a href=\"") +
			i->sED2kLink +
			wxT("\"><img src=\"l_ed2klink.gif\" alt=\"[Ed2klink]\"></a></acronym>");
		sED2kLink.Replace(wxT("[Ed2klink]"), _("ED2K Link(s)"));
		sActions += sED2kLink;

		wxString sNextAction;
		if (IsSessionAdmin(Data,sSession)) {
			if ( i->nFileStatus == PS_PAUSED) {
				sActions += wxT("<acronym title=\"Resume\"><a href=\"?ses=") +
					sSession + wxT("&w=transfer&op=resume&file=") + i->sFileHash +
					wxT("\"><img src=\"l_resume.gif\" alt=\"Resume\"></a></acronym> ");
			} else {
				sActions += wxT("<acronym title=\"Pause\"><a href=\"?ses=") +
					sSession + wxT("&w=transfer&op=pause&file=") + i->sFileHash +
					wxT("\"><img src=\"l_pause.gif\" alt=\"Pause\"></a></acronym> ");
			}
			sActions += wxT("<acronym title=\"Cancel\"><a href=\"?ses=") +
			sSession + wxT("&w=transfer&op=cancel&file=") + i->sFileHash +
			wxT("\" onclick=\"return confirm(\'Are you sure that you want to cancel and delete this file?\\n\')\">"
				"<img src=\"l_cancel.gif\" alt=\"Cancel\"></a></acronym> ");
				
			sActions += wxT("<acronym title=\"[PriorityUp]\"><a href=\"?ses=") +
				sSession + wxT("&amp;w=transfer&op=prioup&file=") +	i->sFileHash +
				wxT("\"><img src=\"l_up.gif\" alt=\"[PriorityUp]\"></a></acronym>");
			sActions += wxT("&nbsp;<acronym title=\"[PriorityDown]\"><a href=\"?ses=") +
				sSession + wxT("&amp;w=transfer&op=priodown&file=") + i->sFileHash +
				wxT("\"><img src=\"l_down.gif\" alt=\"[PriorityDown]\"></a></acronym>");
		} else {
			// lfroen: put something instead of action links ?
		}
		
		wxString HTTPProcessData;
		// if downloading, draw in other color
		if (i->lFileSpeed > 0)
			HTTPProcessData = OutE2;
		else
			HTTPProcessData = OutE;

		if (i->sFileName.Length() > SHORT_FILENAME_LENGTH)
			HTTPProcessData.Replace(wxT("[ShortFileName]"), i->sFileName.Left(SHORT_FILENAME_LENGTH) + wxT("..."));
		else
			HTTPProcessData.Replace(wxT("[ShortFileName]"), i->sFileName);

		HTTPProcessData.Replace(wxT("[FileInfo]"), i->sFileInfo);

		fTotalSize += i->lFileSize;

		HTTPProcessData.Replace(wxT("[2]"), CastItoXBytes(i->lFileSize));

		if (i->lFileTransferred > 0) {
			fTotalTransferred += i->lFileTransferred;
			HTTPProcessData.Replace(wxT("[3]"), CastItoXBytes(i->lFileCompleted));
		} else {
			HTTPProcessData.Replace(wxT("[3]"), wxT("-"));
		}

		if (i->lFileCompleted > 0) {
			fTotalCompleted += i->lFileCompleted;
			HTTPProcessData.Replace(wxT("[4]"), CastItoXBytes(i->lFileTransferred));
		} else {
			HTTPProcessData.Replace(wxT("[4]"), wxT("-"));
		}
		
		int complx = (int)(m_Templates.iProgressbarWidth*i->fCompleted/100);
		if ( complx ) {
			HTTPProcessData.Replace(wxT("[DownloadBar]"), 
				wxString::Format((m_Templates.sProgressbarImgsPercent+wxT("<br>")),
					wxT("greenpercent.gif"),complx) + i->m_Image->GetHTML());			
		} else {
			HTTPProcessData.Replace(wxT("[DownloadBar]"), i->m_Image->GetHTML());
		}

		if (i->lFileSpeed > 0.0f) {
			fTotalSpeed += i->lFileSpeed;

			HTTPProcessData.Replace(wxT("[5]"), wxString::Format(wxT("%8.2f %s"),
				i->lFileSpeed/1024.0 ,_("kB/s")));
		} else
			HTTPProcessData.Replace(wxT("[5]"), wxT("-"));
		
		if (i->lSourceCount > 0) {
			wxString srcstring;
			if ( i->lNotCurrentSourceCount ) {
				srcstring = wxString::Format(wxT("%li&nbsp;/&nbsp;%8li&nbsp;(%li)"),
					i->lSourceCount - i->lNotCurrentSourceCount,
					i->lSourceCount, i->lTransferringSourceCount);
			} else {
				srcstring = wxString::Format(wxT("%li&nbsp;(%li)"),
					i->lSourceCount, i->lTransferringSourceCount);
			}
			if ( i->lSourceCountA4AF ) {
				srcstring += wxString::Format(wxT("+%li"), i->lSourceCountA4AF);
			}
			HTTPProcessData.Replace(wxT("[6]"), srcstring);
		} else
			HTTPProcessData.Replace(wxT("[6]"), wxT("-"));
		
		HTTPProcessData.Replace(wxT("[PrioVal]"), Prio2Str(i->lFilePrio, i->bFileAutoPriority));
		HTTPProcessData.Replace(wxT("[7]"), sActions);

		sDownList += HTTPProcessData;
		i++;
	}

	Out.Replace(wxT("[DownloadFilesList]"), sDownList);
	Out.Replace(wxT("[PriorityUp]"), _("Increase Priority"));
	Out.Replace(wxT("[PriorityDown]"), _("Decrease Priority"));
	// Elandal: cast from float to integral type always drops fractions.
	// avoids implicit cast warning
	Out.Replace(wxT("[TotalDownSize]"), CastItoXBytes((uint64)fTotalSize));
	Out.Replace(wxT("[TotalDownCompleted]"), CastItoXBytes((uint64)fTotalCompleted));
	Out.Replace(wxT("[TotalDownTransferred]"), CastItoXBytes((uint64)fTotalTransferred));
	
	HTTPTemp = wxString::Format(wxT("%8.2f %s"), fTotalSpeed/1024.0,_("kB/s"));
	Out.Replace(wxT("[TotalDownSpeed]"), HTTPTemp);
	OutE = m_Templates.sTransferUpLine;
	
	HTTPTemp = wxString::Format(wxT("%i"),m_Templates.iProgressbarWidth);
	Out.Replace(wxT("[PROGRESSBARWIDTHVAL]"),HTTPTemp);

	fTotalSize = 0;
	fTotalTransferred = 0;
	fTotalSpeed = 0;

	wxString sUpList;

	//upload list
	m_UploadsInfo.ReQuery();
	UploadsInfo::ItemIterator j = m_UploadsInfo.GetBeginIterator();
	while (j != m_UploadsInfo.GetEndIterator()) {
		wxString HTTPProcessData(OutE);
		HTTPProcessData.Replace(wxT("[1]"), j->sUserName);
		SharedFiles *file = m_SharedFilesInfo.GetByID(j->nHash);
		if ( !file ) {
			m_SharedFilesInfo.ReQuery();
			file = m_SharedFilesInfo.GetByID(j->nHash);
		}
		if ( file ) {
			HTTPProcessData.Replace(wxT("[2]"), file->sFileName);
		} else {
			HTTPProcessData.Replace(wxT("[2]"), _("Internal error - no item in container"));
		}
		HTTPProcessData.Replace(wxT("[3]"),
			CastItoXBytes(j->nTransferredDown) + wxT(" / ") + CastItoXBytes(j->nTransferredUp));
		HTTPProcessData.Replace(wxT("[4]"), CastItoXBytes(j->nSpeed) + wxT("/s"));
		
		fTotalSize += j->nTransferredDown;
		fTotalTransferred += j->nTransferredUp;
		fTotalSpeed += j->nSpeed;
		
		sUpList += HTTPProcessData;
		j++;
	}
	
	Out.Replace(wxT("[UploadFilesList]"), sUpList);
	// Elandal: cast from float to integral type always drops fractions.
	// avoids implicit cast warning
	Out.Replace(wxT("[TotalUpTransferred]"), CastItoXBytes(fTotalSize) + wxT(" / ") + CastItoXBytes(fTotalTransferred));
	Out.Replace(wxT("[TotalUpSpeed]"), CastItoXBytes(fTotalSpeed) + wxT(" /s"));

/*	gonosztopi - commented out, because ECv1 is now obsolete, and the corresponding 
	ECv2 methods are still missing.
	By the way, the ECv1 part from ExternalConn.cpp has already been deleted - so it
	didn't work ...
	if (m_Params.bShowUploadQueue) {
		Out.Replace(wxT("[UploadQueue]"), m_Templates.sTransferUpQueueShow);
		Out.Replace(wxT("[UploadQueueList]"), _("On Queue"));
		Out.Replace(wxT("[UserNameTitle]"), _("Username"));
		Out.Replace(wxT("[FileNameTitle]"), _("File Name"));
		Out.Replace(wxT("[ScoreTitle]"), _("Score"));
		Out.Replace(wxT("[BannedTitle]"), _("Banned"));

		OutE = m_Templates.sTransferUpQueueLine;
		// Replace [xx]
		wxString sQueue;
		wxString HTTPProcessData;

		//waiting list
		wxString sTransferWList = webInterface->SendRecvMsg(wxT("TRANSFER W_LIST"));
		while (sTransferWList.Length()>0) {
			int newLinePos=sTransferWList.First(wxT("\n"));

			wxString sEntry = sTransferWList.Left(newLinePos);
			sTransferWList = sTransferWList.Mid(newLinePos+1);

			char HTTPTempC[100] = "";
			HTTPProcessData = OutE;

			int brk = sTransferWList.First(wxT("\t"));
			if (brk==-1) {
				HTTPProcessData.Replace(wxT("[UserName]"), _SpecialChars(sEntry));
				continue;
			}
			HTTPProcessData.Replace(wxT("[UserName]"), _SpecialChars(sEntry.Left(brk)));
			sEntry=sEntry.Mid(brk+1); brk=sEntry.First(wxT("\t"));
			HTTPProcessData.Replace(wxT("[FileName]"), _SpecialChars(sEntry.Left(brk)));
			sEntry=sEntry.Mid(brk+1); brk=sEntry.First(wxT("\t"));
			sprintf(HTTPTempC, "%i" , (int)StrToLong(sEntry.Left(brk)));
			sEntry=sEntry.Mid(brk+1);
			
			wxString HTTPTemp = HTTPTemp.Format(wxT("%s"), HTTPTempC);
			HTTPProcessData.Replace(wxT("[Score]"), HTTPTemp);

			if (StrToLong(sEntry)) {
				HTTPProcessData.Replace(wxT("[Banned]"), _("Yes"));
			} else {
				HTTPProcessData.Replace(wxT("[Banned]"), _("No"));
			}
		}
		
		Out.Replace(wxT("[QueueList]"), sQueue);
	} else {
*/
		Out.Replace(wxT("[UploadQueue]"), m_Templates.sTransferUpQueueHide);
//	}

	Out.Replace(wxT("[ShowQueue]"), _("Show Queue"));
	Out.Replace(wxT("[HideQueue]"), _("Hide Queue"));
	Out.Replace(wxT("[ClearCompletedButton]"), m_Templates.sClearCompleted);
	Out.Replace(wxT("[CLEARCOMPLETED]"), _("Clear Completed"));

	Out.Replace(wxT("[DownloadList]"),
		wxString::Format(wxT("Downloads (%u)"), m_DownloadFilesInfo.ItemCount()));
	Out.Replace(wxT("[UploadList]"),
		wxString::Format(_("Uploads (%i)"), m_UploadsInfo.ItemCount()));
	Out.Replace(wxT("[CatSel]"), sCat);
	Out.Replace(wxT("[Session]"), sSession);

	return Out;
}


wxString CWebServer::_GetDownloadLink(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));

	if (!IsSessionAdmin(Data,sSession)) {
		wxString ad=ad.Format(wxT("<br><br><div align=\"center\" class=\"message\">[Message]</div>"));
		ad.Replace(wxT("[Message]"),_("Access denied!"));
		return ad;
	}
	
	wxString Out = m_Templates.sDownloadLink;

	Out.Replace(wxT("[Download]"), _("Download Selected"));
	Out.Replace(wxT("[Ed2klink]"), _("ED2K Link(s)"));
	Out.Replace(wxT("[Start]"), _("Start"));
	Out.Replace(wxT("[Session]"), sSession);

	// categories
	CECPacket req(EC_OP_GET_PREFERENCES, EC_DETAIL_WEB);
	req.AddTag(CECTag(EC_TAG_SELECT_PREFS, (uint32)EC_PREFS_CATEGORIES));
	CECPacket *reply = webInterface->SendRecvMsg_v2(&req);
	if (reply) {
		// if there are no categories, not even the EC_TAG_PREFS_CATEGORIES will be included :)	
		if (reply->GetTagCount()) {
			InsertCatBox(Out, 0, m_Templates.sCatArrow, reply->GetTagByIndex(0));
		} else {
			Out.Replace(wxT("[CATBOX]"), wxEmptyString);
		}
		delete reply;
	} else {
		Out.Replace(wxT("[CATBOX]"), wxEmptyString);
	}

	return Out;
}


wxString CWebServer::_GetSharedFilesList(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));
	
	//
	// commands: setpriority, reload
	if (!_ParseURL(Data, wxT("hash")).IsEmpty() && !_ParseURL(Data, wxT("setpriority")).IsEmpty() && IsSessionAdmin(Data,sSession)) {
		CECTag tag(EC_TAG_KNOWNFILE, CMD4Hash(_ParseURL(Data, wxT("hash"))));
		tag.AddTag(CECTag(EC_TAG_PARTFILE_PRIO, (uint32)StrToLong(_ParseURL(Data, wxT("setpriority")))));
		CECPacket prio_req(EC_OP_SHARED_SET_PRIO);
		prio_req.AddTag(tag);
		Send_Discard_V2_Request(&prio_req);
	}
	if (_ParseURL(Data, wxT("reload")) == wxT("true")) {
		CECPacket req(EC_OP_SHAREDFILES_RELOAD);
		Send_Discard_V2_Request(&req);
	}

	wxString sSortRev = _ParseURL(Data, wxT("sortreverse"));
	wxString sSort = _ParseURL(Data, wxT("sort"));

	m_SharedFilesInfo.SetSortOrder(sSort, sSortRev);

	//Name sorting link
	wxString Out = m_Templates.sSharedList;
	m_SharedFilesInfo.ProcessHeadersLine(Out);

	Out.Replace(wxT("[Session]"), sSession);

	Out.Replace(wxT("[Message]"), wxEmptyString);

	wxString OutE = m_Templates.sSharedLine; 

	wxString OutE2 = m_Templates.sSharedLineChanged; 
	
	// Populating array
	m_SharedFilesInfo.ReQuery();	

	// Displaying
	wxString sSharedList;
	SharedFilesInfo::ItemIterator i = m_SharedFilesInfo.GetBeginIterator();
	while (i != m_SharedFilesInfo.GetEndIterator()) {
		wxString HTTPProcessData;
		if (i->sFileHash == _ParseURL(Data,wxT("hash")))
			HTTPProcessData = OutE2;
		else
			HTTPProcessData = OutE;

		HTTPProcessData.Replace(wxT("[FileName]"), i->sFileName);
		if (i->sFileName.Length() > SHORT_FILENAME_LENGTH)
			HTTPProcessData.Replace(wxT("[ShortFileName]"), i->sFileName.Left(SHORT_FILENAME_LENGTH) + wxT("..."));
		else
			HTTPProcessData.Replace(wxT("[ShortFileName]"), i->sFileName);

		HTTPProcessData.Replace(wxT("[FileSize]"), CastItoXBytes(i->lFileSize));
		HTTPProcessData.Replace(wxT("[FileLink]"), i->sED2kLink);

		HTTPProcessData.Replace(wxT("[FileTransferred]"), CastItoXBytes(i->nFileTransferred));

		HTTPProcessData.Replace(wxT("[FileAllTimeTransferred]"), CastItoXBytes(i->nFileAllTimeTransferred));

		HTTPProcessData.Replace(wxT("[FileRequests]"), wxString::Format(wxT("%i"), i->nFileRequests));

		HTTPProcessData.Replace(wxT("[FileAllTimeRequests]"), wxString::Format(wxT("%i"), i->nFileAllTimeRequests));

		HTTPProcessData.Replace(wxT("[FileAccepts]"), wxString::Format(wxT("%i"), i->nFileAccepts));

		HTTPProcessData.Replace(wxT("[FileAllTimeAccepts]"), wxString::Format(wxT("%i"), i->nFileAllTimeAccepts));

		HTTPProcessData.Replace(wxT("[Priority]"), Prio2Str(i->nFilePriority, i->bFileAutoPriority));

		HTTPProcessData.Replace(wxT("[FileHash]"), i->sFileHash);

		HTTPProcessData.Replace(wxT("[PriorityUpLink]"),
			wxT("hash=") +  i->sFileHash + wxString::Format(wxT("&setpriority=%i"),
				GetHigherPrioShared(i->nFilePriority, i->bFileAutoPriority)));

		HTTPProcessData.Replace(wxT("[PriorityDownLink]"),
			wxT("hash=") +  i->sFileHash + wxString::Format(wxT("&setpriority=%i"),
				GetLowerPrioShared(i->nFilePriority, i->bFileAutoPriority)));

		sSharedList += HTTPProcessData;
		i++;
	}
	Out.Replace(wxT("[SharedFilesList]"), sSharedList);
	Out.Replace(wxT("[Session]"), sSession);
	Out.Replace(wxT("[PriorityUp]"), _("Increase Priority"));
	Out.Replace(wxT("[PriorityDown]"), _("Decrease Priority"));
	Out.Replace(wxT("[Reload]"), _("Reload List"));

	return Out;
}


wxString CWebServer::_GetGraphs(ThreadData Data) {
	
	wxString Out = m_Templates.sGraphs;	
	wxString sGraphDownload, sGraphUpload, sGraphCons;
	wxString sTmp;	
	wxString sSession = _ParseURL(Data, wxT("ses"));
    
	CECPacket *request = new CECPacket(EC_OP_GET_PREFERENCES);
	if (!request) {
		return wxEmptyString;
	}

	uint32 max_ul = 0;
	uint32 max_dl = 0;
	uint16 max_conn = 0;	

	request->AddTag(CECTag(EC_TAG_SELECT_PREFS, (uint32)EC_PREFS_CONNECTIONS));
	CECPacket *response = webInterface->SendRecvMsg_v2(request);
	delete request;

	if (response) {
		CECTag *t1 = response->GetTagByIndex(0);
		CECTag *t2 = t1 ? t1->GetTagByName(EC_TAG_CONN_UL_CAP) : NULL;
		CECTag *t3 = t1 ? t1->GetTagByName(EC_TAG_CONN_DL_CAP) : NULL;
		CECTag *t4 = t1 ? t1->GetTagByName(EC_TAG_CONN_MAX_CONN) : NULL;
		if(t1 && t2 && t3 && t4) {
			max_ul = t2->GetInt32Data();
			max_dl = t3->GetInt32Data();
			max_conn = t4->GetInt16Data();
			delete response;
		} else {
			delete response;
			return wxEmptyString;
		}
	} else {
		return wxEmptyString;
	}
	
	request = new CECPacket(EC_OP_GET_STATSGRAPHS, EC_DETAIL_WEB);
	if (!request) {
		return wxEmptyString;
	}
	request->AddTag(CECTag(EC_TAG_STATSGRAPH_WIDTH, m_nGraphWidth));
	request->AddTag(CECTag(EC_TAG_STATSGRAPH_SCALE, m_nGraphScale));
	if (_ParseURL(Data, wxT("refetch")) != wxT("yes")) {
		if (!m_sLastHistoryTimeStamp.IsEmpty()) {
			request->AddTag(CECTag(EC_TAG_STATSGRAPH_LAST, m_sLastHistoryTimeStamp));
		}
	}
	
	response = webInterface->SendRecvMsg_v2(request);
	delete request;	
	if (response) {
		CECTag *t1 = response->GetTagByName(EC_TAG_STATSGRAPH_LAST);
		CECTag *dataTag = response->GetTagByName(EC_TAG_STATSGRAPH_DATA);
		if (response->GetOpCode() == EC_OP_STATSGRAPHS && t1 && dataTag) {
			m_sLastHistoryTimeStamp = t1->GetStringData();
			const uint32 *data = (const uint32 *)dataTag->GetTagData();
			unsigned int numItems = dataTag->GetTagDataLen() / sizeof(uint32);
			if (_ParseURL(Data, wxT("refetch")) == wxT("yes")) {
				if (numItems / 3 < m_nGraphWidth) {
					while (m_Params.PointsForWeb.GetCount() > 0) {
						delete m_Params.PointsForWeb[0];
						m_Params.PointsForWeb.RemoveAt(0);
					}
				}
			}
			for (unsigned int i = 0; i < numItems;) {
				UpDown *dataLine = new UpDown;
				dataLine->download = ntohl(data[i++]);
				dataLine->upload = ntohl(data[i++]);
				dataLine->connections = ntohl(data[i++]);
				m_Params.PointsForWeb.Add(dataLine);
				while (m_Params.PointsForWeb.GetCount() > m_nGraphWidth) {
					delete m_Params.PointsForWeb[0];
					m_Params.PointsForWeb.RemoveAt(0);
				}
	
			}
			delete response;
		} else {
			delete response;
			return wxEmptyString;
		}
	} else {
		return wxEmptyString;
	}
	
	for (size_t i = 0; i < m_nGraphWidth; ++i) {
		if (i < m_Params.PointsForWeb.GetCount()) {
			if (i != 0) {
				sGraphDownload.Append(wxT(","));
				sGraphUpload.Append(wxT(","));
				sGraphCons.Append(wxT(","));
			}
			// download
			sTmp = wxString::Format(wxT("%d") , (uint32) (m_Params.PointsForWeb[i]->download));
			sGraphDownload += sTmp;
			// upload
			sTmp = wxString::Format(wxT("%d") , (uint32) (m_Params.PointsForWeb[i]->upload));
			sGraphUpload += sTmp;
			// connections
			sTmp = wxString::Format(wxT("%d") , (uint32) (m_Params.PointsForWeb[i]->connections));
			sGraphCons += sTmp;
		}
	}

	Out.Replace(wxT("[GraphDownload]"), sGraphDownload);
	Out.Replace(wxT("[GraphUpload]"), sGraphUpload);
	Out.Replace(wxT("[GraphConnections]"), sGraphCons);

	Out.Replace(wxT("[TxtDownload]"), _("Download"));
	Out.Replace(wxT("[TxtUpload]"), _("Upload"));
	Out.Replace(wxT("[TxtTime]"), _("Time"));
	Out.Replace(wxT("[TxtConnections]"), _("Active connections"));
	Out.Replace(wxT("[KByteSec]"), _("kB/s"));
	Out.Replace(wxT("[TxtTime]"), _("Time"));

	wxString sScale;
	sScale = CastSecondsToHM(m_nGraphScale * m_nGraphWidth);

	wxString s1, s2, s3;
	s1 = wxString::Format(wxT("%u"), max_dl + 4);
	s2 = wxString::Format(wxT("%u"), max_ul + 4);
	s3 = wxString::Format(wxT("%u"), max_conn + 20);
	
	Out.Replace(wxT("[ScaleTime]"), sScale);
	Out.Replace(wxT("[MaxDownload]"), s1);
	Out.Replace(wxT("[MaxUpload]"), s2);
	Out.Replace(wxT("[MaxConnections]"), s3);

	Out.Replace(wxT("[GraphHeight]"), wxString::Format(wxT("%u"), m_nGraphHeight));
	Out.Replace(wxT("[GraphWidth]"), wxString::Format(wxT("%u"), m_nGraphWidth));

	Out.Replace(wxT("[Session]"), sSession);
	Out.Replace(wxT("[Refetch]"), _("Refetch graph data"));

	return Out;
}


wxString CWebServer::_GetAddServerBox(ThreadData Data) {	

	wxString sSession = _ParseURL(Data, wxT("ses"));

	if (!IsSessionAdmin(Data,sSession)) return wxEmptyString;

	wxString Out = m_Templates.sAddServerBox;
	wxString messageString;

	if (_ParseURL(Data, wxT("addserver")) == wxT("true")) {
		wxString sIP = _ParseURL(Data, wxT("serveraddr"));
		wxString sPort = _ParseURL(Data, wxT("serverport"));
		wxString sName = _ParseURL(Data, wxT("servername"));
		
		wxString ed2k = wxT("ed2k://|server|") + sIP + wxT("|") + sPort + wxT("|/");
		CECPacket request(EC_OP_ED2K_LINK);
		request.AddTag(CECTag(EC_TAG_STRING, ed2k)),
		Send_Discard_V2_Request(&request);

		CECPacket req(EC_OP_GET_LAST_LOG_ENTRY);
		CECPacket *response = webInterface->SendRecvMsg_v2(&req);
		if (response) {
			messageString =
				_SpecialChars(response->GetTagByIndexSafe(0)->GetStringData());
			delete response;
		}
	} else if (_ParseURL(Data, wxT("updateservermetfromurl")) == wxT("true")) {
		CECPacket request(EC_OP_SERVER_UPDATE_FROM_URL);
		request.AddTag(CECTag(EC_TAG_STRING, _ParseURL(Data, wxT("servermeturl"))));
		Send_Discard_V2_Request(&request);
		
		CECPacket req(EC_OP_GET_LAST_LOG_ENTRY);
		CECPacket *response = webInterface->SendRecvMsg_v2(&req);
		if (response) {
			messageString =
				_SpecialChars(response->GetTagByIndexSafe(0)->GetStringData());
			delete response;
		}
	}
	
	Out.Replace(wxT("[Message]"), messageString);
	Out.Replace(wxT("[AddServer]"), _("Received %d new servers"));
	Out.Replace(wxT("[IP]"), _("IP or Address"));
	Out.Replace(wxT("[Port]"), _("Port"));
	Out.Replace(wxT("[Name]"), _("Name"));
	Out.Replace(wxT("[Add]"), _("Add to list"));
	Out.Replace(wxT("[UpdateServerMetFromURL]"), _("Update server.met from URL"));
	Out.Replace(wxT("[URL]"), _("URL"));
	Out.Replace(wxT("[Apply]"), _("Apply"));

	return Out;
}


wxString CWebServer::_GetWebSearch(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));
    
	wxString Out = m_Templates.sWebSearch;
	if (_ParseURL(Data, wxT("tosearch")) != wxEmptyString) {

		Out += wxT("\n<script language=\"javascript\">\n searchwindow=window.open('http://www.filedonkey.com/fdsearch/index.php?media=") +
					_ParseURL(Data, wxT("media")) +
					wxT("&pattern=") + 
					 _ParseURL(Data, wxT("tosearch")) + 
					wxT("&action=search&name=FD-Search&op=modload&file=index&requestby=amule','searchwindow');\n</script>");

	}
	
	Out.Replace(wxT("[Session]"), sSession);
	Out.Replace(wxT("[Name]"), _("Name"));
	Out.Replace(wxT("[Type]"), _("Type"));
	Out.Replace(wxT("[Any]"), _("Any"));
	Out.Replace(wxT("[Archives]"), _("Archive"));
	Out.Replace(wxT("[Audio]"), _("Audio"));
	Out.Replace(wxT("[CD-Images]"), _("CD-Images"));
	Out.Replace(wxT("[Pictures]"), _("Pictures"));
	Out.Replace(wxT("[Programs]"), _("Programs"));
	Out.Replace(wxT("[Texts]"), _("Texts"));
	Out.Replace(wxT("[Videos]"), _("Video"));
	Out.Replace(wxT("[Search]"), _("Start"));
	Out.Replace(wxT("[WebSearch]"), _("Web-based Search"));
	
	return Out;
}


wxString CWebServer::_GetLog(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));

	wxString Out = m_Templates.sLog;

	if (_ParseURL(Data, wxT("clear")) == wxT("yes") && IsSessionAdmin(Data,sSession)) {
		CECPacket req(EC_OP_RESET_LOG);
		Send_Discard_V2_Request(&req);
	}
	
	Out.Replace(wxT("[Clear]"), _("Reset"));
	CECPacket req(EC_OP_GET_LOG);
	CECPacket *response = webInterface->SendRecvMsg_v2(&req);
	wxString logString;
	if (response) {
		logString =
			_SpecialChars(wxGetTranslation(response->GetTagByIndexSafe(0)->GetStringData()));
		delete response;
	}
	logString += wxT("<br><a name=\"end\"></a>");
	Out.Replace(wxT("[Log]"), logString);
	Out.Replace(wxT("[Session]"), sSession);

	return Out;
}


wxString CWebServer::_GetServerInfo(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));

	wxString Out = m_Templates.sServerInfo;

	if (_ParseURL(Data, wxT("clear")) == wxT("yes")) {
		CECPacket req(EC_OP_CLEAR_SERVERINFO);
		Send_Discard_V2_Request(&req);
	}
	
	CECPacket req(EC_OP_GET_SERVERINFO);
	CECPacket *response = webInterface->SendRecvMsg_v2(&req);
	Out.Replace(wxT("[Clear]"), _("Reset"));
	wxString serverInfoString;
	if (response) {
		serverInfoString =
			_SpecialChars(response->GetTagByIndexSafe(0)->GetStringData());
		delete response;
	}
	Out.Replace(wxT("[ServerInfo]"), serverInfoString);
	Out.Replace(wxT("[Session]"), sSession);

	return Out;
}


//shakraw, this is useless in amule 'cause debuglog and log windows are the same.
//so, at the moment, GETALLDEBUGLOGENTRIES has the same behaviour of GETALLLOGENTRIES.
//Note that, when clearing, the log file ~/.aMule/logfile will not be removed here.
wxString CWebServer::_GetDebugLog(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));

	wxString Out = m_Templates.sDebugLog;

	if (_ParseURL(Data, wxT("clear")) == wxT("yes") && IsSessionAdmin(Data,sSession)) {
		CECPacket req(EC_OP_RESET_DEBUGLOG);
		Send_Discard_V2_Request(&req);
	}
	Out.Replace(wxT("[Clear]"), _("Reset"));

	CECPacket req(EC_OP_GET_DEBUGLOG);
	CECPacket *response = webInterface->SendRecvMsg_v2(&req);
	wxString debugLogString;
	if (response) {
		debugLogString =
			_SpecialChars(response->GetTagByIndexSafe(0)->GetStringData());
		delete response;
	}
	debugLogString += wxT("<br><a name=\"end\"></a>");
	Out.Replace(wxT("[DebugLog]"), debugLogString);
	Out.Replace(wxT("[Session]"), sSession);

	return Out;
}


wxString ECTree2Html(CECTag *tree, int depth)
{
        wxString result;

	if (tree) {
		for (int i = 0; i < depth; ++i) {
			result += wxT("&nbsp;&nbsp;&nbsp;");
		}	
		result += tree->GetStringData() + wxT("\r\n");
        	for (int i = 0; i < tree->GetTagCount(); ++i) {
                	result += ECTree2Html(tree->GetTagByIndex(i), depth + 1);
	        }
	}
	
        return result;
}

wxString CWebServer::_GetStats(ThreadData Data) {

	webInterface->DebugShow(wxT("***_GetStats arrived\n"));

	wxString sSession = _ParseURL(Data, wxT("ses"));

	wxString Out = m_Templates.sStats;
	wxString sStats;
	
	CECPacket req(EC_OP_GET_STATSTREE, EC_DETAIL_WEB);
	CECPacket *response = webInterface->SendRecvMsg_v2(&req);
	if (response) {
		CECTag *serverVersion = response->GetTagByName(EC_TAG_SERVER_VERSION);
		CECTag *userNick = response->GetTagByName(EC_TAG_USER_NICK);
		if (serverVersion && userNick) {
			sStats = wxString::Format(wxT("<b>aMule v%s %s [%s]</b>\r\n<br><br>\r\n"),
				serverVersion->GetStringData().GetData(), _("Statistics"),
				userNick->GetStringData().GetData());
			sStats += ECTree2Html(response->GetTagByName(EC_TAG_TREE), 0);
		}
		delete response;
	}

	Out.Replace(wxT("[STATSDATA]"), sStats);

	return Out;
}


wxString CWebServer::_GetPreferences(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));

	wxString Out = m_Templates.sPreferences;
	Out.Replace(wxT("[Session]"), sSession);

	if ((_ParseURL(Data, wxT("saveprefs")) == wxT("true")) && IsSessionAdmin(Data,sSession) ) {
		CECPacket prefs(EC_OP_SET_PREFERENCES);
		CECEmptyTag filePrefs(EC_TAG_PREFS_FILES);
		CECEmptyTag connPrefs(EC_TAG_PREFS_CONNECTIONS);
		CECEmptyTag webPrefs(EC_TAG_PREFS_REMOTECTRL);
		if (_ParseURL(Data, wxT("gzip")) == wxT("true") || _ParseURL(Data, wxT("gzip")) == wxT("on")) {
			webPrefs.AddTag(CECTag(EC_TAG_WEBSERVER_USEGZIP, (uint8)1));
			webInterface->m_UseGzip = true;
		}
		if (_ParseURL(Data, wxT("gzip")) == wxT("false") || _ParseURL(Data, wxT("gzip")) == wxEmptyString) {
			webPrefs.AddTag(CECTag(EC_TAG_WEBSERVER_USEGZIP, (uint8)0));
			webInterface->m_UseGzip = false;
		}
		if (_ParseURL(Data, wxT("showuploadqueue")) == wxT("true") || _ParseURL(Data, wxT("showuploadqueue")) == wxT("on") ) {
			m_Params.bShowUploadQueue = true;
		}
		if(_ParseURL(Data, wxT("showuploadqueue")) == wxT("false") || _ParseURL(Data, wxT("showuploadqueue")) == wxEmptyString) {
			m_Params.bShowUploadQueue = false;
		}
		if (_ParseURL(Data, wxT("refresh")) != wxEmptyString) {
			webPrefs.AddTag(CECTag(EC_TAG_WEBSERVER_REFRESH, (uint32)StrToLong(_ParseURL(Data, wxT("refresh")))));
			webInterface->m_PageRefresh = StrToLong(_ParseURL(Data, wxT("refresh")));
		}
		if (_ParseURL(Data, wxT("maxdown")) != wxEmptyString) {
			connPrefs.AddTag(CECTag(EC_TAG_CONN_MAX_DL, (uint16)StrToLong(_ParseURL(Data, wxT("maxdown")))));
		}
		if (_ParseURL(Data, wxT("maxup")) != wxEmptyString) {
			connPrefs.AddTag(CECTag(EC_TAG_CONN_MAX_UL, (uint16)StrToLong(_ParseURL(Data, wxT("maxup")))));
		}
		if (_ParseURL(Data, wxT("maxcapdown")) != wxEmptyString) {
			connPrefs.AddTag(CECTag(EC_TAG_CONN_DL_CAP, (uint32)StrToLong(_ParseURL(Data, wxT("maxcapdown")))));
		}
		if (_ParseURL(Data, wxT("maxcapup")) != wxEmptyString) {
			connPrefs.AddTag(CECTag(EC_TAG_CONN_UL_CAP, (uint32)StrToLong(_ParseURL(Data, wxT("maxcapup")))));
		}
		if (_ParseURL(Data, wxT("maxsources")) != wxEmptyString) {
			connPrefs.AddTag(CECTag(EC_TAG_CONN_MAX_FILE_SOURCES, (uint16)StrToLong(_ParseURL(Data, wxT("maxsources")))));
		}
		if (_ParseURL(Data, wxT("maxconnections")) != wxEmptyString) {
			connPrefs.AddTag(CECTag(EC_TAG_CONN_MAX_CONN, (uint16)StrToLong(_ParseURL(Data, wxT("maxconnections")))));
		}
		if (_ParseURL(Data, wxT("maxconnectionsperfive")) != wxEmptyString) {
			CECEmptyTag twPrefs(EC_TAG_PREFS_CORETWEAKS);
			twPrefs.AddTag(CECTag(EC_TAG_CORETW_MAX_CONN_PER_FIVE,
				(uint16)StrToLong(_ParseURL(Data, wxT("maxconnectionsperfive")))));
			prefs.AddTag(twPrefs);
		}
		if (_ParseURL(Data, wxT("gwidth")) != wxEmptyString) {
			m_nGraphWidth = (uint16)StrToLong(_ParseURL(Data, wxT("gwidth")));
		}
		if (_ParseURL(Data, wxT("gheight")) != wxEmptyString) {
			m_nGraphHeight = (uint16)StrToLong(_ParseURL(Data, wxT("gheight")));
		}
		if (_ParseURL(Data, wxT("gscale")) != wxEmptyString) {
			m_nGraphScale = (uint16)StrToLong(_ParseURL(Data, wxT("gscale")));
		}

		filePrefs.AddTag(CECTag(EC_TAG_FILES_UL_FULL_CHUNKS,
			(uint8)((_ParseURL(Data, wxT("fullchunks")).MakeLower() == wxT("on")) ? 1 : 0)));
		filePrefs.AddTag(CECTag(EC_TAG_FILES_PREVIEW_PRIO,
			(uint8)((_ParseURL(Data, wxT("firstandlast")).MakeLower() == wxT("on")) ? 1 : 0)));

		prefs.AddTag(filePrefs);
		prefs.AddTag(connPrefs);

		if (webInterface->m_LoadSettingsFromAmule) {
			prefs.AddTag(webPrefs);
		} else {
			if (!wxFileName::DirExists(otherfunctions::GetConfigDir())) {
				wxFileName::Mkdir(otherfunctions::GetConfigDir());
			}
			if (!webInterface->m_configFile) {
				webInterface->m_configFile = new CECFileConfig(webInterface->m_configFileName);
			}
			if (webInterface->m_configFile) {
				webInterface->m_configFile->Write(wxT("/Webserver/UseGzip"), webInterface->m_UseGzip);
				webInterface->m_configFile->Write(wxT("/Webserver/PageRefreshTime"), (long)webInterface->m_PageRefresh);
				webInterface->m_configFile->Write(wxT("/Webserver/GraphHeight"), (long)m_nGraphHeight);
				webInterface->m_configFile->Write(wxT("/Webserver/GraphWidth"), (long)m_nGraphWidth);
				webInterface->m_configFile->Write(wxT("/Webserver/GraphScale"), (long)m_nGraphScale);
				// ensure that changes get written to file in case amuleweb crashes (it won't, just in case)
				webInterface->m_configFile->Flush();
			}
		}

		Send_Discard_V2_Request(&prefs);
	}

	CECPacket req(EC_OP_GET_PREFERENCES);
	uint32 prefsSelect = EC_PREFS_CONNECTIONS | EC_PREFS_FILES | EC_PREFS_CORETWEAKS;
	if (webInterface->m_LoadSettingsFromAmule) {
		prefsSelect |= EC_PREFS_REMOTECONTROLS;
	}
	req.AddTag(CECTag(EC_TAG_SELECT_PREFS, prefsSelect));
	CECPacket *response = webInterface->SendRecvMsg_v2(&req);
	if (response) {
		CECTag *filePrefs = response->GetTagByName(EC_TAG_PREFS_FILES);
		CECTag *connPrefs = response->GetTagByName(EC_TAG_PREFS_CONNECTIONS);
		CECTag *connMaxFileSources = connPrefs ? connPrefs->GetTagByName(EC_TAG_CONN_MAX_FILE_SOURCES) : NULL;
		CECTag *connMaxConn = connPrefs ? connPrefs->GetTagByName(EC_TAG_CONN_MAX_CONN) : NULL;
		CECTag *prefsCoreTweaks = response->GetTagByName(EC_TAG_PREFS_CORETWEAKS);
		CECTag *coreTwMaxConnPerFive = prefsCoreTweaks ? prefsCoreTweaks->GetTagByName(EC_TAG_CORETW_MAX_CONN_PER_FIVE) : NULL;
		CECTag *connMaxDl = connPrefs ? connPrefs->GetTagByName(EC_TAG_CONN_MAX_DL) : NULL;
		CECTag *connMaxUl = connPrefs ? connPrefs->GetTagByName(EC_TAG_CONN_MAX_UL) : NULL;
		CECTag *connDlCap = connPrefs ? connPrefs->GetTagByName(EC_TAG_CONN_DL_CAP) : NULL;
		CECTag *connUlCap = connPrefs ? connPrefs->GetTagByName(EC_TAG_CONN_UL_CAP) : NULL;
		if (webInterface->m_LoadSettingsFromAmule) {
			CECTag *webPrefs = response->GetTagByName(EC_TAG_PREFS_REMOTECTRL);
			CECTag *webserverRefresh = webPrefs ? webPrefs->GetTagByName(EC_TAG_WEBSERVER_REFRESH) : NULL;
			if (webPrefs) {
				webInterface->m_UseGzip = (webPrefs->GetTagByName(EC_TAG_WEBSERVER_USEGZIP) != NULL);
			}
			if (webserverRefresh) {
				webInterface->m_PageRefresh = webserverRefresh->GetInt32Data();
			}
		}
		if (	filePrefs && connPrefs && 
			connMaxFileSources && connMaxConn && prefsCoreTweaks &&
			coreTwMaxConnPerFive && connMaxDl && connMaxUl && connDlCap &&
			connUlCap ) {
			if (webInterface->m_UseGzip) {
				Out.Replace(wxT("[UseGzipVal]"), wxT("checked"));
			} else {
				Out.Replace(wxT("[UseGzipVal]"), wxEmptyString);
			}
			if(m_Params.bShowUploadQueue) {
				Out.Replace(wxT("[ShowUploadQueueVal]"), wxT("checked"));
			} else {
				Out.Replace(wxT("[ShowUploadQueueVal]"), wxEmptyString);
			}
			if (filePrefs->GetTagByName(EC_TAG_FILES_PREVIEW_PRIO)) {
				Out.Replace(wxT("[FirstAndLastVal]"), wxT("checked"));
			} else {
				Out.Replace(wxT("[FirstAndLastVal]"), wxEmptyString);
			}
			if (filePrefs->GetTagByName(EC_TAG_FILES_UL_FULL_CHUNKS)) {
				Out.Replace(wxT("[FullChunksVal]"), wxT("checked"));
			} else {
				Out.Replace(wxT("[FullChunksVal]"), wxEmptyString);
			}
		
			Out.Replace(wxT("[RefreshVal]"), wxString::Format(wxT("%u"), webInterface->m_PageRefresh));
			Out.Replace(wxT("[MaxSourcesVal]"), wxString::Format(wxT("%i"), connMaxFileSources->GetInt16Data()));
			Out.Replace(wxT("[MaxConnectionsVal]"), wxString::Format(wxT("%i"), connMaxConn->GetInt16Data()));
			Out.Replace(wxT("[MaxConnectionsPer5Val]"), wxString::Format(wxT("%i"), coreTwMaxConnPerFive->GetInt16Data()));
	
			wxString colon(wxT(":"));
			Out.Replace(wxT("[KBS]"), _("kB/s"));
			Out.Replace(wxT("[FileSettings]"), _("File Settings"));
			Out.Replace(wxT("[LimitForm]"), _("Connection Limits"));
			Out.Replace(wxT("[MaxSources]"), _("Max Sources Per File")+colon);
			Out.Replace(wxT("[MaxConnections]"), _("Max. Connections")+colon);
			Out.Replace(wxT("[MaxConnectionsPer5]"), _("Max. new connections / 5secs")+colon);
			Out.Replace(wxT("[UseGzipForm]"), _("Gzip Compression"));
			Out.Replace(wxT("[UseGzipComment]"), _("Save traffic, especially in graphs."));
			Out.Replace(wxT("[ShowUploadQueueForm]"), _("Show Queue"));
			Out.Replace(wxT("[ShowUploadQueueComment]"),
			_("Enable or disable the display of waiting queue in transfer page."));
			Out.Replace(wxT("[ShowQueue]"), _("Show Queue"));
			Out.Replace(wxT("[HideQueue]"), _("Hide Queue"));
			Out.Replace(wxT("[RefreshTimeForm]"), _("Refresh-Time of Pages"));
			Out.Replace(wxT("[RefreshTimeComment]"), _("Time in seconds (zero=disabled)")+colon);
			Out.Replace(wxT("[SpeedForm]"), _("Speed Limits"));
			Out.Replace(wxT("[MaxDown]"), _("Download"));
			Out.Replace(wxT("[MaxUp]"), _("Upload"));
			Out.Replace(wxT("[SpeedCapForm]"), _("Bandwidth Limits"));
			Out.Replace(wxT("[MaxCapDown]"), _("Download"));
			Out.Replace(wxT("[MaxCapUp]"), _("Upload"));
			Out.Replace(wxT("[TryFullChunks]"), _("Try to transfer full chunks to all uploads"));
			Out.Replace(wxT("[FirstAndLast]"), _("Try to download first and last chunks first"));
			Out.Replace(wxT("[WebControl]"), _("Web Control Panel"));
			Out.Replace(wxT("[aMuleAppName]"), wxT("aMule"));
			Out.Replace(wxT("[GraphSettings]"), _("Statistics graphs' settings"));
			Out.Replace(wxT("[GraphHeightText]"), _("Graph height")+colon);
			Out.Replace(wxT("[GraphWidthText]"), _("Graph width")+colon);
			Out.Replace(wxT("[pixels]"), _("pixels"));
			Out.Replace(wxT("[GraphHeight]"), wxString::Format(wxT("%u"), m_nGraphHeight));
			Out.Replace(wxT("[GraphWidth]"), wxString::Format(wxT("%u"), m_nGraphWidth));
			Out.Replace(wxT("[GraphScaleText]"), _("In the graph, each pixel represents"));
			Out.Replace(wxT("[seconds]"), _("seconds"));
			Out.Replace(wxT("[GraphScale]"), wxString::Format(wxT("%u"), m_nGraphScale));
			Out.Replace(wxT("[Apply]"), _("Apply"));
		
			int n = connMaxDl->GetInt16Data();
			if (n < 0 || n == 65535) {
				n = 0;
			}
			Out.Replace(wxT("[MaxDownVal]"), wxString::Format(wxT("%d"), n));
			
			n = connMaxUl->GetInt16Data();
			if (n < 0 || n == 65535)  {
				n = 0;
			}
			Out.Replace(wxT("[MaxUpVal]"), wxString::Format(wxT("%d"), n));
			
			Out.Replace(wxT("[MaxCapDownVal]"), wxString::Format(wxT("%i"),
				connDlCap->GetInt32Data()));
			
			Out.Replace(wxT("[MaxCapUpVal]"), wxString::Format(wxT("%i"),
				connUlCap->GetInt32Data()));
		} else {
			Out.Clear();
		}
		delete response;
	} else {
		Out.Clear();
	}
	
	return Out;
}


wxString CWebServer::_GetLoginScreen(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));

	wxString Out = m_Templates.sLogin;

	Out.Replace(wxT("[CharSet]"), WEBCHARSET);
	Out.Replace(wxT("[aMulePlus]"), wxT("aMule"));
	Out.Replace(wxT("[aMuleAppName]"), wxT("aMule"));
	Out.Replace(wxT("[version]"), wxT(VERSION)); //shakraw - was CURRENT_VERSION_LONG);
	Out.Replace(wxT("[Login]"), _("Login"));
	Out.Replace(wxT("[EnterPassword]"), _("Enter your password here"));
	Out.Replace(wxT("[LoginNow]"), _("Login Now"));
	Out.Replace(wxT("[WebControl]"), _("Web Control Panel"));

	return Out;
}


wxString CWebServer::_GetConnectedServer(ThreadData Data) {
	wxString sSession = _ParseURL(Data, wxT("ses"));
	wxString OutS = m_Templates.sConnectedServer;
	OutS.Replace(wxT("[ConnectedServer]"), _("Server"));
	OutS.Replace(wxT("[Servername]"), _("Server name"));
	OutS.Replace(wxT("[Status]"), _("Status"));
	OutS.Replace(wxT("[Usercount]"), _("users"));
	OutS.Replace(wxT("[Action]"), _("Connecting"));
	OutS.Replace(wxT("[URL_Disconnect]"),
		IsSessionAdmin(Data,sSession) ?
			wxString::Format(wxT("?ses=%s&w=server&c=disconnect"), sSession.GetData()) :
			GetPermissionDenied());
	OutS.Replace(wxT("[URL_Connect]"),
		IsSessionAdmin(Data,sSession) ?
			wxString::Format(wxT("?ses=%s&w=server&c=connect"), sSession.GetData()) :
			GetPermissionDenied());
	OutS.Replace(wxT("[Disconnect]"), _("Disconnect"));
	OutS.Replace(wxT("[Connect]"), _("Connect to any server"));
	OutS.Replace(wxT("[URL_ServerOptions]"),
		IsSessionAdmin(Data,sSession) ?
			wxString::Format(wxT("?ses=%s&w=server&c=options"), sSession.GetData()) :
			GetPermissionDenied());
	OutS.Replace(wxT("[ServerOptions]"), _("Server Preferences"));
	OutS.Replace(wxT("[WebSearch]"), _("Web-based Search"));

	CECPacket connstate_req(EC_OP_GET_CONNSTATE, EC_DETAIL_WEB);
	CECPacket *sServerStat = webInterface->SendRecvMsg_v2(&connstate_req);
	CEC_ConnState_Tag *tag = sServerStat ? (CEC_ConnState_Tag *)sServerStat->GetTagByIndexSafe(0) : NULL;
	if (sServerStat && tag) {
		switch (tag->ClientID()) {
		case 0:
			OutS.Replace(wxT("[1]"), _("Disconnected"));
			OutS.Replace(wxT("[2]"), wxEmptyString);
			OutS.Replace(wxT("[3]"), wxEmptyString);
			break;
		case 0xffffffff:
			OutS.Replace(wxT("[1]"), _("Connecting"));
			OutS.Replace(wxT("[2]"), wxEmptyString);
			OutS.Replace(wxT("[3]"), wxEmptyString);
			break;
		default:
			CECTag *server = tag->GetTagByIndex(0);
			CECTag *serverName = server ? server->GetTagByName(EC_TAG_SERVER_NAME) : NULL;
			CECTag *serverUsers = server ? server->GetTagByName(EC_TAG_SERVER_USERS) : NULL;
			if (server && serverName && serverUsers) {
				OutS.Replace(wxT("[1]"), wxString(_("Connected ")) +
					(tag->HaveLowID() ? wxString(_("with LowID")) : wxString(_("with HighID"))));
				OutS.Replace(wxT("[2]"), serverName->GetStringData());
				OutS.Replace(wxT("[3]"), wxString::Format(wxT("%10i"), serverUsers->GetInt32Data()));
			} else {
				OutS.Clear();
			}
			break;
		}
		delete sServerStat;
	} else {
		OutS.Clear();
	}
	return OutS;
}


// We have to add gz-header and some other stuff
// to standard zlib functions in order to use gzip in web pages
int CWebServer::_GzipCompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level) { 
	static const int gz_magic[2] = {0x1f, 0x8b}; // gzip magic header
	int err;
	uLong crc;
	z_stream stream = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;
	crc = crc32(0L, Z_NULL, 0);
	// init Zlib stream
	// NOTE windowBits is passed < 0 to suppress zlib header
	err = deflateInit2(&stream, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
		return err;

	sprintf((char*)dest , "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1],
		Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/, 0 /*xflags*/, 255);
	// wire buffers
	stream.next_in = (Bytef*) source ;
	stream.avail_in = (uInt)sourceLen;
	stream.next_out = ((Bytef*) dest) + 10;
	stream.avail_out = *destLen - 18;
	// doit
	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		return err;
	}
	err = deflateEnd(&stream);
	crc = crc32(crc, (const Bytef *) source ,  sourceLen );
	//CRC
	*(((Bytef*) dest)+10+stream.total_out) = (Bytef)(crc & 0xFF);
	*(((Bytef*) dest)+10+stream.total_out+1) = (Bytef)((crc>>8) & 0xFF);
	*(((Bytef*) dest)+10+stream.total_out+2) = (Bytef)((crc>>16) & 0xFF);
	*(((Bytef*) dest)+10+stream.total_out+3) = (Bytef)((crc>>24) & 0xFF);
	// Length
	*(((Bytef*) dest)+10+stream.total_out+4) = (Bytef)( sourceLen  & 0xFF);
	*(((Bytef*) dest)+10+stream.total_out+5) = (Bytef)(( sourceLen >>8) & 0xFF);
	*(((Bytef*) dest)+10+stream.total_out+6) = (Bytef)(( sourceLen >>16) &	0xFF);
	*(((Bytef*) dest)+10+stream.total_out+7) = (Bytef)(( sourceLen >>24) &	0xFF);
	// return  destLength
	*destLen = 10 + stream.total_out + 8;
	return err;
}


bool CWebServer::_IsLoggedIn(ThreadData Data, long lSession) {

	_RemoveTimeOuts(Data,lSession);

	// find our session
	for (size_t i = 0; i < m_Params.Sessions.GetCount(); ++i) {
		if (m_Params.Sessions[i]->lSession == lSession && lSession != 0) {
			// if found, also reset expiration time
			m_Params.Sessions[i]->startTime = time(NULL);
			return true;
		}
	}

	return false;
}


void CWebServer::_RemoveTimeOuts(ThreadData WXUNUSED(Data), long WXUNUSED(lSession)) {
	// remove expired sessions
	UpdateSessionCount();
}


bool CWebServer::_RemoveSession(ThreadData WXUNUSED(Data), long lSession) {

	// find our session
	for (size_t i = 0; i < m_Params.Sessions.GetCount(); ++i) {
		if (m_Params.Sessions[i]->lSession == lSession && lSession != 0) {
			m_Params.Sessions.RemoveAt(i);
			return true;
		}
	}
	return false;
}


Session CWebServer::GetSessionByID(ThreadData WXUNUSED(Data),long sessionID) {
	
	for (size_t i = 0; i < m_Params.Sessions.GetCount(); ++i) {
		if (m_Params.Sessions[i]->lSession == sessionID && sessionID != 0)
			return *(m_Params.Sessions[i]);
	}

	Session ses;
	ses.admin=false;
	ses.startTime = 0;

	return ses;
}


bool CWebServer::IsSessionAdmin(ThreadData WXUNUSED(Data),wxString SsessionID) {
	long sessionID= StrToLong(SsessionID);

	for (size_t i = 0; i < m_Params.Sessions.GetCount(); ++i) {
		if (m_Params.Sessions[i]->lSession == sessionID && sessionID != 0)
			return m_Params.Sessions[i]->admin;
	}
	return false;
}


wxString CWebServer::GetPermissionDenied() {
	return wxString::Format(wxT("javascript:alert(\'%s\')"), _("Access denied!"));
}


bool CWebServer::_GetFileHash(wxString sHash, uchar *FileHash) {
	char hex_byte[3];
	int byte;
	hex_byte[2] = '\0';
	for (int i = 0; i < 16; ++i) {
		hex_byte[0] = sHash.GetChar(i*2);
		hex_byte[1] = sHash.GetChar((i*2 + 1));
		sscanf(hex_byte, "%02x", &byte);
		FileHash[i] = (uchar)byte;
	}
	return true;
}


wxString CWebServer::_GetSearch(ThreadData Data) {

	wxString sSession = _ParseURL(Data, wxT("ses"));
	wxString Out = m_Templates.sSearch;

	wxString downloads=_ParseURLArray(Data,wxT("downloads"));
	if (!downloads.IsEmpty() && IsSessionAdmin(Data,sSession) ) {
		int brk;
		CECPacket dload_req(EC_OP_DOWNLOAD_SEARCH_RESULT);
		while (downloads.Length()>0) {
			brk=downloads.First(wxT("|"));
			CMD4Hash file_hash(downloads.Left(brk));
			dload_req.AddTag(CECTag(EC_TAG_KNOWNFILE, file_hash));
			downloads=downloads.Mid(brk+1);
		}
		CECPacket *dload_reply = webInterface->SendRecvMsg_v2(&dload_req);
		if (dload_reply) {
			delete dload_reply;
		}
	}

	wxString messageString;

	wxString sToSearch = _ParseURL(Data, wxT("tosearch"));
	if (!sToSearch.IsEmpty() && IsSessionAdmin(Data,sSession)) {
		long min_size = 0, max_size = 0, avail = 0;
		_ParseURL(Data, wxT("min")).ToLong(&min_size);
		_ParseURL(Data, wxT("max")).ToLong(&max_size);
		_ParseURL(Data, wxT("avail")).ToLong(&avail);
		wxString ext = _ParseURL(Data, wxT("ext"));
		wxString method = _ParseURL(Data, wxT("method"));
		wxString type = _ParseURL(Data, wxT("type"));
		EC_SEARCH_TYPE search_type = EC_SEARCH_LOCAL;
		if ( type == wxT("Global") ) {
			search_type = EC_SEARCH_GLOBAL;
		} else if  ( type == wxT("Web") ) {
			search_type = EC_SEARCH_WEB;
		}
		CECPacket search_req(EC_OP_SEARCH_START);
		search_req.AddTag(CEC_Search_Tag (sToSearch, search_type, type, ext, avail, min_size, max_size));
		CECPacket *search_reply = webInterface->SendRecvMsg_v2(&search_req);
		if (search_reply) {
			messageString =
				wxGetTranslation(search_reply->GetTagByIndexSafe(0)->GetStringData());
			delete search_reply;
		}
	} else if (!sToSearch.IsEmpty() && !IsSessionAdmin(Data,sSession) ) {
		messageString = _("Access denied!");
	}

	Out.Replace(wxT("[Message]"), messageString);

	// categoriesa
	CECPacket req(EC_OP_GET_PREFERENCES, EC_DETAIL_WEB);
	req.AddTag(CECTag(EC_TAG_SELECT_PREFS, (uint32)EC_PREFS_CATEGORIES));
	CECPacket *reply = webInterface->SendRecvMsg_v2(&req);
	if (reply) {
		// if there are no categories, not even the EC_TAG_PREFS_CATEGORIES will be included :)
		if ( reply->GetTagCount() ) { 	
			InsertCatBox(Out, 0, m_Templates.sCatArrow, reply->GetTagByIndex(0));
		} else {
			Out.Replace(wxT("[CATBOX]"), wxEmptyString);
		}
		delete reply;
	} else {
		Out.Replace(wxT("[CATBOX]"), wxEmptyString);
	}
	wxString sSort = _ParseURL(Data, wxT("sort"));
	wxString sSearchSortRev = _ParseURL(Data, wxT("sortreverse"));
	m_SearchInfo.SetSortOrder(sSort, sSearchSortRev);
	
	m_SearchInfo.ReQuery();

	wxString result = m_Templates.sSearchHeader;
	m_SearchInfo.ProcessHeadersLine(result);
	
	SearchInfo::ItemIterator i = m_SearchInfo.GetBeginIterator();
	while (i != m_SearchInfo.GetEndIterator()) {
		wxString line = m_Templates.sSearchResultLine;
		if ( i->bPresent ) {
			line.Replace(wxT("[FILENAME]"), wxT("<font color=\"#00FF00\">") + i->sFileName + wxT("</font>"));
		} else {
			line.Replace(wxT("[FILENAME]"), i->sFileName);
		}
		line.Replace(wxT("[FILESIZE]"), CastItoXBytes(i->lFileSize));
		line.Replace(wxT("[SOURCECOUNT]"), wxString::Format(wxT("%lu"), i->lSourceCount));
		line.Replace(wxT("[FILEHASH]"), i->nHash.Encode());
		
		result += line;
		i++;
	}
	Out.Replace(wxT("[SEARCHINFOMSG]"),wxEmptyString);
	Out.Replace(wxT("[RESULTLIST]"), result);
	Out.Replace(wxT("[Result]"), _("Search Results"));
	Out.Replace(wxT("[Session]"), sSession);
	Out.Replace(wxT("[WebSearch]"), _("Web-based Search"));
	Out.Replace(wxT("[Name]"), _("Name"));
	Out.Replace(wxT("[Type]"), _("Type"));
	Out.Replace(wxT("[Any]"), _("Any"));
	Out.Replace(wxT("[Archives]"), _("Archive"));
	Out.Replace(wxT("[Audio]"), _("Audio"));
	Out.Replace(wxT("[CD-Images]"), _("CD-Images"));
	Out.Replace(wxT("[Pictures]"), _("Pictures"));
	Out.Replace(wxT("[Programs]"), _("Programs"));
	Out.Replace(wxT("[Texts]"), _("Texts"));
	Out.Replace(wxT("[Videos]"), _("Video"));
	Out.Replace(wxT("[Search]"), _("Search"));
	Out.Replace(wxT("[RefetchResults]"), _("Refetch Results"));
	Out.Replace(wxT("[Download]"), _("Download"));
	
	Out.Replace(wxT("[Filesize]"), _("Size"));
	Out.Replace(wxT("[Sources]"), _("Sources"));
	Out.Replace(wxT("[Filehash]"), _("File Hash"));
	Out.Replace(wxT("[Filename]"), _("File Name"));
	Out.Replace(wxT("[WebSearch]"), _("Web-based Search"));

	Out.Replace(wxT("[SizeMin]"), _("Min Size"));
	Out.Replace(wxT("[SizeMax]"), _("Max Size"));
	Out.Replace(wxT("[Availabl]"), _("Min Availability"));
	Out.Replace(wxT("[Extention]"), _("Extension"));
	Out.Replace(wxT("[Global]"), _("Global Search"));
	Out.Replace(wxT("[MB]"), _("MB"));
		
	Out.Replace(wxT("[METHOD]"), _("Method"));
	Out.Replace(wxT("[USESSERVER]"), _("Server"));
	Out.Replace(wxT("[Global]"), _("Global (Server)"));

	wxString val;
	val = wxString::Format(wxT("%i"),(m_iSearchSortby!=0 || (m_iSearchSortby==0 && m_bSearchAsc==0 ))?1:0 );
	Out.Replace(wxT("[SORTASCVALUE0]"), val);
	val = wxString::Format(wxT("%i"),(m_iSearchSortby!=1 || (m_iSearchSortby==1 && m_bSearchAsc==0 ))?1:0 );
	Out.Replace(wxT("[SORTASCVALUE1]"), val);
	val = wxString::Format(wxT("%i"),(m_iSearchSortby!=2 || (m_iSearchSortby==2 && m_bSearchAsc==0 ))?1:0 );
	Out.Replace(wxT("[SORTASCVALUE2]"), val);
	val = wxString::Format(wxT("%i"),(m_iSearchSortby!=3 || (m_iSearchSortby==3 && m_bSearchAsc==0 ))?1:0 );
	Out.Replace(wxT("[SORTASCVALUE3]"), val);
	
	return Out;
}


int CWebServer::UpdateSessionCount() {
	// remove old bans
	for (size_t i = 0; i < m_Params.badlogins.GetCount();) {
		uint32 diff= ::GetTickCount() - m_Params.badlogins[i]->timestamp ;
		if (diff >1000U*60U*15U && (::GetTickCount() > m_Params.badlogins[i]->timestamp)) {
			m_Params.badlogins.RemoveAt(i);
		} else 
			++i;
	}

	// count & remove old session
	for (size_t i = 0; i < m_Params.Sessions.GetCount();) {
	  time_t ts=time(NULL)-m_Params.Sessions[i]->startTime;
	  if (ts > SESSION_TIMEOUT_SECS) {
	    m_Params.Sessions.RemoveAt(i);
	  } else
	  	++i;
	}

	return m_Params.Sessions.GetCount();
}

wxString CWebServer::GetStatusBox(wxString &preselect)
{
	wxString result(wxT("<form><select name=\"cat\" size=\"1\""
	"onchange=GotoCat(this.form.cat.options[this.form.cat.selectedIndex].value)>"));
		
	const wxString catnames[] = {
		wxTRANSLATE("all others"),
		wxTRANSLATE("Waiting"),
		wxTRANSLATE("Downloading"),
		wxTRANSLATE("Erroneous"),
		wxTRANSLATE("Paused"),
		wxTRANSLATE("Stopped") };
		
	// those are values that CPartFile->GetPartfileStatus return. They are not meant be translated
	const wxChar *catvalues[] = {
		wxT(""),
		wxT("Waiting"),
		wxT("Downloading"),
		wxT("Erroneous"),
		wxT("Paused"),
		wxT("Stopped") };
		
	for (int i = 0; i < (int)(sizeof(catnames)/sizeof(wxString)); i++) {
		if ( catvalues[i] == preselect ) {
			result += wxT("<option selected value=\"");
		} else {
			result += wxT("<option value=\"");
		}
		result +=
			wxString(catvalues[i]) +
			wxT("\">") +
			wxGetTranslation(catnames[i]) +
			wxT("</option>");
	}
	result += wxT("</select></form>");
	
	return result;
}

void CWebServer::InsertCatBox(wxString &Out, int preselect, wxString boxlabel, CECTag *cats, bool jump) {
	wxString tempBuf;
	wxString catTitle;
	
	tempBuf = wxT("<form><select name=\"cat\" size=\"1\"");
	
	if (jump)  {
		tempBuf += wxT("onchange=GotoCat(this.form.cat.options[this.form.cat.selectedIndex].value)>");
	} else {
		tempBuf += wxT(">");
	}

	// Construct the categories options string.
	if (cats) {
		int catCount = cats->GetTagCount();
		for (int i = 0; i < catCount; ++i) {
			CECTag *tag = cats->GetTagByIndex(i);
			CECTag *categoryTitle = tag ? tag->GetTagByName(EC_TAG_CATEGORY_TITLE) : NULL;
			if (tag && categoryTitle) {
				catTitle = categoryTitle->GetStringData();
				tempBuf << wxT("<option") <<
					((i == preselect) ? wxT(" selected") : wxEmptyString) <<
					wxT(" value=\"") << i << wxT("\">") <<
					((i==0) ? wxString(_("all")) : catTitle) <<
					wxT("</option>");
			}
		}
	}
	Out.Replace(wxT("[CATBOX]"), boxlabel + tempBuf + wxT("</select></form>"));
}


/* 
 * Item container implementation
 */

ServersInfo *ServerEntry::GetContainerInstance()
{
	return ServersInfo::m_This;
}

ServersInfo *ServersInfo::m_This = 0;

ServersInfo::ServersInfo(CamulewebApp *webApp) : ItemsContainer<ServerEntry, xServerSort>(webApp)
{
	m_This = this;
	
	 // Init sorting order maps
	 m_SortHeaders[SERVER_SORT_NAME] = wxT("[SortName]");
	 m_SortHeaders[SERVER_SORT_DESCRIPTION] = wxT("[SortDescription]");
	 m_SortHeaders[SERVER_SORT_IP] = wxT("[SortIP]");
	 m_SortHeaders[SERVER_SORT_USERS] = wxT("[SortUsers]");
	 m_SortHeaders[SERVER_SORT_FILES] = wxT("[SortFiles]");
	 
	 m_SortStrVals[wxT("name")] = SERVER_SORT_NAME;
	 m_SortStrVals[wxT("description")] = SERVER_SORT_DESCRIPTION;
	 m_SortStrVals[wxT("ip")] = SERVER_SORT_IP;
	 m_SortStrVals[wxT("users")] = SERVER_SORT_USERS;
	 m_SortStrVals[wxT("files")] = SERVER_SORT_FILES;
	 
}

bool ServersInfo::ServersInfo::ReQuery()
{
	CECPacket srv_req(EC_OP_GET_SERVER_LIST, EC_DETAIL_WEB);
	CECPacket *srv_reply = m_webApp->SendRecvMsg_v2(&srv_req);
	if (!srv_reply) {
		return false;
	}
	//
	// query succeded - flush existing values and refill
	EraseAll();
	for (int i = 0; i < srv_reply->GetTagCount(); ++i) {
		CECTag *tag = srv_reply->GetTagByIndex(i);
		
		ServerEntry Entry;
		Entry.sServerName =
			_SpecialChars(tag->GetTagByNameSafe(EC_TAG_SERVER_NAME)->GetStringData());
		Entry.sServerDescription =
			_SpecialChars(tag->GetTagByNameSafe(EC_TAG_SERVER_DESC)->GetStringData());
		Entry.sServerIP = tag->GetIPv4Data().StringIP(false);
		Entry.nServerIP = tag->GetIPv4Data().IP();
		Entry.nServerPort = tag->GetIPv4Data().port;
		Entry.nServerUsers =
			tag->GetTagByNameSafe(EC_TAG_SERVER_USERS)->GetInt32Data();
		Entry.nServerMaxUsers =
			tag->GetTagByNameSafe(EC_TAG_SERVER_USERS_MAX)->GetInt32Data();
		Entry.nServerFiles =
			tag->GetTagByNameSafe(EC_TAG_SERVER_FILES)->GetInt32Data();
		AddItem(Entry);
	}
	delete srv_reply;
	SortItems();
	
	return true;
}

bool ServersInfo::CompareItems(const ServerEntry &i1, const ServerEntry &i2)
{
	bool Result;
	switch(m_SortOrder) {
		case SERVER_SORT_NAME:
			Result = i1.sServerName.CmpNoCase(i2.sServerName) < 0;
			break;
		case SERVER_SORT_DESCRIPTION:
			Result = i1.sServerDescription.CmpNoCase(i2.sServerDescription) < 0;
			break;
		case SERVER_SORT_IP:
			Result = i1.sServerIP.CmpNoCase(i2.sServerIP) < 0;
			break;
		case SERVER_SORT_USERS:
			Result = i1.nServerUsers < i2.nServerUsers;
			break;
		case SERVER_SORT_FILES:
			Result = i1.nServerFiles < i2.nServerFiles;
			break;
	}
	return Result ^ m_SortReverse;
}

SharedFiles::SharedFiles(CEC_SharedFile_Tag *tag)
{
		sFileName = _SpecialChars(tag->FileName());
		lFileSize = tag->SizeFull();
		sED2kLink = tag->FileEd2kLink();
		nHash = tag->ID();
		
		ProcessUpdate(tag);
}

void SharedFiles::ProcessUpdate(CEC_SharedFile_Tag *tag)
{
	nFileTransferred = tag->GetXferred();
	nFileAllTimeTransferred = tag->GetAllXferred();
	nFileRequests = tag->GetRequests();
	nFileAllTimeRequests = tag->GetAllRequests();
	nFileAccepts = tag->GetAccepts();
	nFileAllTimeAccepts = tag->GetAllAccepts();
	sFileHash = nHash.Encode();
	nFilePriority = tag->Prio();
	if ( nFilePriority >= 10 ) {
		bFileAutoPriority = true;
		nFilePriority -= 10;
	} else {
		bFileAutoPriority = false;
	}
}

SharedFilesInfo *SharedFiles::GetContainerInstance()
{
	return SharedFilesInfo::m_This;
}

SharedFilesInfo *SharedFilesInfo::m_This = 0;

SharedFilesInfo::SharedFilesInfo(CamulewebApp *webApp) :
	UpdatableItemsContainer<SharedFiles, xSharedSort, CEC_SharedFile_Tag, CMD4Hash>(webApp)
{
	m_This = this;
	m_SortOrder = SHARED_SORT_NAME;
	/*
	 * */
	 // Init sorting order maps
	 m_SortHeaders[SHARED_SORT_NAME] = wxT("[SortName]");
	 m_SortHeaders[SHARED_SORT_SIZE] = wxT("[SortSize]");
	 m_SortHeaders[SHARED_SORT_TRANSFERRED] = wxT("[SortTransferred]");
	 m_SortHeaders[SHARED_SORT_ALL_TIME_TRANSFERRED] = wxT("[SortAllTimeTransferred]");
	 m_SortHeaders[SHARED_SORT_REQUESTS] = wxT("[SortRequests]");
	 m_SortHeaders[SHARED_SORT_ALL_TIME_REQUESTS] = wxT("[SortAllTimeRequests]");
	 m_SortHeaders[SHARED_SORT_ACCEPTS] = wxT("[SortAccepts]");
	 m_SortHeaders[SHARED_SORT_ALL_TIME_ACCEPTS] = wxT("[SortAllTimeAccepts]");
	 m_SortHeaders[SHARED_SORT_PRIORITY] = wxT("[SortPriority]");

	 m_SortStrVals[wxT("")] = SHARED_SORT_NAME;
	 m_SortStrVals[wxT("name")] = SHARED_SORT_NAME;
	 m_SortStrVals[wxT("size")] = SHARED_SORT_SIZE;
	 m_SortStrVals[wxT("transferred")] = SHARED_SORT_TRANSFERRED;
	 m_SortStrVals[wxT("alltimetransferred")] = SHARED_SORT_ALL_TIME_TRANSFERRED;
	 m_SortStrVals[wxT("requests")] = SHARED_SORT_REQUESTS;
	 m_SortStrVals[wxT("alltimerequests")] = SHARED_SORT_ALL_TIME_REQUESTS;
	 m_SortStrVals[wxT("accepts")] = SHARED_SORT_ACCEPTS;
	 m_SortStrVals[wxT("alltimeaccepts")] = SHARED_SORT_ALL_TIME_ACCEPTS;
	 m_SortStrVals[wxT("priority")] = SHARED_SORT_PRIORITY;

}


bool SharedFilesInfo::ReQuery()
{
	DoRequery(EC_OP_GET_SHARED_FILES, EC_TAG_KNOWNFILE);
	
	SortItems();

	return true;
}

bool SharedFilesInfo::CompareItems(const SharedFiles &i1, const SharedFiles &i2)
{
	bool Result;
	switch(m_SortOrder) {
       case SHARED_SORT_NAME:
            Result = i1.sFileName.CmpNoCase(i2.sFileName) < 0;
            break;
        case SHARED_SORT_SIZE:
            Result = i1.lFileSize < i2.lFileSize;
            break;
        case SHARED_SORT_TRANSFERRED:
            Result = i1.nFileTransferred < i2.nFileTransferred;
            break;
        case SHARED_SORT_ALL_TIME_TRANSFERRED:
            Result = i1.nFileAllTimeTransferred < i2.nFileAllTimeTransferred;
            break;
        case SHARED_SORT_REQUESTS:
            Result = i1.nFileRequests < i2.nFileRequests;
            break;
       case SHARED_SORT_ALL_TIME_REQUESTS:
            Result = i1.nFileAllTimeRequests < i2.nFileAllTimeRequests;
            break;
        case SHARED_SORT_ACCEPTS:
            Result = i1.nFileAccepts < i2.nFileAccepts;
            break;
        case SHARED_SORT_ALL_TIME_ACCEPTS:
            Result = i1.nFileAllTimeAccepts < i2.nFileAllTimeAccepts;
            break;
        case SHARED_SORT_PRIORITY:
           //Very low priority is define equal to 4 ! Must adapte sorting code
            if (i1.nFilePriority == 4) {
                Result = (i2.nFilePriority != 4);
            } else {
                if (i2.nFilePriority == 4) {
                        Result = false;
                } else
                        Result = i1.nFilePriority < i2.nFilePriority;
            }
            break;
	}
	return Result ^ m_SortReverse;
}

DownloadFiles::DownloadFiles(CEC_PartFile_Tag *tag)
{
	nHash = tag->ID();
	sFileName = _SpecialChars(tag->FileName());
	lFileSize = tag->SizeFull();
	sFileHash = nHash.Encode();
	sED2kLink = _SpecialChars(tag->FileEd2kLink());
	lFileCompleted = tag->SizeDone();
	lFileTransferred = tag->SizeXfer();
	lFileSpeed = tag->Speed();
	fCompleted = (100.0*lFileCompleted) / lFileSize;
	
	m_Encoder = PartFileEncoderData( (lFileSize + (PARTSIZE - 1)) / PARTSIZE, 10);

	ProcessUpdate(tag);							
}

void DownloadFiles::ProcessUpdate(CEC_PartFile_Tag *tag)
{
	if (!tag) {
		return;
	}
	
	lFilePrio = tag->Prio();
	if ( lFilePrio >= 10 ) {
		lFilePrio -= 10;
		bFileAutoPriority = true;
	} else {
		bFileAutoPriority = false;
	}
	nFileStatus = tag->FileStatus();
	sFileStatus = tag->GetFileStatusString();
	lSourceCount = tag->SourceCount();
	lNotCurrentSourceCount = tag->SourceNotCurrCount();
	lTransferringSourceCount = tag->SourceXferCount();
	lSourceCountA4AF = tag->SourceCountA4AF();
	if ( lTransferringSourceCount > 0 ) {
		lFileCompleted = tag->SizeDone();
		lFileTransferred = tag->SizeXfer();
		lFileSpeed = tag->Speed();
		fCompleted = (100.0*lFileCompleted) / lFileSize;
	} else {
		lFileSpeed = 0;
	}
	CECTag *gaptag = tag->GetTagByName(EC_TAG_PARTFILE_GAP_STATUS);
	CECTag *parttag = tag->GetTagByName(EC_TAG_PARTFILE_PART_STATUS);
	CECTag *reqtag = tag->GetTagByName(EC_TAG_PARTFILE_REQ_STATUS);
	if (gaptag && parttag && reqtag) {
		m_Encoder.Decode(
			(unsigned char *)gaptag->GetTagData(), gaptag->GetTagDataLen(),
			(unsigned char *)parttag->GetTagData(), parttag->GetTagDataLen());

		const Gap_Struct *reqparts = (const Gap_Struct *)reqtag->GetTagData();
		int reqcount = reqtag->GetTagDataLen() / sizeof(Gap_Struct);
		m_ReqParts.resize(reqcount);
		for (int i = 0; i < reqcount;i++) {
			m_ReqParts[i].start = ntohl(reqparts[i].start);
			m_ReqParts[i].end   = ntohl(reqparts[i].end);
		}
	}
}

DownloadFilesInfo *DownloadFiles::GetContainerInstance()
{
	return DownloadFilesInfo::m_This;
}

DownloadFilesInfo *DownloadFilesInfo::m_This = 0;

DownloadFilesInfo::DownloadFilesInfo(CamulewebApp *webApp, CImageLib *imlib) :
	UpdatableItemsContainer<DownloadFiles, xDownloadSort, CEC_PartFile_Tag, CMD4Hash>(webApp)
{
	m_This = this;
	m_ImageLib = imlib;
	
	m_SortHeaders[DOWN_SORT_NAME] = wxT("[SortName]");
	m_SortHeaders[DOWN_SORT_SIZE] = wxT("[SortSize]");
	m_SortHeaders[DOWN_SORT_COMPLETED] = wxT("[SortCompleted]");
	m_SortHeaders[DOWN_SORT_TRANSFERRED] = wxT("[SortTransferred]");
	m_SortHeaders[DOWN_SORT_SPEED] = wxT("[SortSpeed]");
	m_SortHeaders[DOWN_SORT_PROGRESS] = wxT("[SortProgress]");
	
	m_SortStrVals[wxT("")] = DOWN_SORT_NAME;
	m_SortStrVals[wxT("name")] = DOWN_SORT_NAME;
	m_SortStrVals[wxT("size")] = DOWN_SORT_SIZE;
	m_SortStrVals[wxT("completed")] = DOWN_SORT_COMPLETED;
	m_SortStrVals[wxT("transferred")] = DOWN_SORT_TRANSFERRED;
	m_SortStrVals[wxT("progress")] = DOWN_SORT_PROGRESS;
	m_SortStrVals[wxT("speed")] = DOWN_SORT_SPEED;
}

void DownloadFilesInfo::LoadImageParams(wxString &tpl, int width, int height)
{
	m_Template = tpl;
	m_width = width;
	m_height = height;
}

void DownloadFilesInfo::ItemInserted(DownloadFiles *item)
{
	item->m_Image = new CDynImage(m_width, m_height, m_Template, item);

#ifdef WITH_LIBPNG
	m_ImageLib->AddImage(item->m_Image, wxT("/") + item->m_Image->Name());
#endif
}

void DownloadFilesInfo::ItemDeleted(DownloadFiles *item)
{
#ifdef WITH_LIBPNG
	m_ImageLib->RemoveImage(wxT("/") + item->m_Image->Name());
#endif
	delete item->m_Image;
}

bool DownloadFilesInfo::ReQuery()
{
	DoRequery(EC_OP_GET_DLOAD_QUEUE, EC_TAG_PARTFILE);
	
	SortItems();

	return true;
}

bool DownloadFilesInfo::CompareItems(const DownloadFiles &i1, const DownloadFiles &i2)
{
	bool Result;
	switch(m_SortOrder) {
		case DOWN_SORT_NAME:
            Result = i1.sFileName.CmpNoCase(i2.sFileName) < 0;
			break;
		case DOWN_SORT_SIZE:
			Result = i1.lFileSize < i2.lFileSize;
			break;
		case DOWN_SORT_COMPLETED:
			Result = i1.lFileCompleted < i2.lFileCompleted;
			break;
		case DOWN_SORT_TRANSFERRED:
			Result = i1.lFileTransferred < i2.lFileTransferred;
			break;
		case DOWN_SORT_SPEED:
			Result = i1.lFileSpeed < i2.lFileSpeed;
			break;
		case DOWN_SORT_PROGRESS:
			Result = i1.fCompleted < i2.fCompleted;
			break;
	}
	return Result ^ m_SortReverse;
}

UploadFiles::UploadFiles(CEC_UpDownClient_Tag *tag)
{
	nHash = tag->FileID();
	sUserName = _SpecialChars(tag->ClientName());
	nSpeed = tag->SpeedUp();
	nTransferredUp = tag->XferUp();
	nTransferredDown = tag->XferDown();
}

UploadsInfo::UploadsInfo(CamulewebApp *webApp) : ItemsContainer<UploadFiles, int>(webApp)
{
}

bool UploadsInfo::ReQuery()
{
	CECPacket up_req(EC_OP_GET_ULOAD_QUEUE, EC_DETAIL_WEB);
	CECPacket *up_reply = m_webApp->SendRecvMsg_v2(&up_req);
	if (!up_reply) {
		return false;
	}
	//
	// query succeded - flush existing values and refill
	EraseAll();
	for(int i = 0; i < up_reply->GetTagCount(); i ++) {
		
		UploadFiles curr((CEC_UpDownClient_Tag *)up_reply->GetTagByIndex(i));
		
		AddItem(curr);
	}
	delete up_reply;
	
	return true;
}

SearchFile::SearchFile(CEC_SearchFile_Tag *tag)
{
	nHash = tag->FileHash();
	sFileName = _SpecialChars(tag->FileName());
	lFileSize = tag->SizeFull();
	lSourceCount = tag->SourceCount();
	bPresent = tag->AlreadyHave();
}

void SearchFile::ProcessUpdate(CEC_SearchFile_Tag *tag)
{
	lSourceCount = tag->SourceCount();
}

SearchInfo *SearchFile::GetContainerInstance()
{
	return SearchInfo::m_This;
}

SearchInfo *SearchInfo::m_This = 0;

SearchInfo::SearchInfo(CamulewebApp *webApp) :
	UpdatableItemsContainer<SearchFile, xSearchSort, CEC_SearchFile_Tag, CMD4Hash>(webApp)
{
	m_This = this;
	
	m_SortHeaders[SEARCH_SORT_NAME] = wxT("[SortName]");
	m_SortHeaders[SEARCH_SORT_SIZE] = wxT("[SortSize]");
	m_SortHeaders[SEARCH_SORT_SOURCES] = wxT("[SortSources]");
	
	m_SortStrVals[wxT("")] = SEARCH_SORT_NAME;
	m_SortStrVals[wxT("name")] = SEARCH_SORT_NAME;
	m_SortStrVals[wxT("size")] = SEARCH_SORT_SIZE;
	m_SortStrVals[wxT("sources")] = SEARCH_SORT_SOURCES;
}

bool SearchInfo::ReQuery()
{
	DoRequery(EC_OP_SEARCH_RESULTS, EC_TAG_SEARCHFILE);
	
	SortItems();

	return true;
}

bool SearchInfo::CompareItems(const SearchFile &i1, const SearchFile &i2)
{
	bool Result;
	switch(m_SortOrder) {
		case SEARCH_SORT_NAME:
			Result = i1.sFileName.CmpNoCase(i2.sFileName) < 0;
			break;
		case SEARCH_SORT_SIZE:
			Result = i1.lFileSize < i2.lFileSize;
			break;
		case SEARCH_SORT_SOURCES:
			Result = i1.lSourceCount < i2.lSourceCount;
			break;
	}
	return Result ^ m_SortReverse;
}

/*!
 * Image classes:
 * 
 * CFileImage: simply represent local file
 * CDynImage: dynamically generated from gap info
 */
 
CAnyImage::CAnyImage(int size)
{
	m_size = 0;
	m_alloc_size = size;
	if ( m_alloc_size ) {
		m_data = new unsigned char[m_alloc_size];
	} else {
		m_data = 0;
	}
}

CAnyImage::~CAnyImage()
{
	if ( m_data ) {
		delete [] m_data;
	}
}

void CAnyImage::Realloc(int size)
{
	if ( size == m_alloc_size ) {
		return;
	}
	// always grow, but shrink only x2
	if ( (size > m_alloc_size) || (size < (m_alloc_size / 2)) ) {
		m_alloc_size = size;
		if ( m_data ) {
			delete [] m_data;
		}
		m_data = new unsigned char[m_alloc_size];
	}
}

unsigned char *CAnyImage::RequestData(int &size)
{
	size = m_size;
	return m_data;
}

void CAnyImage::SetHttpType(wxString ext)
{
	m_Http = wxT("Content-Type: ") + ext + wxT("\r\n");

	time_t t = time(NULL);
	char tmp[255];
	strftime(tmp, 255, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t));
	m_Http += wxT("Last-Modified: ") + wxString(char2unicode(tmp)) + wxT("\r\n");

	m_Http += wxT("ETag: ") + MD5Sum(char2unicode(tmp)).GetHash() + wxT("\r\n");
}

CFileImage::CFileImage(const wxString& name) : CAnyImage(0), m_name(name)
{
	m_size = 0;
	wxFFile fis(m_name);
	// FIXME: proper logging is needed
	if ( fis.IsOpened() ) {
		size_t file_size = fis.Length();
		if ( file_size ) {
			Realloc(fis.Length());
			m_size = fis.Read(m_data,file_size);
		} else {
			printf("CFileImage: file %s have zero length\n", (const char *)unicode2char(m_name));
		}
		wxString ext = m_name.Right(3).MakeLower();
		if ( ext == wxT("css") ) {
			SetHttpType(wxT("text/css"));
		} else {
			SetHttpType(wxT("image/") + ext);
		}
	} else {
		printf("CFileImage: failed to open %s\n", (const char *)unicode2char(m_name));
	}
}

/*!
 * "Modifiers" for 3D look of progress bar. Those modifiers must be substracted from
 * image (with saturation), values, and not multiplied, as amule doesn for some reason.
 * 
 */
CImage3D_Modifiers::CImage3D_Modifiers(int width)
{
	m_width = width;
	m_modifiers = new unsigned char[m_width];
	for(int i = 0; i < m_width; i++) {
		// "70" - webserver uses fixed depth
		double f_curr_mod = 30 * (1 + cos( (2 * M_PI) * ( m_width - (((double)i)/m_width) ) ) );
		m_modifiers[i] = (unsigned char)f_curr_mod;
	}
}

CImage3D_Modifiers::~CImage3D_Modifiers()
{
	delete [] m_modifiers;
}

CProgressImage::CProgressImage(int width, int height, wxString &tmpl, DownloadFiles *file) :
		CAnyImage(width * height * sizeof(uint32)), m_template(tmpl)
{
	m_width = width;
	m_height = height;
	m_file = file;

	m_gap_buf_size = m_gap_alloc_size = m_file->m_Encoder.m_gap_status.Size() / (2 * sizeof(uint32));
	m_gap_buf = new Gap_Struct[m_gap_alloc_size];
	
	m_ColorLine = new uint32[m_width];
}

CProgressImage::~CProgressImage()
{
	delete [] m_gap_buf;
	delete [] m_ColorLine;
}

void CProgressImage::ReallocGapBuffer()
{
	int size = m_file->m_Encoder.m_gap_status.Size() / (2 * sizeof(uint32));
	if ( size == m_gap_alloc_size ) {
		return;
	}
	if ( (size > m_gap_alloc_size) || (size < m_gap_alloc_size/2) ) {
		m_gap_buf_size = m_gap_alloc_size = size;
		delete [] m_gap_buf;
		m_gap_buf = new Gap_Struct[m_gap_alloc_size];
	} else {
		m_gap_buf_size = size;
	}
}

void CProgressImage::InitSortedGaps()
{
	ReallocGapBuffer();

	const uint32 *gap_info = (const uint32 *)m_file->m_Encoder.m_gap_status.Buffer();
	m_gap_buf_size = m_file->m_Encoder.m_gap_status.Size() / (2 * sizeof(uint32));
	
	//memcpy(m_gap_buf, gap_info, m_gap_buf_size*2*sizeof(uint32));
	for (int j = 0; j < m_gap_buf_size;j++) {
		uint32 gap_start = ntohl(gap_info[2*j]);
		uint32 gap_end = ntohl(gap_info[2*j+1]);
		m_gap_buf[j].start = gap_start;
		m_gap_buf[j].end = gap_end;
	}
	qsort(m_gap_buf, m_gap_buf_size, 2*sizeof(uint32), compare_gaps);
}

#define RGB(r, g, b) ( (((unsigned char)b) << 16) | (((unsigned char)g) << 8) | ((unsigned char)r))

void CProgressImage::CreateSpan()
{
	// Step 1: sort gaps list in accending order
	InitSortedGaps();
	
	// allocate for worst case !
	int color_gaps_alloc = 2 * (2*m_gap_buf_size + m_file->lFileSize / PARTSIZE + 1);
	Color_Gap_Struct *colored_gaps = new Color_Gap_Struct[color_gaps_alloc];
	
	// Step 2: combine gap and part status information
	const unsigned char *part_info = m_file->m_Encoder.m_part_status.Buffer();
	
	// Init first item to dummy info, so we will always have "previous" item
	int colored_gaps_size = 0;
	colored_gaps[0].start = 0;
	colored_gaps[0].end = 0;
	colored_gaps[0].color = 0xffffffff;
	for (int j = 0; j < m_gap_buf_size;j++) {
		uint32 gap_start = m_gap_buf[j].start;
		uint32 gap_end = m_gap_buf[j].end;
		//printf("DEBUG: current gap [%08x %08x]\n", gap_start, gap_end);

		uint32 start = gap_start / PARTSIZE;
		uint32 end = (gap_end / PARTSIZE) + 1;

		for (uint32 i = start; i < end; i++) {
			COLORREF color = RGB(255, 0, 0);
			if ( part_info[i] ) {
				int blue = 210 - ( 22 * ( part_info[i] - 1 ) );
				color = RGB( 0, ( blue < 0 ? 0 : blue ), 255 );
			}

			uint32 fill_gap_begin = ( (i == start)   ? gap_start: PARTSIZE * i );
			uint32 fill_gap_end   = ( (i == (end - 1)) ? gap_end   : PARTSIZE * ( i + 1 ) );
			
			wxASSERT(colored_gaps_size < color_gaps_alloc);
			
			if ( (colored_gaps[colored_gaps_size].end == fill_gap_begin) &&
				(colored_gaps[colored_gaps_size].color == color) ) {
				colored_gaps[colored_gaps_size].end = fill_gap_end;
			} else {
				colored_gaps_size++;
				colored_gaps[colored_gaps_size].start = fill_gap_begin;
				colored_gaps[colored_gaps_size].end = fill_gap_end;
				colored_gaps[colored_gaps_size].color = color;
			}
		}
		
	}
	//
	// Now line rendering
	int i;
	for(i = 0; i < m_width; ++i) {
		m_ColorLine[i] = 0x0;
	}	
	if (m_file->lFileSize < (uint32)m_width) {
		//
		// if file is that small, draw it in single step
		//
		if (m_file->m_ReqParts.size()) {
			for(i = 0; i < m_width; ++i) {
				m_ColorLine[i] = RGB(255, 208, 0);
			}
		} else if ( colored_gaps_size ) {
			for(i = 0; i < m_width; ++i) {
				m_ColorLine[i] = colored_gaps[0].color;
			}
		}
	} else {
		uint32 factor = m_file->lFileSize / m_width;
		for(int i = 1; i <= colored_gaps_size;i++) {
			uint32 start = colored_gaps[i].start / factor;
			uint32 end = colored_gaps[i].end / factor;
			for(uint32 j = start; j < end; j++) {
				m_ColorLine[j] = colored_gaps[i].color;
			}
		}
		// overwrite requested parts
		for(uint32 i = 0; i < m_file->m_ReqParts.size(); i++) {
			uint32 start = m_file->m_ReqParts[i].start / factor;
			uint32 end = m_file->m_ReqParts[i].end / factor;
			for(uint32 j = start; j < end; j++) {
				m_ColorLine[j] = RGB(255, 208, 0);
			}
		}
	}
	delete [] colored_gaps;
}

int CProgressImage::compare_gaps(const void *g1, const void *g2)
{
	return ((const Gap_Struct *)g1)->start - ((const Gap_Struct *)g2)->start;
}

#ifdef WITH_LIBPNG

CDynImage::CDynImage(int width, int height, wxString &tmpl, DownloadFiles *file) :
	CProgressImage(width, height, tmpl, file), m_modifiers(height)
{
	m_name = wxT("dyn_") + m_file->sFileHash + wxT(".png");
	
	//
	// Allocate array of "row pointers" - libpng need it in this form
	//
	
	// When allocating an array with a new expression, GCC used to allow 
	// parentheses around the type name. This is actually ill-formed and it is 
	// now rejected. From gcc3.4
	m_row_ptrs = new png_bytep [m_height];
	for (int i = 0; i < m_height;i++) {
		m_row_ptrs[i] = new png_byte[3*m_width];
		memset(m_row_ptrs[i], 0, 3*m_width);
	}
}

CDynImage::~CDynImage()
{
	for (int i = 0; i < m_height;i++) {
		delete [] m_row_ptrs[i];
	}
	delete [] m_row_ptrs;
}

void CDynImage::png_write_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	CDynImage *This = (CDynImage *)png_get_io_ptr(png_ptr);
	wxASSERT((png_size_t)(This->m_size + length) <= (png_size_t)This->m_alloc_size);
	memcpy(This->m_data + This->m_size, data, length);
	This->m_size += length;
}

wxString CDynImage::GetHTML()
{
	// template contain %s (name) %d (width)
	return wxString::Format(m_template, m_name.GetData(), m_width);
}
	
void CDynImage::DrawImage()
{
	CreateSpan();

	for(int i = 0; i < m_height; i++) {
			memset(m_row_ptrs[i], 0, 3*m_width);
	}
	for(int i = 0; i < m_height/2; i++) {
		png_bytep u_row = m_row_ptrs[i];
		png_bytep d_row = m_row_ptrs[m_height-i-1];
		for(int j = 0; j < m_width; j++) {
			set_rgb_color_val(u_row+3*j, m_ColorLine[j], m_modifiers[i]);
			set_rgb_color_val(d_row+3*j, m_ColorLine[j], m_modifiers[i]);
		}
	}
}

unsigned char *CDynImage::RequestData(int &size)
{
	// create new one
	DrawImage();

	// write png into buffer
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	png_set_IHDR(png_ptr, info_ptr, m_width, m_height, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_write_fn(png_ptr, this, png_write_fn, 0);
	
	//printf("PNG: writing %p [ ", this);
	
	m_size = 0;
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, (png_bytep *)m_row_ptrs);
	png_write_end(png_ptr, 0);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	
	return CProgressImage::RequestData(size);
}

#else

CDynImage::CDynImage(int width, int height, wxString &tmpl, DownloadFiles *file) :
	CProgressImage(width, height, tmpl, file)
{
	m_name = wxT("dyn_") + m_file->sFileHash + wxT(".png");
	
}


wxString CDynImage::GetHTML()
{
	static wxString progresscolor[12] = {
		wxT("transparent.gif"), wxT("black.gif"), wxT("yellow.gif"), wxT("red.gif"),
		wxT("blue1.gif"),       wxT("blue2.gif"), wxT("blue3.gif"),  wxT("blue4.gif"),
		wxT("blue5.gif"),       wxT("blue6.gif"), wxT("green.gif"),  wxT("greenpercent.gif") };
		
	CreateSpan();
	
	wxString str;
	uint32 lastcolor = m_ColorLine[0];
	int lastindex = 0;
	for(int i = 0; i < m_width; i++) {
		if ( (lastcolor != m_ColorLine[i]) || (i == (m_width - 1)) ) {
			int color_idx = -1;
			if ( lastcolor & RGB(0, 0, 0xff) ) { // blue
				int green = (lastcolor >> 8) & 0xff;
				// reverse calculation:  green = 210 - ( 22 * ( part_info[i] - 1 ) )
				wxASSERT( !green || (green < 211) );
				color_idx = (green) ? (210 - green) / 22 + 1 : 11;
				// now calculate it same way as PartFile did
				color_idx = (color_idx > 10) ? 9 : (4 + color_idx / 2);
			} else {
				if ( lastcolor & RGB(0, 0xff, 0) ) { // yellow
					color_idx = 2;
				} else {
					if ( lastcolor & RGB(0xff, 0, 0) ) { // red
						color_idx = 3;
					} else {
						color_idx = 1;
					}
				}
			}
			// Evil unicode2char, but nothing we ca do against it because of the template.
			str += wxString::Format(m_template,
				(const char*) unicode2char(progresscolor[color_idx]), i - lastindex);
			lastindex = i;
			lastcolor = m_ColorLine[i];
		}
	}

	return str;
}

#endif

#ifdef WITH_LIBPNG
CDynPngImage::CDynPngImage(int w, int h, const unsigned char* Data, wxString name) :
		CAnyImage(w * h * sizeof(uint32)),m_width(w),m_height(h),m_name(name) {
	
	//
	// Allocate array of "row pointers" - libpng need it in this form
	// Fill it also with the image data
	//
	
	// When allocating an array with a new expression, GCC used to allow 
	// parentheses around the type name. This is actually ill-formed and it is 
	// now rejected. From gcc3.4

	m_row_ptrs = new png_bytep [m_height];
	for (int i = 0; i < m_height;i++) {
		m_row_ptrs[i] = new png_byte[3*m_width];
		memcpy(m_row_ptrs[i], (void*)((unsigned char*)Data + (m_height-1)*m_width), 3*m_width);
	}
	
}

CDynPngImage::~CDynPngImage()
{
	for (int i = 0; i < m_height;i++) {
		delete [] m_row_ptrs[i];
	}
	delete [] m_row_ptrs;
}

void CDynPngImage::png_write_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
	CDynPngImage *This = (CDynPngImage *)png_get_io_ptr(png_ptr);
	wxASSERT((png_size_t)(This->m_size + length) <= (png_size_t)This->m_alloc_size);
	memcpy(This->m_data + This->m_size, data, length);
	This->m_size += length;
}

wxString CDynPngImage::GetHTML()
{
	// template contain %s (name) %d (width)
	return wxString::Format(/*m_template*/wxT("%s %d"), m_name.GetData(), m_width);
}

unsigned char *CDynPngImage::RequestData(int &size)
{

	// write png into buffer
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	png_set_IHDR(png_ptr, info_ptr, m_width, m_height, 8, PNG_COLOR_TYPE_RGB,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_set_write_fn(png_ptr, this, png_write_fn, 0);
	
	//printf("PNG: writing %p [ ", this);
	
	m_size = 0;
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, (png_bytep *)m_row_ptrs);
	png_write_end(png_ptr, 0);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	
	return CAnyImage::RequestData(size);
}
#endif

CImageLib::CImageLib(wxString image_dir) : m_image_dir(image_dir)
{
}

CImageLib::~CImageLib()
{
}

void CImageLib::AddImage(CAnyImage *img, const wxString &name)
{
	CAnyImage *prev = m_image_map[name];
	if ( prev ) {
		delete prev;
	}
	m_image_map[name] = img;
}

void CImageLib::RemoveImage(const wxString &name)
{
	CAnyImage *prev = m_image_map[name];
	if ( prev ) {
		m_image_map.erase(name);
	}
}

CAnyImage *CImageLib::GetImage(wxString &name)
{
	CAnyImage *img = m_image_map[name];
	if ( img ) {
		return img;
	}
	wxFileName filename(m_image_dir + name);
	CFileImage *fimg = new CFileImage(filename.GetFullPath());
	if ( fimg->OpenOk() ) {
		m_image_map[name] = fimg;
		return fimg;
	} else {
		delete fimg;
		return 0;
	}
}
