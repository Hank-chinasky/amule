//
// This file is part of the aMule Project.
//
// Copyright (c) 2005 aMule Team ( admin@amule.org / http://www.amule.org )
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

#include "Logger.h"
#include "amule.h"
#include "Preferences.h"
#include "StringFunctions.h"

#include <wx/thread.h>


CDebugCategory::CDebugCategory( DebugType type, const wxString& name )
	: m_name( name ),
	  m_type( type )
{
	m_enabled = false;
}


bool CDebugCategory::IsEnabled() const
{
	return m_enabled;
}


void CDebugCategory::SetEnabled( bool enabled )
{
	m_enabled = enabled;
}


const wxString& CDebugCategory::GetName() const
{
	return m_name;
}


DebugType CDebugCategory::GetType() const
{
	return m_type;
}








CDebugCategory g_debugcats[] = {
	CDebugCategory( logGeneral,		wxT("General") ),
	CDebugCategory( logHasher,		wxT("Hasher") ),
	CDebugCategory( logClient,		wxT("ED2k Client") ),
	CDebugCategory( logLocalClient,		wxT("Local Client Protocol") ),
	CDebugCategory( logRemoteClient,	wxT("Remote Client Protocol") ),
	CDebugCategory( logPacketErrors,	wxT("Packet Parsing Errors") ),
	CDebugCategory( logCFile,		wxT("CFile") ),
	CDebugCategory( logFileIO,		wxT("FileIO") ),
	CDebugCategory( logZLib,		wxT("ZLib") ),
	CDebugCategory( logAICHThread,		wxT("AICH-Hasher") ),
	CDebugCategory( logAICHTransfer,	wxT("AICH-Transfer") ),
	CDebugCategory( logAICHRecovery,	wxT("AICH-Recovery") ),
	CDebugCategory( logListenSocket,	wxT("ListenSocket") ),
	CDebugCategory( logCredits,		wxT("Credits") ),
	CDebugCategory( logClientUDP,		wxT("ClientUDPSocket") ),
	CDebugCategory( logDownloadQueue,	wxT("DownloadQueue") ),
	CDebugCategory( logIPFilter,		wxT("IPFilter") ),
	CDebugCategory( logKnownFiles,		wxT("KnownFileList") ),
	CDebugCategory( logPartFile,		wxT("PartFiles") ),
	CDebugCategory( logSHAHashSet,		wxT("SHAHashSet") ),
	CDebugCategory( logServer,		wxT("Servers") ),
	CDebugCategory( logProxy,		wxT("Proxy") ),
	CDebugCategory( logSearch,		wxT("Searching") ),
	CDebugCategory( logServerUDP,		wxT("ServerUDP") ),
	CDebugCategory( logClientKadUDP,		wxT("Client Kademlia UDP") ),
	CDebugCategory( logKadSearch,		wxT("Kademlia Search") ),
	CDebugCategory( logKadRouting,		wxT("Kademlia Routing") ),
	CDebugCategory( logKadIndex,		wxT("Kademlia Indexing") ),
	CDebugCategory( logKadMain,		wxT("Kademlia Main Thread") ),
	CDebugCategory( logKadPrefs,		wxT("Kademlia Preferences") ),
	CDebugCategory( logPfConvert,		wxT("PartFileConvert") ),
	CDebugCategory( logMuleUDP,			wxT("MuleUDPSocket" ) )
};


const int categoryCount = sizeof( g_debugcats ) / sizeof( g_debugcats[0] );



bool CLogger::IsEnabled( DebugType type )
{
#ifdef __DEBUG__
	int index = (int)type;
	
	if ( index >= 0 && index < categoryCount ) {
		const CDebugCategory& cat = g_debugcats[ index ];
		wxASSERT( type == cat.GetType() );

		return ( cat.IsEnabled() && thePrefs::GetVerbose() );
	} 

	wxASSERT( false );
#endif
	return false;
}


void CLogger::SetEnabled( DebugType type, bool enabled ) 
{
	int index = (int)type;
	
	if ( index >= 0 && index < categoryCount ) {
		CDebugCategory& cat = g_debugcats[ index ];
		wxASSERT( type == cat.GetType() );

		cat.SetEnabled( enabled );
	} else {
		wxASSERT( false );
	}
}


void CLogger::AddLogLine( bool critical, const wxString str )
{
	GUIEvent event( ADDLOGLINE, critical, str );
	
	if ( wxThread::IsMain() ) {
		theApp.NotifyEvent( event );
	} else {
		wxPostEvent( &theApp, event );
	}
}


void CLogger::AddDebugLogLine( bool critical, DebugType type, const wxString& str )
{
	int index = (int)type;
	
	if ( index >= 0 && index < categoryCount ) {
		const CDebugCategory& cat = g_debugcats[ index ];
		wxASSERT( type == cat.GetType() );

		wxString line = cat.GetName() + wxT(": ") + str;
		
#ifdef __DEBUG__
		GUIEvent event( ADDDEBUGLOGLINE, critical, line );
		
		if ( wxThread::IsMain() ) {
			theApp.NotifyEvent( event );
		} else {
			wxPostEvent( &theApp, event );
		}
#else
		printf("%s\n", (const char*)unicode2char( line ) );
#endif
	} else {
		wxASSERT( false );
	}
}


const CDebugCategory& CLogger::GetDebugCategory( int index )
{
	wxASSERT( index >= 0 && index < categoryCount );

	return g_debugcats[ index ];
}


unsigned int CLogger::GetDebugCategoryCount()
{
	return categoryCount;
}
