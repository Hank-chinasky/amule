//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2005 aMule Team ( admin@amule.org / http://www.amule.org )
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

#include "DataToText.h"

#include "KnownFile.h"		// Needed by PriorityToStr
#include "updownclient.h"	// Needed by DownloadStateToStr and GetSoftName

#include <wx/string.h>
#include <wx/intl.h>


wxString PriorityToStr( int priority, bool isAuto )
{
	if ( isAuto ) {
		switch ( priority ) {
			case PR_LOW:		return _("Auto [Lo]");
			case PR_NORMAL:		return _("Auto [No]");
			case PR_HIGH:		return _("Auto [Hi]");
		}
	} else {
		switch ( priority ) {
			case PR_VERYLOW:	return _("Very low");
			case PR_LOW:		return _("Low");
			case PR_NORMAL:		return _("Normal");
			case PR_HIGH:		return _("High");
			case PR_VERYHIGH:	return _("Very High");
			case PR_POWERSHARE:	return _("Release");
		}
	}

	wxASSERT( false );

	return _("Unknown");
}


wxString DownloadStateToStr( int state, bool queueFull )
{
	switch ( state ) {
		case DS_CONNECTING:		return  _("Connecting");
		case DS_CONNECTED:		return _("Asking");
		case DS_WAITCALLBACK:		return _("Connecting via server");
		case DS_ONQUEUE:		return ( queueFull ? _("Queue Full") : _("On Queue") );
		case DS_DOWNLOADING:		return _("Transferring");
		case DS_REQHASHSET:		return _("Receiving hashset");
		case DS_NONEEDEDPARTS:		return _("No needed parts");
		case DS_LOWTOLOWIP:		return _("Cannot connect LowID to LowID");
		case DS_TOOMANYCONNS:		return _("Too many connections");
		case DS_NONE:			return _("Unknown");
		case DS_WAITCALLBACKKAD: 	return _("Connecting via Kad");
		case DS_TOOMANYCONNSKAD:	return _("Too many Kad connections");
		case DS_BANNED:			return _("Banned");
		case DS_ERROR:			return _("Connection Error");
		case DS_REMOTEQUEUEFULL:	return _("Remote Queue Full");
	}
	
	wxASSERT( false );

	return _("Unknown");
}


const wxString GetSoftName(unsigned int software_ident)
{
	switch (software_ident) {
		case SO_OLDEMULE:
		case SO_EMULE:
			return wxT("eMule");
		case SO_CDONKEY:
			return wxT("cDonkey");
		case SO_LXMULE:
			return wxT("(l/x)Mule");
		case SO_AMULE:
			return wxT("aMule");
		case SO_SHAREAZA:
		case SO_NEW_SHAREAZA:
			return wxT("Shareaza");
		case SO_EMULEPLUS:
			return wxT("eMule+");
		case SO_HYDRANODE:
			return wxT("HydraNode");
		case SO_MLDONKEY:
			return _("Old MLDonkey");
		case SO_NEW_MLDONKEY:
		case SO_NEW2_MLDONKEY:
			return _("New MlDonkey");
		case SO_LPHANT:
			return wxT("lphant");
		case SO_EDONKEYHYBRID:
			return wxT("eDonkeyHybrid");
		case SO_EDONKEY:
			return wxT("eDonkey");
		case SO_UNKNOWN:
			return _("Unknown");
		case SO_COMPAT_UNK:
			return _("eMule Compat");
		default:
			return wxEmptyString;
	}
}
