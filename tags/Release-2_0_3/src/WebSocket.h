//
// This file is part of the aMule Project.
//  
// Copyright (c) 2004-2005 shakraw ( shakraw@users.sourceforge.net )
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

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "WebSocket.h"
#endif

#include <wx/thread.h>
#include <wx/dynarray.h>

#include "WebServer.h"

class CWebServer;


class CWebSocket {
	public:
		void OnReceived(char* pData, uint32 dwDataSize);
		void OnRequestReceived(char* pHeader, uint32 dwHeaderLen, char* pData, uint32 dwDataLen);
		void Disconnect();
		void SendContent(const char* szStdResponse, const void* pContent, uint32 dwContentSize);
		void SendData(const void* pData, uint32 dwDataSize);
	
		CWebServer *m_pParent;
		wxSocketBase *m_hSocket;
	
		class CChunk {
			public:
				char* m_pData;
				char* m_pToSend;
				uint32 m_dwSize;
			
				CChunk* m_pNext;
				~CChunk() { if (m_pData) delete[] m_pData; }
		};

		CChunk *m_pHead; // tails of what has to be sent
		CChunk *m_pTail;
		
		char *m_pBuf;
		uint32 m_dwBufSize;
		uint32 m_dwRecv;
		uint32 m_dwHttpHeaderLen;
		uint32 m_dwHttpContentLen;
		bool m_bValid;
		bool m_bCanRecv;
		bool m_bCanSend;
};


class CWCThread : public wxThread { //WC stands for web client socket. not for WC :)
	public:
		CWCThread(CWebServer *ws, wxSocketBase *sock);
		~CWCThread();
	
		//thread execution starts here
		virtual void *Entry();
	
	private:
		CWebSocket stWebSocket;
};


class CWSThread : public wxThread {
	public:
		CWSThread(CWebServer *webserver); //web socket thread ctor

		//thread execution starts here
		virtual void *Entry();
	
	private:
		wxSocketServer *m_WSSocket;
		CWebServer *ws;
		int wsport;
};


#endif //WEBSERVER_H
