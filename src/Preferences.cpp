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

#include <cstdio>
#include <cstdlib>

#include <wx/defs.h>

#if wxCHECK_VERSION(2,5,1)
	#include <wx/stopwatch.h>
#endif

#include <wx/timer.h> // Needed for wxGetLocalTimeMillis

#include <wx/tokenzr.h>
#include <wx/filename.h>
#include <wx/textfile.h>
#include <wx/config.h>

#ifdef __WXMSW__
	#include <wx/msw/winundef.h>
	#include <winerror.h>
	#include <shlobj.h>
#endif

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "Preferences.h"
#endif

#include "amule.h"
#ifdef HAVE_CONFIG_H
#include "config.h"		// Needed for PACKAGE_STRING
#endif
#include "OtherFunctions.h"
#include "StringFunctions.h"
#include "OPCodes.h"		// Needed for PREFFILE_VERSION
#include "Preferences.h"
#include "CFile.h"
#include "MD5Sum.h"
#include "Logger.h"
#include "Format.h"		// Needed for CFormat

#ifndef AMULE_DAEMON
#include <wx/valgen.h>
#include <wx/control.h>
#include <wx/slider.h>
#include "muuli_wdr.h"
#include "StatisticsDlg.h"
#include <wx/choice.h>
#endif

using namespace otherfunctions;

#define DEFAULT_TCP_PORT 4662

// Static variables
COLORREF		CPreferences::s_colors[cntStatColors];
COLORREF		CPreferences::s_colors_ref[cntStatColors];

CPreferences::CFGMap	CPreferences::s_CfgList;
CPreferences::CFGList	CPreferences::s_MiscList;


/* Proxy */
CProxyData	CPreferences::s_ProxyData;

/* The rest, organize it! */
wxString	CPreferences::s_nick;
uint16		CPreferences::s_maxupload;
uint16		CPreferences::s_maxdownload;
uint16		CPreferences::s_slotallocation;
uint16		CPreferences::s_port;
uint16		CPreferences::s_udpport;
bool		CPreferences::s_UDPDisable;
uint16		CPreferences::s_maxconnections;
bool		CPreferences::s_reconnect;
bool		CPreferences::s_autoconnect;
bool		CPreferences::s_autoconnectstaticonly;
bool		CPreferences::s_autoserverlist;
bool		CPreferences::s_deadserver;
wxString	CPreferences::s_incomingdir;
wxString	CPreferences::s_tempdir;
bool		CPreferences::s_ICH;
uint8		CPreferences::s_depth3D;
bool		CPreferences::s_scorsystem;
bool		CPreferences::s_mintotray;
bool		CPreferences::s_trayiconenabled;
bool		CPreferences::s_addnewfilespaused;
bool		CPreferences::s_addserversfromserver;
bool		CPreferences::s_addserversfromclient;
uint16		CPreferences::s_maxsourceperfile;
uint16		CPreferences::s_trafficOMeterInterval;
uint16		CPreferences::s_statsInterval;
uint32		CPreferences::s_maxGraphDownloadRate;
uint32		CPreferences::s_maxGraphUploadRate;
bool		CPreferences::s_confirmExit;
bool		CPreferences::s_filterLanIP;
bool		CPreferences::s_onlineSig;
uint16		CPreferences::s_OSUpdate;
uint64		CPreferences::s_totalDownloadedBytes;
uint64		CPreferences::s_totalUploadedBytes;
wxString	CPreferences::s_languageID;
bool		CPreferences::s_transferDoubleclick;
uint8		CPreferences::s_iSeeShares;
uint8		CPreferences::s_iToolDelayTime;
uint8		CPreferences::s_splitterbarPosition;
uint16		CPreferences::s_deadserverretries;
uint32		CPreferences::s_dwServerKeepAliveTimeoutMins;
uint8		CPreferences::s_statsMax;
uint8		CPreferences::s_statsAverageMinutes;
bool		CPreferences::s_bpreviewprio;
bool		CPreferences::s_smartidcheck;
uint8		CPreferences::s_smartidstate;
bool		CPreferences::s_safeServerConnect;
bool		CPreferences::s_startMinimized;
uint16		CPreferences::s_MaxConperFive;
bool		CPreferences::s_checkDiskspace;
uint32		CPreferences::s_uMinFreeDiskSpace;
wxString	CPreferences::s_yourHostname;
bool		CPreferences::s_bVerbose;
bool		CPreferences::s_bmanualhighprio;
bool		CPreferences::s_btransferfullchunks;
bool		CPreferences::s_bstartnextfile;
bool		CPreferences::s_bstartnextfilesame;
bool		CPreferences::s_bshowoverhead;
bool		CPreferences::s_bDAP;
bool		CPreferences::s_bUAP;
bool		CPreferences::s_bDisableKnownClientList;
bool		CPreferences::s_bDisableQueueList;
bool		CPreferences::s_ShowRatesOnTitle;
wxString	CPreferences::s_VideoPlayer;
bool		CPreferences::s_moviePreviewBackup;
bool		CPreferences::s_indicateratings;
bool		CPreferences::s_showAllNotCats;
bool		CPreferences::s_msgonlyfriends;
bool		CPreferences::s_msgsecure;
uint8		CPreferences::s_filterlevel;
uint8		CPreferences::s_iFileBufferSize;
uint8		CPreferences::s_iQueueSize;
uint16		CPreferences::s_maxmsgsessions;
wxString 	CPreferences::s_datetimeformat;
wxString	CPreferences::s_sWebPassword;
wxString	CPreferences::s_sWebLowPassword;
uint16		CPreferences::s_nWebPort;
bool		CPreferences::s_bWebEnabled;
bool		CPreferences::s_bWebUseGzip;
uint32		CPreferences::s_nWebPageRefresh;
bool		CPreferences::s_bWebLowEnabled;
bool		CPreferences::s_showCatTabInfos;
uint32		CPreferences::s_allcatType;
uint32		CPreferences::s_desktopMode;
uint8		CPreferences::s_NoNeededSources;
bool		CPreferences::s_DropFullQueueSources;
bool		CPreferences::s_DropHighQueueRankingSources;
uint32		CPreferences::s_HighQueueRanking;
uint32		CPreferences::s_AutoDropTimer;
bool 		CPreferences::s_AcceptExternalConnections;
bool 		CPreferences::s_ECUseTCPPort;
uint32		CPreferences::s_ECPort;
wxString	CPreferences::s_ECPassword;
bool		CPreferences::s_IPFilterOn;
bool		CPreferences::s_UseSrcSeeds;
bool		CPreferences::s_ProgBar;
bool		CPreferences::s_Percent;	
bool		CPreferences::s_SecIdent;
bool		CPreferences::s_ExtractMetaData;
bool		CPreferences::s_AllocFullPart;
bool		CPreferences::s_AllocFullChunk;
uint16		CPreferences::s_Browser;
wxString	CPreferences::s_CustomBrowser;
bool		CPreferences::s_BrowserTab;
wxString	CPreferences::s_OSDirectory;
wxString	CPreferences::s_SkinFile;
bool		CPreferences::s_UseSkinFile;
bool		CPreferences::s_FastED2KLinksHandler;
int		CPreferences::s_perms_files;
int		CPreferences::s_perms_dirs;
bool		CPreferences::s_AICHTrustEveryHash;
bool		CPreferences::s_IPFilterAutoLoad;
wxString	CPreferences::s_IPFilterURL;
CMD4Hash	CPreferences::s_userhash;
bool		CPreferences::s_MustFilterMessages;
wxString 	CPreferences::s_MessageFilterString;
bool		CPreferences::s_FilterAllMessages;
bool		CPreferences::s_FilterSomeMessages;
bool		CPreferences::s_ShareHiddenFiles;
bool		CPreferences::s_AutoSortDownload;

/**
 * Template Cfg class for connecting with widgets.
 *
 * This template provides the base functionionality needed to syncronize a 
 * variable with a widget. However, please note that wxGenericValidator only
 * supports a few types (int, wxString, bool and wxArrayInt), so this template 
 * can't always be used directly.
 *
 * Cfg_Str and Cfg_Bool are able to use this template directly, whereas Cfg_Int
 * makes use of serveral workaround to enable it to be used with integers other
 * than int.
 */
