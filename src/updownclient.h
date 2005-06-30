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

#ifndef UPDOWNCLIENT_H
#define UPDOWNCLIENT_H

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
// implementation in amule.cpp & amule-remote-gui.cpp
#pragma interface "updownclient.h"
#endif

#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/string.h>		// Needed for wxString
#include <wx/intl.h>
#include "Types.h"		// Needed for int8, int16, uint8, uint16, uint32 and uint64
#include "CTypedPtrList.h"	// Needed for CTypedPtrList
#include "GetTickCount.h"	// Needed for GetTickCount
#include "CMD4Hash.h"
#include "StringFunctions.h"
#include "NetworkFunctions.h"

#include <map>
#include <vector>


typedef std::vector<bool> BitVector;


class CPartFile;
class CClientReqSocket;
class CClientCredits;
class CPacket;
class CFriend;
class Requested_Block_Struct;
class CKnownFile;
class Pending_Block_Struct;
class CSafeMemFile;
class CMemFile;
class Requested_File_Struct;
class CAICHHash;


// uploadstate
#define	US_UPLOADING		0
#define	US_ONUPLOADQUEUE	1
#define	US_WAITCALLBACK		2
#define	US_CONNECTING		3
#define	US_PENDING		4
#define	US_LOWTOLOWIP		5
#define US_BANNED		6
#define US_ERROR		7
#define US_NONE			8

// downloadstate
enum EDownloadState {
	DS_DOWNLOADING = 0,
	DS_ONQUEUE,
	DS_CONNECTED,
	DS_CONNECTING,
	DS_WAITCALLBACK,
	DS_WAITCALLBACKKAD,
	DS_REQHASHSET,
	DS_NONEEDEDPARTS,
	DS_TOOMANYCONNS,
	DS_TOOMANYCONNSKAD,
	DS_LOWTOLOWIP,
	DS_BANNED,
	DS_ERROR,
	DS_NONE,
	DS_REMOTEQUEUEFULL  // not used yet, except in statistics
};

// m_byChatstate
enum {
	MS_NONE = 0,
	MS_CHATTING,
	MS_CONNECTING,
	MS_UNABLETOCONNECT
};

enum ESourceFrom {
	SF_SERVER		= 0,
	SF_KADEMLIA		= 1,
	SF_SOURCE_EXCHANGE	= 2,
	SF_PASSIVE		= 3,
	SF_LINK			= 4
};

enum EClientSoftware {
	SO_EMULE		= 0,
	SO_CDONKEY		= 1,
	SO_LXMULE		= 2,
	SO_AMULE		= 3,
	SO_SHAREAZA		= 4,
	SO_EMULEPLUS		= 5,
	SO_HYDRANODE            = 6,
	SO_NEW2_MLDONKEY	= 10,
	SO_LPHANT		= 20,
	SO_NEW_SHAREAZA		= 68, // shazzas love to send 0x44 lately.
	SO_EDONKEYHYBRID	= 50,
	SO_EDONKEY		= 51,
	SO_MLDONKEY		= 52,
	SO_OLDEMULE		= 53,
	SO_UNKNOWN		= 54,
	SO_NEW_MLDONKEY		= 152,
	SO_COMPAT_UNK		= 0xFF
};

