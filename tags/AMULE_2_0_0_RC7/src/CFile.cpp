// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
//
/////////////////////////////////////////////////////////////////////////////
// Name:        file.cpp
// Purpose:     wxFile - encapsulates low-level "file descriptor"
//              wxTempFile
// Author:      Vadim Zeitlin
// Modified by:
// Created:     29/01/98
// RCS-ID:      $Id$
// Copyright:   (c) 1998 Vadim Zeitlin <zeitlin@dptmaths.ens-cachan.fr>
// Licence:     wxWindows license
/////////////////////////////////////////////////////////////////////////////

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

#ifdef __WXMAC__
	#include <wx/wx.h>
#endif
#include <unistd.h>		// Needed for close(2)

#include "CFile.h"		// Interface declarations.

#include "amule.h"		// Needed for theApp

#include "Preferences.h"	// Needed for CPreferences

#include "otherfunctions.h" // unicode2char

#ifdef HAVE_CONFIG_H
#include "config.h"             // Needed for HAVE_SYS_PARAM_H
#endif

// Test if we have _GNU_SOURCE before the next step will mess up 
// setting __USE_GNU 
// (only needed for gcc-2.95 compatibility, gcc 3.2 always defines it)
#include <wx/setup.h>

// Mario Sergio Fujikawa Ferreira <lioux@FreeBSD.org>
// to detect if this is a *BSD system
#if defined(HAVE_SYS_PARAM_H)
#include <sys/param.h>
#endif

#ifdef __BORLANDC__
#	pragma hdrstop
#endif

#include <wx/log.h>

// standard
#if defined(__WXMSW__) && !defined(__GNUWIN32__) && !defined(__WXWINE__) && !defined(__WXMICROWIN__)
#	include <io.h>

#ifndef __SALFORDC__
#	define   WIN32_LEAN_AND_MEAN
#	define   NOSERVICE
#	define   NOIME
#	define   NOATOM
#	define   NOGDI
#	define   NOGDICAPMASKS
#	define   NOMETAFILE
#	define   NOMINMAX
#	define   NOMSG
#	define   NOOPENFILE
#	define   NORASTEROPS
#	define   NOSCROLL
#	define   NOSOUND
#	define   NOSYSMETRICS
#	define   NOTEXTMETRIC
#	define   NOWH
#	define   NOCOMM
#	define   NOKANJI
#	define   NOCRYPT
#	define   NOMCX
#endif

#elif (defined(__UNIX__) || defined(__GNUWIN32__))
#	include <unistd.h>
#	ifdef __GNUWIN32__
#		include <windows.h>
#	endif
#elif defined(__DOS__)
#	if defined(__WATCOMC__)
#		include <io.h>
#	elif defined(__DJGPP__)
#		include <io.h>
#		include <unistd.h>
#		include <cstdio>
#	else
#		error  "Please specify the header with file functions declarations."
#	endif
#elif (defined(__WXPM__))
#	include <io.h>
#elif (defined(__WXSTUBS__))
	// Have to ifdef this for different environments
#	include <io.h>
#elif (defined(__WXMAC__))
#if __MSL__ < 0x6000
int access( const char *path, int mode ) { return 0 ; }
#else
int _access( const char *path, int mode ) { return 0 ; }
#endif
char* mktemp( char * path ) { return path ;}
#	include <stat.h>
#	include <unistd.h>
#else
#	error  "Please specify the header with file functions declarations."
#endif  //Win/UNIX

#include <cstdio>       // SEEK_xxx constants
#include <fcntl.h>       // O_RDONLY &c

#if !defined(__MWERKS__) || defined(__WXMSW__)
#	include <sys/types.h>   // needed for stat
#	include <sys/stat.h>    // stat
#endif

// Windows compilers don't have these constants
#ifndef W_OK
enum {
	F_OK = 0,   // test for existence
	X_OK = 1,   //          execute permission
	W_OK = 2,   //          write
	R_OK = 4    //          read
};
#endif // W_OK

// there is no distinction between text and binary files under Unix, so define
// O_BINARY as 0 if the system headers don't do it already
#if defined(__UNIX__) && !defined(O_BINARY)
#	define   O_BINARY    (0)
#endif  //__UNIX__

#ifdef __SALFORDC__
#	include <unix.h>
#endif

// some broken compilers don't have 3rd argument in open() and creat()
#ifdef __SALFORDC__
#	define ACCESS(access)
#	define stat    _stat
#else // normal compiler
#	define ACCESS(access)  , (access)
#endif // Salford C

// wxWindows
#ifndef WX_PRECOMP
#	include <wx/string.h>
#	include <wx/intl.h>
#	include <wx/log.h>
#endif // !WX_PRECOMP

