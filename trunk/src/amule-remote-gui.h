//
// This file is part of the aMule Project.
//
// Copyright (c) 2005 aMule Team ( admin@amule.org / http://www.amule.org )
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

#ifndef AMULE_REMOTE_GUI_H
#define AMULE_REMOTE_GUI_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "amule-remote-gui.h"
#endif

#include <wx/string.h>

#include "CTypedPtrList.h"
#include "ECSpecialTags.h"
#include "Statistics.h"
#include "Preferences.h"
#include "Statistics.h"
#include "OtherFunctions.h"

#include <map>
#include <list>
#include <vector>

class CED2KFileLink;
class CServer;
class CKnownFile;
class CSearchFile;
class CPartFile;
class CUpDownClient;
class CStatistics;

#include <wx/dialog.h>

class CEConnectDlg : public wxDialog {
		wxString host;
		int port;
		
		wxString pwd_hash;
		wxString login, passwd;
		bool m_save_user_pass;
		
		void OnOK(wxCommandEvent& event);
		
		DECLARE_EVENT_TABLE()
	public:
		CEConnectDlg();
		
		wxString Host() { return host; }
		int Port() { return port; }

		wxString Login() { return login; }
		wxString PassHash();
		bool SaveUserPass() { return m_save_user_pass; }
};

class CRemoteConnect {
		ECSocket *m_ECSocket;
		bool m_isConnected;
		bool m_busy;
	public:
		CRemoteConnect();
		~CRemoteConnect();
		
		bool Connect(const wxString &host, int port, const wxString& login, const wxString &pass);

		CECPacket *SendRecv(CECPacket *);
		void Send(CECPacket *);
		
		bool Busy() { return m_busy; }
};

class CPreferencesRem : public CPreferences {
		CRemoteConnect *m_conn;
	public:
		CPreferencesRem(CRemoteConnect *);
		
		bool LoadRemote();
		void SendToRemote();
};

//
// T - type if item in container
// I - type of id of item
// G - type of tag used to create/update items
//
template <class T, class I, class G = CECTag>
class CRemoteContainer {
	protected:
		CRemoteConnect *m_conn;

		std::list<T *> m_items;
		std::map<I, T *> m_items_hash;
		
		// for GetByIndex
		std::vector<T *> m_idx_items;
		
		// .size() is O(N) operation in stl
		int m_item_count;
		
		// use incremental tags algorithm
		bool m_inc_tags;
	public:
		CRemoteContainer(CRemoteConnect *conn, bool inc_tags = false)
		{
			m_conn = conn;
			m_item_count = 0;
			
			m_inc_tags = inc_tags;
		}
		
		virtual ~CRemoteContainer()
		{
		}
		
		uint32 GetCount()
		{
			return m_item_count;
		}
		
		void AddItem(T *item)
		{
			m_items.push_back(item);
			m_items_hash[GetItemID(item)] = item;
			m_idx_items.resize(m_item_count+1);
			m_idx_items[m_item_count] = item;
			m_item_count++;
		}
	
		T *GetByID(I id)
		{
			// avoid creating nodes
			return m_items_hash.count(id) ? m_items_hash[id] : NULL;
		}
		
		T *GetByIndex(int index)
		{
			return ( (index >= 0) && (index < m_item_count) ) ? m_idx_items[index] : NULL;
		}
		
		void Flush()
		{
			m_items.erase(this->m_items.begin(), this->m_items.end());
			m_items_hash.erase(this->m_items_hash.begin(), this->m_items_hash.end());
			m_item_count = 0;
		}
				
		//
		// Flush & reload
		//
		bool FullReload(int cmd)
		{
			CECPacket req(cmd);
			CECPacket *reply = this->m_conn->SendRecv(&req);
			if ( !reply ) {
				return false;
			}
			for(typename std::list<T *>::iterator j = this->m_items.begin(); j != this->m_items.end(); j++) {
				this->DeleteItem(*j);
			}
			
			Flush();
			
			ProcessFull(reply);
			
			delete reply;
			return true;
		}
		
