//
// This file is part of the aMule Project.
//
// Parts of this file are based on work from pan One (http://home-3.tiscali.nl/~meost/pms/)
//
// Copyright (c) 2003 aMule Team ( http://www.amule-project.net )
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

#if defined(__GNUG__) && !defined(NO_GCC_PRAGMA)
#pragma implementation "KnownFile.h"
#endif

#include <wx/filefn.h>

#include <wx/file.h>
#include <wx/ffile.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <wx/string.h>
#include <wx/filename.h>
#include <wx/confbase.h>
#include <wx/config.h>

#include "KnownFile.h"		// Interface declarations.
#include "OtherFunctions.h"	// Needed for nstrdup
#include "UploadQueue.h"	// Needed for CUploadQueue
#include "SafeFile.h"		// Needed for CSafeMemFile
#include "updownclient.h"	// Needed for CUpDownClient
#include "Packet.h"		// Needed for CTag
#include "Preferences.h"	// Needed for CPreferences
#include "SharedFileList.h"	// Needed for CSharedFileList
#include "KnownFileList.h"	// Needed for CKnownFileList
#include "amule.h"			// Needed for theApp
#include "PartFile.h"		// Needed for SavePartFile
#include "ArchSpecific.h"
#include "Logger.h"

#include <wx/arrimpl.cpp> // this is a magic incantation which must be done!

#include "CryptoPP_Inc.h"       // Needed for MD4

WX_DEFINE_OBJARRAY(ArrayOfCMD4Hash);
WX_DEFINE_OBJARRAY(ArrayOfCTag);

CFileStatistic::CFileStatistic() : 
	requested(0), 
	transfered(0),
	accepted(0),
	alltimerequested(0),
	alltimetransferred(0),
	alltimeaccepted(0) 
{
}

#ifndef CLIENT_GUI

void CFileStatistic::AddRequest(){
	requested++;
	alltimerequested++;
	theApp.knownfiles->requested++;
	theApp.sharedfiles->UpdateItem(fileParent);
}
	
void CFileStatistic::AddAccepted(){
	accepted++;
	alltimeaccepted++;
	theApp.knownfiles->accepted++;
	theApp.sharedfiles->UpdateItem(fileParent);
}
	
void CFileStatistic::AddTransferred(uint64 bytes){
	transfered += bytes;
	alltimetransferred += bytes;
	theApp.knownfiles->transfered += bytes;
	theApp.sharedfiles->UpdateItem(fileParent);
}

#endif // CLIENT_GUI

CAbstractFile::CAbstractFile() :
	m_nFileSize(0),
	m_iRate(0)
{
}


void CAbstractFile::SetFileName(const wxString& strFileName)
{ 
	m_strFileName = strFileName;
} 

CKnownFile::CKnownFile() :
	date(0),
	m_nCompleteSourcesTime(time(NULL)),
	m_nCompleteSourcesCount(0),
	m_nCompleteSourcesCountLo(0),
	m_nCompleteSourcesCountHi(0),
	m_bCommentLoaded(false),
	m_iPartCount(0),
	m_iED2KPartCount(0),
	m_iED2KPartHashCount(0),
	m_iQueuedCount(0),
	m_PublishedED2K(false)
{
	statistic.fileParent = this;
	
	m_bAutoUpPriority = thePrefs::GetNewAutoUp();
	m_iUpPriority = ( m_bAutoUpPriority ) ? PR_HIGH : PR_NORMAL;
	m_pAICHHashSet = new CAICHHashSet(this);
}

#ifdef CLIENT_GUI

CKnownFile::CKnownFile(CEC_SharedFile_Tag *tag)
{
	m_pAICHHashSet = new CAICHHashSet(this);
	
	m_strFileName = tag->FileName();
	m_abyFileHash = tag->ID();
	m_nFileSize = tag->SizeFull();
	m_iPartCount = (m_nFileSize + (PARTSIZE - 1)) / PARTSIZE;
	#if wxCHECK_VERSION(2, 5, 0)
	m_AvailPartFrequency.SetCount(m_iPartCount);
	#else
	// wx2.4 has no SetCount.
	if (m_AvailPartFrequency.Count() < m_iPartCount) {
		m_AvailPartFrequency.Add(0, m_iPartCount - m_AvailPartFrequency.Count());
	}
	#endif
	m_iUpPriority = tag->Prio();
	if ( m_iUpPriority >= 10 ) {
		m_iUpPriority-= 10;
		m_bAutoUpPriority = true;
	} else {
		m_bAutoUpPriority = false;
	}
}