template <typename TYPE>
class Cfg_Tmpl : public Cfg_Base
{
public:
	/**
	 * Constructor.
	 *
	 * @param keyname
	 * @param value
	 * @param defaultVal
	 */
	Cfg_Tmpl( const wxString& keyname, TYPE& value, const TYPE& defaultVal )
	 : Cfg_Base( keyname ),
	   m_value( value ),
	   m_default( defaultVal ),
	   m_widget( NULL )
	{}

#ifndef AMULE_DAEMON
	/**
	 * Connects the Cfg to a widget.
	 * 
	 * @param id The ID of the widget to be connected.
	 * @param parent The parent of the widget. Use this to speed up searches.
	 *
	 * This function works by setting the wxValidator of the class. This however
	 * poses some restrictions on which variable types can be used for this
	 * template, as noted above. It also poses some limits on the widget types,
	 * refer to the wx documentation for those.
	 */
	virtual	bool ConnectToWidget( int id, wxWindow* parent = NULL )
	{
		if ( id ) {
			m_widget = wxWindow::FindWindowById( id, parent );
		
			if ( m_widget ) {
				wxGenericValidator validator( &m_value );

				m_widget->SetValidator( validator );
			
				return true;
			}
		} else {
			m_widget = NULL;
		}

		return false;
	}
	
	
	/**
	 * Sets the assosiated variable to the value of the widget.
	 *
	 * @return True on success, false otherwise.
	 */
	virtual bool TransferFromWindow()
	{
		if ( m_widget ) {
			wxValidator* validator = m_widget->GetValidator();

			if ( validator ) {
				TYPE temp = m_value;
			
				if ( validator->TransferFromWindow() ) {
					SetChanged( temp != m_value );

					return true;
				}
			}
		} 
		
		return false;
	}
	
	/**
	 * Sets the assosiated variable to the value of the widget.
	 *
	 * @return True on success, false otherwise.
	 */
	virtual bool TransferToWindow()
	{
		if ( m_widget ) {
			wxValidator* validator = m_widget->GetValidator();

			if ( validator )
				return validator->TransferToWindow();
		}

		return false;
	}

#endif

protected:
	//! Reference to the associated variable
	TYPE&	m_value;

	//! Default variable value
	TYPE	m_default;
	
	//! Pointer to the widget assigned to the Cfg instance
	wxWindow*	m_widget;
};


/**
 * Cfg class for wxStrings.
 */
class Cfg_Str : public Cfg_Tmpl<wxString>
{
public:
	/**
	 * Constructor.
	 */
	Cfg_Str( const wxString& keyname, wxString& value, const wxString& defaultVal = wxEmptyString )
	 : Cfg_Tmpl<wxString>( keyname, value, defaultVal )
	{}

	/**
	 * Saves the string to specified wxConfig.
	 *
	 * @param cfg The wxConfig to save the variable to.
	 */
	virtual void LoadFromFile(wxConfigBase* cfg)
	{
		cfg->Read( GetKey(), &m_value, m_default );
	}

	
	/**
	 * Loads the string to specified wxConfig using the specified default value.
	 *
	 * @param cfg The wxConfig to load the variable from.
	 */
	virtual void SaveToFile(wxConfigBase* cfg)
	{
		cfg->Write( GetKey(), m_value );
	}
};


/**
 * Cfg-class for encrypting strings, for example for passwords.
 */
class Cfg_Str_Encrypted : public Cfg_Str
{
public:
	Cfg_Str_Encrypted( const wxString& keyname, wxString& value, const wxString& defaultVal = wxEmptyString )
	 : Cfg_Str( keyname, value, defaultVal )
	{}

#ifndef AMULE_DAEMON
	virtual bool TransferFromWindow()
	{
		// Shakraw: when storing value, store it encrypted here (only if changed in prefs)
		if ( Cfg_Str::TransferFromWindow() ) {

			// Only recalucate the hash for new, non-empty passwords
			if ( HasChanged() && !m_value.IsEmpty() ) {
				m_value = MD5Sum( m_value ).GetHash();
			}

			return true;
		}


		return false;
	}
#endif
};


/**
 * Cfg class that takes care of integer types.
 *
 * This template is needed since wxValidator only supports normals ints, and 
 * wxConfig for the matter only supports longs, thus some worksarounds are
 * needed. 
 *
 * There are two work-arounds:
 *  1) wxValidator only supports int*, so we need a immediate variable to act
 *     as a storage. Thus we use Cfg_Tmpl<int> as base class. Thus this class
 *     contains a integer which we use to pass the value back and forth 
 *     between the widgets.
 *
 *  2) wxConfig uses longs to save and read values, thus we need an immediate
 *     stage when loading and saving the value.
 */
template <typename TYPE>
class Cfg_Int : public Cfg_Tmpl<int>
{
public:
	Cfg_Int( const wxString& keyname, TYPE& value, int defaultVal = 0 )
	 : Cfg_Tmpl<int>( keyname, m_temp_value, defaultVal ),
	   m_real_value( value ),
	   m_temp_value( value )
	{}


	virtual void LoadFromFile(wxConfigBase* cfg)
	{
		long tmp = 0;
		cfg->Read( GetKey(), &tmp, m_default ); 
		
		// Set the temp value
		m_temp_value = (int)tmp;
		// Set the actual value
		m_real_value = (TYPE)tmp;
	}

	virtual void SaveToFile(wxConfigBase* cfg)
	{
		cfg->Write( GetKey(), (long)m_real_value );
	}


#ifndef AMULE_DAEMON
	virtual bool TransferFromWindow()
	{
		if ( Cfg_Tmpl<int>::TransferFromWindow() ) { 
			m_real_value = (TYPE)m_temp_value;

			return true;
		} 
		
		return false;
	}

	virtual bool TransferToWindow()
	{
		m_temp_value = (int)m_real_value;

		if ( Cfg_Tmpl<int>::TransferToWindow() ) {
			
			// In order to let us update labels on slider-changes, we trigger a event
			wxSlider *slider = dynamic_cast<wxSlider *>(m_widget);
			if (slider) {
				int id = m_widget->GetId();
				int pos = slider->GetValue();
				wxScrollEvent evt( wxEVT_SCROLL_THUMBRELEASE, id, pos );
				m_widget->ProcessEvent( evt );
			}

			return true;
		}
		
		return false;
	}
#endif

protected:

	TYPE&	m_real_value;
	int		m_temp_value;
};


/**
 * Helper function for creating new Cfg_Ints.
 *
 * @param keyname The cfg-key under which the item should be saved.
 * @param value The variable to syncronize. The type of this variable defines the type used to create the Cfg_Int.
 * @param defaultVal The default value if the key isn't found when loading the value.
 * @return A pointer to the new Cfg_Int object. The caller is responsible for deleting it.
 *
 * This template-function returns a Cfg_Int of the appropriate type for the 
 * variable used as argument and should be used to avoid having to specify
 * the integer type when adding a new Cfg_Int, since that's just increases
 * the maintainence burden.
 */
template <class TYPE>
Cfg_Base* MkCfg_Int( const wxString& keyname, TYPE& value, int defaultVal )
{
	return new Cfg_Int<TYPE>( keyname, value, defaultVal );
}


/**
 * Cfg-class for bools.
 */
class Cfg_Bool : public Cfg_Tmpl<bool>
{
public:
	Cfg_Bool( const wxString& keyname, bool& value, bool defaultVal )
	 : Cfg_Tmpl<bool>( keyname, value, defaultVal )
	{}

	
	virtual void LoadFromFile(wxConfigBase* cfg)
	{
		cfg->Read( GetKey(), &m_value, m_default );
	}

	virtual void SaveToFile(wxConfigBase* cfg)
	{
		cfg->Write( GetKey(), m_value );
	}
};


/**
 * Cfg-class for uint64s, with no assisiated widgets.
 */
class Cfg_Counter : public Cfg_Base
{
public:
	Cfg_Counter( const wxString& keyname, uint64& value )
	 : Cfg_Base( keyname ),
	   m_value( value )
	{}