		//
		// Following are like basicly same code as in webserver. Eventually it must
		// be same class
		//
		bool DoRequery(int cmd, int tag)
		{
			CECPacket req_sts(cmd, m_inc_tags ? EC_DETAIL_INC_UPDATE : EC_DETAIL_UPDATE);
		
			//
			// Phase 1: request status
			CECPacket *reply = this->m_conn->SendRecv(&req_sts);
			if ( !reply ) {
				return false;
			}
			
			if ( !this->Phase1Done(reply) ) {
				// if derived class choose not to proceed, retrun - but with good status
				return true;
			}
			//
			// Phase 2: update status, mark new files for subsequent query
			CECPacket req_full(cmd);
		
			ProcessUpdate(reply, &req_full, tag);
		
			delete reply;
		
			if ( !m_inc_tags ) {
				// Phase 3: request full info about files we don't have yet
				if ( req_full.GetTagCount() ) {
					reply = this->m_conn->SendRecv(&req_full);
					if ( !reply ) {
						return false;
					}
					ProcessFull(reply);
				}
			}
			return true;
		}

		void ProcessFull(CECPacket *reply)
		{
			for (int i = 0;i < reply->GetTagCount();i++) {
				G *tag = (G *)reply->GetTagByIndex(i);
				// initialize item data from EC tag
				T *item = this->CreateItem(tag);
				AddItem(item);
			}
		}

		void ProcessUpdate(CECPacket *reply, CECPacket *full_req, int req_type)
		{
			std::set<I> core_files;
			for (int i = 0;i < reply->GetTagCount();i++) {
				G *tag = (G *)reply->GetTagByIndex(i);
				if ( tag->GetTagName() != req_type ) {
					continue;
				}
		
				core_files.insert(tag->ID());
				if ( m_items_hash.count(tag->ID()) ) {
					T *item = m_items_hash[tag->ID()];
					ProcessItemUpdate(tag, item);
				} else {
					if ( m_inc_tags ) {
						T *item = this->CreateItem(tag);
						AddItem(item);
					} else {
						full_req->AddTag(CECTag(req_type, tag->ID()));
					}
				}
			}
			std::list<I> del_ids;
			for(typename std::list<T *>::iterator j = this->m_items.begin(); j != this->m_items.end(); j++) {
				I item_id = GetItemID(*j);
				if ( core_files.count(item_id) == 0 ) {
					// item may contain data that need to be freed externally, before
					// dtor is called and memory freed
					this->DeleteItem(*j);
					
					del_ids.push_back(item_id);
				}
			}
			for(typename std::list<I>::iterator j = del_ids.begin(); j != del_ids.end(); j++) {
				for(int idx = 0;idx < m_item_count;idx++) {
					if ( this->GetItemID(m_idx_items[idx]) == *j ) {
						m_idx_items[idx] = m_idx_items[m_item_count-1];
						break;
					}
				}
				m_item_count--;
				m_items_hash.erase(*j);
				for(typename std::list<T *>::iterator k = this->m_items.begin(); k != this->m_items.end(); k++) {
					if ( *j == GetItemID(*k) ) {
						this->m_items.erase(k);
						break;
					}
				}
			}
		}

		virtual T *CreateItem(G *)
		{
			return 0;
		}
		virtual void DeleteItem(T *)
		{
		}
		virtual I GetItemID(T *)
		{
			return 0;
		}
		virtual void ProcessItemUpdate(G *, T *)
		{
		}

		virtual bool Phase1Done(CECPacket *)
		{
			return true;
		}
};

class CServerConnectRem {
		CRemoteConnect *m_Conn;
		uint32 m_ID;
		
		CServer *m_CurrServer;
	public:
		CServerConnectRem(CRemoteConnect *);
		bool ReQuery();
		
		bool IsConnected() { return (m_ID != 0) && (m_ID != 0xffffffff); }
		bool IsConnecting() { return m_ID == 0xffffffff; }
		bool IsLowID() { return m_ID < 16777216; }
		uint32 GetClientID() { return m_ID; }
		CServer *GetCurrentServer() { return m_CurrServer; }
		
		//
		// Actions
		//
		void ConnectToServer(CServer* server);
		void ConnectToAnyServer();
		void StopConnectionTry();
		void Disconnect();
};