CKnownFile::~CKnownFile()
{
	hashlist.Clear();
	delete m_pAICHHashSet;
}

#else // ! CLIENT_GUI

CKnownFile::~CKnownFile(){
	
	hashlist.Clear();
			
	for (size_t i = 0; i != taglist.GetCount(); i++) {
		delete taglist[i];
	}		
	
	SourceSet::iterator it = m_ClientUploadList.begin();
	for ( ; it != m_ClientUploadList.end(); it++ ) {
		(*it)->ResetUploadFile();
	}
	
	delete m_pAICHHashSet;
}

void CKnownFile::AddUploadingClient(CUpDownClient* client){
	m_ClientUploadList.insert(client);
}




void CKnownFile::RemoveUploadingClient(CUpDownClient* client){
	m_ClientUploadList.erase(client);
}

void CKnownFile::SetFilePath(const wxString& strFilePath)
{
	m_strFilePath = strFilePath;
}

bool CKnownFile::CreateAICHHashSetOnly()
{
	wxASSERT( !IsPartFile() );
	m_pAICHHashSet->FreeHashSet();
	
	CFile file(GetFilePath() +  wxFileName::GetPathSeparator() + GetFileName(),CFile::read);
	if ( !file.IsOpened() ){
		AddDebugLogLineM( false, logAICHThread, wxT("CreateAICHHashSetOnly(): Failed to open file: ") + file.GetFilePath() );
		return false;
	}

	// create aichhashset
	uint32 togo = m_nFileSize;
	uint16 hashcount;
	for (hashcount = 0; togo >= PARTSIZE; ) {
		CAICHHashTree* pBlockAICHHashTree = m_pAICHHashSet->m_pHashTree.FindHash(hashcount*PARTSIZE, PARTSIZE);
		wxASSERT( pBlockAICHHashTree != NULL );
		CreateHashFromFile(&file, PARTSIZE, NULL, pBlockAICHHashTree);
		// SLUGFILLER: SafeHash - quick fallback
		if ( !theApp.IsRunning()){ // in case of shutdown while still hashing
			file.Close();
			return false;
		}

		togo -= PARTSIZE;
		hashcount++;
	}

	if (togo != 0){
		CAICHHashTree* pBlockAICHHashTree = m_pAICHHashSet->m_pHashTree.FindHash(hashcount*PARTSIZE, togo);
		wxASSERT( pBlockAICHHashTree != NULL );
		CreateHashFromFile(&file, togo, NULL, pBlockAICHHashTree);
	}
	
	m_pAICHHashSet->ReCalculateHash(false);
	if ( m_pAICHHashSet->VerifyHashTree(true) ){
		m_pAICHHashSet->SetStatus(AICH_HASHSETCOMPLETE);
		if (!m_pAICHHashSet->SaveHashSet()){
			AddDebugLogLineM( true, logAICHThread, wxT("Failed to save AICH Hashset!") );
		}
	}
	else{
		// now something went pretty wrong
		AddDebugLogLineM(true, logAICHThread, wxT("Failed to calculate AICH Hashset from file ") + GetFileName() );
	}
	
	file.Close();	
	return true;	
}

