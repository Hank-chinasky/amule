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

#ifndef SAFEFILE_H
#define SAFEFILE_H

#include "CFile.h"		// Needed for CFile
#include "types.h"		// Needed for LPCSTR
#include "CMemFile.h"		// Needed for CMemFile


namespace Kademlia{
	class CUInt128;
};


enum EUtf8Str
{
	utf8strNone,
	utf8strOptBOM,
	utf8strRaw
};

///////////////////////////////////////////////////////////////////////////////
class CFileDataIO
{
 public:
	virtual off_t Read(void *pBuf, off_t nCount) const = 0;
	virtual size_t Write(const void *pBuf, size_t nCount) = 0;
	virtual off_t GetPosition() const = 0;
	virtual off_t GetLength() const = 0;
 
	virtual uint8		ReadUInt8() const;
	virtual uint16		ReadUInt16() const;
	virtual uint32		ReadUInt32() const;
//	virtual void		ReadUInt128(Kademlia::CUInt128 *pVal) const;
	virtual void		ReadHash16(unsigned char* pVal) const;
#ifdef __NET_UNICODE__
 	virtual CString ReadString(bool bOptUTF8);
	virtual CString ReadString(bool bOptUTF8, UINT uRawSize);
	virtual CStringW ReadStringUTF8();
#else
 	virtual wxString	ReadString() const;
#endif

	virtual void WriteUInt8(uint8 nVal);
	virtual void WriteUInt16(uint16 nVal);
	virtual void WriteUInt32(uint32 nVal);
//	virtual void WriteUInt128(const Kademlia::CUInt128 *pVal);
	virtual void WriteHash16(const unsigned char* pVal);
#ifdef __NET_UNICODE__	
 	virtual void WriteString(const CString& rstr, EUtf8Str eEncode = utf8strNone);
	virtual void WriteString(LPCSTR psz);
	virtual void WriteLongString(const CString& rstr, EUtf8Str eEncode = utf8strNone);
	virtual void WriteLongString(LPCSTR psz);
#else
	virtual void WriteString(const wxString& rstr);
#endif
 };
 


///////////////////////////////////////////////////////////////////////////////
class CSafeFile : public CFile, public CFileDataIO
{
 public:
	CSafeFile() {}
	CSafeFile(const wxChar* lpszFileName, OpenMode mode = read)
		: CFile(lpszFileName, mode) {}

	virtual off_t Read(void *pBuf, off_t nCount) const;
	virtual size_t Write(const void *pBuf, size_t nCount);
	virtual off_t GetPosition() const {
		return CFile::GetPosition();
	}
	virtual off_t GetLength() const {
		return CFile::GetLength();
	}
	virtual off_t Seek(off_t lOff, CFile::SeekMode nFrom = CFile::start) const {
		return CFile::Seek(lOff, nFrom);
	}
	
 };
 


///////////////////////////////////////////////////////////////////////////////
class CSafeMemFile : public CMemFile, public CFileDataIO
{
public:
	CSafeMemFile(UINT nGrowBytes = 512)
		: CMemFile(nGrowBytes) {}
	CSafeMemFile(BYTE* lpBuffer, UINT nBufferSize, UINT nGrowBytes = 0)
		: CMemFile(lpBuffer, nBufferSize, nGrowBytes) {}

	// CMemFile already does the needed checks
	virtual off_t Read(void *pBuf, off_t nCount) const {
		return CMemFile::Read( pBuf, nCount );
	}
	
	virtual size_t Write(const void *pBuf, size_t nCount) {
		return CMemFile::Write( pBuf, nCount );
	}

	virtual off_t Seek(off_t lOff, wxSeekMode from = wxFromStart) {
		return CMemFile::Seek(lOff, from);
	}
	
	virtual off_t GetPosition() const {
		return CMemFile::GetPosition();
	}
	virtual off_t GetLength() const {
		return CMemFile::GetLength();
	}

	virtual uint8		ReadUInt8() const;
	virtual uint16		ReadUInt16() const;
	virtual uint32		ReadUInt32() const;
//	virtual void		ReadUInt128(Kademlia::CUInt128 *pVal) const;
	virtual void		ReadHash16(unsigned char* pVal) const;
	
	virtual void WriteUInt8(uint8 nVal);
	virtual void WriteUInt16(uint16 nVal);
	virtual void WriteUInt32(uint32 nVal);
//	virtual void WriteUInt128(const Kademlia::CUInt128 *pVal);
	virtual void WriteHash16(const unsigned char* pVal);

};



///////////////////////////////////////////////////////////////////////////////
// This is just a workaround
class CSafeBufferedFile : public CFile, public CFileDataIO
{
 public:
	CSafeBufferedFile() {}
	CSafeBufferedFile(const wxChar* lpszFileName, OpenMode mode = read)
		: CFile(lpszFileName, mode) {}

	virtual off_t Read(void *pBuf, off_t nCount) const;
	virtual size_t Write(const void *pBuf, size_t nCount);
	virtual off_t Seek(off_t lOff, CFile::SeekMode nFrom = CFile::start) {
		return CFile::Seek(lOff, nFrom);
	}
	virtual off_t GetPosition() const {
		return CFile::GetPosition();
	}
	virtual off_t GetLength() const {
		return CFile::GetLength();
	}
};
 


///////////////////////////////////////////////////////////////////////////////
// Peek - helper functions for read-accessing memory without modifying the memory pointer

__inline uint8 PeekUInt8(const void* p)
{
	return *((uint8*)p);
}

__inline uint16 PeekUInt16(const void* p)
{
	return *((uint16*)p);
}

__inline uint32 PeekUInt32(const void* p)
{
	return *((uint32*)p);
}


///////////////////////////////////////////////////////////////////////////////
// Poke - helper functions for write-accessing memory without modifying the memory pointer

__inline void PokeUInt8(void* p, uint8 nVal)
{
	*((uint8*)p) = nVal;
}

__inline void PokeUInt16(void* p, uint16 nVal)
{
	*((uint16*)p) = nVal;
}

__inline void PokeUInt32(void* p, uint32 nVal)
{
	*((uint32*)p) = nVal;
}


#endif // SAFEFILE_H