class CServerListRem : public CRemoteContainer<CServer, uint32, CEC_Server_Tag> {
		uint32 m_TotalUser, m_TotalFile;
	public:
		CServerListRem(CRemoteConnect *);
		void GetUserFileStatus(uint32 &total_user, uint32 &total_file)
		{
			total_user = m_TotalUser;
			total_file = m_TotalFile;
		}
		CServer *GetServerByAddress(const wxString& address, uint16 port);

		void ReloadControl();

		//
		// Actions
		//
		void RemoveServer(CServer* server);
		void UpdateServerMetFromURL(wxString url);
		void SaveServerMet();
		
		//
		// template
		//
		CServer *CreateItem(CEC_Server_Tag *);
		void DeleteItem(CServer *);
		uint32 GetItemID(CServer *);
		void ProcessItemUpdate(CEC_Server_Tag *, CServer *);
};

class CUpDownClientListRem : public CRemoteContainer<CUpDownClient, uint32, CEC_UpDownClient_Tag> {
		std::list<CUpDownClient *>::iterator it;
		int m_viewtype;
	public:
		CUpDownClientListRem(CRemoteConnect *, int viewtype);

		POSITION GetFirstFromList();
		CUpDownClient *GetNextFromList(POSITION &curpos);

		//
		// template
		//
		CUpDownClient *CreateItem(CEC_UpDownClient_Tag *);
		void DeleteItem(CUpDownClient *);
		uint32 GetItemID(CUpDownClient *);
		void ProcessItemUpdate(CEC_UpDownClient_Tag *, CUpDownClient *);
};

class CUpQueueRem {
		uint32 m_waiting_user_count;
		float m_kbps;
		uint32 m_data_overhead;

		CUpDownClientListRem m_up_list, m_wait_list;
	public:
		CUpQueueRem(CRemoteConnect *);
		
		bool ReQueryUp() { return m_up_list.DoRequery(EC_OP_GET_ULOAD_QUEUE, EC_TAG_UPDOWN_CLIENT); }
		bool ReQueryWait() { return m_wait_list.DoRequery(EC_OP_GET_WAIT_QUEUE, EC_TAG_UPDOWN_CLIENT); }
		
		uint32 GetWaitingUserCount() { return m_waiting_user_count; }
		float GetKBps() { return m_kbps; }
		uint32 GetUpDatarateOverhead() { return m_data_overhead; }
		
		POSITION GetFirstFromUploadList() { return m_up_list.GetFirstFromList(); }
		CUpDownClient *GetNextFromUploadList(POSITION &curpos) { return m_up_list.GetNextFromList(curpos); }
		
		POSITION GetFirstFromWaitingList() { return m_wait_list.GetFirstFromList(); }
		CUpDownClient *GetNextFromWaitingList(POSITION &curpos) { return m_wait_list.GetNextFromList(curpos); }
		
		void UpdateStats(CEC_Stats_Tag *);
};

class CDownQueueRem : public CRemoteContainer<CPartFile, CMD4Hash, CEC_PartFile_Tag> {
		float m_kbps;
		uint32 m_data_overhead;
		
		std::list<CUpDownClient *>::iterator it;
		
		std::map<CMD4Hash, otherfunctions::PartFileEncoderData> m_enc_map;
	public:
		CDownQueueRem(CRemoteConnect *);
		
		float GetKBps() { return m_kbps; }
		uint32 GetDownDatarateOverhead() { return m_data_overhead; }

		uint32 GetFileCount() { return GetCount(); }
		CKnownFile *GetFileByID(CMD4Hash id) { return (CKnownFile *)GetByID(id); }
		CPartFile *GetFileByIndex(unsigned int idx) { return GetByIndex(idx); }
		
		bool IsPartFile(const CKnownFile* totest) const;
		
		//
		// User actions
		//
		void Prio(CPartFile *file, uint8 prio);
		void AutoPrio(CPartFile *file, bool flag);
		void Category(CPartFile *file, uint8 cat);
		