void CKnownFile::SetFileSize(uint32 nFileSize)
{
	CAbstractFile::SetFileSize(nFileSize);
	m_pAICHHashSet->SetFileSize(nFileSize);
	
	// Examples of parthashs, hashsets and filehashs for different filesizes
	// according the ed2k protocol
	//----------------------------------------------------------------------
	//
	//File size: 3 bytes
	//File hash: 2D55E87D0E21F49B9AD25F98531F3724
	//Nr. hashs: 0
	//
	//
	//File size: 1*PARTSIZE
	//File hash: A72CA8DF7F07154E217C236C89C17619
	//Nr. hashs: 2
	//Hash[  0]: 4891ED2E5C9C49F442145A3A5F608299
	//Hash[  1]: 31D6CFE0D16AE931B73C59D7E0C089C0	*special part hash*
	//
	//
	//File size: 1*PARTSIZE + 1 byte
	//File hash: 2F620AE9D462CBB6A59FE8401D2B3D23
	//Nr. hashs: 2
	//Hash[  0]: 121795F0BEDE02DDC7C5426D0995F53F
	//Hash[  1]: C329E527945B8FE75B3C5E8826755747
	//
	//
	//File size: 2*PARTSIZE
	//File hash: A54C5E562D5E03CA7D77961EB9A745A4
	//Nr. hashs: 3
	//Hash[  0]: B3F5CE2A06BF403BFB9BFFF68BDDC4D9
	//Hash[  1]: 509AA30C9EA8FC136B1159DF2F35B8A9
	//Hash[  2]: 31D6CFE0D16AE931B73C59D7E0C089C0	*special part hash*
	//
	//
	//File size: 3*PARTSIZE
	//File hash: 5E249B96F9A46A18FC2489B005BF2667
	//Nr. hashs: 4
	//Hash[  0]: 5319896A2ECAD43BF17E2E3575278E72
	//Hash[  1]: D86EF157D5E49C5ED502EDC15BB5F82B
	//Hash[  2]: 10F2D5B1FCB95C0840519C58D708480F
	//Hash[  3]: 31D6CFE0D16AE931B73C59D7E0C089C0	*special part hash*
	//
	//
	//File size: 3*PARTSIZE + 1 byte
	//File hash: 797ED552F34380CAFF8C958207E40355
	//Nr. hashs: 4
	//Hash[  0]: FC7FD02CCD6987DCF1421F4C0AF94FB8
	//Hash[  1]: 2FE466AF8A7C06DA3365317B75A5ACFE
	//Hash[  2]: 873D3BF52629F7C1527C6E8E473C1C30
	//Hash[  3]: BCE50BEE7877BB07BB6FDA56BFE142FB
	//

	// File size       Data parts      ED2K parts      ED2K part hashs
	// ---------------------------------------------------------------
	// 1..PARTSIZE-1   1               1               0(!)
	// PARTSIZE        1               2(!)            2(!)
	// PARTSIZE+1      2               2               2
	// PARTSIZE*2      2               3(!)            3(!)
	// PARTSIZE*2+1    3               3               3

	if (nFileSize == 0){
		//wxASSERT(0); // Kry - Why commented out by lemonfan? it can never be 0
		m_iPartCount = 0;
		m_iED2KPartCount = 0;
		m_iED2KPartHashCount = 0;
		return;
	}

	// nr. of data parts
	m_iPartCount = ((uint64)nFileSize + (PARTSIZE - 1)) / PARTSIZE;

	// nr. of parts to be used with OP_FILESTATUS
	m_iED2KPartCount = nFileSize / PARTSIZE + 1;
	wxASSERT(m_iED2KPartCount <= 441);
	// nr. of parts to be used with OP_HASHSETANSWER
	m_iED2KPartHashCount = nFileSize / PARTSIZE;
	if (m_iED2KPartHashCount != 0)
		m_iED2KPartHashCount += 1;
}


// needed for memfiles. its probably better to switch everything to CFile...
bool CKnownFile::LoadHashsetFromFile(const CFileDataIO* file, bool checkhash){
	CMD4Hash checkid;
	file->ReadHash16(checkid);
	
	uint16 parts = file->ReadUInt16();
	
	
	for (uint16 i = 0; i < parts; i++){
		CMD4Hash cur_hash;
		file->ReadHash16(cur_hash);
		hashlist.Add(cur_hash);
	}
	
	// SLUGFILLER: SafeHash - always check for valid hashlist
	if (!checkhash){
		m_abyFileHash = checkid;
		if (parts <= 1) {	// nothing to check
			return true;
		}
	} else {
		if ( m_abyFileHash != checkid ) {
			return false;	// wrong file?
		} else {
			if (parts != GetED2KPartHashCount()) {
				return false;
			}
		}
	}
	// SLUGFILLER: SafeHash
	
	// trust noone ;-)
	// lol, useless comment but made me lmao

	if (!hashlist.IsEmpty()){
		uchar* buffer = new uchar[hashlist.GetCount()*16];
		for (size_t i = 0;i != hashlist.GetCount();i++) {
			md4cpy(buffer+(i*16),hashlist[i]);
		}
		CreateHashFromString(buffer,hashlist.GetCount()*16,checkid);
		delete[] buffer;
	}
	if ( m_abyFileHash == checkid ) {
		return true;
	} else {
		hashlist.Clear();
		/*
		for (int i = 0; i < hashlist.GetCount(); i++) {
			delete[] hashlist[i];
		}
		hashlist.RemoveAll();
		*/
		return false;
	}
}