	virtual void LoadFromFile(wxConfigBase* cfg)
	{
		wxString buffer;
	
		cfg->Read( GetKey(), &buffer, wxT("0") );

		uint64 tmp = 0;
		for (unsigned int i = 0; i < buffer.Length(); ++i) {
			if ((buffer[i] >= wxChar('0')) &&(buffer[i] <= wxChar('9'))) {
				tmp = tmp * 10 + (buffer[i] - wxChar('0'));
			} else {
				tmp = 0;
				break;
			}
		}
		m_value = tmp;
	}

	virtual void SaveToFile(wxConfigBase* cfg)
	{
		wxString str = CFormat( wxT("%llu") ) % m_value;
	
		cfg->Write( GetKey(), str );
	}

	
protected:
	uint64& m_value;
};


#ifndef AMULE_DAEMON

typedef struct {
	int	id;
	wxString name;
} LangInfo;


/**
 * The languages aMule has translation for.
 *
 * Add new languages here, as this list overrides the one defined in muuli.wdr
 */
static LangInfo aMuleLanguages[] = {
	{ wxLANGUAGE_DEFAULT,			wxTRANSLATE("System default") },
	{ wxLANGUAGE_ARABIC,			wxTRANSLATE("Arabic") },
	{ wxLANGUAGE_BASQUE,			wxTRANSLATE("Basque") },
	{ wxLANGUAGE_BULGARIAN,			wxTRANSLATE("Bulgarian") },
	{ wxLANGUAGE_CATALAN,			wxTRANSLATE("Catalan") },
	{ wxLANGUAGE_CHINESE_SIMPLIFIED,	wxTRANSLATE("Chinese (Simplified)") },
	{ wxLANGUAGE_CHINESE_TRADITIONAL,	wxTRANSLATE("Chinese (Traditional)") },
	{ wxLANGUAGE_CROATIAN,			wxTRANSLATE("Croatian") },
	{ wxLANGUAGE_DANISH,			wxTRANSLATE("Danish") },
	{ wxLANGUAGE_DUTCH,			wxTRANSLATE("Dutch") },
	{ wxLANGUAGE_ENGLISH_UK,		wxTRANSLATE("English (U.K.)") },
	{ wxLANGUAGE_ENGLISH_US,		wxTRANSLATE("English (U.S.)") },
	{ wxLANGUAGE_ESTONIAN,			wxTRANSLATE("Estonian") },
	{ wxLANGUAGE_FINNISH,			wxTRANSLATE("Finnish") },
	{ wxLANGUAGE_FRENCH,			wxTRANSLATE("French") },
	{ wxLANGUAGE_GALICIAN,			wxTRANSLATE("Galician") },
	{ wxLANGUAGE_GERMAN,			wxTRANSLATE("German") },
	{ wxLANGUAGE_HUNGARIAN,			wxTRANSLATE("Hungarian") },
	{ wxLANGUAGE_ITALIAN,			wxTRANSLATE("Italian") },
// Hmm, it_NA.po not present ...
//	{ wxLANGUAGE_ITALIAN_NAPOLITAN,		wxTRANSLATE("Italian (Napolitan)") },
	{ wxLANGUAGE_ITALIAN_SWISS,		wxTRANSLATE("Italian (Swiss)") },
	{ wxLANGUAGE_KOREAN,			wxTRANSLATE("Korean") },
// There is no such file as lt.po
//	{ wxLANGUAGE_LITHUANIAN,		wxTRANSLATE("Lithuanian") },
	{ wxLANGUAGE_POLISH,			wxTRANSLATE("Polish") },
	{ wxLANGUAGE_PORTUGUESE,		wxTRANSLATE("Portuguese") },
	{ wxLANGUAGE_PORTUGUESE_BRAZILIAN,	wxTRANSLATE("Portuguese (Brazilian)") },
	{ wxLANGUAGE_RUSSIAN,			wxTRANSLATE("Russian") },
	{ wxLANGUAGE_SLOVENIAN,			wxTRANSLATE("Slovenian") },
	{ wxLANGUAGE_SPANISH,			wxTRANSLATE("Spanish") },
// Apparently there's no es_CL.po
//	{ wxLANGUAGE_SPANISH_CHILE,		wxTRANSLATE("Spanish (Chile)") },
	{ wxLANGUAGE_SPANISH_MEXICAN,		wxTRANSLATE("Spanish (Mexican)") },
// Turkish had caused problems with the config file, disabled until tested
//	{ wxLANGUAGE_TURKISH,			wxTRANSLATE("Turkish") },
// Yet no real support for "custom"
//	{ wxLANGUAGE_CUSTOM,			wxTRANSLATE("Custom") },
};


bool TranslatedSort( const LangInfo& a, const LangInfo& b ) {
	return wxStricmp( wxGetTranslation( a.name ), wxGetTranslation( b.name) ) < 0;
}


class Cfg_Lang : public Cfg_Tmpl<int>
{
public:
	Cfg_Lang()
		: Cfg_Tmpl<int>( wxEmptyString, m_selection, 0 )
	{
	}

	virtual void LoadFromFile(wxConfigBase* WXUNUSED(cfg)) {}
	virtual void SaveToFile(wxConfigBase* WXUNUSED(cfg)) {}


	virtual bool TransferFromWindow()
	{
		if ( Cfg_Tmpl<int>::TransferFromWindow() ) { 
			// find wx ID of selected language
			int id = aMuleLanguages[m_selection].id;
		
			// save language selection
			thePrefs::SetLanguageID(otherfunctions::wxLang2Str(id));

			return true;
		}

		return false;
	}


	virtual bool TransferToWindow()
	{
		// Sort the list after the current locale
		std::sort( aMuleLanguages + 1, // Dont include DEFAULT
			   aMuleLanguages + itemsof(aMuleLanguages),
			   TranslatedSort );
			
		wxChoice *langSelector = dynamic_cast<wxChoice*>(m_widget);
		// clear existing list
		langSelector->Clear();

		m_selection = 0;
		int wxId = otherfunctions::StrLang2wx(thePrefs::GetLanguageID());
	
		// Add all other languages in alphabetical order
		// and find the index of the selected language.
		for ( unsigned int i = 0; i < itemsof(aMuleLanguages); i++) {
			if ( aMuleLanguages[i].id == wxId ) {
				m_selection = i;
			}
			
			langSelector->Append( wxGetTranslation(aMuleLanguages[i].name) );
		}

		return Cfg_Tmpl<int>::TransferToWindow();
	}

protected:
	int	m_selection;
};

#endif /* ! AMULE_DAEMON */


/// new implementation
CPreferences::CPreferences()
{
	srand( wxGetLocalTimeMillis().GetLo() ); // we need random numbers sometimes

	CreateUserHash();

	// load preferences.dat or set standart values
	wxString fullpath(theApp.ConfigDir + wxT("preferences.dat"));

	CFile preffile;
	if ( wxFileExists( fullpath ) ) {
		if ( preffile.Open(fullpath, CFile::read) ) {
			Preferences_Ext_Struct prefsExt;
	
			memset( &prefsExt, 0, sizeof(Preferences_Ext_Struct) );
			// NOTE: This Read is dangerous.  It doesn't do The Right Thing
			// with respect to endianness for the fields of prefsExt.  At this
			// time, it doesn't matter because no fields except the version and
			// userhash are used and they aren't sensitive to endianness.  If
			// other fields are used in the future, this will have to be revisited.
			off_t read = preffile.Read( &prefsExt, sizeof(Preferences_Ext_Struct) );

			if ( read == sizeof(Preferences_Ext_Struct) ) {
				md4cpy(s_userhash, prefsExt.userhash);
			} else {
				SetStandartValues();
			}
			
			preffile.Close();	
		} else {
			SetStandartValues();
		}
	} else {
		SetStandartValues();
	}
	
#ifndef CLIENT_GUI
	LoadPreferences();


	 ReloadSharedFolders();

	// serverlist adresses
	wxTextFile slistfile(theApp.ConfigDir + wxT("addresses.dat"));
	if ( slistfile.Exists() && slistfile.Open()) {
		for ( size_t i = 0; i < slistfile.GetLineCount(); i++ ) {
    		adresses_list.Add( slistfile.GetLine( i ) );
		}
		
		slistfile.Close();
	}

	s_userhash[5] = 14;
	s_userhash[14] = 111;

	if (!::wxDirExists(GetIncomingDir())) {
		::wxMkdir( GetIncomingDir(), GetDirPermissions() );
	}

	if (!::wxDirExists(GetTempDir())) {
		::wxMkdir( GetTempDir(), GetDirPermissions() );
	}

	if (s_userhash.IsEmpty()) {
		CreateUserHash();
	}
	
#endif
}

