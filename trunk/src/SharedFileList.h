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

#ifndef SHAREDFILELIST_H
#define SHAREDFILELIST_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "SharedFileList.h"
#endif

#include <map>
#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/thread.h>		// Needed for wxMutex

#include "Types.h"		// Needed for uint16 and uint64
#include "CTypedPtrList.h"	// Needed for CTypedPtrList

struct UnknownFile_Struct;

class CKnownFileList;
class CKnownFile;
class CSafeMemFile;
class CMD4Hash;
class CServer;
class CUpDownClient;
class CPublishKeywordList;

typedef std::map<CMD4Hash,CKnownFile*> CKnownFileMap;

class CSharedFileList {
	friend class CSharedFilesCtrl;
	friend class CClientReqSocket;
public:
	CSharedFileList(CKnownFileList* in_filelist);
	~CSharedFileList();
	bool	AddFile(CKnownFile* pFile);
	void 	Reload(bool firstload = false);
	void	SafeAddKFile(CKnownFile* toadd, bool bOnlyAdd = false);
	void	RemoveFile(CKnownFile* toremove);
	wxMutex	list_mut;
	CKnownFile*	GetFileByID(const CMD4Hash& filehash);
	short	GetFilePriorityByID(const CMD4Hash& filehash);
	const CKnownFile* GetFileByIndex(unsigned int index) const;
	CKnownFileList*	filelist;
	void	CreateOfferedFilePacket(CKnownFile* cur_file,CSafeMemFile* files, CServer* pServer, CUpDownClient* pClient);
	uint64	GetDatasize();
	uint32	GetCount()	{return m_Files_map.size(); }
	uint32  GetFileCount()	{return m_Files_map.size(); }
	void	UpdateItem(CKnownFile* toupdate);
	void	AddFilesFromDirectory(wxString directory);
	void    GetSharedFilesByDirectory(const wxString directory,CTypedPtrList<CPtrList, CKnownFile*>& list);
	void	ClearED2KPublishInfo();
	void	RepublishFile(CKnownFile* pFile);
	void	Process();
	void	PublishNextTurn()	{ m_lastPublishED2KFlag = true; }
	
	/* Kad Stuff */
	void	Publish();
	void	AddKeywords(CKnownFile* pFile);
	void	RemoveKeywords(CKnownFile* pFile);	
	// This is actually unused, but keep it here - will be needed later.
	void	ClearKadSourcePublishInfo();
	
private:
	void	FindSharedFiles();
	bool	reloading;
	
	void	SendListToServer();
	uint32 m_lastPublishED2K;
	bool	 m_lastPublishED2KFlag;	

	CKnownFileMap		m_Files_map;

	
	/* Kad Stuff */
	CPublishKeywordList* m_keywords;
	unsigned int m_currFileSrc;
	unsigned int m_currFileNotes;
	unsigned int m_currFileKey;
	uint32 m_lastPublishKadSrc;
	uint32 m_lastPublishKadNotes;
};

#endif // SHAREDFILELIST_H