		void SendFileCommand(CPartFile *file, ec_tagname_t cmd);
		//
		// Actions
		//
		void StopUDPRequests();
		void AddFileLinkToDownload(CED2KFileLink*, uint8);
		bool AddED2KLink(const wxString &link, int category = 0);
		void UnsetCompletedFilesExist();
		void ResetCatParts(int cat);
		void AddSearchToDownload(CSearchFile* toadd, uint8 category);
		
		void UpdateStats(CEC_Stats_Tag *);
		//
		// template
		//
		CPartFile *CreateItem(CEC_PartFile_Tag *);
		void DeleteItem(CPartFile *);
		CMD4Hash GetItemID(CPartFile *);
		void ProcessItemUpdate(CEC_PartFile_Tag *, CPartFile *);
		bool Phase1Done(CECPacket *);
};

class CSharedFilesRem : public CRemoteContainer<CKnownFile, CMD4Hash, CEC_SharedFile_Tag> {
		std::map<CMD4Hash, otherfunctions::RLE_Data> m_enc_map;
	public:
		CSharedFilesRem(CRemoteConnect *);
		
		CKnownFile *GetFileByID(CMD4Hash id) { return GetByID(id); }

		void SetFilePrio(CKnownFile *file, uint8 prio);

		//
		// Actions
		//
		void AddFilesFromDirectory(wxString );
		void Reload(bool sendtoserver = true, bool firstload = false);

		//
		// template
		//
		CKnownFile *CreateItem(CEC_SharedFile_Tag *);
		void DeleteItem(CKnownFile *);
		CMD4Hash GetItemID(CKnownFile *);
		void ProcessItemUpdate(CEC_SharedFile_Tag *, CKnownFile *);
		bool Phase1Done(CECPacket *);
};

class CKnownFilesRem {
		CSharedFilesRem *m_shared_files;
	public:
		CKnownFilesRem(CSharedFilesRem *shared)
		{
			m_shared_files = shared;
			
			requested = 0;
			transfered = 0;
			accepted = 0;
		}
		
		CKnownFile *FindKnownFileByID(const CMD4Hash& id)
		{
			return m_shared_files->GetByID(id); 
		}

        uint16 requested;
        uint32 transfered;
        uint16 accepted;
};

class CClientCreditsRem {
		bool m_crypt_avail;
	public:
		bool CryptoAvailable() { return m_crypt_avail; }
};

class CClientListRem {
		CRemoteConnect *m_conn;
		uint32 m_banned_count;

		//
		// map of user_ID -> client
		std::multimap<uint32, CUpDownClient*> m_client_list;
	public:
		CClientListRem(CRemoteConnect *);
		
		const std::multimap<uint32, CUpDownClient*>& GetClientList() { return m_client_list; }
		uint32 GetBannedCount() { return m_banned_count; }
		
		//
		// Actions
		//
		void AddClient(CUpDownClient *);
		void FilterQueues();
};

class CIPFilterRem {
		CRemoteConnect *m_conn;
	public:
		//
		// Actions
		//
		void Reload();
		void Update(wxString strURL = wxEmptyString);
};

class CSearchListRem : public CRemoteContainer<CSearchFile, CMD4Hash, CEC_SearchFile_Tag> {
	public:
		CSearchListRem(CRemoteConnect *);
		
		int m_curr_search;
		std::map<long, std::vector<CSearchFile *> > m_Results;

		void RemoveResults(long nSearchID);
		//
		// Actions
		//

		bool StartNewSearch(long nSearchID, bool global_search, wxString &searchString, 
			wxString& typeText, wxString &extension, uint32 min, uint32 max, uint32 availability);
			
		void StopGlobalSearch();
		
		//
		// template
		//
		CSearchFile *CreateItem(CEC_SearchFile_Tag *);
		void DeleteItem(CSearchFile *);
		CMD4Hash GetItemID(CSearchFile *);
		void ProcessItemUpdate(CEC_SearchFile_Tag *, CSearchFile *);
		bool Phase1Done(CECPacket *);
};

class CListenSocketRem {
		uint32 m_peak_connections;
	public:
		uint32 GetPeakConnections() { return m_peak_connections; }
};

class CStatisticsRem {
	CStatistics *m_stat_data;
	public:
		void UpdateStats(CEC_Stats_Tag *);
};

#endif /* AMULE_REMOTE_GUI_H */
