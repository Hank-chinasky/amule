//
// This file is part of the aMule Project
//
// Copyright (C) 2003-2004 aMule Team ( http://www.amule-project.net )
// This fle Copyright (C) 2003 Kry ( elkry@users.sourceforge.net   http://www.amule-project.net )
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
// 

#ifndef WEBSERVER_H
#define WEBSERVER_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma interface "WebServer.h"
#endif
//-------------------------------------------------------------------
#ifdef __WXMSW__
	#include <winsock.h>
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif
#include <zlib.h>		// Needed for Bytef etc.

//#define WITH_LIBPNG	1

#ifdef WITH_LIBPNG
	#include <png.h>
#endif

#include <list>
#include <map>
#include <vector>


//-------------------------------------------------------------------
#ifndef WIN32
	#include "config.h"
#endif
//-------------------------------------------------------------------
#define wxUSE_DDE_FOR_IPC  0
//#include <wx/ipc.h>
#include <wx/wx.h>
#include <wx/filename.h>

#ifdef __WXMSW__
	#include <wx/msw/winundef.h>
#endif

#include "types.h"
#include "otherfunctions.h"
#include "WebInterface.h"
#include "KnownFile.h"
#include "ECPacket.h"

class TransferredData;
class CWSThread;
class CWebSocket;
class CMD4Hash;

#define SESSION_TIMEOUT_SECS	300	// 5 minutes session expiration
#define SHORT_FILENAME_LENGTH	40	// Max size of file name.

typedef struct { float download; float upload; long connections; } UpDown;

typedef struct { time_t startTime; long lSession; bool admin;} Session;

typedef enum {
	DOWN_SORT_NAME,
	DOWN_SORT_SIZE,
	DOWN_SORT_COMPLETED,
	DOWN_SORT_TRANSFERRED,
	DOWN_SORT_SPEED,
	DOWN_SORT_PROGRESS
} xDownloadSort;

typedef enum {
	SHARED_SORT_NAME,
	SHARED_SORT_SIZE,
	SHARED_SORT_TRANSFERRED,
	SHARED_SORT_ALL_TIME_TRANSFERRED,
	SHARED_SORT_REQUESTS,
	SHARED_SORT_ALL_TIME_REQUESTS,
	SHARED_SORT_ACCEPTS,
	SHARED_SORT_ALL_TIME_ACCEPTS,
	SHARED_SORT_PRIORITY
} xSharedSort;


typedef enum {
	SERVER_SORT_NAME,
	SERVER_SORT_DESCRIPTION,
	SERVER_SORT_IP,
	SERVER_SORT_USERS,
	SERVER_SORT_FILES
} xServerSort;

typedef enum {
	SEARCH_SORT_NAME,
	SEARCH_SORT_SIZE,
	SEARCH_SORT_SOURCES,
} xSearchSort;

WX_DECLARE_OBJARRAY(UpDown*, ArrayOfUpDown);
WX_DECLARE_OBJARRAY(Session*, ArrayOfSession);
WX_DECLARE_OBJARRAY(TransferredData*, ArrayOfTransferredData);

class CEC_PartFile_Tag;
class CEC_SharedFile_Tag;
class CEC_UpDownClient_Tag;
class CEC_SearchFile_Tag;
class CProgressImage;

class DownloadFiles {
	public:
		wxString	sFileName;
		uint8		nFileStatus;
		wxString	sFileStatus;
		unsigned long	lFileSize;
		unsigned long	lFileCompleted;
		unsigned long	lFileTransferred;
		unsigned long	lFileSpeed;
		long		lSourceCount;
		long		lNotCurrentSourceCount;
		long		lTransferringSourceCount;
		long		lSourceCountA4AF;
		double		fCompleted;
		long		lFilePrio;
		wxString	sFileHash;
		wxString	sED2kLink;
		wxString	sFileInfo;

		CMD4Hash	nHash;
		
		CProgressImage *m_Image;
		otherfunctions::PartFileEncoderData m_Encoder;
		std::vector<Gap_Struct> m_ReqParts;

