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

#include "IOException.h"
#include <wx/string.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


////////////////////////////////////////
using namespace Kademlia;
////////////////////////////////////////

bool CIOException::GetErrorMessage(wxString* lpszError, uint32 nMaxError, uint32* pnHelpContext)
{
	wxString msg = wxT("ONLY AVAILABLE WITH EMULE INTEGRATION");

	(*lpszError) = msg.Left(nMaxError);

	if (pnHelpContext != NULL) {
		*pnHelpContext = 0;
	}

	return true;
}