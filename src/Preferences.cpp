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


#ifdef HAVE_CONFIG_H
#include "config.h"		// Needed for PACKAGE_NAME and PACKAGE_STRING
#endif

#include <cstdio>
#include <cstdlib>
#include <clocale>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <wx/filename.h>
#include "otherfunctions.h"	// Needed for atoll
#include "ini2.h"			// Needed for CIni
#ifdef __WXMSW__
	#include <wx/msw/winundef.h>
#endif
#include "PrefsUnifiedDlg.h"			// Needed for Rse
#include "opcodes.h"		// Needed for PREFFILE_VERSION
#include "color.h"			// Needed for RGB

#include <wx/arrimpl.cpp> // this is a magic incantation which must be done!

WX_DEFINE_OBJARRAY(ArrayOfCategory_Struct);

/// new implementation
CPreferences::CPreferences()
{
	srand((uint32)time(0)); // we need random numbers sometimes
	prefs = new Preferences_Struct;	
	memset(prefs,0,sizeof(Preferences_Struct));
	prefsExt=new Preferences_Ext_Struct;
	
	memset(prefsExt,0,sizeof(Preferences_Ext_Struct));

	//get application start directory
	char buffer[490];

	// Use home directory to save preferences 
	snprintf(buffer,400,"%s/." PACKAGE_NAME,getenv("HOME"));

	if (!wxFileName::DirExists(buffer)) {
		wxFileName::Mkdir(buffer,0777);
	}
	strncat(buffer,"/",1);

	appdir = nstrdup(buffer);
	CreateUserHash();
	md4cpy(&prefs->userhash,&userhash);
	
	// load preferences.dat or set standart values
	char* fullpath = new char[strlen(appdir)+16];
	sprintf(fullpath,"%spreferences.dat",appdir);
	FILE* preffile = fopen(fullpath,"rb");
	delete[] fullpath;

	PrefsUnifiedDlg::BuildItemList(prefs, appdir);

	LoadPreferences();
	
	if (!preffile) {
		SetStandartValues();
		//if (Ask4RegFix(true)) Ask4RegFix(false);
	} else {
		fread(prefsExt,sizeof(Preferences_Ext_Struct),1,preffile);
		if (ferror(preffile)) {
			SetStandartValues();
		}
		// import old pref-files
		if (prefsExt->version<20) {
			if (prefsExt->version>17) { // v0.20b+
				prefsImport20b=new Preferences_Import20b_Struct;
				memset(prefsImport20b,0,sizeof(Preferences_Import20b_Struct));
				fseek(preffile,0,0);
				fread(prefsImport20b,sizeof(Preferences_Import20b_Struct),1,preffile);
				memcpy(&prefs->userhash,&prefsImport20b->userhash,16);
				memcpy(&prefs->incomingdir,&prefsImport20b->incomingdir,510);
				memcpy(&prefs->tempdir,&prefsImport20b->tempdir,510);
				sprintf(prefs->nick,"%s", prefsImport20b->nick);
				prefs->totalDownloadedBytes=prefsImport20b->totalDownloadedBytes;
				prefs->totalUploadedBytes=prefsImport20b->totalUploadedBytes;
			} else if (prefsExt->version>7) { // v0.20a
				prefsImport20a=new Preferences_Import20a_Struct;
				memset(prefsImport20a,0,sizeof(Preferences_Import20a_Struct));
				fseek(preffile,0,0);
				fread(prefsImport20a,sizeof(Preferences_Import20a_Struct),1,preffile);
				memcpy(&prefs->userhash,&prefsImport20a->userhash,16);
				memcpy(&prefs->incomingdir,&prefsImport20a->incomingdir,510);
				memcpy(&prefs->tempdir,&prefsImport20a->tempdir,510);
				sprintf(prefs->nick,"%s", prefsImport20a->nick);
				prefs->totalDownloadedBytes=prefsImport20a->totalDownloaded;
				prefs->totalUploadedBytes=prefsImport20a->totalUploaded;
			} else { //v0.19c-
				prefsImport19c=new Preferences_Import19c_Struct;
				memset(prefsImport19c,0,sizeof(Preferences_Import19c_Struct));
				fseek(preffile,0,0);
				fread(prefsImport19c,sizeof(Preferences_Import19c_Struct),1,preffile);
				if (prefsExt->version<3) {
					CreateUserHash();
					memcpy(&prefs->userhash,&userhash,16);
				} else {
					memcpy(&prefs->userhash,&prefsImport19c->userhash,16);
				}
				memcpy(&prefs->incomingdir,&prefsImport19c->incomingdir,510);memcpy(&prefs->tempdir,&prefsImport19c->tempdir,510);
				sprintf(prefs->nick,"%s",prefsImport19c->nick);
			}
 		} else {
			memcpy(&prefs->userhash,&prefsExt->userhash,16);
			prefs->EmuleWindowPlacement=prefsExt->EmuleWindowPlacement;
		}
		fclose(preffile);
		memcpy(&userhash,&prefs->userhash,16);
		prefs->smartidstate=0;
	}

	// shared directories
	fullpath = new char[strlen(appdir)+MAX_PATH]; // i_a
	sprintf(fullpath,"%sshareddir.dat",appdir);
	//CStdioFile* sdirfile = new CStdioFile();
	FILE* sdirfile=fopen(fullpath,"r");
	//if (sdirfile->Open(fullpath,CFile::modeRead)){
	if(sdirfile) {
		// CString toadd;
		char buffer[4096];
		while(!feof(sdirfile)) {
			memset(buffer,0,sizeof(buffer));
			fgets(buffer,sizeof(buffer)-1,sdirfile);
			char* ptr=strchr(buffer,'\n');
			if(ptr) {
				*ptr=0;
			}
			if(strlen(buffer)>1) {
				shareddir_list.Add(buffer);//new CString(buffer));
			}
		}
		fclose(sdirfile);
	}
	// delete[] sdirfile;
	delete[] fullpath;

	// serverlist adresses
	fullpath = new char[strlen(appdir)+20];
	sprintf(fullpath,"%saddresses.dat",appdir);
	// sdirfile = new CStdioFile();
	sdirfile=fopen(fullpath,"r");
	// if (sdirfile->Open(fullpath,CFile::modeRead)){
	if(sdirfile) {
		// CString toadd;
		char buffer[4096];
		while(!feof(sdirfile)) {
			memset(buffer,0,sizeof(buffer));
			fgets(buffer,sizeof(buffer)-1,sdirfile);
			char* ptr=strchr(buffer,'\n');
			if(ptr) {
				*ptr=0;
			}
			if(strlen(buffer)>1) {
				adresses_list.Append(new CString(buffer));
			}
		}
		fclose(sdirfile);
	}
	// delete[] sdirfile;
	delete[] fullpath;	
	fullpath=NULL;
	userhash[5] = 14;
	userhash[14] = 111;
	if (!wxFileName::DirExists(GetIncomingDir())) {
		wxFileName::Mkdir(GetIncomingDir(),0777);
	}
	if (!wxFileName::DirExists(GetTempDir())) {
		wxFileName::Mkdir(GetTempDir(),0777);
	}

	if (((int*)prefs->userhash)[0] == 0 && ((int*)prefs->userhash)[1] == 0 && ((int*)prefs->userhash)[2] == 0 && ((int*)prefs->userhash)[3] == 0) {
		CreateUserHash();
	}
}