bool CKnownFile::LoadTagsFromFile(const CFileDataIO* file)
{
	try {
		uint32 tagcount;
		tagcount = file->ReadUInt32();
		for (uint32 j = 0; j != tagcount;j++){
			CTag newtag(*file, true);
			switch(newtag.GetNameID()){
				case FT_FILENAME:{
					#if wxUSE_UNICODE
					if (GetFileName().IsEmpty())
					#endif
						SetFileName(newtag.GetStr());
					break;
				}
				case FT_FILESIZE:{
					SetFileSize(newtag.GetInt());
					m_AvailPartFrequency.Clear();
					m_AvailPartFrequency.Add(0, GetPartCount());
					break;
				}
				case FT_ATTRANSFERED:{
					statistic.alltimetransferred += newtag.GetInt();
					break;
				}
				case FT_ATTRANSFEREDHI:{
					statistic.alltimetransferred =
						(((uint64)newtag.GetInt()) << 32) +
						((uint64)statistic.alltimetransferred);
					break;
				}
				case FT_ATREQUESTED:{
					statistic.alltimerequested = newtag.GetInt();
					break;
				}
 				case FT_ATACCEPTED:{
					statistic.alltimeaccepted = newtag.GetInt();
					break;
				}
				case FT_ULPRIORITY:{
					m_iUpPriority = newtag.GetInt();
					if( m_iUpPriority == PR_AUTO ){
						m_iUpPriority = PR_HIGH;
						m_bAutoUpPriority = true;
					}
					else {
						if (	m_iUpPriority != PR_VERYLOW &&
							m_iUpPriority != PR_LOW &&
							m_iUpPriority != PR_NORMAL &&
							m_iUpPriority != PR_HIGH &&
							m_iUpPriority != PR_VERYHIGH &&
							m_iUpPriority != PR_POWERSHARE) {
							m_iUpPriority = PR_NORMAL;
						}					
						m_bAutoUpPriority = false;
					}
					break;
				}
				case FT_PERMISSIONS:{
					// Ignore it, it's not used anymore.
					break;
				}
				case FT_AICH_HASH: {
					CAICHHash hash;
					bool hashSizeOk =
						hash.DecodeBase32(newtag.GetStr()) == CAICHHash::GetHashSize();
					wxASSERT(hashSizeOk);
					if (hashSizeOk) {
						m_pAICHHashSet->SetMasterHash(hash, AICH_HASHSETCOMPLETE);
					}
					break;
				}				
				default:
					// Store them here and write them back on saving.
					taglist.Add(new CTag(newtag));
			}	
		}
		return true;
	} catch (...) {
		printf("Caught exception in CKnownFile::LoadTagsFromFile!\n");
		return false;
	}
}

bool CKnownFile::LoadDateFromFile(const CFileDataIO* file){
	date = file->ReadUInt32();
	return true;
}

bool CKnownFile::LoadFromFile(const CFileDataIO* file){
	// SLUGFILLER: SafeHash - load first, verify later
	bool ret1 = LoadDateFromFile(file);
	bool ret2 = LoadHashsetFromFile(file,false);
	bool ret3 = LoadTagsFromFile(file);
	UpdatePartsInfo();
	return ret1 && ret2 && ret3 && GetED2KPartHashCount()==GetHashCount();// Final hash-count verification, needs to be done after the tags are loaded.
	// SLUGFILLER: SafeHash
}


