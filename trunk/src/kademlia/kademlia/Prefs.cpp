//
// This file is part of the aMule Project.
//
// Copyright (c) 2004-2005 Angel Vidal (Kry) ( kry@amule.org )
// Copyright (c) 2004-2005 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2003 Barry Dunne (http://www.emule-project.net)
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

// Note To Mods //
/*
Please do not change anything here and release it..
There is going to be a new forum created just for the Kademlia side of the client..
If you feel there is an error or a way to improve something, please
post it in the forum first and let us look at it.. If it is a real improvement,
it will be added to the offical client.. Changing something without knowing
what all it does can cause great harm to the network if released in mass form..
Any mod that changes anything within the Kademlia side will not be allowed to advertise
there client on the eMule forum..
*/

//#include "stdafx.h"
//#include "../utils/MiscUtils.h"
#include "Prefs.h"
#include "../kademlia/SearchManager.h"
#include "../../OPCodes.h"
#include "../routing/RoutingZone.h"
#include "../kademlia/Kademlia.h"
#include "../kademlia/Indexed.h"
#include "Preferences.h"
#include "amule.h"
#include "amuleDlg.h"
#include "SafeFile.h"
#include "ServerList.h"
#include "Logger.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


////////////////////////////////////////
using namespace Kademlia;
////////////////////////////////////////

CPrefs::CPrefs()
{
	init(theApp.ConfigDir + wxT("preferencesKad.dat"));
}

CPrefs::~CPrefs()
{
	if (!m_filename.IsEmpty()) {
		writeFile();
	}
}

void CPrefs::init(const wxString& filename)
{
	m_clientID.setValueRandom();
	m_lastContact = 0;
	m_recheckip = 0;
	m_firewalled = 0;
	m_totalFile = 0;
	m_totalStoreSrc = 0;
	m_totalStoreKey = 0;
	m_totalSource = 0;
	m_totalNotes = 0;
	m_totalStoreNotes = 0;
	m_Publish = false;
	m_clientHash.setValue((const unsigned char*)thePrefs::GetUserHash());
	m_ip			= 0;
	m_ipLast		= 0;
	m_recheckip		= 0;
	m_firewalled	= 0;
	m_findBuddy		= false;
	m_kademliaUsers	= 0;
	m_kademliaFiles	= 0;
	m_filename = filename;
	m_lastFirewallState = true;
	readFile();
}

void CPrefs::readFile()
{
	try {
		CSafeBufferedFile file;
		if (file.Open(m_filename,CFile::read)) {
			m_ip = file.ReadUInt32();
			file.ReadUInt16();
			file.ReadUInt128(&m_clientID);
			file.Close();
		}
	} catch (...) {
		AddDebugLogLineM(false, logKadPrefs, wxT("Exception in CPrefs::readFile"));
	}
}

void CPrefs::writeFile()
{
	try {
		CSafeBufferedFile file;
		if (file.Open(m_filename, CFile::write)) {
			file.WriteUInt32(m_ip);
			file.WriteUInt16(0); //This is no longer used.
			file.WriteUInt128(&m_clientID);
			file.WriteUInt8(0); //This is to tell older clients there are no tags..
			file.Close();
		}
	} catch (...) {
		AddDebugLogLineM(false, logKadPrefs, wxT("Exception in CPrefs::writeFile"));
	}
}

void CPrefs::setIPAddress(uint32 val)
{
	//This is our first check on connect, init our IP..
	if( !val || !m_ipLast ) {
		m_ip = val;
		m_ipLast = val;
	}
	//If the last check matches this one, reset our current IP.
	//If the last check does not match, wait for our next incoming IP.
	//This happens for two reasons.. We just changed our IP, or a client responsed with a bad IP.
	if( val == m_ipLast ) {
		m_ip = val;
	} else {
		m_ipLast = val;
	}
}


bool CPrefs::hasLostConnection() const
{
	if( m_lastContact ) {
		return !((time(NULL) - m_lastContact) < KADEMLIADISCONNECTDELAY);
	}
	return false;
}

bool CPrefs::hasHadContact() const
{
	if( m_lastContact ) {
		return ((time(NULL) - m_lastContact) < KADEMLIADISCONNECTDELAY);
	}
	return false;
}

bool CPrefs::getFirewalled() const
{
	if( m_firewalled<2 ) {
		//Not enough people have told us we are open but we may be doing a recheck
		//at the moment which will give a false lowID.. Therefore we check to see
		//if we are still rechecking and will report our last known state..
		if(getRecheckIP()) {
			return m_lastFirewallState;
		}
		return true;
	}
	//We had enough tell us we are not firewalled..
	return false;
}

void CPrefs::setFirewalled()
{
	//Are are checking our firewall state.. Let keep a snapshot of our
	//current state to prevent false reports during the recheck..
	m_lastFirewallState = (m_firewalled<2);
	m_firewalled = 0;
	theApp.amuledlg->ShowConnectionState();
}

void CPrefs::incFirewalled()
{
	m_firewalled++;
	theApp.amuledlg->ShowConnectionState();
}

bool CPrefs::getFindBuddy() /*const*/
{
	if( m_findBuddy ) {
		m_findBuddy = false;
		return true;
	}
	return false;
}

void CPrefs::setKademliaFiles()
{
	//There is no real way to know how many files are in the Kad network..
	//So we first try to see how many files per user are in the ED2K network..
	//If that fails, we use a set value based on previous tests..
	uint32 nServerAverage = theApp.serverlist->GetAvgFile();
	uint32 nKadAverage = Kademlia::CKademlia::getIndexed()->GetFileKeyCount();

#ifdef __DEBUG__
	wxString method;
#endif
	if( nServerAverage > nKadAverage ) {
#ifdef __DEBUG__
		method = wxString::Format(wxT("Kad file estimate used Server avg(%u)"), nServerAverage);
#endif
		nKadAverage = nServerAverage;
	}
#ifdef __DEBUG__
	   else {
		method = wxString::Format(wxT("Kad file estimate used Kad avg(%u)"), nKadAverage);
	}
#endif
	if( nKadAverage < 108 ) {
#ifdef __DEBUG__
		method = wxString::Format(wxT("Kad file estimate used default avg(108, min value)"));
#endif
		nKadAverage = 108;
	}
#ifdef ___DEBUG__
	AddDebugLogLineM(false, logKadPrefs, method);
#endif
	m_kademliaFiles = nKadAverage*m_kademliaUsers;
}