void CPreferences::SetStandartValues()
{
	CreateUserHash();
	md4cpy(&prefs->userhash,&userhash);
	WINDOWPLACEMENT defaultWPM;
	defaultWPM.length = sizeof(WINDOWPLACEMENT);
	defaultWPM.rcNormalPosition.left=10;defaultWPM.rcNormalPosition.top=10;
	defaultWPM.rcNormalPosition.right=700;defaultWPM.rcNormalPosition.bottom=500;
	defaultWPM.showCmd=0;
	prefs->EmuleWindowPlacement=defaultWPM;
	prefs->versioncheckLastAutomatic=0;
	Save();
}

uint16 CPreferences::GetMaxDownload()
{
	return prefs->maxdownload;
}

bool CPreferences::Save()
{
	bool error = false;
	char* fullpath = new char[strlen(appdir)+MAX_PATH]; // i_a
	sprintf(fullpath,"%spreferences.dat",appdir);

	FILE* preffile = fopen(fullpath,"wb");
	prefsExt->version = PREFFILE_VERSION;

	if (preffile) {
		prefsExt->version=PREFFILE_VERSION;
		prefsExt->EmuleWindowPlacement=prefs->EmuleWindowPlacement;
		memcpy(&prefsExt->userhash,&prefs->userhash,16);
		error = fwrite(prefsExt,sizeof(Preferences_Ext_Struct),1,preffile);
		fclose(preffile);
	} else {
		error = true;
	}

	SavePreferences();
	delete[] fullpath;

	fullpath = new char[strlen(appdir)+14];
	sprintf(fullpath,"%sshareddir.dat",appdir);
	FILE* sdirfile=fopen(fullpath,"w");
	if(sdirfile) {
		for(unsigned int ii = 0; ii < shareddir_list.GetCount(); ++ii) {
			fprintf(sdirfile,"%s\n",shareddir_list[ii].GetData());
		}
		fclose(sdirfile);
	} else {
		error = true;
	}
	delete[] fullpath;
	if (!wxFileName::DirExists(GetIncomingDir())) {
		wxFileName::Mkdir(GetIncomingDir(),0777);
	}
	if (!wxFileName::DirExists(GetTempDir())) {
		wxFileName::Mkdir(GetTempDir(),0777);
	}
	return error;
}