bool CKnownFile::WriteToFile(CFileDataIO* file){
	// date
	file->WriteUInt32(date); 
	// hashset
	file->WriteHash16(m_abyFileHash);
	
	uint16 parts = hashlist.GetCount();
	file->WriteUInt16(parts);

	for (int i = 0; i < parts; i++)
		file->WriteHash16(hashlist[i]);
	
	//tags
	const int iFixedTags = 7;
	uint32 tagcount = iFixedTags;
	if (m_pAICHHashSet->HasValidMasterHash() && (m_pAICHHashSet->GetStatus() == AICH_HASHSETCOMPLETE || m_pAICHHashSet->GetStatus() == AICH_VERIFIED)) {	
		tagcount++;
	}
	// Float meta tags are currently not written. All older eMule versions < 0.28a have 
	// a bug in the meta tag reading+writing code. To achive maximum backward 
	// compatibility for met files with older eMule versions we just don't write float 
	// tags. This is OK, because we (eMule) do not use float tags. The only float tags 
	// we may have to handle is the '# Sent' tag from the Hybrid, which is pretty 
	// useless but may be received from us via the servers.
	// 
	// The code for writing the float tags SHOULD BE ENABLED in SOME MONTHS (after most 
	// people are using the newer eMule versions which do not write broken float tags).	
	for (size_t j = 0; j < taglist.GetCount(); j++){
		if (taglist[j]->IsInt() || taglist[j]->IsStr()) {
			tagcount++;
		}
	}
	
	#if wxUSE_UNICODE
	++tagcount;
	#endif
	
	// standard tags

	file->WriteUInt32(tagcount);
	
	#if wxUSE_UNICODE
	CTag nametag_unicode(FT_FILENAME, GetFileName());
	// We write it with BOM to kep eMule compatibility
	nametag_unicode.WriteTagToFile(file,utf8strOptBOM);	
	#endif
	
	CTag nametag(FT_FILENAME, GetFileName());
	nametag.WriteTagToFile(file);
	
	CTag sizetag(FT_FILESIZE, m_nFileSize);
	sizetag.WriteTagToFile(file);
	
	// statistic
	uint32 tran;
	tran=statistic.alltimetransferred;
	CTag attag1(FT_ATTRANSFERED, tran);
	attag1.WriteTagToFile(file);
	
	tran=statistic.alltimetransferred>>32;
	CTag attag4(FT_ATTRANSFEREDHI, tran);
	attag4.WriteTagToFile(file);

	CTag attag2(FT_ATREQUESTED, statistic.GetAllTimeRequests());
	attag2.WriteTagToFile(file);
	
	CTag attag3(FT_ATACCEPTED, statistic.GetAllTimeAccepts());
	attag3.WriteTagToFile(file);

	// priority N permission
	CTag priotag(FT_ULPRIORITY, IsAutoUpPriority() ? PR_AUTO : m_iUpPriority);
	priotag.WriteTagToFile(file);

	//AICH Filehash
	if (m_pAICHHashSet->HasValidMasterHash() && (m_pAICHHashSet->GetStatus() == AICH_HASHSETCOMPLETE || m_pAICHHashSet->GetStatus() == AICH_VERIFIED)){
		CTag aichtag(FT_AICH_HASH, m_pAICHHashSet->GetMasterHash().GetString());
		aichtag.WriteTagToFile(file);
	}

	//other tags
	for (size_t j = 0; j < taglist.GetCount(); j++){
		if (taglist[j]->IsInt() || taglist[j]->IsStr()) {
			taglist[j]->WriteTagToFile(file);
		}
	}
	return true;
}

#ifndef _AFX
#ifndef VERIFY
#define VERIFY(x) (void(x))
#endif //VERIFY
#endif //_AFX