//
// Gets called at init time
// 
void CPreferences::BuildItemList( const wxString& appdir )
{
#ifndef AMULE_DAEMON
	#define NewCfgItem(ID, COMMAND)	s_CfgList[ID] = COMMAND
#else
	int current_id = 0;
	#define NewCfgItem(ID, COMMAND)	s_CfgList[++current_id] = COMMAND
#endif /* AMULE_DAEMON */
	
	/**
	 * User settings
	 **/
	NewCfgItem(IDC_NICK,		(new Cfg_Str(  wxT("/eMule/Nick"), s_nick, wxT("http://www.aMule.org") )));
#ifndef AMULE_DAEMON
	NewCfgItem(IDC_LANGUAGE,	(new Cfg_Lang()));
#endif

	/**
	 * Misc
	 **/
	#ifdef __WXMAC__
		int			browser = 8; // this is a "magic number" and will break if
								 // more browser choices are added in the interface,
								 // but there isn't a symbolic name defined
		wxString	customBrowser = wxT("/usr/bin/open");
	#else 
		int			browser = 0;
		wxString	customBrowser; // left empty
	#endif

	NewCfgItem(IDC_FCHECK,		(MkCfg_Int( wxT("/FakeCheck/Browser"), s_Browser, browser )));
	NewCfgItem(IDC_FCHECKTABS,	(new Cfg_Bool( wxT("/FakeCheck/BrowserTab"), s_BrowserTab, true )));
	NewCfgItem(IDC_FCHECKSELF,	(new Cfg_Str(  wxT("/FakeCheck/CustomBrowser"), s_CustomBrowser, customBrowser )));
	NewCfgItem(IDC_QUEUESIZE,	(MkCfg_Int( wxT("/eMule/QueueSizePref"), s_iQueueSize, 50 )));


#ifdef __DEBUG__
	/**
	 * Debugging
	 **/
	NewCfgItem(ID_VERBOSEDEBUG, (new Cfg_Bool( wxT("/eMule/VerboseDebug"), s_bVerbose, false )));
#endif

	/**
	 * Connection settings
	 **/
	NewCfgItem(IDC_MAXUP,		(MkCfg_Int( wxT("/eMule/MaxUpload"), s_maxupload, 0 )));
	NewCfgItem(IDC_MAXDOWN,		(MkCfg_Int( wxT("/eMule/MaxDownload"), s_maxdownload, 0 )));
	NewCfgItem(IDC_SLOTALLOC,	(MkCfg_Int( wxT("/eMule/SlotAllocation"), s_slotallocation, 2 )));
	NewCfgItem(IDC_PORT,		(MkCfg_Int( wxT("/eMule/Port"), s_port, DEFAULT_TCP_PORT )));
	NewCfgItem(IDC_UDPPORT,		(MkCfg_Int( wxT("/eMule/UDPPort"), s_udpport, 4672 )));
	NewCfgItem(IDC_UDPDISABLE,	(new Cfg_Bool( wxT("/eMule/UDPDisable"), s_UDPDisable, false )));
	NewCfgItem(IDC_AUTOCONNECT,	(new Cfg_Bool( wxT("/eMule/Autoconnect"), s_autoconnect, true )));
	NewCfgItem(IDC_MAXSOURCEPERFILE,	(MkCfg_Int( wxT("/eMule/MaxSourcesPerFile"), s_maxsourceperfile, 300 )));
	NewCfgItem(IDC_MAXCON,		(MkCfg_Int( wxT("/eMule/MaxConnections"), s_maxconnections, GetRecommendedMaxConnections() )));
	NewCfgItem(IDC_MAXCON5SEC,	(MkCfg_Int( wxT("/eMule/MaxConnectionsPerFiveSeconds"), s_MaxConperFive, 20 )));

	/**
	 * Proxy
	 **/
	NewCfgItem(ID_PROXY_ENABLE_PROXY,	(new Cfg_Bool( wxT("/Proxy/ProxyEnableProxy"), s_ProxyData.m_proxyEnable, false )));
	NewCfgItem(ID_PROXY_TYPE,		(MkCfg_Int( wxT("/Proxy/ProxyType"), s_ProxyData.m_proxyType, 0 )));
	NewCfgItem(ID_PROXY_NAME,		(new Cfg_Str( wxT("/Proxy/ProxyName"), s_ProxyData.m_proxyHostName, wxEmptyString )));
	NewCfgItem(ID_PROXY_PORT,		(MkCfg_Int( wxT("/Proxy/ProxyPort"), s_ProxyData.m_proxyPort, 1080 )));
	NewCfgItem(ID_PROXY_ENABLE_PASSWORD,	(new Cfg_Bool( wxT("/Proxy/ProxyEnablePassword"), s_ProxyData.m_enablePassword, false )));
	NewCfgItem(ID_PROXY_USER,		(new Cfg_Str( wxT("/Proxy/ProxyUser"), s_ProxyData.m_userName, wxEmptyString )));
	NewCfgItem(ID_PROXY_PASSWORD,		(new Cfg_Str( wxT("/Proxy/ProxyPassword"), s_ProxyData.m_password, wxEmptyString )));
// These were copied from eMule config file, maybe someone with windows can complete this?
//	NewCfgItem(ID_PROXY_AUTO_SERVER_CONNECT_WITHOUT_PROXY,	(new Cfg_Bool( wxT("/Proxy/Proxy????"), s_Proxy????, false )));
	
	/**
	 * Servers
	 **/ 
	NewCfgItem(IDC_REMOVEDEAD,	(new Cfg_Bool( wxT("/eMule/RemoveDeadServer"), s_deadserver, 1 )));
	NewCfgItem(IDC_SERVERRETRIES,	(MkCfg_Int( wxT("/eMule/DeadServerRetry"), s_deadserverretries, 2 )));
	NewCfgItem(IDC_SERVERKEEPALIVE,	(MkCfg_Int( wxT("/eMule/ServerKeepAliveTimeout"), s_dwServerKeepAliveTimeoutMins, 0 )));
	NewCfgItem(IDC_RECONN,		(new Cfg_Bool( wxT("/eMule/Reconnect"), s_reconnect, true )));
	NewCfgItem(IDC_SCORE,		(new Cfg_Bool( wxT("/eMule/Scoresystem"), s_scorsystem, true )));
	NewCfgItem(IDC_AUTOSERVER,	(new Cfg_Bool( wxT("/eMule/Serverlist"), s_autoserverlist, false )));
	NewCfgItem(IDC_UPDATESERVERCONNECT,	(new Cfg_Bool( wxT("/eMule/AddServersFromServer"), s_addserversfromserver, true)));
	NewCfgItem(IDC_UPDATESERVERCLIENT,	(new Cfg_Bool( wxT("/eMule/AddServersFromClient"), s_addserversfromclient, true )));
	NewCfgItem(IDC_SAFESERVERCONNECT,	(new Cfg_Bool( wxT("/eMule/SafeServerConnect"), s_safeServerConnect, false )));
	NewCfgItem(IDC_AUTOCONNECTSTATICONLY,	(new Cfg_Bool( wxT("/eMule/AutoConnectStaticOnly"), s_autoconnectstaticonly, false )));
	NewCfgItem(IDC_SMARTIDCHECK,	(new Cfg_Bool( wxT("/eMule/SmartIdCheck"), s_smartidcheck, true )));

	/**
	 * Files
	 **/
	NewCfgItem(IDC_TEMPFILES,	(new Cfg_Str(  wxT("/eMule/TempDir"), 	s_tempdir, appdir + wxT("Temp") )));
	
	#ifdef __WXMAC__
		wxString incpath = wxGetHomeDir() + wxFileName::GetPathSeparator() + wxT("Documents") + wxFileName::GetPathSeparator() + wxT("aMule Downloads");
	#elif defined(__WXMSW__)
		wxString incpath;
		LPITEMIDLIST pidl;
		HRESULT hr = SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl);
		if (SUCCEEDED(hr)) {
			if (SHGetPathFromIDList(pidl, wxStringBuffer(incpath, MAX_PATH))) {
				incpath.append(wxString(wxFileName::GetPathSeparator()) + wxString(wxT("aMule Downloads")));
			} else {
				incpath = appdir + wxT("Incoming");
			}
		} else {
			incpath = appdir + wxT("Incoming");
		}
		if (pidl) {
			LPMALLOC pMalloc;
			SHGetMalloc(&pMalloc);
			if (pMalloc) {
				pMalloc->Free(pidl);
				pMalloc->Release();
			}
		}
	#else 
		wxString incpath = appdir + wxT("Incoming");
	#endif
	NewCfgItem(IDC_INCFILES,	(new Cfg_Str(  wxT("/eMule/IncomingDir"), s_incomingdir, incpath )));
	
	NewCfgItem(IDC_ICH,		(new Cfg_Bool( wxT("/eMule/ICH"), s_ICH, true )));
	NewCfgItem(IDC_AICHTRUST,	(new Cfg_Bool( wxT("/eMule/AICHTrust"), s_AICHTrustEveryHash, false )));
	NewCfgItem(IDC_METADATA,	(new Cfg_Bool( wxT("/ExternalConnect/ExtractMetaDataTags"), s_ExtractMetaData, false )));
	NewCfgItem(IDC_CHUNKALLOC,	(new Cfg_Bool( wxT("/ExternalConnect/FullChunkAlloc"), s_AllocFullChunk, false )));
	NewCfgItem(IDC_FULLALLOCATE,	(new Cfg_Bool( wxT("/ExternalConnect/FullPartAlloc"), s_AllocFullPart, false )));
	NewCfgItem(IDC_CHECKDISKSPACE,	(new Cfg_Bool( wxT("/eMule/CheckDiskspace"), s_checkDiskspace, true )));
	NewCfgItem(IDC_MINDISKSPACE,	(MkCfg_Int( wxT("/eMule/MinFreeDiskSpace"), s_uMinFreeDiskSpace, 1 )));
	NewCfgItem(IDC_ADDNEWFILESPAUSED,	(new Cfg_Bool( wxT("/eMule/AddNewFilesPaused"), s_addnewfilespaused, false )));
	NewCfgItem(IDC_PREVIEWPRIO,	(new Cfg_Bool( wxT("/eMule/PreviewPrio"), s_bpreviewprio, false )));
	NewCfgItem(IDC_MANUALSERVERHIGHPRIO,	(new Cfg_Bool( wxT("/eMule/ManualHighPrio"), s_bmanualhighprio, false )));
	NewCfgItem(IDC_FULLCHUNKTRANS,	(new Cfg_Bool( wxT("/eMule/FullChunkTransfers"), s_btransferfullchunks, true )));
	NewCfgItem(IDC_STARTNEXTFILE,	(new Cfg_Bool( wxT("/eMule/StartNextFile"), s_bstartnextfile, false )));
	NewCfgItem(IDC_STARTNEXTFILE_SAME,	(new Cfg_Bool( wxT("/eMule/StartNextFileSameCat"), s_bstartnextfilesame, false )));
	NewCfgItem(IDC_FILEBUFFERSIZE,	(MkCfg_Int( wxT("/eMule/FileBufferSizePref"), s_iFileBufferSize, 16 )));
	NewCfgItem(IDC_DAP,		(new Cfg_Bool( wxT("/eMule/DAPPref"), s_bDAP, true )));
	NewCfgItem(IDC_UAP,		(new Cfg_Bool( wxT("/eMule/UAPPref"), s_bUAP, true )));

	/**
	 * External Connections
	 */
	NewCfgItem(IDC_OSDIR,		(new Cfg_Str(  wxT("/eMule/OSDirectory"), s_OSDirectory,	appdir )));
	NewCfgItem(IDC_ONLINESIG,	(new Cfg_Bool( wxT("/eMule/OnlineSignature"), s_onlineSig, false )));
	NewCfgItem(IDC_OSUPDATE,	(MkCfg_Int( wxT("/eMule/OnlineSignatureUpdate"), s_OSUpdate, 5 )));
	NewCfgItem(IDC_ENABLE_WEB,	(new Cfg_Bool( wxT("/WebServer/Enabled"), s_bWebEnabled, false )));
	NewCfgItem(IDC_WEB_PASSWD,	(new Cfg_Str_Encrypted( wxT("/WebServer/Password"), s_sWebPassword )));
	NewCfgItem(IDC_WEB_PASSWD_LOW,	(new Cfg_Str_Encrypted( wxT("/WebServer/PasswordLow"), s_sWebLowPassword )));
	NewCfgItem(IDC_WEB_PORT,	(MkCfg_Int( wxT("/WebServer/Port"), s_nWebPort, 4711 )));
	NewCfgItem(IDC_WEB_GZIP,	(new Cfg_Bool( wxT("/WebServer/UseGzip"), s_bWebUseGzip, true )));
	NewCfgItem(IDC_ENABLE_WEB_LOW,	(new Cfg_Bool( wxT("/WebServer/UseLowRightsUser"), s_bWebLowEnabled, false )));
	NewCfgItem(IDC_WEB_REFRESH_TIMEOUT,	(MkCfg_Int( wxT("/WebServer/PageRefreshTime"), s_nWebPageRefresh, 120 )));
	NewCfgItem(IDC_EXT_CONN_ACCEPT,	(new Cfg_Bool( wxT("/ExternalConnect/AcceptExternalConnections"), s_AcceptExternalConnections, false )));
	NewCfgItem(IDC_EXT_CONN_USETCP,	(new Cfg_Bool( wxT("/ExternalConnect/ECUseTCPPort"), s_ECUseTCPPort, false )));
	NewCfgItem(IDC_EXT_CONN_TCP_PORT,	(MkCfg_Int( wxT("/ExternalConnect/ECPort"), s_ECPort, 4712 )));
	NewCfgItem(IDC_EXT_CONN_PASSWD,	(new Cfg_Str_Encrypted( wxT("/ExternalConnect/ECPassword"), s_ECPassword, wxEmptyString )));

	/**
	 * GUI behavior
	 **/
	NewCfgItem(IDC_ENABLETRAYICON,	(new Cfg_Bool( wxT("/eMule/EnableTrayIcon"), s_trayiconenabled, false )));
	NewCfgItem(IDC_MINTRAY,		(new Cfg_Bool( wxT("/eMule/MinToTray"), s_mintotray, false )));
	NewCfgItem(IDC_EXIT,		(new Cfg_Bool( wxT("/eMule/ConfirmExit"), s_confirmExit, false )));
	NewCfgItem(IDC_DBLCLICK,	(new Cfg_Bool( wxT("/eMule/TransferDoubleClick"), s_transferDoubleclick, true )));
	NewCfgItem(IDC_STARTMIN,	(new Cfg_Bool( wxT("/eMule/StartupMinimized"), s_startMinimized, false )));

	/**
	 * GUI appearence
	 **/
	NewCfgItem(IDC_3DDEPTH,		(MkCfg_Int( wxT("/eMule/3DDepth"), s_depth3D, 10 )));
	NewCfgItem(IDC_TOOLTIPDELAY,	(MkCfg_Int( wxT("/eMule/ToolTipDelay"), s_iToolDelayTime, 1 )));
	NewCfgItem(IDC_SHOWOVERHEAD,	(new Cfg_Bool( wxT("/eMule/ShowOverhead"), s_bshowoverhead, false )));
	NewCfgItem(IDC_EXTCATINFO,	(new Cfg_Bool( wxT("/eMule/ShowInfoOnCatTabs"), s_showCatTabInfos, false )));
	NewCfgItem(IDC_FED2KLH,		(new Cfg_Bool( wxT("/Razor_Preferences/FastED2KLinksHandler"), s_FastED2KLinksHandler, true )));
	NewCfgItem(IDC_PROGBAR,		(new Cfg_Bool( wxT("/ExternalConnect/ShowProgressBar"), s_ProgBar, true )));
	NewCfgItem(IDC_PERCENT,		(new Cfg_Bool( wxT("/ExternalConnect/ShowPercent"), s_Percent, false )));
	NewCfgItem(IDC_USESKIN,		(new Cfg_Bool( wxT("/SkinGUIOptions/UseSkinFile"), s_UseSkinFile, false )));
	NewCfgItem(IDC_SKINFILE,	(new Cfg_Str(  wxT("/SkinGUIOptions/SkinFile"), s_SkinFile, wxEmptyString )));
	NewCfgItem(IDC_SHOWRATEONTITLE,	(new Cfg_Bool( wxT("/eMule/ShowRatesOnTitle"), s_ShowRatesOnTitle, false )));
	
	/**
	 * External Apps
	 */
	NewCfgItem(IDC_VIDEOPLAYER,	(new Cfg_Str(  wxT("/eMule/VideoPlayer"), s_VideoPlayer, wxEmptyString )));
	NewCfgItem(IDC_VIDEOBACKUP,	(new Cfg_Bool( wxT("/eMule/VideoPreviewBackupped"), s_moviePreviewBackup, true )));

	/**
	 * Statistics
	 **/
	NewCfgItem(IDC_SLIDER,		(MkCfg_Int( wxT("/eMule/StatGraphsInterval"), s_trafficOMeterInterval, 3 )));
	NewCfgItem(IDC_SLIDER2,		(MkCfg_Int( wxT("/eMule/statsInterval"), s_statsInterval, 30 )));
	NewCfgItem(IDC_DOWNLOAD_CAP,	(MkCfg_Int( wxT("/eMule/DownloadCapacity"), s_maxGraphDownloadRate, 3 )));
	NewCfgItem(IDC_UPLOAD_CAP,	(MkCfg_Int( wxT("/eMule/UploadCapacity"), s_maxGraphUploadRate, 3 )));
	NewCfgItem(IDC_SLIDER3,		(MkCfg_Int( wxT("/eMule/StatsAverageMinutes"), s_statsAverageMinutes, 5 )));
	NewCfgItem(IDC_SLIDER4,		(MkCfg_Int( wxT("/eMule/VariousStatisticsMaxValue"), s_statsMax, 100 )));

	/**
	 * Sources
	 **/
	NewCfgItem(IDC_ENABLE_AUTO_FQS,	(new Cfg_Bool( wxT("/Razor_Preferences/FullQueueSources"), s_DropFullQueueSources,  false )));
	NewCfgItem(IDC_ENABLE_AUTO_HQRS,	(new Cfg_Bool( wxT("/Razor_Preferences/HighQueueRankingSources"), s_DropHighQueueRankingSources, false )));
	NewCfgItem(IDC_HQR_VALUE,	(MkCfg_Int( wxT("/Razor_Preferences/HighQueueRanking"), s_HighQueueRanking, 1200 )));
	NewCfgItem(IDC_AUTO_DROP_TIMER,	(MkCfg_Int( wxT("/Razor_Preferences/AutoDropTimer"), s_AutoDropTimer, 240 )));
	NewCfgItem(IDC_NNS_HANDLING,	(MkCfg_Int( wxT("/Razor_Preferences/NoNeededSourcesHandling"), s_NoNeededSources, 2 )));
	NewCfgItem(IDC_SRCSEEDS,	(new Cfg_Bool( wxT("/ExternalConnect/UseSrcSeeds"), s_UseSrcSeeds, false )));

	/**
	 * Security
	 **/
	NewCfgItem(IDC_SEESHARES,	(MkCfg_Int( wxT("/eMule/SeeShare"),	s_iSeeShares, 2 )));
	NewCfgItem(IDC_SECIDENT,	(new Cfg_Bool( wxT("/ExternalConnect/UseSecIdent"), s_SecIdent, true )));
	NewCfgItem(IDC_IPFONOFF,	(new Cfg_Bool( wxT("/ExternalConnect/IpFilterOn"), s_IPFilterOn, true )));
	NewCfgItem(IDC_FILTER,		(new Cfg_Bool( wxT("/eMule/FilterLanIPs"), s_filterLanIP, true )));
	NewCfgItem(IDC_AUTOIPFILTER,	(new Cfg_Bool( wxT("/eMule/IPFilterAutoLoad"), s_IPFilterAutoLoad, true )));
	NewCfgItem(IDC_IPFILTERURL,	(new Cfg_Str(  wxT("/eMule/IPFilterURL"), s_IPFilterURL, wxEmptyString )));
	NewCfgItem(ID_IPFILTERLEVEL,	(MkCfg_Int( wxT("/eMule/FilterLevel"), s_filterlevel, 127 )));
	
	/** 
	 * Message Filter 
	 **/
	NewCfgItem(IDC_MSGFILTER,	(new Cfg_Bool( wxT("/eMule/FilterMessages"), s_MustFilterMessages, false )));
	NewCfgItem(IDC_MSGFILTER_ALL,	(new Cfg_Bool( wxT("/eMule/FilterAllMessages"), s_FilterAllMessages, false )));
	NewCfgItem(IDC_MSGFILTER_NONFRIENDS,	(new Cfg_Bool( wxT("/eMule/MessagesFromFriendsOnly"),	s_msgonlyfriends, false )));
	NewCfgItem(IDC_MSGFILTER_NONSECURE,	(new Cfg_Bool( wxT("/eMule/MessageFromValidSourcesOnly"),	s_msgsecure, true )));
	NewCfgItem(IDC_MSGFILTER_WORD,	(new Cfg_Bool( wxT("/eMule/FilterWordMessages"), s_FilterSomeMessages, false )));
	NewCfgItem(IDC_MSGWORD,		(new Cfg_Str(  wxT("/eMule/MessageFilter"), s_MessageFilterString, wxEmptyString )));
	 
	/**
	 * Hidden files sharing
	 **/	  
	NewCfgItem(IDC_SHAREHIDDENFILES,	(new Cfg_Bool( wxT("/eMule/ShareHiddenFiles"), s_ShareHiddenFiles, false )));

	/**
	 * Auto-Sorting of downloads
	 **/
	 NewCfgItem(IDC_AUTOSORT,	 (new Cfg_Bool( wxT("/eMule/AutoSortDownloads"), s_AutoSortDownload, false )));

	/**
	 * The following doesn't have an associated widget.
	 **/
	s_MiscList.push_back( new Cfg_Str(  wxT("/eMule/Language"),			s_languageID ) );
	s_MiscList.push_back( new Cfg_Counter( wxT("/Statistics/TotalDownloadedBytes"), s_totalDownloadedBytes ) );
	s_MiscList.push_back( new Cfg_Counter( wxT("/Statistics/TotalUploadedBytes"),	s_totalUploadedBytes ) );
	s_MiscList.push_back(    MkCfg_Int( wxT("/eMule/SplitterbarPosition"),		s_splitterbarPosition, 75 ) );
	s_MiscList.push_back( new Cfg_Str(  wxT("/eMule/YourHostname"),			s_yourHostname, wxEmptyString ) );
	s_MiscList.push_back( new Cfg_Str(  wxT("/eMule/DateTimeFormat"),		s_datetimeformat, wxT("%A, %x, %X") ) );

	s_MiscList.push_back( new Cfg_Bool( wxT("/eMule/IndicateRatings"),		s_indicateratings, true ) );
	s_MiscList.push_back(    MkCfg_Int( wxT("/eMule/AllcatType"),			s_allcatType, 0 ) );
	s_MiscList.push_back( new Cfg_Bool( wxT("/eMule/ShowAllNotCats"),		s_showAllNotCats, false ) );
	s_MiscList.push_back( new Cfg_Bool( wxT("/eMule/DisableKnownClientList"),	s_bDisableKnownClientList, false ) );
	s_MiscList.push_back( new Cfg_Bool( wxT("/eMule/DisableQueueList"),		s_bDisableQueueList, false ) );
	s_MiscList.push_back(    MkCfg_Int( wxT("/eMule/MaxMessageSessions"),		s_maxmsgsessions, 50 ) );

	s_MiscList.push_back(	 MkCfg_Int( wxT("/Statistics/DesktopMode"), s_desktopMode, 4 ) );
	s_MiscList.push_back(	 MkCfg_Int( wxT("/eMule/PermissionsFiles"),	s_perms_files, 0640 ) );
	s_MiscList.push_back(	 MkCfg_Int( wxT("/eMule/PermissionsDirs"),	s_perms_dirs, 0750 ) );

