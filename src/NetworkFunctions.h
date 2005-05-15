//
// This file is part of the aMule Project.
//
// Copyright (c) 2004-2005 Angel Vidal Veiga - Kry (kry@amule.org)
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

#ifndef NETWORK_FUNCTIONS_H
#define NETWORK_FUNCTIONS_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "NetworkFunctions.h"
#endif

#include "Types.h"		// Needed for uint16 and uint32
#include <wx/defs.h>
#include <wx/string.h>
#include <wx/thread.h>
#include <wx/socket.h>

// Network ip/host handling functions
// These functions takes IPs in anti-host order and IPs in anti-host order

inline wxString Uint32toStringIP(uint32 ip)
{
	return wxString::Format(wxT("%u.%u.%u.%u"),(uint8)ip,(uint8)(ip>>8),(uint8)(ip>>16),(uint8)(ip>>24));	
}

inline wxString Uint32_16toStringIP_Port(uint32 ip, uint16 port)
{
	return wxString::Format(wxT("%u.%u.%u.%u:%u"),(uint8)ip,(uint8)(ip>>8),(uint8)(ip>>16),(uint8)(ip>>24),port);	
}


/**
 * Parses a String-IP and saves the IP in the referenced variable.
 *
 * @param strIP A string-ip in the format "a.b.c.d".
 * @param Ip The value to save the result in.
 * @return True if the string was parsed, false otherwise.
 *
 * The IP will be saved in anti-host order.
 * 
 * Note: The reference value will not be changed unless the string
 *       contains a valid IP adress.
 */
bool	StringIPtoUint32(const wxString &strIP, uint32& Ip);


/**
 * Parses a String-IP and returns the IP or 0 if it was invalid.
 * 
 * @param strIP A string-ip in the format "a.b.c.d".
 * @return The resulting IP-address or zero if invalid (or 0.0.0.0).
 * 
 * The IP will be saved in anti-host order.
 */
inline uint32 StringIPtoUint32(const wxString &strIP)
{
	uint32 ip = 0;
	StringIPtoUint32( strIP, ip );
	
	return ip;
}
/**
 * Parses a String-IHost and returns the IP or 0 if it was invalid.
 * 
 * @param Host A string with the Host to convert.
 * @return The resulting IP-address or zero if invalid (or 0.0.0.0).
 * 
 * The IP will be saved in anti-host order.
 */
uint32 StringHosttoUint32(const wxString &Host);


/**
 * Returns our Local Host.
 */
wxString GetLocalHost();

/**
 * Checks for invalid IP-values.
 *
 * @param IP the IP-address to check.
 * @param filterLAN Specifies if LAN IP-ranges should be filtered.
 * @return True if it was valid, false otherwise.
 * 
 * Note: IP must be in anti-host order (BE on LE platform, LE on BE platform).
 */
bool IsGoodIP( uint32 IP, bool filterLAN );

inline bool IsGoodIPPort(uint32 nIP, uint16 nPort)
{
	return IsGoodIP(nIP, true) && nPort!=0;
}

#ifndef EC_REMOTE
// Not needed for remote apps.

// Implementation of Asynchronous dns resolving using wxThread 
//	 and internal wxIPV4address handling of dns

enum DnsSolveType {
	DNS_UDP,
	DNS_SOURCE,
	DNS_SERVER_CONNECT
};

class CAsyncDNS : public wxThread
{
public:
	CAsyncDNS(const wxString& ipName, DnsSolveType type, void* socket = NULL);
	virtual ExitCode Entry();

private:
	DnsSolveType m_type;
	wxString m_ipName;
	void* m_socket;
};

#endif /* ! EC_REMOTE */

#endif // NETWORK_FUNCTIONS_H
