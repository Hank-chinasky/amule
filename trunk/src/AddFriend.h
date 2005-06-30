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

#ifndef ADDFRIEND_H
#define ADDFRIEND_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "AddFriend.h"
#endif

#include <wx/dialog.h>		// Needed for wxDialog

class CAddFriend : public wxDialog
{
public:
	CAddFriend(wxWindow* parent);
	~CAddFriend() {};

protected:
	DECLARE_EVENT_TABLE()

private:
	void OnAddBtn(wxCommandEvent& evt);
	void OnCloseBtn(wxCommandEvent& evt);
};

#endif