#ifndef AMULE_DAEMON
	// Colors have been moved from global prefs to CStatisticsDlg
	for ( int i = 0; i < cntStatColors; i++ ) {  
		wxString str = wxString::Format(wxT("/eMule/StatColor%i"),i);
		
		s_MiscList.push_back( MkCfg_Int( str, CStatisticsDlg::acrStat[i], CStatisticsDlg::acrStat[i] ) );
	}
#endif
}


void CPreferences::EraseItemList()
{
	while ( s_CfgList.begin() != s_CfgList.end() ) {
		delete s_CfgList.begin()->second;
		s_CfgList.erase( s_CfgList.begin() );
	}
	
	CFGList::iterator it = s_MiscList.begin();
	for ( ; it != s_MiscList.end();  ) {
		delete *it;
		it = s_MiscList.erase( it );
	}
}


void CPreferences::LoadAllItems(wxConfigBase* cfg)
{
	// Connect the Cfgs with their widgets
	CFGMap::iterator it_a = s_CfgList.begin();
	for ( ; it_a != s_CfgList.end(); ++it_a ) {
		it_a->second->LoadFromFile( cfg );
	}

	CFGList::iterator it_b = s_MiscList.begin();
	for ( ; it_b != s_MiscList.end(); ++it_b ) {
		(*it_b)->LoadFromFile( cfg ); 
	}

// Load debug-categories
#ifdef __DEBUG__
	int count = CLogger::GetDebugCategoryCount();

	for ( int i = 0; i < count; i++ ) {
		const CDebugCategory& cat = CLogger::GetDebugCategory( i );
		
		bool enabled = false;
		cfg->Read( wxT("/Debug/Cat_") + cat.GetName(), &enabled );

		CLogger::SetEnabled( cat.GetType(), enabled );
	}	
#endif
	
	// Now do some post-processing / sanity checking on the values we just loaded
	CheckUlDlRatio();
	SetPort(s_port);
}