		// container require this		
		static class DownloadFilesInfo *GetContainerInstance();
		DownloadFiles(CEC_PartFile_Tag *);
		void ProcessUpdate(CEC_PartFile_Tag *);
		CMD4Hash ID() { return nHash; }
};

class SharedFiles {
	public:
		wxString	sFileName;
		long		lFileSize;
		uint32		nFileTransferred;
		uint64		nFileAllTimeTransferred;
		uint16		nFileRequests;
		uint32		nFileAllTimeRequests;
		uint16		nFileAccepts;
		uint32		nFileAllTimeAccepts;
		uint8		nFilePriority;
		wxString	sFilePriority;
		bool		bFileAutoPriority;
		wxString 	sFileHash;
		wxString	sED2kLink;

		CMD4Hash	nHash;

		static class SharedFilesInfo *GetContainerInstance();
		SharedFiles(CEC_SharedFile_Tag *);
		void ProcessUpdate(CEC_SharedFile_Tag *);
		CMD4Hash ID() { return nHash; }
};

class ServerEntry {
	public:
		wxString	sServerName;
		wxString	sServerDescription;
		uint32		nServerIP;
		uint16		nServerPort;
		wxString	sServerIP;
		int		nServerUsers;
		int		nServerMaxUsers;
		int		nServerFiles;
	
		static class ServersInfo *GetContainerInstance();
		uint32 ID() { return nServerIP; }
};

class UploadFiles {
	public:
		wxString  sUserName;
		uint32 nTransferredUp;
		uint32 nTransferredDown;
		uint32 nSpeed;
		//
		// Don't need filename - sharedfiles already have it
		CMD4Hash  nHash;

		UploadFiles(CEC_UpDownClient_Tag *tag);
		
		static class UploadsInfo *GetContainerInstance();
		CMD4Hash ID() { return nHash; }
};

class SearchFile {
	public:
		wxString sFileName;
		long lFileSize;
		CMD4Hash  nHash;
		long lSourceCount;
		bool bPresent;
		
		SearchFile(CEC_SearchFile_Tag *);
		
		void ProcessUpdate(CEC_SearchFile_Tag *);
		static class SearchInfo *GetContainerInstance();
		CMD4Hash ID() { return nHash; }
};

/*!
 * Each item of type T must implement GetContainerInstance(T) to return ptr
 * to container holding such items.
 * Parameter "T" is used for compiler to distinguish between functions of
 * different types
 */

template <class T>
bool operator < (const T &i1, const T &i2)
{
	return T::GetContainerInstance()->CompareItems(i1, i2);
}

/*!
 * T - type of items in container
 * E - type of enum for sort order
 */
template <class T, class E>
class ItemsContainer {
	protected:
		CamulewebApp *m_webApp;
		std::list<T> m_items;

		// map string value to enum: derived class
		// must init this map in ctor
		std::map<wxString, E> m_SortStrVals;
		// map sort order enums to names of sections
		// in html template: derived class
		// must init this map in ctor
		std::map<E, wxString> m_SortHeaders;
		
		
		bool m_SortReverse;
		// type is int, so derived class must cast it
		// to right enum type
		E m_SortOrder;
	
		void EraseAll()
		{
			m_items.erase(m_items.begin(), m_items.end());
		}
	public:
		ItemsContainer(CamulewebApp *webApp)
		{
			m_webApp = webApp;
			m_SortReverse = false;
			// by default, sort by first enum
			m_SortOrder = (E)0;
		}
		virtual ~ItemsContainer() { }
		
		void SortItems()
		{
			m_items.sort();
		}

		E GetSortOrder()
		{
			return m_SortOrder;
		}

		int ItemCount()
		{
			return m_items.size();
		}
		
		bool IsSortingReverse()
		{
			return m_SortReverse;
		}
		
		T *AddItem(T &item)
		{
			m_items.push_back(item);
			T *real_ptr = &(m_items.back());
			return real_ptr;
		}