void CPreferences::CreateUserHash()
{
	for (int i = 0;i != 8; i++) {
		uint16	random = rand();
		memcpy(&userhash[i*2],&random,2);
	}
	// mark as emule client. that will be need in later version
	userhash[5] = 14;
	userhash[14] = 111;
}

int32 CPreferences::GetColumnWidth(Table t, int index) const
{
	switch(t) {
	case tableDownload:
		return prefs->downloadColumnWidths[index];
	case tableUpload:
		return prefs->uploadColumnWidths[index];
	case tableQueue:
		return prefs->queueColumnWidths[index];
	case tableSearch:
		return prefs->searchColumnWidths[index];
	case tableShared:
		return prefs->sharedColumnWidths[index];
	case tableServer:
		return prefs->serverColumnWidths[index];
	case tableClientList:
		return prefs->clientListColumnWidths[index];
	case tableNone:
	default:
		return 0;
	}
}

void CPreferences::SetColumnWidth(Table t, int index, int32 width)
{
	switch(t) {
	case tableDownload:
		prefs->downloadColumnWidths[index] = width;
		break;
	case tableUpload:
		prefs->uploadColumnWidths[index] = width;
		break;
	case tableQueue:
		prefs->queueColumnWidths[index] = width;
		break;
	case tableSearch:
		prefs->searchColumnWidths[index] = width;
		break;
	case tableShared:
		prefs->sharedColumnWidths[index] = width;
		break;
	case tableServer:
		prefs->serverColumnWidths[index] = width;
		break;
	case tableClientList:
		prefs->clientListColumnWidths[index] = width;
		break;
	case tableNone:
	default:
		break;
	}
}