inline const wxString GetSoftName(uint8 software_ident) {
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


enum ESecureIdentState{
	IS_UNAVAILABLE		= 0,
	IS_ALLREQUESTSSEND	= 0,
	IS_SIGNATURENEEDED	= 1,
	IS_KEYANDSIGNEEDED	= 2
};

enum EInfoPacketState{
	IP_NONE			= 0,
	IP_EDONKEYPROTPACK	= 1,
	IP_EMULEPROTPACK	= 2,
	IP_BOTH			= 3
};

enum EKadState{
	KS_NONE,
	KS_QUEUED_FWCHECK,
	KS_CONNECTING_FWCHECK,
	KS_CONNECTED_FWCHECK,
	KS_QUEUED_BUDDY,
	KS_INCOMING_BUDDY,
	KS_CONNECTING_BUDDY,
	KS_CONNECTED_BUDDY,
	KS_NONE_LOWID,
	KS_WAITCALLBACK_LOWID,
	KS_QUEUE_LOWID
};

//! Used to keep track of the state of the client
enum ClientState
{
	//! New is for clients that have just been created.
	CS_NEW = 0,
	//! Listed is for clients that are on the clientlist
	CS_LISTED,
	//! Dying signifies clients that have been queued for deletion
	CS_DYING
};


class CUpDownClient
{
	friend class CClientList;
	friend class CUpDownClientListRem;
private:
	/**
	 * Please note that only the ClientList is allowed to delete the clients.
	 * To schedule a client for deletion, call the CClientList::AddToDeleteQueue
	 * funtion, which will safely remove dead clients once every second.
	 */
	~CUpDownClient();
	
public:
#ifdef CLIENT_GUI
	CUpDownClient(class CEC_UpDownClient_Tag *);
#else
	//base
	CUpDownClient(CClientReqSocket* sender = 0);
	CUpDownClient(uint16 in_port, uint32 in_userid, uint32 in_serverup, uint16 in_serverport,CPartFile* in_reqfile, bool ed2kID, bool checkfriend);
#endif
	/**
	 * This function should be called when the client object is to be deleted.
	 * It'll close the socket of the client and add it to the deletion queue
	 * owned by the CClientList class. However, if the CUpDownClient isn't on 
	 * the normal clientlist, it will be deleted immediatly.
	 * 
	 * The purpose of this is to avoid clients suddenly being removed due to 
	 * asyncronous events, such as socket errors, which can result in the 
	 * problems, as each CUpDownClient object is often kept in multiple lists,
	 * and instantly removing the client poses the risk of invalidating 
	 * currently used iterators and/or creating dangling pointers.
	 * 
	 * @see CClientList::AddToDeleteQueue
	 * @see CClientList::Process
	 */	
	void		Safe_Delete();

	/**
	 * Specifies if the client has been queued for deletion.
	 *
	 * @return True if Safe_Delete has been called, false otherwise.
	 */
	bool		HasBeenDeleted()		{ return m_clientState == CS_DYING; }

	ClientState	GetClientState()		{ return m_clientState; }

	bool		Disconnected(const wxString& strReason, bool bFromSocket = false);
	bool		TryToConnect(bool bIgnoreMaxCon = false);
	void		ConnectionEstablished();
	const wxString&	GetUserName() const		{ return m_Username; }
	//Only use this when you know the real IP or when your clearing it.
	void		SetIP( uint32 val );
	uint32		GetIP() const 			{ return m_dwUserIP; }
	bool		HasLowID() const 		{ return IsLowID(m_nUserIDHybrid); }
	const wxString&	GetFullIP() const		{ return m_FullUserIP; }
	uint32		GetConnectIP() const		{ return m_nConnectIP; }
	uint32		GetUserIDHybrid() const		{ return m_nUserIDHybrid; }
	void		SetUserIDHybrid(uint32 val);
	uint32		GetUserPort() const		{ return m_nUserPort; }
	uint32		GetTransferedDown() const	{ return m_nTransferedDown; }
	uint32		GetServerIP() const		{ return m_dwServerIP; }
	void		SetServerIP(uint32 nIP)		{ m_dwServerIP = nIP; }
	uint16		GetServerPort()	const		{ return m_nServerPort; }
	void		SetServerPort(uint16 nPort)	{ m_nServerPort = nPort; }	
	const CMD4Hash&	GetUserHash() const		{ return m_UserHash; }
	void		SetUserHash(const CMD4Hash& userhash);
	void		ValidateHash()			{ m_HasValidHash = !m_UserHash.IsEmpty(); }
	bool		HasValidHash() const		{ return m_HasValidHash; }
	uint32		GetVersion() const		{ return m_nClientVersion;}
	uint8		GetMuleVersion() const		{ return m_byEmuleVersion;}
	bool		ExtProtocolAvailable() const	{ return m_bEmuleProtocol;}
	bool		IsEmuleClient()	const		{ return m_byEmuleVersion;}
	CClientCredits*	Credits()		{ return credits;}
	const CClientCredits*	Credits() const { return credits;}
	bool		IsBanned() const;
	const wxString&	GetClientFilename() const	{ return m_clientFilename; }
	uint16		GetUDPPort() const		{ return m_nUDPPort; }
	void		SetUDPPort(uint16 nPort)	{ m_nUDPPort = nPort; }
	uint8		GetUDPVersion() const		{ return m_byUDPVer; }
	uint8		GetExtendedRequestsVersion() const { return m_byExtendedRequestsVer; }
	bool		IsFriend() const 		{ return m_Friend != NULL; }

	void		ClearDownloadBlockRequests();
	void		RequestSharedFileList();
	void		ProcessSharedFileList(const char* pachPacket, uint32 nSize, wxString& pszDirectory);

	wxString	GetUploadFileInfo();

	void		SetUserName(const wxString& NewName) { m_Username = NewName; }

	uint8		GetClientSoft() const		{ return m_clientSoft; }
	void		ReGetClientSoft();
	bool		ProcessHelloAnswer(const char *pachPacket, uint32 nSize);
	bool		ProcessHelloPacket(const char *pachPacket, uint32 nSize);
	void		SendHelloAnswer();
	bool		SendHelloPacket();
	void		SendMuleInfoPacket(bool bAnswer, bool OSInfo = false);
	bool		ProcessMuleInfoPacket(const char* pachPacket, uint32 nSize);
	void		ProcessMuleCommentPacket(const char *pachPacket, uint32 nSize);
	bool		Compare(const CUpDownClient* tocomp, bool bIgnoreUserhash = false);
	void		SetLastSrcReqTime()		{ m_dwLastSourceRequest = ::GetTickCount(); }
	void		SetLastSrcAnswerTime()		{ m_dwLastSourceAnswer = ::GetTickCount(); }
	void		SetLastAskedForSources()	{ m_dwLastAskedForSources = ::GetTickCount(); }
	uint32		GetLastSrcReqTime() const 	{ return m_dwLastSourceRequest; }
	uint32		GetLastSrcAnswerTime() const	{ return m_dwLastSourceAnswer; }
	uint32		GetLastAskedForSources() const	{ return m_dwLastAskedForSources; }
	bool		GetFriendSlot() const 		{ return m_bFriendSlot; }
	void		SetFriendSlot(bool bNV)		{ m_bFriendSlot = bNV; }
	void		SetCommentDirty(bool bDirty = true)	{ m_bCommentDirty = bDirty; }
	uint8		GetSourceExchangeVersion() const	{ return m_bySourceExchangeVer; }
	bool		SafeSendPacket(CPacket* packet);

	void		SendPublicKeyPacket();
	void		SendSignaturePacket();
	void		ProcessPublicKeyPacket(const byte* pachPacket, uint32 nSize);
	void		ProcessSignaturePacket(const byte* pachPacket, uint32 nSize);
	uint8		GetSecureIdentState(); 

	void		SendSecIdentStatePacket();
	void		ProcessSecIdentStatePacket(const byte* pachPacket, uint32 nSize);

	uint8		GetInfoPacketsReceived() const	{ return m_byInfopacketsReceived; }
	void		InfoPacketsReceived();

	CClientCredits	*credits;
	CFriend 	*m_Friend;

	//upload
	uint8		GetUploadState() const		{ return m_nUploadState; }
	void		SetUploadState(uint8 news);
	uint32		GetTransferredUp() const	{ return m_nTransferredUp; }
	uint32		GetSessionUp() const		{ return m_nTransferredUp - m_nCurSessionUp; }
	void		ResetSessionUp() {
						m_nCurSessionUp = m_nTransferredUp;
						m_addedPayloadQueueSession = 0;
						m_nCurQueueSessionPayloadUp = 0;
					}
	uint32		GetUploadDatarate() const	{ return m_nUpDatarate; }

#ifndef CLIENT_GUI
	uint32		GetWaitTime() const 		{ return m_dwUploadTime - GetWaitStartTime(); }
	uint32		GetUpStartTimeDelay() const	{ return ::GetTickCount() - m_dwUploadTime; }
	uint32		GetWaitStartTime() const;
#else
	uint32		m_WaitTime, m_UpStartTimeDelay, m_WaitStartTime;
	uint32		GetWaitTime() const		{ return m_WaitTime; }
	uint32		GetUpStartTimeDelay() const	{ return m_UpStartTimeDelay; }
	uint32		GetWaitStartTime() const	{ return m_WaitStartTime; }
#endif

	bool		IsDownloading()	const 		{ return (m_nUploadState == US_UPLOADING); }

#ifdef CLIENT_GUI
	uint32 m_base_score, m_score;
	uint32		GetScore(
				bool WXUNUSED(sysvalue),
				bool WXUNUSED(isdownloading) = false,
				bool WXUNUSED(onlybasevalue) = false) const
	{
		// lfroen: it's calculated
		return 0;
	}
#else
	uint32		GetScore(
				bool sysvalue,
				bool isdownloading = false,
				bool onlybasevalue = false) const;
#endif

	void		AddReqBlock(Requested_Block_Struct* reqblock);
	void		CreateNextBlockPackage();
	void		SetUpStartTime() 		{ m_dwUploadTime = ::GetTickCount(); }
	void		SetWaitStartTime();
	void		ClearWaitStartTime();
	void		SendHashsetPacket(const CMD4Hash& forfileid);
	bool		SupportMultiPacket() const	{ return m_bMultiPacket; }

	void		SetUploadFileID(CKnownFile *newreqfile);

	/**
	 *Gets the file actually on upload
	 *
	 */
	const CKnownFile* GetUploadFile() const		{ return m_uploadingfile; }

	void		SendOutOfPartReqsAndAddToWaitingQueue();
	void		ProcessExtendedInfo(const CSafeMemFile *data, CKnownFile *tempreqfile);
	void		ProcessFileInfo(const CSafeMemFile* data, const CPartFile* file);
	void		ProcessFileStatus(bool bUdpPacket, const CSafeMemFile* data, const CPartFile* file);

	const CMD4Hash&	GetUploadFileID() const		{ return m_requpfileid; }
	void		SetUploadFileID(const CMD4Hash& new_id);
	void		ClearUploadFileID()		{ m_requpfileid.Clear(); m_uploadingfile = NULL;};
	uint32		SendBlockData();
	void		ClearUploadBlockRequests();
	void		SendRankingInfo();
	void		SendCommentInfo(CKnownFile *file);
	bool 		IsDifferentPartBlock() const;
	void		UnBan();
	void		Ban();
	bool		m_bAddNextConnect;      // VQB Fix for LowID slots only on connection
	uint32		GetAskedCount() const 		{ return m_cAsked; }
	void		AddAskedCount()			{ m_cAsked++; }
	void		ClearAskedCount()		{ m_cAsked = 1; }	// 1, because it's cleared *after* the first request...
	void		FlushSendBlocks();	// call this when you stop upload, 
						// or the socket might be not able to send
	void		SetLastUpRequest()		{ m_dwLastUpRequest = ::GetTickCount(); }
	uint32		GetLastUpRequest() const 	{ return m_dwLastUpRequest; }
	uint16		GetUpPartCount() const 		{ return m_upPartStatus.size(); }


	//download
	void 		SetRequestFile(CPartFile* reqfile); 
	CPartFile*	GetRequestFile() const		{ return m_reqfile; }

	uint8		GetDownloadState() const	{ return m_nDownloadState; }
	void		SetDownloadState(uint8 byNewState);
	uint32		GetLastAskedTime() const	{ return m_dwLastAskedTime; }

	bool		IsPartAvailable(uint16 iPart) const
					{ return ( iPart < m_downPartStatus.size() ) ? m_downPartStatus[iPart] : 0; }
	bool		IsUpPartAvailable(uint16 iPart) const 
					{ return ( iPart < m_upPartStatus.size() ) ? m_upPartStatus[iPart] : 0;}

	const BitVector& GetPartStatus() const		{ return m_downPartStatus; }
	const BitVector& GetUpPartStatus() const	{ return m_upPartStatus; }
	float		GetKBpsDown() const		{ return kBpsDown; }
	float		CalculateKBpsDown();
	uint16		GetRemoteQueueRank() const	{ return m_nRemoteQueueRank; }
	uint16		GetOldRemoteQueueRank() const	{ return m_nOldRemoteQueueRank; }
	void		SetRemoteQueueFull(bool flag)	{ m_bRemoteQueueFull = flag; }
	bool		IsRemoteQueueFull() const 	{ return m_bRemoteQueueFull; }
	void		SetRemoteQueueRank(uint16 nr);
	bool		AskForDownload();
	void		SendStartupLoadReq();
	void		SendFileRequest();
	void		ProcessHashSet(const char *packet, uint32 size);
	bool		AddRequestForAnotherFile(CPartFile* file);
	bool		DeleteFileRequest(CPartFile* file);
	void		DeleteAllFileRequests();
	void		SendBlockRequests();
	void		ProcessBlockPacket(const char* packet, uint32 size, bool packed = false);

#ifndef CLIENT_GUI
	uint16		GetAvailablePartCount() const;
#else
	uint16		m_AvailPartCount;
	uint16		GetAvailablePartCount() const	{ return m_AvailPartCount; }
#endif

	bool		SwapToAnotherFile(bool bIgnoreNoNeeded, bool ignoreSuspensions, bool bRemoveCompletely, CPartFile* toFile = NULL);
	void		UDPReaskACK(uint16 nNewQR);
	void		UDPReaskFNF();
	void		UDPReaskForDownload();
	bool		IsSourceRequestAllowed();
	uint16		GetUpCompleteSourcesCount() const	{ return m_nUpCompleteSourcesCount; }
	void		SetUpCompleteSourcesCount(uint16 n)	{ m_nUpCompleteSourcesCount = n; }

	//chat
	uint8		GetChatState()			{ return m_byChatstate; }
	void		SetChatState(uint8 nNewS)	{ m_byChatstate = nNewS; }

	//File Comment
	const wxString&	GetFileComment() const 		{ return m_strComment; }
	uint8		GetFileRating() const		{ return m_iRating; }

	wxString	GetSoftStr() const 		{ return m_clientVerString.Left(m_SoftLen); }
	wxString	GetSoftVerStr() const		{ return m_clientVerString.Mid(m_SoftLen+1); }
	
	uint16		GetKadPort() const		{ return m_nKadPort; }
	void		SetKadPort(uint16 nPort)	{ m_nKadPort = nPort; }

	// Kry - AICH import
	void		SetReqFileAICHHash(CAICHHash* val);
	CAICHHash*	GetReqFileAICHHash() const	{return m_pReqFileAICHHash;}
	bool		IsSupportingAICH() const	{return m_fSupportsAICH & 0x01;}
	void		SendAICHRequest(CPartFile* pForFile, uint16 nPart);
	bool		IsAICHReqPending() const	{return m_fAICHRequested; }
	void		ProcessAICHAnswer(const char* packet, uint32 size);
	void		ProcessAICHRequest(const char* packet, uint32 size);
	void		ProcessAICHFileHash(CSafeMemFile* data, const CPartFile* file);	

	EUtf8Str	GetUnicodeSupport() const;

	// Barry - Process zip file as it arrives, don't need to wait until end of block
	int 		unzip(Pending_Block_Struct *block, byte *zipped, uint32 lenZipped, byte **unzipped, uint32 *lenUnzipped, int iRecursion = 0);
	// Barry - Sets string to show parts downloading, eg NNNYNNNNYYNYN
	wxString	ShowDownloadingParts() const;
	void 		UpdateDisplayedInfo(bool force = false);
	int 		GetFileListRequested() const 	{ return m_iFileListRequested; }
	void 		SetFileListRequested(int iFileListRequested) { m_iFileListRequested = iFileListRequested; }

	void		ResetFileStatusInfo();

	bool		CheckHandshakeFinished(uint32 protocol, uint32 opcode) const;

	bool		GetSentCancelTransfer() const	{ return m_fSentCancelTransfer; }
	void		SetSentCancelTransfer(bool bVal)	{ m_fSentCancelTransfer = bVal; }

	wxString	GetClientFullInfo();

	const wxString& GetClientOSInfo() const		{ return m_sClientOSInfo; }

	void		ProcessPublicIPAnswer(const byte* pbyData, uint32 uSize);
	void		SendPublicIPRequest();

	/**
	 * Sets the current socket of the client.
	 * 
	 * @param socket The pointer to the new socket, can be NULL.
	 *
	 * Please note that this function DOES NOT delete the old socket.
	 */
	void 		SetSocket(CClientReqSocket* socket)	{ m_socket = socket; }

	/**
	 * Function for accessing the socket owned by a client.
	 * 
	 * @return The pointer (can be NULL) to the socket used by this client.
	 *
	 * Please note that the socket object is quite volatile and can be removed
	 * from one function call to the next, therefore, you should normally use 
	 * the safer functions below, which all check if the socket is valid before
	 * deferring it.
	 */
	CClientReqSocket* GetSocket() const		{ return m_socket; }

	/**
	 * Safe function for checking if the socket is connected.
	 *
	 * @return True if the socket exists and is connected, false otherwise.
	 */
#ifndef CLIENT_GUI
	bool		IsConnected() const;
#else
	bool		m_IsConnected;
	bool		IsConnected() const		{ return m_IsConnected; }
#endif

	/**
	 * Safe function for sending packets.
	 *
	 * @return True if the socket exists and the packet was sent, false otherwise.
	 */
	bool		SendPacket(CPacket* packet, bool delpacket = true, bool controlpacket = true);

	/**
	 * Safe function for setting the download limit of the socket.
	 *
	 * @return Current download speed of the client.
	 */
	float		SetDownloadLimit(uint32 reducedownload);

	/**
	 * Sends a message to a client
	 *
	 * @return True if sent, false if connecting
	 */
	bool	SendMessage(const wxString& message);

	uint32		GetPayloadInBuffer() const	{ return m_addedPayloadQueueSession - GetQueueSessionPayloadUp(); }
	uint32		GetQueueSessionPayloadUp() const	{ return m_nCurQueueSessionPayloadUp; }
	void		SendCancelTransfer(CPacket* packet = NULL);
	bool		HasBlocks() const		{ return !m_BlockRequests_queue.IsEmpty(); }

	/* Source comes from? */
	ESourceFrom		GetSourceFrom() const	{ return (ESourceFrom)m_nSourceFrom; }
	void			SetSourceFrom(ESourceFrom val)	{ m_nSourceFrom = val; }

	/* Kad buddy support */
	// ID
	const byte*	GetBuddyID() const		{ return m_achBuddyID; }
	void		SetBuddyID(const byte* m_achTempBuddyID);
	bool		HasValidBuddyID() const		{ return m_bBuddyIDValid; }
	/* IP */
	void		SetBuddyIP( uint32 val )	{ m_nBuddyIP = val; }
	uint32		GetBuddyIP() const		{ return m_nBuddyIP; }
	/* Port */
	void		SetBuddyPort( uint16 val )	{ m_nBuddyPort = val; }
	uint16		GetBuddyPort() const		{ return m_nBuddyPort; }

	//KadIPCheck
	bool		SendBuddyPingPong()		{ return m_dwLastBuddyPingPongTime < ::GetTickCount(); }
	bool		AllowIncomeingBuddyPingPong()	{ return m_dwLastBuddyPingPongTime < (::GetTickCount()-(3*60*1000)); }
	void		SetLastBuddyPingPongTime()	{ m_dwLastBuddyPingPongTime = (::GetTickCount()+(10*60*1000)); }	
	EKadState	GetKadState() const		{ return m_nKadState; }
	void		SetKadState(EKadState nNewS)	{ m_nKadState = nNewS; }
	uint8		GetKadVersion()			{ return m_byKadVersion; }
	// Kad added by me
	bool			SendBuddyPing();

private:
	uint32		m_nTransferredUp;
	uint32		m_nCurQueueSessionPayloadUp;
	uint32		m_addedPayloadQueueSession;

	struct TransferredData {
		uint32	datalen;
		uint32	timestamp;
	};

	//////////////////////////////////////////////////////////
	// Upload data rate computation
	//
	uint32		m_nUpDatarate;
	uint32		m_nSumForAvgUpDataRate;
	CList<TransferredData> m_AvarageUDR_list;


	/**
	 * This struct is used to keep track of CPartFiles which this source shares.
	 */
	struct A4AFStamp {
		//! Signifies if this sources has needed parts for this file. 
		bool NeededParts;
		//! This is set when we wish to avoid swapping to this file for a while.
		uint32 timestamp;
	};

	//! I typedef in the name of readability!
	typedef std::map<CPartFile*, A4AFStamp> A4AFList;
	//! This list contains all PartFiles which this client can be used as a source for.
	A4AFList m_A4AF_list;

	/**
	 * Helper function used by SwapToAnotherFile().
	 *
	 * @param it The iterator of the PartFile to be examined.
	 * @param ignorenoneeded Do not check for the status NoNeededParts when checking the file.
	 * @param ignoresuspended Do not check the timestamp when checking the file.
	 * @return True if the file is a viable target, false otherwise.
	 *
	 * This function is used to perform checks to see if we should consider 
	 * this file a viable target for A4AF swapping. Unless ignoresuspended is
	 * true, it will examine the timestamp of the file and reset it if needed.
	 */
	bool		IsValidSwapTarget( A4AFList::iterator it, bool ignorenoneeded = false, bool ignoresuspended = false );

	CPartFile*	m_reqfile;

	// base
	void		Init();
	bool		ProcessHelloTypePacket(const CSafeMemFile& data);
	void		SendHelloTypePacket(CSafeMemFile* data);
	void		ClearHelloProperties(); // eMule 0.42
	uint32		m_dwUserIP;
	uint32		m_nConnectIP;		// holds the supposed IP or (after we had a connection) the real IP
	uint32		m_dwServerIP;
	uint32		m_nUserIDHybrid;
	int16		m_nUserPort;
	int16		m_nServerPort;
	uint32		m_nClientVersion;
	uint32		m_cSendblock;
	uint8		m_byEmuleVersion;
	uint8		m_byDataCompVer;
	uint8		m_SoftLen;
	bool		m_bEmuleProtocol;
	wxString	m_Username;
	wxString	m_FullUserIP;
	CMD4Hash	m_UserHash;
	bool		m_HasValidHash;
	uint16		m_nUDPPort;
	uint8		m_byUDPVer;
	uint8		m_bySourceExchangeVer;
	uint8		m_byAcceptCommentVer;
	uint8		m_byExtendedRequestsVer;
	uint8		m_clientSoft;
	uint32		m_dwLastSourceRequest;
	uint32		m_dwLastSourceAnswer;
	uint32		m_dwLastAskedForSources;
	int		m_iFileListRequested;
	bool		m_bFriendSlot;
	bool		m_bCommentDirty;
	bool		m_bIsHybrid;
	bool		m_bIsML;
 	bool		m_bSupportsPreview;
	bool		m_bUnicodeSupport;	
	uint16		m_nKadPort;
	bool		m_bMultiPacket;
	ClientState	m_clientState;
	CClientReqSocket*	m_socket;		
	bool		m_fNeedOurPublicIP; // we requested our IP from this client

	// Kry - Secure User Ident import
	ESecureIdentState	m_SecureIdentState; 
	uint8		m_byInfopacketsReceived;		// have we received the edonkeyprot and emuleprot packet already (see InfoPacketsReceived() )
	uint32		m_dwLastSignatureIP;
	uint8		m_bySupportSecIdent;

	uint32		m_byCompatibleClient;
	CList<CPacket*>	m_WaitingPackets_list;
	uint32		m_lastRefreshedDLDisplay;

	//upload
	void CreateStandartPackets(const unsigned char* data,uint32 togo, Requested_Block_Struct* currentblock);
	void CreatePackedPackets(const unsigned char* data,uint32 togo, Requested_Block_Struct* currentblock);

	uint8		m_nUploadState;
	uint32		m_dwUploadTime;
	uint32		m_cAsked;
	uint32		m_dwLastUpRequest;
	uint32		m_nCurSessionUp;
	uint16		m_nUpPartCount;
	CMD4Hash	m_requpfileid;
	uint16		m_nUpCompleteSourcesCount;

public:
	//! This vector contains the avilability of parts for the file that the user
	//! is requesting. When changing it, be sure to call CKnownFile::UpdatePartsFrequency
	//! so that the files know the actual availability of parts.
	BitVector	m_upPartStatus;
	uint16		m_lastPartAsked;
	wxString	m_strModVersion;

	CList<Requested_Block_Struct*>	m_BlockRequests_queue;
	CList<Requested_Block_Struct*>	m_DoneBlocks_list;

	//download
	bool		m_bRemoteQueueFull;
	uint8		m_nDownloadState;
	uint16		m_nPartCount;
	uint32		m_dwLastAskedTime;
	wxString	m_clientFilename;
	uint32		m_nTransferedDown;
	uint32		m_nLastBlockOffset;   // Patch for show parts that you download [Cax2]
	uint16		m_cShowDR;
	uint32		m_dwLastBlockReceived;
	uint16		m_nRemoteQueueRank;
	uint16		m_nOldRemoteQueueRank;
	bool		m_bCompleteSource;
	bool		m_bReaskPending;
	bool		m_bUDPPending;
	bool		m_bHashsetRequested;

	CList<Pending_Block_Struct*>	m_PendingBlocks_list;
	CList<Requested_Block_Struct*>	m_DownloadBlocks_list;

	float		kBpsDown;
	float		fDownAvgFilter;
	uint32		msReceivedPrev;
	uint32		bytesReceivedCycle;
	// chat
	uint8 		m_byChatstate;
	wxString	m_strComment;
	int8		m_iRating;
	unsigned int
		m_fHashsetRequesting : 1, // we have sent a hashset request to this client
		m_fNoViewSharedFiles : 1, // client has disabled the 'View Shared Files' feature, 
					  // if this flag is not set, we just know that we don't know 
					  // for sure if it is enabled
		m_fSupportsPreview   : 1,
		m_fSentCancelTransfer: 1, // we have sent an OP_CANCELTRANSFER in the current connection
		m_fSharedDirectories : 1, // client supports OP_ASKSHAREDIRS opcodes
		m_fSupportsAICH      : 3,
		m_fAICHRequested     : 1,
		m_fSentOutOfPartReqs : 1;

	unsigned int
		m_fOsInfoSupport : 1;

	/* Razor 1a - Modif by MikaelB */

	int		GetHashType() const;
	bool		m_bHelloAnswerPending;

	// Kry - Atribute to get the 1.x / 2.x / CVS flags
	// Why this way? Well, on future is expected that count(2.x) > count(1x)
	// So I prefer to set the 1.x flag because it will be less CPU.
	// I know. I'm paranoid on CPU.
	// (Extended_aMule_SO & 1)  -> 1.x
	// !(Extended_aMule_SO & 1) -> 2.x
	// (Extended_aMule_SO & 2)  -> CVS
	uint8		Extended_aMule_SO;

	//! This vector contains the avilability of parts for the file we requested 
	//! from this user. When changing it, be sure to call CPartFile::UpdatePartsFrequency
	//! so that the files know the actual availability of parts.
	BitVector	m_downPartStatus;

	CAICHHash* 	m_pReqFileAICHHash; 

	bool		m_bMsgFiltered;

	ESourceFrom	m_nSourceFrom;

	/* Kad Stuff */
	byte		m_achBuddyID[16];
	bool		m_bBuddyIDValid;
	uint32		m_nBuddyIP;
	uint16		m_nBuddyPort;

	EKadState	m_nKadState;	

	uint8		m_byKadVersion;
	uint32		m_dwLastBuddyPingPongTime;

public:
	/**
	 * Checks that a client isn't aggressively re-asking for files.
	 * 
	 * Call this when a file is requested. If the time since the last request is
	 * less than MIN_REQUESTTIME, 3 is added to the m_Aggressiveness variable.
	 * If the time since the last request is >= MIN_REQUESTTIME, the variable is
	 * decremented by 1. The client is banned if the variable reaches 10 or above.
	 *
	 * To check if a client is aggressive use the IsClientAggressive() function.
	 * 
	 * Currently this function is called when the following packets are recieved:
	 *  - OP_STARTUPLOADREQ
	 *  - OP_REASKFILEPING
	 */
	void		CheckForAggressive();

	/**
	 * Specifies if a client has aggressivly requested files.
	 *
	 * @return True if the client is EVIL, false otherwise.
	 */
	bool		IsClientAggressive() const	{ return ( m_Aggressiveness >= 10 ); }

	uint8		GetExtended_aMule_SO() const	{ return Extended_aMule_SO; };

private:
	//! This keeps track of aggressive requests for files. 
	uint16		m_Aggressiveness;
	//! This tracks the time of the last time since a file was requested
	uint32		m_LastFileRequest;

public:
	const wxString&	GetClientModString() const	{ return m_strModVersion; }
	const wxString&	GetClientVerString() const	{ return m_clientVerString; }

private:
	bool 		m_OSInfo_sent;
	wxString	m_clientVerString;
	wxString	m_sClientOSInfo;
	wxString	m_pendingMessage;

	int		SecIdentSupRec;

	CKnownFile*	m_uploadingfile;
};


#define	MAKE_CLIENT_VERSION(mjr, min, upd) \
	((uint32)(mjr)*100U*10U*100U + (uint32)(min)*100U*10U + (uint32)(upd)*100U)


#endif // UPDOWNCLIENT_H