		/*!
		 * Substitute sort-order templates
		 */
		void ProcessHeadersLine(wxString &line)
		{
			wxString sField = m_SortHeaders[m_SortOrder];
			// invert sort order in link
			wxString sSortRev(m_SortReverse ? wxT("&sortreverse=true") : wxT("&sortreverse=false"));
			for(typename std::map<E, wxString>::iterator i = m_SortHeaders.begin();
				i != m_SortHeaders.end(); i++) {
					if (sField == i->second) {
						line.Replace(i->second, sSortRev);
					} else {
						line.Replace(i->second, wxEmptyString);
					}
				}
		}
		
		/*!
		 * Convert string to right enum value
		 */
		void SetSortOrder(wxString &order, wxString &reverse)
		{
			if ( !order.IsEmpty() ) {
				m_SortOrder = m_SortStrVals[order];
			}
			if ( !reverse.IsEmpty() ) {
				m_SortReverse = (reverse == wxT("true")) ? false : true;
			}
		}
		/*!
		 * Re-query server: refresh all dataset
		 */
		virtual bool ReQuery() = 0;

		typedef typename std::list<T>::iterator ItemIterator;
		ItemIterator GetBeginIterator()
		{
			return m_items.begin();
		}
		ItemIterator GetEndIterator()
		{
			return m_items.end();
		}
};

/*!
 * T - type of items in container
 * E - type of enum for sort order
 * I - type of item ID
 * G - type of tag in EC
 */
template <class T, class E, class G, class I>
class UpdatableItemsContainer : public ItemsContainer<T, E> {
	protected:
		// need duplicate list with a map, so check "do we already have"
		// will take O(log(n)) instead of O(n)
		// map will contain pointers to items in list 
		std::map<I, T *> m_items_hash;
	public:
		UpdatableItemsContainer(CamulewebApp *webApp) : ItemsContainer<T, E>(webApp)
		{
		}
		
		T *AddItem(T &item)
		{
			T *real_ptr = ItemsContainer<T, E>::AddItem(item);
			m_items_hash[item.ID()] = real_ptr;
			return real_ptr;
		}
	
		T *GetByID(I id)
		{
			// avoid creating nodes
			return m_items_hash.count(id) ? m_items_hash[id] : NULL;
		}
		/*!
		 * Process answer of update request, create list of new items for
		 * full request later. Also remove items that no longer exist in core
		 */
		void ProcessUpdate(CECPacket *reply, CECPacket *full_req, int req_type)
		{
			std::set<I> core_files;
			for (int i = 0;i < reply->GetTagCount();i++) {
				G *tag = (G *)reply->GetTagByIndex(i);
		
				core_files.insert(tag->ID());
				if ( m_items_hash.count(tag->ID()) ) {
					T *item = m_items_hash[tag->ID()];
					item->ProcessUpdate(tag);
				} else {
					full_req->AddTag(CECTag(req_type, tag->ID()));
				}
			}
			std::list<I> del_ids;
			for(typename std::list<T>::iterator j = this->m_items.begin(); j != this->m_items.end(); j++) {
				if ( core_files.count(j->ID()) == 0 ) {
					// item may contain data that need to be freed externally, before
					// dtor is called and memory freed
					
					T *real_ptr = &*j;
					this->ItemDeleted(real_ptr);
					
					del_ids.push_back(j->ID());
				}
			}
			for(typename std::list<I>::iterator j = del_ids.begin(); j != del_ids.end(); j++) {
				m_items_hash.erase(*j);
				for(typename std::list<T>::iterator k = this->m_items.begin(); k != this->m_items.end(); k++) {
					if ( *j == k->ID() ) {
						this->m_items.erase(k);
						break;
					}
				}
			}
		}
		
		void ProcessFull(CECPacket *reply)
		{
			for (int i = 0;i < reply->GetTagCount();i++) {
				G *tag = (G *)reply->GetTagByIndex(i);
				// initialize item data from EC tag
				T item(tag);
				T *real_ptr = AddItem(item);
				// initialize any external data that may depend on this item
				this->ItemInserted(real_ptr);
			}
		}
		