bool CPreferences::GetColumnHidden(Table t, int index) const
{
	switch(t) {
	case tableDownload:
		return prefs->downloadColumnHidden[index];
	case tableUpload:
		return prefs->uploadColumnHidden[index];
	case tableQueue:
		return prefs->queueColumnHidden[index];
	case tableSearch:
		return prefs->searchColumnHidden[index];
	case tableShared:
		return prefs->sharedColumnHidden[index];
	case tableServer:
		return prefs->serverColumnHidden[index];
	case tableClientList:
		return prefs->clientListColumnHidden[index];
	case tableNone:
	default:
		return FALSE;
	}
}

void CPreferences::SetColumnHidden(Table t, int index, bool bHidden)
{
	switch(t) {
	case tableDownload:
		prefs->downloadColumnHidden[index] = bHidden;
		break;
	case tableUpload:
		prefs->uploadColumnHidden[index] = bHidden;
		break;
	case tableQueue:
		prefs->queueColumnHidden[index] = bHidden;
		break;
	case tableSearch:
		prefs->searchColumnHidden[index] = bHidden;
		break;
	case tableShared:
		prefs->sharedColumnHidden[index] = bHidden;
		break;
	case tableServer:
		prefs->serverColumnHidden[index] = bHidden;
		break;
	case tableClientList:
		prefs->clientListColumnHidden[index] = bHidden;
		break;
	case tableNone:
	default:
		break;
	}
}

int32 CPreferences::GetColumnOrder(Table t, int index) const
{
	switch(t) {
	case tableDownload:
		return prefs->downloadColumnOrder[index];
	case tableUpload:
		return prefs->uploadColumnOrder[index];
	case tableQueue:
		return prefs->queueColumnOrder[index];
	case tableSearch:
		return prefs->searchColumnOrder[index];
	case tableShared:
		return prefs->sharedColumnOrder[index];
	case tableServer:
		return prefs->serverColumnOrder[index];
	case tableClientList:
		return prefs->clientListColumnOrder[index];
	case tableNone:
	default:
		return 0;
	}
}

void CPreferences::SetColumnOrder(Table t, INT *piOrder)
{
	switch(t) {
	case tableDownload:
		memcpy(prefs->downloadColumnOrder, piOrder, sizeof(prefs->downloadColumnOrder));
		break;
	case tableUpload:
		memcpy(prefs->uploadColumnOrder, piOrder, sizeof(prefs->uploadColumnOrder));
		break;
	case tableQueue:
		memcpy(prefs->queueColumnOrder, piOrder, sizeof(prefs->queueColumnOrder));
		break;
	case tableSearch:
		memcpy(prefs->searchColumnOrder, piOrder, sizeof(prefs->searchColumnOrder));
		break;
	case tableShared:
		memcpy(prefs->sharedColumnOrder, piOrder, sizeof(prefs->sharedColumnOrder));
		break;
	case tableServer:
		memcpy(prefs->serverColumnOrder, piOrder, sizeof(prefs->serverColumnOrder));
		break;
	case tableClientList:
		memcpy(prefs->clientListColumnOrder, piOrder, sizeof(prefs->clientListColumnOrder));
		break;
	case tableNone:
	default:
		break;
	}
}

CPreferences::~CPreferences()
{
	
	Category_Struct* delcat;
	while (!catMap.IsEmpty()) {
		delcat=catMap[0]; 
		catMap.RemoveAt(0); 
		delete delcat;
	}
	
	catMap.Clear();
	
	if (adresses_list.GetCount()>0) {
		// the 'true' tells wxList to do 'delete' on the nodes.
		adresses_list.DeleteContents(true); 
	}
	
	delete[] appdir;
	delete prefs;
	delete prefsExt;
}

int32 CPreferences::GetRecommendedMaxConnections()
{
	int iRealMax = ::GetMaxConnections();
	if(iRealMax == -1 || iRealMax > 520) {
		return 500;
	}
	if(iRealMax < 20) {
		return iRealMax;
	}
	if(iRealMax <= 256) {
		return iRealMax - 10;
	}
	return iRealMax - 20;
}