void CKnownFile::CreateHashFromInput(CFile* file, uint32 Length, uchar* Output, uchar* in_string, CAICHHashTree* pShaHashOut) const
{
	wxASSERT( Output != NULL || pShaHashOut != NULL);

	CFile* data = NULL;
	
	bool delete_in_string = false;
	
	if (file) {
		wxASSERT(!in_string);
		in_string = new unsigned char[Length]; 	 
		file->Read(in_string,Length); 	 
		delete_in_string = true;
	}
	
	data = new CMemFile(in_string,Length);
		
	uint32 Required = Length;
	uchar   X[64*128];
	
	uint32	posCurrentEMBlock = 0;
	uint32	nIACHPos = 0;
	CAICHHashAlgo* pHashAlg = m_pAICHHashSet->GetNewHashAlgo();

	// This is all AICH.
	
	while (Required >= 64){
		uint32 len = Required / 64; 
		if (len > sizeof(X)/(64 * sizeof(X[0]))) {
			len = sizeof(X)/(64 * sizeof(X[0])); 
		}
		data->Read(&X,len*64);

		// SHA hash needs 180KB blocks
		if (pShaHashOut != NULL){
			if (nIACHPos + len*64 >= EMBLOCKSIZE){
				uint32 nToComplete = EMBLOCKSIZE - nIACHPos;
				pHashAlg->Add(X, nToComplete);
				wxASSERT( nIACHPos + nToComplete == EMBLOCKSIZE );
				pShaHashOut->SetBlockHash(EMBLOCKSIZE, posCurrentEMBlock, pHashAlg);
				posCurrentEMBlock += EMBLOCKSIZE;
				pHashAlg->Reset();
				pHashAlg->Add(X+nToComplete,(len*64) - nToComplete);
				nIACHPos = (len*64) - nToComplete;
			}
			else{
				pHashAlg->Add(X, len*64);
				nIACHPos += len*64;
			}
		}

		Required -= len*64;
	}
	// bytes to read
	Required = Length % 64;
	if (Required != 0){
		data->Read(&X,Required);

		if (pShaHashOut != NULL){
			if (nIACHPos + Required >= EMBLOCKSIZE){
				uint32 nToComplete = EMBLOCKSIZE - nIACHPos;
				pHashAlg->Add(X, nToComplete);
				wxASSERT( nIACHPos + nToComplete == EMBLOCKSIZE );
				pShaHashOut->SetBlockHash(EMBLOCKSIZE, posCurrentEMBlock, pHashAlg);
				posCurrentEMBlock += EMBLOCKSIZE;
				pHashAlg->Reset();
				pHashAlg->Add(X+nToComplete, Required - nToComplete);
				nIACHPos = Required - nToComplete;
			}
			else{
				pHashAlg->Add(X, Required);
				nIACHPos += Required;
			}

		}
	}
	if (pShaHashOut != NULL){
		if(nIACHPos > 0){
			pShaHashOut->SetBlockHash(nIACHPos, posCurrentEMBlock, pHashAlg);
			posCurrentEMBlock += nIACHPos;
		}
		wxASSERT( posCurrentEMBlock == Length );
		VERIFY( pShaHashOut->ReCalculateHash(pHashAlg, false) );
	}

	if (Output != NULL){
		// MD4: use crypto.
	 
         CryptoPP::MD4 md4_hasher; 	 
  	 
         md4_hasher.CalculateDigest(Output,in_string,Length);		
		
	}
		
	delete pHashAlg;
	delete data;
	if (delete_in_string) {
		delete[] in_string;		
	}
}

const CMD4Hash& CKnownFile::GetPartHash(uint16 part) const {
	wxASSERT( part < hashlist.GetCount() );
		
	return hashlist[part];
}

CPacket*	CKnownFile::CreateSrcInfoPacket(const CUpDownClient* forClient)
{
	if (m_ClientUploadList.empty() )
		return NULL;


	CSafeMemFile data(1024);
	uint16 nCount = 0;

	data.WriteHash16(forClient->GetUploadFileID());
	data.WriteUInt16(nCount);
	uint32 cDbgNoSrc = 0;

	
	SourceSet::iterator it = m_ClientUploadList.begin();
	for ( ; it != m_ClientUploadList.end(); it++ ) {
		const CUpDownClient *cur_src = *it;
		
		if ( cur_src->HasLowID() || cur_src == forClient || !(cur_src->GetUploadState() == US_UPLOADING || cur_src->GetUploadState() == US_ONUPLOADQUEUE))
			continue;

		bool bNeeded = false;
		const BitVector& rcvstatus = forClient->GetUpPartStatus();

		if ( !rcvstatus.empty() ) {
			const BitVector& srcstatus = cur_src->GetUpPartStatus();
			if ( !srcstatus.empty() ) {
				if ( cur_src->GetUpPartCount() == forClient->GetUpPartCount() ) {
					for (int x = 0; x < GetPartCount(); x++ ) {
						if ( srcstatus[x] && !rcvstatus[x] ) {
							// We know the recieving client needs a chunk from this client.
							bNeeded = true;
							break;
						}
					}
				}
			} else {
				cDbgNoSrc++;
				// This client doesn't support upload chunk status. So just send it and hope for the best.
				bNeeded = true;
			}
		} else {
			// remote client does not support upload chunk status, search sources which have at least one complete part
			// we could even sort the list of sources by available chunks to return as much sources as possible which
			// have the most available chunks. but this could be a noticeable performance problem.
			const BitVector& srcstatus = cur_src->GetUpPartStatus();
			if ( !srcstatus.empty() ) {
				for (int x = 0; x < GetPartCount(); x++ ) {
					if ( srcstatus[x] ) {
						// this client has at least one chunk
						bNeeded = true;
						break;
					}
				}
			} else {
				// This client doesn't support upload chunk status. So just send it and hope for the best.
				bNeeded = true;
			}
		}

		if ( bNeeded ) {
			nCount++;
			uint32 dwID = cur_src->GetIP();
		    
			data.WriteUInt32(dwID);
		    data.WriteUInt16(cur_src->GetUserPort());
		    data.WriteUInt32(cur_src->GetServerIP());
		    data.WriteUInt16(cur_src->GetServerPort());
			
			if (forClient->GetSourceExchangeVersion() > 1)
			    data.WriteHash16(cur_src->GetUserHash());
			
			if (nCount > 500)
				break;
		}
	}
	
	if (!nCount)
		return 0;
	
	data.Seek(16, wxFromStart);
	data.WriteUInt16(nCount);

	CPacket* result = new CPacket(&data, OP_EMULEPROT);
	result->SetOpCode( OP_ANSWERSOURCES );
	
	if ( result->GetPacketSize() > 354 )
		result->PackPacket();
	
	return result;
}