		bool DoRequery(int cmd, int tag)
		{
			CECPacket req_sts(cmd, EC_DETAIL_UPDATE);
		
			//
			// Phase 1: request status
			CECPacket *reply = this->m_webApp->SendRecvMsg_v2(&req_sts);
			if ( !reply ) {
				return false;
			}
			
			//
			// Phase 2: update status, mark new files for subsequent query
			CECPacket req_full(cmd);
		
			ProcessUpdate(reply, &req_full, tag);
		
			delete reply;
		
			// Phase 3: request full info about files we don't have yet
			if ( req_full.GetTagCount() ) {
				reply = this->m_webApp->SendRecvMsg_v2(&req_full);
				if ( !reply ) {
					return false;
				}
				ProcessFull(reply);	
			}
			return true;
		}
		
		virtual void ItemDeleted(T *) { }
		virtual void ItemInserted(T *) { }
};

class UploadsInfo : public ItemsContainer<UploadFiles, int> {
	public:
		UploadsInfo(CamulewebApp *webApp);

		virtual bool ReQuery();
};

class ServersInfo : public ItemsContainer<ServerEntry, xServerSort> {
	public:
		// can be only one instance.
		static ServersInfo *m_This;
		
		ServersInfo(CamulewebApp *webApp);

		virtual bool ReQuery();

		bool CompareItems(const ServerEntry &i1, const ServerEntry &i2);
};


class SharedFilesInfo : public UpdatableItemsContainer<SharedFiles, xSharedSort, CEC_SharedFile_Tag, CMD4Hash> {
	public:
		// can be only one instance.
		static SharedFilesInfo *m_This;

		SharedFilesInfo(CamulewebApp *webApp);

		virtual bool ReQuery();

		bool CompareItems(const SharedFiles &i1, const SharedFiles &i2);
};

class SearchInfo : public UpdatableItemsContainer<SearchFile, xSearchSort, CEC_SearchFile_Tag, CMD4Hash> {
	public:
		static SearchInfo *m_This;
		
		SearchInfo(CamulewebApp *webApp);
		
		virtual bool ReQuery();

		bool CompareItems(const SearchFile &i1, const SearchFile &i2);
};

class CImageLib;
class DownloadFilesInfo : public UpdatableItemsContainer<DownloadFiles, xDownloadSort, CEC_PartFile_Tag, CMD4Hash> {
		CImageLib *m_ImageLib;
		
		// parameters of progress images
		wxString m_Template;
		int m_width, m_height;
	public:
		// can be only one instance.
		static DownloadFilesInfo *m_This;
		
		DownloadFilesInfo(CamulewebApp *webApp, CImageLib *imlib);
		
		void LoadImageParams(wxString &tpl, int width, int height);
		
		virtual bool ReQuery();

		// container requirements
		bool CompareItems(const DownloadFiles &i1, const DownloadFiles &i2);
		void ItemInserted(DownloadFiles *item);
		void ItemDeleted(DownloadFiles *item);
};

class CAnyImage {
	protected:
		unsigned char *m_data;
		int m_size, m_alloc_size;
		wxString m_Http;
		
		void Realloc(int size);
		
		void SetHttpType(wxString ext);
	public:
		CAnyImage(int size);
		virtual ~CAnyImage();

		char *GetHTTP() { return (char *)unicode2char(m_Http); }
		
		virtual unsigned char *RequestData(int &size);
};

class CFileImage : public CAnyImage {
		wxString m_name;
	public:
		CFileImage(const char *name);
		
		bool OpenOk() { return m_size != 0; }
};

class CImage3D_Modifiers {
		unsigned char *m_modifiers;
		int m_width;
	public:
		CImage3D_Modifiers(int width);
		~CImage3D_Modifiers();
		
		unsigned char operator[](int i)
		{
			return (i < m_width) ? m_modifiers[i] : 0;
		}
};

class CProgressImage : public CAnyImage {
	protected:
		DownloadFiles *m_file;
		
		wxString m_template;
		int m_width, m_height;
		wxString m_name;
		