void CPreferences::SavePreferences()
{
	CString buffer;
	char* fullpath = new char[strlen(appdir)+MAX_PATH]; // i_a
	sprintf(fullpath,"%spreferences.ini",appdir);
	
	wxString fp(fullpath), bf("eMule");
	
	printf("Preferences saving in %s\n",fullpath);
	
	CIni ini(fp, bf);
	delete[] fullpath;
	fullpath=NULL;
	//---
	ini.WriteString("AppVersion", PACKAGE_STRING);
	//---

	PrefsUnifiedDlg::SaveAllItems(ini);

}

void CPreferences::SaveCats()
{
	CIni ini( wxString(appdir)+"preferences.ini" , "Category" );

	// Cats
	CString catinif,ixStr,buffer;
	catinif.Format("%sCategory.ini",appdir);
	if(wxFileName::FileExists(catinif)) {
		BackupFile(catinif, ".old");
	}
	if (GetCatCount()>0) {
		CIni catini( catinif, "Category" );
		printf("Opening %s\n",catinif.GetData());
		catini.WriteInt("Count",catMap.GetCount()-1,"General");
		for (size_t ix=1;ix<catMap.GetCount();ix++) {
			ixStr.Format("Cat#%i",ix);
			catini.WriteString("Title",catMap[ix]->title,(char*)ixStr.GetData());
			catini.WriteString("Incoming",catMap[ix]->incomingpath,(char*)ixStr.GetData());
			catini.WriteString("Comment",catMap[ix]->comment,(char*)ixStr.GetData());
			buffer.Format("%lu",(uint32)catMap[ix]->color);
			catini.WriteString("Color",buffer,(char*)ixStr.GetData());
			catini.WriteInt("Priority",catMap[ix]->prio,(char*)ixStr.GetData());
		}
	}
}


void CPreferences::LoadPreferences()
{
	//--- Quick hack to add version tag to preferences.ini-file and solve the issue with the FlatStatusBar tag...
	CString strFileName;
	strFileName.Format("%spreferences.ini", appdir);
	CIni* pIni = new CIni(strFileName, "eMule");

	CString strCurrVersion, strPrefsVersion;

	strCurrVersion = CURRENT_VERSION_LONG;
	strPrefsVersion = CString(pIni->GetString("AppVersion").GetData());
	delete pIni;
	prefs->m_bFirstStart = false;

	if ((strCurrVersion != strPrefsVersion) && wxFileName::FileExists(strFileName)) {
		// CFile file;
	  	BackupFile(strFileName, ".old");
		strFileName += ".old";
	}
	
	printf("Loading preferences from %s\n",strFileName.c_str());
	
	CIni ini(strFileName, "eMule");
	//--- end Ozon :)

	PrefsUnifiedDlg::LoadAllItems(ini);

	LoadCats();
	
}

void CPreferences::LoadCats() {
	CString ixStr,catinif,cat_a,cat_b,cat_c;
	char buffer[100];

	catinif.Format("%sCategory.ini",appdir);

	// default cat
	Category_Struct* newcat=new Category_Struct;
	newcat->title[0] = 0;
	newcat->incomingpath[0] = 0;
	newcat->comment[0] = 0;
	newcat->prio=0;
	newcat->color=0;
	AddCat(newcat);

	// if (!PathFileExists(catinif)) return;
	// surprise, surprise it won't exist
	// the system is now stupid enough to put everything in .eMule :(
	// if(!wxFileName::FileExists(catinif)) return;

	CIni catini( catinif, "Category" );
	int max=catini.GetInt("Count",0,"General");

	for (int ix=1;ix<=max;ix++) {
		ixStr.Format("Cat#%i",ix);

		Category_Struct* newcat=new Category_Struct;
		sprintf(newcat->title,"%s",catini.GetString("Title","",(char*)ixStr.GetData()).c_str());
		sprintf(newcat->incomingpath,"%s",catini.GetString("Incoming","",(char*)ixStr.GetData()).c_str());
		MakeFoldername(newcat->incomingpath);
		sprintf(newcat->comment,"%s",catini.GetString("Comment","",(char*)ixStr.GetData()).c_str());
		newcat->prio =catini.GetInt("Priority",0,(char*)ixStr.GetData());
		sprintf(buffer,"%s",catini.GetString("Color","0",(char*)ixStr.GetData()).c_str());
		newcat->color=atoll(buffer);

		AddCat(newcat);
		if (!wxFileName::DirExists(newcat->incomingpath)) {
			wxFileName::Mkdir(newcat->incomingpath,0777);
		}
	}
}