void CPreferences::SaveAllItems(wxConfigBase* cfg)
{
	// Save the Cfg values
	CFGMap::iterator it_a = s_CfgList.begin();
	for ( ; it_a != s_CfgList.end(); ++it_a )
		it_a->second->SaveToFile( cfg );

	CFGList::iterator it_b = s_MiscList.begin();
	for ( ; it_b != s_MiscList.end(); ++it_b )
		(*it_b)->SaveToFile( cfg ); 


// Save debug-categories
#ifdef __DEBUG__
	int count = CLogger::GetDebugCategoryCount();

	for ( int i = 0; i < count; i++ ) {
		const CDebugCategory& cat = CLogger::GetDebugCategory( i );

		wxString entry = wxT("/Debug/Cat_") + cat.GetName();
		if ( cat.IsEnabled() ) {
			cfg->Write( entry, true );
		} else if ( cfg->Exists( entry ) ) {
			// Avoid a buildup of stale entries
			cfg->DeleteEntry( entry );
		}
	}	
#endif
}

void CPreferences::SetMaxUpload(uint16 in)
{
	if ( s_maxupload != in ) {
		s_maxupload = in;

		// Ensure that the ratio is upheld
		CheckUlDlRatio();
	}
}


void CPreferences::SetMaxDownload(uint16 in)
{
	if ( s_maxdownload != in ) {
		s_maxdownload = in;

		// Ensure that the ratio is upheld
		CheckUlDlRatio();
	}
}


