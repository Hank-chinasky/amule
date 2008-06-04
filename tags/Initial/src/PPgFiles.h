// This file is part of the aMule project.
//
// Copyright (c) 2003,
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

#ifndef PPGFILES_H
#define PPGFILES_H

#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/panel.h>		// Needed for wxPanel

#include "resource.h"		// Needed for IDD_PPG_FILES

class CPreferences;

class CPPgFiles : public wxPanel //CPropertyPage
{
  //DECLARE_DYNAMIC(CPPgFiles)
  DECLARE_DYNAMIC_CLASS(CPPgFiles)
    CPPgFiles() {};

public:
	CPPgFiles(wxWindow* parent);
	virtual ~CPPgFiles();

	void SetPrefs(CPreferences* in_prefs) {	app_prefs = in_prefs; }

// Dialog Data
	enum { IDD = IDD_PPG_FILES };
protected:
	CPreferences *app_prefs;
protected:
	//virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	//DECLARE_MESSAGE_MAP()
public:
	//virtual bool OnInitDialog();
 public:
	void LoadSettings(void);
public:
#if 0
	virtual bool OnApply();
	afx_msg void OnBnClickedSeeshare1()	{ SetModified(); }
	afx_msg void OnBnClickedSeeshare2()	{ SetModified(); }
	afx_msg void OnBnClickedSeeshare3()	{ SetModified(); }
	afx_msg void OnBnClickedIch()		{ SetModified(); }
#endif
	virtual void OnApply();

	void Localize(void);
};

#endif // PPGFILES_H