uint16 CPreferences::GetDefaultMaxConperFive()
{
	return MAXCONPER5SEC;
}


// Barry - Provide a mechanism for all tables to store/retrieve sort order
int32 CPreferences::GetColumnSortItem(Table t) const
{
	switch(t) {
		case tableDownload:
			return prefs->tableSortItemDownload;
		case tableUpload:
			return prefs->tableSortItemUpload;
		case tableQueue:
			return prefs->tableSortItemQueue;
		case tableSearch:
			return prefs->tableSortItemSearch;
		case tableShared:
			return prefs->tableSortItemShared;
		case tableServer:
			return prefs->tableSortItemServer;
		case tableClientList:
			return prefs->tableSortItemClientList;
		case tableNone:
		default:
			return 0;
	}
}

// Barry - Provide a mechanism for all tables to store/retrieve sort order
bool CPreferences::GetColumnSortAscending(Table t) const
{
	switch(t) {
		case tableDownload:
			return prefs->tableSortAscendingDownload;
		case tableUpload:
			return prefs->tableSortAscendingUpload;
		case tableQueue:
			return prefs->tableSortAscendingQueue;
		case tableSearch:
			return prefs->tableSortAscendingSearch;
		case tableShared:
			return prefs->tableSortAscendingShared;
		case tableServer:
			return prefs->tableSortAscendingServer;
		case tableClientList:
			return prefs->tableSortAscendingClientList;
		case tableNone:
		default:
			return true;
	}
}

// Barry - Provide a mechanism for all tables to store/retrieve sort order
void CPreferences::SetColumnSortItem(Table t, int32 sortItem)
{
	switch(t) {
		case tableDownload:
			prefs->tableSortItemDownload = sortItem;
			break;
		case tableUpload:
			prefs->tableSortItemUpload = sortItem;
			break;
		case tableQueue:
			prefs->tableSortItemQueue = sortItem;
			break;
		case tableSearch:
			prefs->tableSortItemSearch = sortItem;
			break;
		case tableShared:
			prefs->tableSortItemShared = sortItem;
			break;
		case tableServer:
			prefs->tableSortItemServer = sortItem;
			break;
		case tableClientList:
			prefs->tableSortItemClientList = sortItem;
			break;
		case tableNone:
		default:
			break;
	}
}

// Barry - Provide a mechanism for all tables to store/retrieve sort order
void CPreferences::SetColumnSortAscending(Table t, bool sortAscending)
{
	switch(t) {
		case tableDownload:
			prefs->tableSortAscendingDownload = sortAscending;
			break;
		case tableUpload:
			prefs->tableSortAscendingUpload = sortAscending;
			break;
		case tableQueue:
			prefs->tableSortAscendingQueue = sortAscending;
			break;
		case tableSearch:
			prefs->tableSortAscendingSearch = sortAscending;
			break;
		case tableShared:
			prefs->tableSortAscendingShared = sortAscending;
			break;
		case tableServer:
			prefs->tableSortAscendingServer = sortAscending;
			break;
		case tableClientList:
			prefs->tableSortAscendingClientList = sortAscending;
			break;
		case tableNone:
		default:
			break;
	}
}

void CPreferences::RemoveCat(size_t index)
{
	if (index>=0 && index<catMap.GetCount()) {
		catMap.RemoveAt(index);
	}
}