int CPreferences::GetFilePermissions()
{
	// We need at least r/w access for user
	return s_perms_files | wxS_IRUSR | wxS_IWUSR;
}


void CPreferences::SetFilePermissions( int perms )
{
	// We need at least r/w access for user
	s_perms_files = perms | wxS_IRUSR | wxS_IWUSR;
}


int CPreferences::GetDirPermissions()
{
	// We need at least r/w/x access for user
	return s_perms_dirs | wxS_IRUSR | wxS_IWUSR | wxS_IXUSR;
}


void CPreferences::SetDirPermissions( int perms )
{
	// We need at least r/w/x access for user
	s_perms_dirs = perms | wxS_IRUSR | wxS_IWUSR | wxS_IXUSR;
}


void CPreferences::UnsetAutoServerStart()
{
	s_autoserverlist = false;
}


// Here we slightly limit the users' ability to be a bad citizen: for very low upload rates
// we force a low download rate, so as to discourage this type of leeching.  
// We're Open Source, and whoever wants it can do his own mod to get around this, but the 
// packaged product will try to enforce good behavior. 
//
// Kry note: of course, any leecher mod will be banned asap.
void CPreferences::CheckUlDlRatio()
{
	// Backwards compatibility
	if ( s_maxupload == 0xFFFF )
		s_maxupload = UNLIMITED;

	// Backwards compatibility
	if ( s_maxdownload == 0xFFFF )
		s_maxdownload = UNLIMITED;
		
	if ( s_maxupload == UNLIMITED )
		return;
	
	// Enforce the limits
	if ( s_maxupload < 4  ) {
		if ( ( s_maxupload * 3 < s_maxdownload ) || ( s_maxdownload == 0 ) )
			s_maxdownload = s_maxupload * 3 ;
	} else if ( s_maxupload < 10  ) {
		if ( ( s_maxupload * 4 < s_maxdownload ) || ( s_maxdownload == 0 ) )
			s_maxdownload = s_maxupload * 4;
	}
}


void CPreferences::SetStandartValues()
{
	CreateUserHash();
	Save();
}

bool CPreferences::Save()
{
	wxString fullpath(theApp.ConfigDir + wxT("preferences.dat"));

	bool error = false;

	CFile preffile;
	
	if ( !wxFileExists( fullpath ) )
		preffile.Create( fullpath );
	
	if ( preffile.Open(fullpath, CFile::read_write) ) {
		Preferences_Ext_Struct prefsExt;
		memset( &prefsExt, 0, sizeof(Preferences_Ext_Struct) );
		
		prefsExt.version = PREFFILE_VERSION;
		md4cpy( prefsExt.userhash, s_userhash.GetHash() );
		
		// NOTE: This Write is dangerous.  It doesn't do The Right Thing
		// with respect to endianness for the fields of prefsExt.  At this
		// time, it doesn't matter because no fields except the version and
		// userhash are used and they aren't sensitive to endianness.  If
		// other fields are used in the future, this will have to be revisited.
		off_t written = preffile.Write( &prefsExt, sizeof(Preferences_Ext_Struct) );

		error = written != sizeof(Preferences_Ext_Struct);
		
		preffile.Close();
	} else {
		error = true;
	}

	SavePreferences();

	wxString shareddir(theApp.ConfigDir + wxT("shareddir.dat"));

	wxRemoveFile(shareddir);

	wxTextFile sdirfile(shareddir);

	if(sdirfile.Create()) {
		for(unsigned int ii = 0; ii < shareddir_list.GetCount(); ++ii) {
			sdirfile.AddLine(shareddir_list[ii]);
		}
		sdirfile.Write(),
		sdirfile.Close();
	} else {
		error = true;
	}

	return error;
}


void CPreferences::CreateUserHash()
{
	for (int i = 0;i != 8; i++) {
		uint16	random = rand();
		memcpy(&s_userhash[i*2],&random,2);
	}
	// mark as emule client. that will be need in later version
	s_userhash[5] = 14;
	s_userhash[14] = 111;
}


CPreferences::~CPreferences()
{
	while ( !m_CatList.empty() ) {
		delete m_CatList.front();
		m_CatList.erase( m_CatList.begin() );
	}

	m_CatList.clear();
}