// Updates priority of file if autopriority is activated
//void CKnownFile::UpdateUploadAutoPriority(void)
void CKnownFile::UpdateAutoUpPriority(void)
{
		if (!this->IsAutoUpPriority())
			return;

		if ( GetQueuedCount() > 20 ) {
			if ( GetUpPriority() != PR_LOW ) {
				SetUpPriority(PR_LOW, false);
				Notify_SharedFilesUpdateItem(this);
			}
			return;
		}

		if ( GetQueuedCount() > 1 ) {
			if ( GetUpPriority() != PR_NORMAL ) {
				SetUpPriority(PR_NORMAL, false);
				Notify_SharedFilesUpdateItem(this);
		}
		return;
	}
	if ( GetUpPriority() != PR_HIGH ) {
		SetUpPriority(PR_HIGH, false);
		Notify_SharedFilesUpdateItem(this);
	}
}

//For File Comment // 
void CKnownFile::LoadComment()
{
	wxString strCfgPath = wxT("/") + m_abyFileHash.Encode() + wxT("/");

	wxConfigBase* cfg = wxConfigBase::Get();
	
	m_strComment = cfg->Read( strCfgPath + wxT("Comment"), wxEmptyString);
	m_iRate = cfg->Read( strCfgPath + wxT("Rate"), 0l);
	m_bCommentLoaded = true;
}    

void CKnownFile::SetFileComment(const wxString& strNewComment)
{ 
	wxString strCfgPath = wxT("/") + m_abyFileHash.Encode() + wxT("/");

	wxConfigBase* cfg = wxConfigBase::Get();
	cfg->Write( strCfgPath + wxT("Comment"), strNewComment);
     
	m_strComment = strNewComment;
  
	SourceSet::iterator it = m_ClientUploadList.begin();
	for ( ; it != m_ClientUploadList.end(); it++ ) {
		(*it)->SetCommentDirty();
	}
}


// For File rate 
void CKnownFile::SetFileRate(int8 iNewRate)
{ 
	wxString strCfgPath = wxT("/") + m_abyFileHash.Encode() + wxT("/");
	wxConfigBase* cfg = wxConfigBase::Get();
	cfg->Write( strCfgPath + wxT("Rate"), iNewRate);
	m_iRate = iNewRate; 

	SourceSet::iterator it = m_ClientUploadList.begin();
	for ( ; it != m_ClientUploadList.end(); it++ ) {
		(*it)->SetCommentDirty();
	}
} 


void CKnownFile::SetUpPriority(uint8 iNewUpPriority, bool m_bsave){
	m_iUpPriority = iNewUpPriority;
	if( this->IsPartFile() && m_bsave )
		((CPartFile*)this)->SavePartFile();
}

void CKnownFile::SetPublishedED2K(bool val){
	m_PublishedED2K = val;
	Notify_SharedFilesUpdateItem(this);
}