#ifdef __WXMSW__
#include <wx/msw/mslu.h>
#endif
#include <wx/filename.h>
#include <wx/filefn.h>

//#define FILE_TRACKER

#ifdef FILE_TRACKER
	#include <wx/event.h>
	#include "GuiEvents.h"
	#include <unistd.h>       
	#ifndef __WXMAC__
		#include <execinfo.h>
	#endif

	void get_caller(int value) {
#ifndef __WXMAC__
		void *bt_array[4];	
		char **bt_strings;
		int num_entries;
	
		if ((num_entries = backtrace(bt_array, 6)) < 0) {
			theApp.QueueLogLine(false, _("* Could not generate backtrace\n"));
		} else {
			if ((bt_strings = backtrace_symbols(bt_array, num_entries)) == NULL) {
				theApp.QueueLogLine(false, _("* Could not get symbol names for backtrace\n"));
			}  else {
				wxString wherefrom = bt_strings[value];
				int starter = wherefrom.Find('(');
				int ender = wherefrom.Find(')');
				wherefrom = wherefrom.Mid(starter, ender-starter+1);
				theApp.QueueLogLine(false, _("Called From: ") + wherefrom);
			}
		}	
#endif
}


#endif


// ============================================================================
// implementation of CFile
// ============================================================================

// ----------------------------------------------------------------------------
// static functions
// ----------------------------------------------------------------------------

bool CFile::Exists(const wxChar *name)
{
	return wxFileExists(name);
}

bool CFile::Access(const wxChar *name, OpenMode mode)
{
	int how;

	switch ( mode )
	{
		default:
			wxFAIL_MSG(wxT("bad CFile::Access mode parameter."));
			// fall through
		case read:
			how = R_OK;
			break;

		case write:
			how = W_OK;
			break;
		case read_write:
			how = R_OK | W_OK;
			break;
	}
	return wxAccess(name, how) == 0;
}

// ----------------------------------------------------------------------------
// opening/closing
// ----------------------------------------------------------------------------

// ctors
CFile::CFile(const wxChar *szFileName, OpenMode mode)
{
	m_fd = fd_invalid;
	m_error = FALSE;

	#ifdef FILE_TRACKER
		Open(szFileName, mode,12345);
	#else
		Open(szFileName, mode);
	#endif
}

// create the file, fail if it already exists and bOverwrite
bool CFile::Create(const wxChar *szFileName, bool bOverwrite, int accessMode)
{
	if ( accessMode == -1 )
		accessMode = CPreferences::GetFilePermissions();

	fFilePath=szFileName;

    if (m_fd != fd_invalid) {
	    Close();	
    }

	// if bOverwrite we create a new file or truncate the existing one,
	// otherwise we only create the new file and fail if it already exists
#if defined(__WXMAC__) && !defined(__UNIX__)
	// Dominic Mazzoni [dmazzoni+@cs.cmu.edu] reports that open is still broken on the mac, so we replace
	// int fd = open(wxUnix2MacFilename( szFileName ), O_CREAT | (bOverwrite ? O_TRUNC : O_EXCL), access);
	m_fd = creat( szFileName , accessMode);
#else
	m_fd = wxOpen( szFileName,
			O_BINARY | O_WRONLY | O_CREAT |
			(bOverwrite ? O_TRUNC : O_EXCL)
			ACCESS(accessMode) );
#endif
	
	#ifdef FILE_TRACKER
		AddLogLineM(false,wxString(_("Created file ")) + fFilePath + wxString::Format(_(" with file descriptor %i"),m_fd));
    		get_caller(2);
	#endif
	
	if ( m_fd == -1 ) {
		wxLogSysError(_("can't create file '%s'"), szFileName);
		return FALSE;
	} else {
		//Attach(m_fd);
		return TRUE;
	}
}

