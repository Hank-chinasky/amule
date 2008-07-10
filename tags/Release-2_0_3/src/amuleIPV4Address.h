//
// This file is part of the aMule Project.
//
// Copyright (c) 2004-2005 Carlo Wood ( carlo@alinoe.com )
// Copyright (c) 2004-2005 aMule Team ( admin@amule.org / http://www.amule.org )
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

#ifndef AMULEIPV4ADDRESS_H
#define AMULEIPV4ADDRESS_H

#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/object.h>		// Needed by wx/sckaddr.h
#include <wx/sckaddr.h>		// Needed for wxIPV4address
#include <wx/log.h>		// Needed for wxLogWarning

#include "NetworkFunctions.h"	// Needed for unicode2char
#include "ArchSpecific.h"


// This is fscking hard to maintain. wxWidgets 2.5.2 has changed internal
// ipaddress structs.
// prevent fscking dns queries
class amuleIPV4Address : public wxIPV4address {
public:
	amuleIPV4Address(void) {}
	amuleIPV4Address(const wxIPV4address &a) : wxIPV4address(a) {}

	virtual bool Hostname(const wxString& name) {
		// Some people are sometimes fool.
		if (name.IsEmpty()) {
//			wxLogWarning(wxT("Trying to set a NULL host: giving up") );
			return FALSE;
		}
		return Hostname(StringIPtoUint32(name));
	}

	virtual bool Hostname(uint32 ip) {
		// Some people are sometimes fool.
		if (!ip) {
//			wxLogWarning(wxT("Trying to set a wrong ip: giving up") );
//			wxASSERT(0);
			return FALSE;
		}
		// We have to take care that wxIPV4address's internals changed on 2.5.2
		#if wxCHECK_VERSION(2,5,2)
			return GAddress_INET_SetHostAddress(m_address,wxUINT32_SWAP_ALWAYS(ip))==GSOCK_NOERROR;
		#else
			return GAddress_INET_SetHostAddress(m_address,ENDIAN_SWAP_32(ip))==GSOCK_NOERROR;
		#endif
	}
	
};

#endif // AMULEIPV4ADDRESS_H