		//
		// sorted list of gaps
		int m_gap_buf_size, m_gap_alloc_size;
		Gap_Struct *m_gap_buf;
		
		void ReallocGapBuffer();
		void InitSortedGaps();
		// for qsort
		static int compare_gaps(const void *, const void *);
		
		//
		// Turn list of gaps, partstatus into array of color strips
		typedef struct Color_Gap_Struct : public Gap_Struct {
			uint32 color;
		} Color_Gap_Struct;

		// result of rendering - single line
		uint32 *m_ColorLine;
		void CreateSpan();
	public:
		CProgressImage(int w, int h, wxString &tmpl, DownloadFiles *file);

		~CProgressImage();
				
		const wxString &Name() { return m_name; }
				
		virtual wxString GetHTML() = 0;
};

#ifdef WITH_LIBPNG

//
// Dynamic png image generation from gap info
class CDynImage : public CProgressImage {
		CImage3D_Modifiers m_modifiers;
		png_bytep *m_row_ptrs;
		
		static void png_write_fn(png_structp png_ptr, png_bytep data, png_size_t length);
		
		void DrawImage();
	public:
		CDynImage(int w, int h,	wxString &tmpl, DownloadFiles *file);
		~CDynImage();
		
		virtual unsigned char *RequestData(int &size);
		virtual wxString GetHTML();
};

#else


//
// Fallback to original implementation
class CDynImage : public CProgressImage {
	public:
		CDynImage(int w, int h,	wxString &tmpl, DownloadFiles *file);

		virtual wxString GetHTML();
};

#endif

#ifdef WITH_LIBPNG

//
// Dynamic png image generation from aMule data.

class CDynPngImage : public CAnyImage {
		
	public:
		CDynPngImage(int w, int h, const unsigned char* Data, wxString name);
		~CDynPngImage();
		
		virtual unsigned char *RequestData(int &size);
		virtual wxString GetHTML();
		const wxString &Name() { return m_name; }
	
	private:
		int m_width, m_height;
		wxString m_name;
		png_bytep *m_row_ptrs;
		
		static void png_write_fn(png_structp png_ptr, png_bytep data, png_size_t length);
		
};

#endif

class CImageLib {
		std::map<wxString, CAnyImage *> m_image_map;
		wxString m_image_dir;
	public:
		CImageLib(wxString image_dir);
		~CImageLib();
		
		CAnyImage *GetImage(wxString &name);
		void AddImage(CAnyImage *img, const wxString &name);
		void RemoveImage(const wxString &name);
};

typedef struct {
	uint32		nUsers;
	bool		bShowUploadQueue;

	ArrayOfUpDown		PointsForWeb;
	ArrayOfSession		Sessions;
	ArrayOfTransferredData	badlogins;

} GlobalParams;

typedef struct {
	wxString	sURL;
	in_addr		inadr;
	CWebSocket	*pSocket;
} ThreadData;

typedef struct {
	wxString	sHeader;
	wxString	sHeaderMetaRefresh;
	wxString	sHeaderStylesheet;
	wxString	sFooter;
	wxString	sServerList;
	wxString	sServerLine;
	wxString	sTransferImages;
	wxString	sTransferList;
	wxString	sTransferDownHeader;
	wxString	sTransferDownFooter;
	wxString	sTransferDownLine;
	wxString	sTransferDownLineGood;
	wxString	sTransferUpHeader;
	wxString	sTransferUpFooter;
	wxString	sTransferUpLine;
	wxString	sTransferUpQueueShow;
	wxString	sTransferUpQueueHide;
	wxString	sTransferUpQueueLine;
	wxString	sTransferBadLink;
	wxString	sDownloadLink;
	wxString	sSharedList;
	wxString	sSharedLine;
	wxString	sSharedLineChanged;
	wxString	sGraphs;
	wxString	sLog;
	wxString	sServerInfo;
	wxString 	sDebugLog;
	wxString 	sStats;
	wxString 	sPreferences;
	wxString	sLogin;
	wxString	sConnectedServer;
	wxString	sAddServerBox;
	wxString	sWebSearch;
	wxString	sSearch;
	wxString	sProgressbarImgs;
	wxString 	sProgressbarImgsPercent;
	uint16		iProgressbarWidth;
	wxString	sSearchResultLine;
	wxString	sSearchHeader;
	wxString	sClearCompleted;
	wxString	sCatArrow;
} WebTemplates;