// open the file
bool CFile::Open(const wxChar *szFileName, OpenMode mode, int accessMode)
{
	if ( accessMode == -1 )
		accessMode = CPreferences::GetFilePermissions();

    int flags = O_BINARY;

    fFilePath=szFileName;

	#ifdef FILE_TRACKER
		bool fromConstructor = false;
		if (accessMode == 12345) {
			fromConstructor = true;
			accessMode = wxS_DEFAULT;
		} 
    #endif
    
    switch ( mode )
    {
        case read:
            flags |= O_RDONLY;
            break;

        case write_append:
            if ( CFile::Exists(szFileName) )
            {
                flags |= O_WRONLY | O_APPEND;
                break;
            }
            //else: fall through as write_append is the same as write if the
            //      file doesn't exist

        case write:
            flags |= O_WRONLY | O_CREAT | O_TRUNC;
            break;

        case write_excl:
            flags |= O_WRONLY | O_CREAT | O_EXCL;
            break;

        case read_write:
            flags |= O_RDWR;
            break;
    }

    if (m_fd != fd_invalid) {
	    Close();	
    }

    m_fd = wxOpen( szFileName, flags ACCESS(accessMode));
      
	#ifdef FILE_TRACKER
		theApp.QueueLogLine(false,wxString(_("Opened file ")) + fFilePath  + wxString::Format(_(" with file descriptor %i"),m_fd));
    		if (fromConstructor) {
			get_caller(3);    
		} else {
			get_caller(3);    
		}
	#endif
    
    if ( m_fd == -1 )
    {
   		theApp.QueueLogLine(true, wxString::Format(_("Can't open file '%s'"), szFileName));
		/*
			get_caller(4);    	    
			get_caller(3);    
			get_caller(2);    
	    */
        	return FALSE;
    }
       else {
   //     Attach(m_fd);
        return TRUE;
    }
    
}

// close
bool CFile::Close() 
{

	#ifdef FILE_TRACKER
		AddLogLineM(false,wxString(_("Closing file ")) + fFilePath + wxString::Format(_(" with file descriptor %i"),m_fd));
		get_caller(2);
		wxASSERT(!fFilePath.IsEmpty());
	#endif

	if ( IsOpened() ) {
        if ( close(m_fd) == -1 ) {
            wxLogSysError(_("can't close file descriptor %d"), m_fd);
            m_fd = fd_invalid;
            return FALSE;
        }
        else
            m_fd = fd_invalid;
    } else wxASSERT(0);

    return TRUE;
}

// ----------------------------------------------------------------------------
// read/write
// ----------------------------------------------------------------------------

// read
off_t CFile::Read(void *pBuf, off_t nCount) const
{
    wxCHECK( (pBuf != NULL) && IsOpened(), 0 );

#ifdef __MWERKS__
    off_t iRc = ::read(m_fd, (char*) pBuf, nCount);
#else
    off_t iRc = ::read(m_fd, pBuf, nCount);
#endif
    if ( iRc == -1 ) {
        wxLogSysError(_("can't read from file descriptor %d"), m_fd);
	    m_error = TRUE;
        return wxInvalidOffset;
    }
    else {
		return (off_t)iRc;
    }
}

// write
size_t CFile::Write(const void *pBuf, size_t nCount)
{
    wxCHECK( (pBuf != NULL) && IsOpened(), 0 );

#ifdef __MWERKS__
#if __MSL__ >= 0x6000
    size_t iRc = ::write(m_fd, (void*) pBuf, nCount);
#else
    size_t iRc = ::write(m_fd, (const char*) pBuf, nCount);
#endif
#else
    size_t iRc = ::write(m_fd, pBuf, nCount);
#endif
    if ( ((int)iRc) == -1 ) {
        wxLogSysError(_("can't write to file descriptor %d"), m_fd);
        m_error = TRUE;
        return (size_t)0;
    }
    else
        return iRc;
}

// flush
bool CFile::Flush()
{
    if ( IsOpened() ) {
		#ifdef __WXMSW__
		if (_commit(m_fd) == -1)
		#else
	        if ( fsync(m_fd) == -1 ) 
		#endif
		{
		    wxLogSysError(_("can't flush file descriptor %d"), m_fd);
	    		m_error = TRUE;			
	            return FALSE;
		}
    }
    return TRUE;
}

// ----------------------------------------------------------------------------
// seek
// ----------------------------------------------------------------------------
#include <cerrno>
#include <cstring>

// seek
off_t CFile::Seek(off_t ofs, CFile::SeekMode mode) const
{
	wxASSERT( IsOpened() );

	int origin;
	switch ( mode ) {
	default:
		wxFAIL_MSG(_("unknown seek origin"));

	case wxFromStart:
		origin = SEEK_SET;
		break;

	case wxFromCurrent:
		origin = SEEK_CUR;
		break;

	case wxFromEnd:
		origin = SEEK_END;
		break;
	}

	off_t iRc = lseek(m_fd, ofs, origin);
	if ( iRc == -1 ) {
		printf("Error in lseek: %s\n", strerror(errno));
		m_error = TRUE;
		return wxInvalidOffset;
	} else {
		return (off_t) iRc;
	}
}

// get current off_t
off_t CFile::GetPosition() const
{
    wxASSERT( IsOpened() );

    off_t iRc = wxTell(m_fd);
    if ( iRc == -1 ) {
        wxLogSysError(_("can't get seek position on file descriptor %d"), m_fd);
        return wxInvalidOffset;
    }
    else
        return (off_t)iRc;
}