int32 CPreferences::GetRecommendedMaxConnections()
{
	int iRealMax = ::GetMaxConnections();
	if(iRealMax == -1 || iRealMax > 520) {
		return 500;
	}
	if(iRealMax < 20) {
		return iRealMax;
	}
	if(iRealMax <= 256) {
		return iRealMax - 10;
	}
	return iRealMax - 20;
}


void CPreferences::SavePreferences()
{
	wxConfigBase* cfg = wxConfigBase::Get();

	cfg->Write( wxT("/eMule/AppVersion"), wxT(PACKAGE_STRING) );

	// Save the options
	SaveAllItems( cfg );

	// Ensure that the changes are saved to disk.
	cfg->Flush();
}


void CPreferences::SaveCats()
{
	if ( GetCatCount() ) {
		wxConfigBase* cfg = wxConfigBase::Get();

		// Save the main cat.
		cfg->Write( wxT("/eMule/AllcatType"), (int)s_allcatType);
		
		// The first category is the default one and should not be counted

		cfg->Write( wxT("/General/Count"), (long)(m_CatList.size() - 1) );

		for ( size_t i = 1; i < m_CatList.size(); i++ ) {
			cfg->SetPath( wxString::Format(wxT("/Cat#%i"), i) );

			cfg->Write( wxT("Title"),	m_CatList[i]->title );
			cfg->Write( wxT("Incoming"),	m_CatList[i]->incomingpath );
			cfg->Write( wxT("Comment"),	m_CatList[i]->comment );
			cfg->Write( wxT("Color"),	wxString::Format(wxT("%u"), m_CatList[i]->color) );
			cfg->Write( wxT("Priority"),	m_CatList[i]->prio );
		}
		
		cfg->Flush();
	}
}


void CPreferences::LoadPreferences()
{
	LoadCats();
}


void CPreferences::LoadCats() {
	// default cat ... Meow! =(^.^)=
	Category_Struct* newcat = new Category_Struct;
	newcat->prio = 0;
	newcat->color = 0;

	AddCat( newcat );

	wxConfigBase* cfg = wxConfigBase::Get();

	long max = cfg->Read( wxT("/General/Count"), 0l );

	for ( int i = 1; i <= max ; i++ ) {
		cfg->SetPath( wxString::Format(wxT("/Cat#%i"), i) );

		Category_Struct* newcat = new Category_Struct;

		newcat->title = cfg->Read( wxT("Title"), wxEmptyString );
		newcat->incomingpath = cfg->Read( wxT("Incoming"), wxEmptyString );

		// Some sainity checking
		if ( newcat->title.IsEmpty() || newcat->incomingpath.IsEmpty() ) {
			printf("Invalid category found, skipping\n");
			
			delete newcat;
			continue;
		}

		newcat->incomingpath = MakeFoldername(newcat->incomingpath);
		newcat->comment = cfg->Read( wxT("Comment"), wxEmptyString );

		newcat->prio = cfg->Read( wxT("Priority"), 0l );

		wxString color = cfg->Read( wxT("Color"), wxT("0") );
		newcat->color = StrToULong(color);

		AddCat(newcat);
		
		if (!wxFileName::DirExists(newcat->incomingpath)) {
			wxFileName::Mkdir( newcat->incomingpath, GetDirPermissions() );
		}
	}
}


uint16 CPreferences::GetDefaultMaxConperFive()
{
	return MAXCONPER5SEC;
}


uint32 CPreferences::AddCat(Category_Struct* cat)
{
	m_CatList.push_back( cat );
	
	return m_CatList.size() - 1;
}


void CPreferences::RemoveCat(size_t index)
{
	if ( index < m_CatList.size() ) {
		CatList::iterator it = m_CatList.begin() + index;
	
		delete *it;
		
		m_CatList.erase( it );
	}
}


uint32 CPreferences::GetCatCount()
{
	return m_CatList.size();
}


Category_Struct* CPreferences::GetCategory(size_t index)
{
	wxASSERT( index < m_CatList.size() );

	return m_CatList[index];
}


const wxString&	CPreferences::GetCatPath(uint8 index)
{
	wxASSERT( index < m_CatList.size() );
	
	return m_CatList[index]->incomingpath;
}


uint32 CPreferences::CPreferences::GetCatColor(size_t index)
{
	wxASSERT( index < m_CatList.size() );

	return m_CatList[index]->color;
}


// Jacobo221 - Several issues on the browsers:
// netscape is named Netscape on some systems
// MozillaFirebird is named mozilla-firebird and also firebird on some systems
// Niether Galeon tabs nor epiphany tabs have been tested
// Konqueror alternatives is (Open on current window, fails if no konq already open):
//	dcop `dcop konqueror-* | head -n1` konqueror-mainwindow#1 openURL '%s'
// IMPORTANT: if you add cases to the switches and change the number which
//			  corresponds to s_CustomBrowser, you must change the default
//			  set for the Mac in BuildItemList, above.
wxString CPreferences::GetBrowser()
{
	wxString cmd;
	if( s_BrowserTab )
                switch ( s_Browser ) {
                        case 0: cmd = wxT("kfmclient exec '%s'"); break;
                        case 1: cmd = wxT("sh -c \"if ! mozilla -remote 'openURL(%s, new-tab)'; then mozilla '%s'; fi\""); break;
                        case 2: cmd = wxT("sh -c \"if ! firefox -remote 'openURL(%s, new-tab)'; then firefox '%s'; fi\""); break;
                        case 3: cmd = wxT("sh -c \"if ! MozillaFirebird -remote 'openURL(%s, new-tab)'; then MozillaFirebird '%s'; fi\""); break;
                        case 4: cmd = wxT("opera --newpage '%s'"); break;
                        case 5: cmd = wxT("sh -c \"if ! netscape -remote 'openURLs(%s,new-tab)'; then netscape '%s'; fi\""); break;
                        case 6: cmd = wxT("galeon -n '%s'"); break;
                        case 7: cmd = wxT("epiphany -n '%s'"); break;
                        case 8: cmd = s_CustomBrowser; break;
                        default:
                                AddLogLineM( true, wxT("Unable to determine selected browser!") );
                }
        else
		switch ( s_Browser ) {
			case 0: cmd = wxT("konqueror '%s'"); break;
			case 1: cmd = wxT("sh -c 'mozilla %s'"); break;
			case 2: cmd = wxT("firefox '%s'"); break;
			case 3:	cmd = wxT("MozillaFirebird '%s'"); break;
			case 4:	cmd = wxT("opera '%s'"); break;
			case 5: cmd = wxT("netscape '%s'"); break;
			case 6: cmd = wxT("galeon '%s'"); break;
			case 7: cmd = wxT("epiphany '%s'"); break;
			case 8: cmd = s_CustomBrowser; break;
			default:
				AddLogLineM( true, wxT("Unable to determine selected browser!") );
		}
	
	return cmd;
}


#include "ClientList.h"
void CPreferences::SetIPFilterLevel(uint8 level)
{
	if (level != s_filterlevel) {
		// We only need to recheck if the new level is more restrictive
		bool filter = level > s_filterlevel;
		
		// Set the new access-level
		s_filterlevel = level;
		
		if ( filter ) {
			theApp.clientlist->FilterQueues();
		}
	}
}

void CPreferences::SetPort(uint16 val) { 
	// Warning: Check for +3, because server UDP is TCP+3
	
	if (val +3 > 65535) {
		AddLogLineM(true, _("TCP port can't be higher than 65532 due to server UDP socket being TCP+3"));
		AddLogLineM(false, wxString::Format(_("Default port will be used (%d)"),DEFAULT_TCP_PORT));
		s_port = DEFAULT_TCP_PORT;
	} else {
		s_port = val;
	}
}

void CPreferences::ReloadSharedFolders() {

	wxTextFile sdirfile(theApp.ConfigDir + wxT("shareddir.dat"));
	
	shareddir_list.Clear();
	
	if( sdirfile.Exists() && sdirfile.Open() ) {
		if (sdirfile.GetLineCount()) {
			shareddir_list.Add(sdirfile.GetFirstLine());				
			while (!sdirfile.Eof()) {
				shareddir_list.Add(sdirfile.GetNextLine());
			}
		}
		sdirfile.Close();
	}
}
