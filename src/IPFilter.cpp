//this file is part of aMule
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


#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/intl.h>		// Needed for _

#include "IPFilter.h"		// Interface declarations.
#include "amuleDlg.h"		// Needed for CamuleDlg
#include "Preferences.h"	// Needed for CPreferences
#include "CamuleAppBase.h"	// Needed for theApp

#include <wx/arrimpl.cpp> // this is a magic incantation which must be done!

WX_DEFINE_OBJARRAY(ArrayOfIPRange_Struct);

CIPFilter::CIPFilter(){
	lasthit="";
	LoadFromFile();
}

CIPFilter::~CIPFilter(){
	RemoveAllIPs();
}

void CIPFilter::Reload(){
	wxMutexLocker lock(s_IPfilter_Data_mutex);
	RemoveAllIPs();
	lasthit="";
	LoadFromFile();
}

void CIPFilter::AddBannedIPRange(uint32 IPfrom,uint32 IPto,uint8 filter, CString desc){
	IPRange_Struct* newFilter=new IPRange_Struct();
	IPRange_Struct* search;
	bool inserted=false;

	newFilter->IPstart=IPfrom;
	newFilter->IPend=IPto;
	newFilter->filter=filter;
	newFilter->description=desc;

	if (iplist.GetCount()==0) iplist.Insert(newFilter,0); else {
		for (size_t i=0;i<iplist.GetCount();i++) {
			search=iplist[i];
			if (search->IPstart>IPfrom) {
				iplist.Insert(newFilter,i);
				inserted=true;
				break; 
			}
		}
		if (!inserted) iplist.Add(newFilter);
	}
}

#if 0 // incomplete function
wxString Tokenize(wxString input,wxString token,int startat)
{
  wxString tmp=input.Right(input.Length()-startat);
  int pos=tmp.Find(token);
  if(pos>=0) {
    
  }
}
#endif

int CIPFilter::LoadFromFile(){
	wxString sbuffer,sbuffer2,sbuffer3,sbuffer4;
	int pos,filtercounter;
	uint32 ip1,ip2;
	uint8 filter;
	char buffer[1024];
	int lenBuf = 1024;
	filtercounter=0;

	RemoveAllIPs();

	FILE* readFile= fopen(wxString(theApp.glob_prefs->GetAppDir())+"ipfilter.dat", "r");
	if (readFile!=NULL) {
		while (!feof(readFile)) {
			if (fgets(buffer,lenBuf,readFile)==0) break;
			sbuffer=buffer;
			
			// ignore comments & too short lines
			if (sbuffer.GetChar(0) == '#' || sbuffer.GetChar(0) == '/' || sbuffer.Length()<5)
				continue;
			
			// get & test & process IP range
			pos=sbuffer.Find(",");
			if (pos==-1) continue;
			sbuffer2=sbuffer.Left(pos).Trim();
			pos=sbuffer2.Find("-");
			if (pos==-1) continue;
			sbuffer3=sbuffer2.Left(pos).Trim();
			sbuffer4=sbuffer2.Right(sbuffer2.Length()-pos-1).Trim();

			ip1=ip2=0;
			CString temp;
			bool skip=false;

			unsigned int s3[4];
			unsigned int s4[4];
			memset(s3,0,sizeof(s3));
			memset(s4,0,sizeof(s4));
			sscanf(sbuffer3.GetData(),"%d.%d.%d.%d",&s3[0],&s3[1],&s3[2],&s3[3]);
			sscanf(sbuffer4.GetData(),"%d.%d.%d.%d",&s4[0],&s4[1],&s4[2],&s4[3]);

			if ((s3[0]==s3[1]==s3[2]==s3[3]==0) || (s4[0]==s4[1]==s4[2]==s4[3]==0)) {
				skip=true;
			}

			for(int i=0;i<4;i++) {
				ip1|=(uint32)(s3[i]<<(8*(3-i)));
				ip2|=(uint32)(s4[i]<<(8*(3-i)));
			}
#if 0
			for( int i = 0; i < 4; i++){
			  //sbuffer3.Tokenize(".",counter);
			  Tokenize(sbuffer3,".",counter);
			  if( counter == -1 ){ skip=true;break;}
			}
			counter=0;
			for( int i = 0; i < 4; i++){
			  //sbuffer4.Tokenize(".",counter);
			  Tokenize(sbuffer4,counter);
			  if( counter == -1 ){ skip=true;break;}
			}
			if (skip) continue;
			
			counter=0;
			for(int i=0; i<4; i++){ 
			  temp = Tokenize(sbuffer3,".",counter);//sbuffer3.Tokenize(".",counter);
			  ip1 |= (uint32) (atoi(temp) << (8 * (3-i)));
			  temp = Tokenize(sbuffer4,".",counter);//sbuffer4.Tokenize(".",counter);
			  ip2 |= (uint32)(atoi(temp) << (8*(3-i)));
			}
#endif

			// filter
			bool found=false;
			for(unsigned int m = pos + 1; m < sbuffer.Length(); ++m) {
			  if(sbuffer.GetChar(m)==',') {
			    pos=m;
			    found=true;
			    break;
			  }
			}
			if(!found) pos=-1;
			//pos=sbuffer.Find(",",pos+1);
			//int pos2=sbuffer.Find(",",pos+1);
			int pos2 = (-1);
			found=false;
			for(unsigned int m = pos + 1; m < sbuffer.Length(); ++m) {
			  if(sbuffer.GetChar(m)==',') {
			    pos2=m;
			    found=true;
			    break;
			  }
			}

			if (pos2==-1) continue;
			sbuffer2=sbuffer.Mid(pos+1,pos2-pos-1).Trim();
			filter=atoi(sbuffer2);

			sbuffer2=sbuffer.Right(sbuffer.Length()-pos2-1);
			if (sbuffer2.GetChar(sbuffer2.Length()-1)==10) sbuffer2.Remove(sbuffer2.Length()-1);

			// add a filter
			AddBannedIPRange(ip1,ip2,filter,CString(sbuffer2.GetData()) );
			filtercounter++;

		}
		fclose(readFile);
	}
	theApp.amuledlg->AddLogLine(true, CString(_("Loaded ipfilter with %d IP addresses.")),filtercounter);
	return filtercounter;
}

void CIPFilter::SaveToFile(){
}

void CIPFilter::RemoveAllIPs(){
	IPRange_Struct* search;
	while (iplist.GetCount()>0) {
        search=iplist[0];
		iplist.RemoveAt(0);
		delete search;
	}
}

bool CIPFilter::IsFiltered(uint32 IP2test){
	if ((iplist.GetCount()==0) || (!theApp.glob_prefs->GetIPFilterOn()) ) return false;

	//CSingleLock(&m_Mutex,TRUE); // will be unlocked on exit

	IPRange_Struct* search;
	uint32 IP2test_=(uint8)(IP2test>>24) | (uint8)(IP2test>>16)<<8 | (uint8)(IP2test>>8)<<16 | (uint8)(IP2test)<<24 ;

	uint16 lo=0;
	uint16 hi=iplist.GetCount()-1;
	uint16 mi;

	while (true) {
		mi=((hi-lo)/2) +lo;
		search=iplist[mi];
		if (search->IPstart<=IP2test_ && search->IPend>=IP2test_ ) {
			if (search->filter<theApp.glob_prefs->GetIPFilterLevel() ) {
				lasthit=search->description;
				return true;
			}
			return false;
		}
		if (lo==hi) return false;
		if (IP2test_<search->IPstart) hi=mi; 
			else lo=mi+1;
	}
	return false;
}