// get current file length
off_t CFile::Length() const
{
    wxASSERT( IsOpened() );

#ifdef __VISUALC__
    off_t iRc = _filelength(m_fd);
#else // !VC++
    off_t iRc = wxTell(m_fd);
    if ( iRc != -1 ) {
        // @ have to use const_cast :-(
        off_t iLen = ((CFile *)this)->SeekEnd();
        if ( iLen != -1 ) {
            // restore old position
            if ( ((CFile *)this)->Seek(iRc) == -1 ) {
                // error
                iLen = -1;
            }
        }

        iRc = iLen;
    }
#endif  // VC++

    if ( iRc == -1 ) {
        wxLogSysError(_("can't find length of file on file descriptor %d"), m_fd);
        return wxInvalidOffset;
    }
    else
        return (off_t)iRc;
}
bool CFile::SetLength(off_t new_len) {

	#ifdef __WXMSW__
	return chsize(this->fd(), new_len);
	#else
	return ftruncate(this->fd(), new_len);
	#endif
}	

// is end of file reached?
bool CFile::Eof() const
{
    wxASSERT( IsOpened() );

    off_t iRc;

#if defined(__DOS__) || defined(__UNIX__) || defined(__GNUWIN32__) || defined( __MWERKS__ ) || defined(__SALFORDC__)
    // @@ this doesn't work, of course, on unseekable file descriptors
    off_t ofsCur = GetPosition(),
    ofsMax = Length();
    if ( ofsCur == wxInvalidOffset || ofsMax == wxInvalidOffset )
        iRc = -1;
    else
        iRc = ofsCur == ofsMax;
#else  // Windows and "native" compiler
    iRc = eof(m_fd);
#endif // Windows/Unix

    switch ( iRc ) {
        case 1:
            break;

        case 0:
            return FALSE;

        case -1:
            wxLogSysError(_("can't determine if the end of file is reached on descriptor %d"), m_fd);
                break;

        default:
            wxFAIL_MSG(_("invalid eof() return value."));
    }

    return TRUE;
}


CDirIterator::CDirIterator(wxString dir) {
	DirStr = dir;
	if (DirStr.Last() != wxFileName::GetPathSeparator()) {
		DirStr += wxFileName::GetPathSeparator();
	}
	
	if (((DirPtr = opendir(unicode2char(dir)))) == NULL) {
		theApp.QueueLogLine(true, wxT("Error enumerating files for dir ")+dir);
    }
}

CDirIterator::~CDirIterator() {	
	closedir (DirPtr);
}

wxString CDirIterator::FindFirstFile(FileType search_type, wxString search_mask) {
	seekdir(DirPtr, 0);// 2 if we want to skip . and ..
	FileMask = search_mask;
	type = search_type;
	return FindNextFile();
}

wxString  CDirIterator::FindNextFile() {
	struct dirent *dp;
	dp = readdir(DirPtr);

	bool found = false;
	
	wxString FoundName;
	
	struct stat* buf=(struct stat*)malloc(sizeof(struct stat));
	
	while (dp!=NULL && !found) {
		if ((type == CDirIterator::Any)) {
			// return anything.
			found = true;
		} else {		
			switch (dp->d_type) {
				case DT_DIR:
					if (type == CDirIterator::Dir)  {
						found = true;
					} else {
						dp = readdir(DirPtr);	
					}
					break;
				case DT_REG:
					if (type == CDirIterator::File)  {
						found = true;
					} else {
						dp = readdir(DirPtr);					
					}
					break;
				default:
					// Fallback to stat
					stat(unicode2char(DirStr + char2unicode(dp->d_name)),buf);
					if (S_ISREG(buf->st_mode)) {
						if (type == CDirIterator::File) { 
							found = true; 
						} else { 
							dp = readdir(DirPtr);
						} 
					} else {
						if (S_ISDIR(buf->st_mode)) {
							if (type == CDirIterator::Dir) {
								found = true; 
							} else { 
								dp = readdir(DirPtr);
							}
						} else {						
							// unix socket, block device, etc
							dp = readdir(DirPtr);
						}
					}
					break;
			}
		}
		if (found) {
			FoundName = char2unicode(dp->d_name);
			if (
				(!FileMask.IsEmpty() && !FoundName.Matches(FileMask)) 
				|| FoundName.IsSameAs(wxT(".")) || FoundName.IsSameAs(wxT(".."))) {
				found = false;	
				dp = readdir(DirPtr);
			}
		}
	}
			
	free(buf);
	
	if (dp!=NULL) {
		return DirStr + FoundName;	
	} else {
		return wxEmptyString;
	}
}