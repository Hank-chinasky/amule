// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
// Copyright (C) 2002 Merkur ( merkur-@users.sourceforge.net / http://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef FRIEND_H
#define FRIEND_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "Friend.h"
#endif

#include <wx/string.h>		// Needed for wxString

#include "Types.h"		// Needed for uint32
#include "CMD4Hash.h"

class CUpDownClient;
class CFile;
class CFileDataIO;

#define	FF_NAME		0x01

class CFriend {
public:
	CFriend();
	~CFriend() {};
	
	CFriend(CUpDownClient* client);
	CFriend( const CMD4Hash& userhash, uint32 tm_dwLastSeen, uint32 tm_dwLastUsedIP, uint32 tm_nLastUsedPort, uint32 tm_dwLastChatted, const wxString& tm_strName);
	
	void	SetUserHash(const CMD4Hash& userhash) { m_UserHash = userhash;}
	bool	HasHash() { return !m_UserHash.IsEmpty(); }
	const	CMD4Hash& GetUserHash() const { return m_UserHash; }
	
	void SetName(const wxString& name) { m_strName = name; }
	
	void	LinkClient(CUpDownClient* client, bool unlink = true);
	CUpDownClient* GetLinkedClient() const { return m_LinkedClient; }
	void	UnLinkClient() {  m_LinkedClient = NULL; }
	
	bool	HasFriendSlot();

	const wxString& GetName() { return m_strName; }
	uint16 GetPort() const { return m_nLastUsedPort; }
	uint32 GetIP() const { return m_dwLastUsedIP; }
	
	void	LoadFromFile(CFileDataIO* file);
	void	WriteToFile(CFileDataIO* file);

private:
	
	CUpDownClient* m_LinkedClient;

	CMD4Hash	m_UserHash;
	uint32	m_dwLastSeen;
	uint32	m_dwLastUsedIP;
	uint16	m_nLastUsedPort;
	uint32	m_dwLastChatted;
	wxString	m_strName;
		
};

#endif