class CWebServer {
	friend class CWebSocket;

	ServersInfo m_ServersInfo;
	SharedFilesInfo m_SharedFilesInfo;
	DownloadFilesInfo m_DownloadFilesInfo;
	UploadsInfo m_UploadsInfo;
	SearchInfo m_SearchInfo;
	
	CImageLib m_ImageLib;
	
	public:
		CWebServer(CamulewebApp *webApp);
		~CWebServer(void);

		void 	StartServer(void);
		void 	RestartServer(void);
		void 	StopServer(void);
		void 	ReloadTemplates(void);
	
		int	UpdateSessionCount();
		uint16	GetSessionCount()	{ return m_Params.Sessions.GetCount();}
		int 	GetWSPrefs();
		void	Print(const wxString &s);
		void	Send_Discard_V2_Request(CECPacket *request);

	protected:
		void	ProcessURL(ThreadData);
		void	ProcessFileReq(ThreadData);
		void 	ProcessImgFileReq(ThreadData);
		void 	ProcessStyleFileReq(ThreadData);
	
	private:
		wxString	_GetHeader(ThreadData, long lSession);
		wxString	_GetFooter(ThreadData);
		wxString	_GetServerList(ThreadData);
		wxString	_GetTransferList(ThreadData);
		wxString	_GetDownloadLink(ThreadData);
		wxString	_GetSharedFilesList(ThreadData);
		wxString	_GetGraphs(ThreadData);
		wxString	_GetLog(ThreadData);
		wxString	_GetServerInfo(ThreadData);
		wxString	_GetDebugLog(ThreadData);
		wxString	_GetStats(ThreadData);
		wxString	_GetPreferences(ThreadData);
		wxString	_GetLoginScreen(ThreadData);
		wxString	_GetConnectedServer(ThreadData);
		wxString _GetAddServerBox(ThreadData Data);
		wxString	_GetWebSearch(ThreadData Data);
		wxString _GetSearch(ThreadData);

		wxString	_ParseURL(ThreadData Data, wxString fieldname); 
		wxString	_ParseURLArray(ThreadData Data, wxString fieldname);
		bool	_IsLoggedIn(ThreadData Data, long lSession);
		void	_RemoveTimeOuts(ThreadData Data, long lSession);
		bool	_RemoveSession(ThreadData Data, long lSession);
		bool	_GetFileHash(wxString sHash, unsigned char *FileHash);
		wxString	_GetPlainResString(UINT nID, bool noquote = false);
		int	_GzipCompress(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int level);
		void	_SetSharedFilePriority(wxString hash, uint8 priority);
		wxString	_LoadTemplate(wxString sAll, wxString sTemplateName);
		Session	GetSessionByID(ThreadData Data,long sessionID);
		bool	IsSessionAdmin(ThreadData Data,wxString SsessionID);
		wxString	GetPermissionDenied();

		void	InsertCatBox(wxString &Out, int preselect, wxString boxlabel, CECTag *cats, bool jump=false);
		wxString GetStatusBox(wxString &preselect);

		// Common data
		CamulewebApp	*webInterface;
		wxMutex *m_mutexChildren;
		CWSThread	*wsThread;
		GlobalParams	m_Params;
		WebTemplates	m_Templates;
		bool		m_bServerWorking;
		int		m_iSearchSortby;
		bool		m_bSearchAsc;
		unsigned int	m_nRefresh;

		// Graph related
		wxString	m_sLastHistoryTimeStamp;
		uint16		m_nGraphHeight;
		uint16		m_nGraphWidth;
		uint16		m_nGraphScale;
};

#endif // WEBSERVER_H
