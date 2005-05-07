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

#ifndef SERVERLIST_H
#define SERVERLIST_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "ServerList.h"
#endif

#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/timer.h>		// Needed for wxTimer

#include "Types.h"		// Needed for int8, uint16 and uint32
#include "CTypedPtrList.h"	// Needed for CTypedPtrList

#include "ObservableQueue.h"


class CServer;
class CPacket;

class CServerList : public CObservableQueue<CServer*>
{
	friend class CServerListCtrl;
public:
	CServerList();
	~CServerList();
	bool		Init();
	bool		AddServer(CServer* in_server, bool fromUser = false);
	void		RemoveServer(CServer* out_server);
	void		RemoveAllServers();
	void		RemoveDeadServers();	
	bool		LoadServerMet(const wxString& strFile);
	bool		SaveServerMet();
	void		ServerStats();
	void		ResetServerPos()	{serverpos = 0;}
	CServer*	GetNextServer();
	CServer*	GetNextStatServer();
	CServer*	GetServerByIndex(uint32 pos)	{return list.GetAt(list.FindIndex(pos));}
	uint32		GetServerCount()	{return list.GetCount();}
	CServer*	GetServerByAddress(const wxString& address, uint16 port);
	CServer*	GetServerByIP(uint32 nIP);
	CServer*	GetServerByIP(uint32 nIP, uint16 nPort);	
	void		GetStatus( uint32 &total, uint32 &failed, uint32 &user, uint32 &file, uint32 &tuser, uint32 &tfile, float &occ);
	void		GetUserFileStatus( uint32 &user, uint32 &file);
	void		Sort();
	uint32		GetServerPostion()	{return serverpos;}
	void		SetServerPosition(uint32 newPosition) { if (newPosition<(uint32)list.GetCount() ) serverpos=newPosition; else serverpos=0;}
	uint32		GetDeletedServerCount()		{return delservercount;}
	void 		UpdateServerMetFromURL(const wxString& strURL);	
	void		DownloadFinished(uint32 result);	
	void		AutoDownloadFinished(uint32 result);	
	uint32	CServerList::GetAvgFile() const;

private:
	virtual void 	ObserverAdded( ObserverType* );
	void			AutoUpdate();
	
	void			LoadStaticServers( const wxString& filename );
	uint8			current_url_index;
	uint32		serverpos;
	uint32		statserverpos;
	CTypedPtrList<CPtrList, CServer*>	list;
	uint32		delservercount;
	uint32		m_nLastED2KServerLinkCheck;// emanuelw(20030924) added
	wxString		URLUpdate;
	wxString		URLAutoUpdate;
};

#endif // SERVERLIST_H