void CKnownFile::UpdatePartsInfo()
{
	// Cache part count
	uint16 partcount = GetPartCount();
	bool flag = (time(NULL) - m_nCompleteSourcesTime > 0); 

	// Ensure the frequency-list is ready
	if ( m_AvailPartFrequency.GetCount() != GetPartCount() ) {
		m_AvailPartFrequency.Clear();

		m_AvailPartFrequency.Add( 0, GetPartCount() );
	}

	if (flag) {
		ArrayOfUInts16 count;	
		count.Alloc(m_ClientUploadList.size());	
		
		SourceSet::iterator it = m_ClientUploadList.begin();
		for ( ; it != m_ClientUploadList.end(); it++ ) {
			if ( !(*it)->GetUpPartStatus().empty() && (*it)->GetUpPartCount() == partcount ) {
				count.Add( (*it)->GetUpCompleteSourcesCount() );
			}
		}
	
		m_nCompleteSourcesCount = m_nCompleteSourcesCountLo = m_nCompleteSourcesCountHi = 0;

		if( partcount > 0) {
			m_nCompleteSourcesCount = m_AvailPartFrequency[0];
		}
		for (uint16 i = 1; i < partcount; i++) {
			if( m_nCompleteSourcesCount > m_AvailPartFrequency[i]) {
				m_nCompleteSourcesCount = m_AvailPartFrequency[i];
			}
		}
	
		count.Add(m_nCompleteSourcesCount);
		
		count.Shrink();
	
		int32 n = count.GetCount();	

		if (n > 0) {
			
			// Kry - Native wx functions instead
			count.Sort(Uint16CompareValues);
			
			// calculate range
			int i = n >> 1;			// (n / 2)
			int j = (n * 3) >> 2;	// (n * 3) / 4
			int k = (n * 7) >> 3;	// (n * 7) / 8

			//For complete files, trust the people your uploading to more...

			//For low guess and normal guess count
			//	If we see more sources then the guessed low and normal, use what we see.
			//	If we see less sources then the guessed low, adjust network accounts for 100%, we account for 0% with what we see and make sure we are still above the normal.
			//For high guess
			//  Adjust 100% network and 0% what we see.
			if (n < 20) {
				if ( count[i] < m_nCompleteSourcesCount ) {
					m_nCompleteSourcesCountLo = m_nCompleteSourcesCount;
				} else {
					m_nCompleteSourcesCountLo = count[i];
				}
				m_nCompleteSourcesCount= m_nCompleteSourcesCountLo;
				m_nCompleteSourcesCountHi = count[j];
				if( m_nCompleteSourcesCountHi < m_nCompleteSourcesCount ) {
					m_nCompleteSourcesCountHi = m_nCompleteSourcesCount;
				}
			} else {
			//Many sources..
			//For low guess
			//	Use what we see.
			//For normal guess
			//	Adjust network accounts for 100%, we account for 0% with what we see and make sure we are still above the low.
			//For high guess
			//  Adjust network accounts for 100%, we account for 0% with what we see and make sure we are still above the normal.

				m_nCompleteSourcesCountLo = m_nCompleteSourcesCount;
				m_nCompleteSourcesCount = count[j];
				if( m_nCompleteSourcesCount < m_nCompleteSourcesCountLo ) {
					m_nCompleteSourcesCount = m_nCompleteSourcesCountLo;
				}
				m_nCompleteSourcesCountHi= count[k];
				if( m_nCompleteSourcesCountHi < m_nCompleteSourcesCount ) {
					m_nCompleteSourcesCountHi = m_nCompleteSourcesCount;
				}
			}
		}
		m_nCompleteSourcesTime = time(NULL) + (60);
	}
	
	Notify_SharedFilesUpdateItem(this);
}


void CKnownFile::UpdateUpPartsFrequency( CUpDownClient* client, bool increment )
{
	const BitVector& freq = client->GetUpPartStatus();

	if ( m_AvailPartFrequency.GetCount() != GetPartCount() ) {
		m_AvailPartFrequency.Clear();

		m_AvailPartFrequency.Add( 0, GetPartCount() );
	
		if ( !increment ) {
			return;
		}
	}

	
	unsigned int size = freq.size();
	
	if ( size != m_AvailPartFrequency.GetCount() ) {
		return;
	}

	
	if ( increment ) {
		for ( unsigned int i = 0; i < size; i++ ) {
			if ( freq[i] ) {
				m_AvailPartFrequency[i]++;
			}
		}
	} else {
		for ( unsigned int i = 0; i < size; i++ ) {
			if ( freq[i] ) {
				m_AvailPartFrequency[i]--;
			}
		}
	}
}

#endif // CLIENT_GUI
