//
// This file is part of the aMule Project.
//
// Copyright (c) 2004-2005 aMule Team ( http://www.amule.org )
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
#pragma implementation "ECPacket.h"
#pragma implementation "ECcodes.h"
#pragma implementation "CMD4Hash.h"
#endif

#include "ECPacket.h"	// Needed for ECTag, ECPacket
#include "ECSocket.h"	// Needed for ECSocket
#include <stdlib.h>	// Needed for malloc(), realloc(), free(), NULL
#include <string.h>	// Needed for memcpy(), strlen()
#include "CMD4Hash.h"	// Needed for CMD4Hash

/**********************************************************
 *							  *
 *	CECTag class					  *
 *							  *
 **********************************************************/

//! Defines the Null tag which may be returned by GetTagByNameSafe.
const CECTag CECTag::s_theNullTag(static_cast<NullTagConstructorSelector*>(0));

//! Defines the data for the Null tag.  Large enough (16 bytes) for GetMD4Data.
const uint32 CECTag::s_theNullTagData[4] = { 0, 0, 0, 0 };

/**
 * Creates a new null-valued CECTag instance
 *
 * @see s_theNullTag
 * @see s_theNullTagData
 * @see GetTagByNameSafe
 */
CECTag::CECTag(const NullTagConstructorSelector*) :
	m_error(0),
	m_tagData(s_theNullTagData),
	m_tagName(0),
	m_dataLen(0),
	m_dynamic(false),
	m_tagList()
{
}

/**
 * Creates a new CECTag instance from the given data
 *
 * @param name	 TAG name
 * @param length length of data buffer
 * @param data	 TAG data
 * @param copy	 whether to create a copy of the TAG data at \e *data, or should use the provided pointer.
 *
 * \note When you set \e copy to \b false, the provided data buffer must exist
 * in the whole lifetime of the packet.
 */
CECTag::CECTag(ec_tagname_t name, unsigned int length, const void *data, bool copy) : m_tagName(name), m_dynamic(copy)
{
	m_error = 0;
	m_dataLen = length;
	if (copy && (data != NULL)) {
		m_tagData = malloc(m_dataLen);
		if (m_tagData != NULL) {
			memcpy((void *)m_tagData, data, m_dataLen);
		} else {
			m_dataLen = 0;
			m_error = 1;
		}
	} else {
		m_tagData = data;
	}
}

/**
 * Creates a new CECTag instance for custom data
 *
 * @param name	 	TAG name
 * @param length 	length of data buffer that will be alloc'ed
 * @param dataptr	pointer to internal TAG data buffer
 *
 * 
 */
CECTag::CECTag(ec_tagname_t name, unsigned int length, void **dataptr)  : m_tagName(name), m_dynamic(true)
{
	m_error = 0;
	m_dataLen = length;
	m_tagData = malloc(m_dataLen);
	if ( !m_tagData ) {
		m_dataLen = 0;
		m_error = 1;
	}
	*dataptr = (void *)m_tagData;
}

/**
 * Creates a new CECTag instance, which contains an IPv4 address.
 *
 * This function takes care of the endianness of the port number.
 *
 * @param name TAG name
 * @param data The EC_IPv4_t class containing the IPv4 address.
 *
 * @see GetIPv4Data()
 */
CECTag::CECTag(ec_tagname_t name, const EC_IPv4_t& data) : m_tagName(name), m_dynamic(true)
{

	m_dataLen = sizeof(EC_IPv4_t);
	m_tagData = malloc(sizeof(EC_IPv4_t));
	if (m_tagData != NULL) {
		RawPokeUInt32( ((EC_IPv4_t *)m_tagData)->ip, RawPeekUInt32( data.ip ) );
		((EC_IPv4_t *)m_tagData)->port = ENDIAN_HTONS(data.port);
		m_error = 0;
	} else {
		m_error = 1;
	}
}

/**
 * Creates a new CECTag instance, which contains a MD4 hash.
 *
 * This function takes care to store hash in network byte order.
 *
 * @param name TAG name
 * @param data The CMD4Hash class containing the MD4 hash.
 *
 * @see GetMD4Data()
 */
CECTag::CECTag(ec_tagname_t name, const CMD4Hash& data) : m_tagName(name), m_dynamic(true)
{

	m_dataLen = 16;
	m_tagData = malloc(16);
	if (m_tagData != NULL) {
		RawPokeUInt64( (char*)m_tagData,		RawPeekUInt64( data.GetHash() ) );
		RawPokeUInt64( (char*)m_tagData + 8,	RawPeekUInt64( data.GetHash() + 8 ) );
		m_error = 0;
	} else {
		m_error = 1;
	}
}

/**
 * Creates a new CECTag instance, which contains a string
 *
 * @param name TAG name
 * @param data wxString object, it's contents are converted to UTF-8.
 *
 * @see GetStringData()
 */
CECTag::CECTag(ec_tagname_t name, const wxString& data) : m_tagName(name), m_dynamic(true)
{
	const wxCharBuffer buf = wxConvUTF8.cWC2MB(data.wc_str(aMuleConv));
	const char *utf8 = (const char *)buf;

	m_dataLen = strlen(utf8) + 1;
	m_tagData = malloc(m_dataLen);
	if (m_tagData != NULL) {
		memcpy((void *)m_tagData, utf8, m_dataLen);
		m_error = 0;
	} else {
		m_error = 1;
	}
}

/**
 * Copy constructor
 */
CECTag::CECTag(const CECTag& tag) : m_tagName( tag.m_tagName ), m_dynamic( tag.m_dynamic )
{
	m_error = 0;
	m_dataLen = tag.m_dataLen;
	if (m_dataLen != 0) {
		if (m_dynamic) {
			m_tagData = malloc(m_dataLen);
			if (m_tagData != NULL) {
				memcpy((void *)m_tagData, tag.m_tagData, m_dataLen);
			} else {
				m_dataLen = 0;
				m_error = 1;
				return;
			}
		} else {
			m_tagData = tag.m_tagData;
		}
	} else m_tagData = NULL;
	if (!tag.m_tagList.empty()) {
		m_tagList.reserve(tag.m_tagList.size());
		for (TagList::size_type i=0; i<tag.m_tagList.size(); i++) {
			m_tagList.push_back(tag.m_tagList[i]);
			if (m_tagList.back().m_error != 0) {
				m_error = m_tagList.back().m_error;
#ifndef KEEP_PARTIAL_PACKETS
				m_tagList.pop_back();
#endif
				break;
			}
		}
	}
}

/**
 * Creates a new CECTag instance, which contains an uint8 value.
 *
 * This takes care of endianness problems with numbers.
 *
 * @param name TAG name.
 * @param data uint8 number.
 *
 * @see GetInt8Data()
 */
CECTag::CECTag(ec_tagname_t name, uint8 data) : m_tagName(name), m_dynamic(true)
{
	m_dataLen = 1;
	m_tagData = malloc(m_dataLen);
	if (m_tagData != NULL) {
		PokeUInt8( (void*)m_tagData, data );
		m_error = 0;
	} else {
		m_error = 1;
	}
}

/**
 * Creates a new CECTag instance, which contains an uint16 value.
 *
 * This takes care of endianness problems with numbers.
 *
 * @param name TAG name.
 * @param data uint16 number.
 *
 * @see GetInt16Data()
 */
CECTag::CECTag(ec_tagname_t name, uint16 data) : m_tagName(name), m_dynamic(true)
{
	m_dataLen = 2;
	m_tagData = malloc(m_dataLen);
	if (m_tagData != NULL) {
		RawPokeUInt16( (void*)m_tagData, ENDIAN_HTONS( data ) );
		m_error = 0;
	} else {
		m_error = 1;
	}
}

/**
 * Creates a new CECTag instance, which contains an uint32 value.
 *
 * This takes care of endianness problems with numbers.
 *
 * @param name TAG name.
 * @param data uint32 number.
 *
 * @see GetInt32Data()
 */
CECTag::CECTag(ec_tagname_t name, uint32 data) : m_tagName(name), m_dynamic(true)
{
	m_dataLen = 4;
	m_tagData = malloc(m_dataLen);
	if (m_tagData != NULL) {
		RawPokeUInt32( (void*)m_tagData, ENDIAN_HTONL( data ) );
		m_error = 0;
	} else {
		m_error = 1;
	}
}

/**
 * Creates a new CECTag instance, which contains an uint64 value.
 *
 * This takes care of endianness problems with numbers.
 *
 * @param name TAG name.
 * @param data uint64 number.
 *
 * @see GetInt64Data()
 */
CECTag::CECTag(ec_tagname_t name, uint64 data) : m_tagName(name), m_dynamic(true)
{
	m_dataLen = 8;
	m_tagData = malloc(m_dataLen);
	uint32 low = data & 0xffffffff;
	uint32 high = data >> 32;
	if (m_tagData != NULL) {
		RawPokeUInt32( (void *)m_tagData, ENDIAN_HTONL( high ) );
		RawPokeUInt32( ((unsigned char*)m_tagData) + sizeof(uint32), ENDIAN_HTONL( low ) );
		m_error = 0;
	} else {
		m_error = 1;
	}
}

/**
 * Destructor - frees allocated data and deletes child TAGs.
 */
CECTag::~CECTag(void)
{
	if (m_dynamic) free((void *)m_tagData);
}

/**
 * Copy assignment operator.
 *
 * std::vector uses this, but the compiler-supplied version wouldn't properly
 * handle m_dynamic and m_tagData.  This wouldn't be necessary if m_tagData
 * was a smart pointer (Hi, Kry!).
 */
CECTag& CECTag::operator=(const CECTag& rhs)
{
	if (&rhs != this)
	{
		// This is a trick to reuse the implementation of the copy constructor
		// so we don't have to duplicate it here.  temp is constructed as a
		// copy of rhs, which properly handles m_dynamic and m_tagData.  Then
		// temp's members are swapped for this object's members.  So,
		// effectively, this object has been made a copy of rhs, which is the
		// point.  Then temp is destroyed as it goes out of scope, so its
		// destructor cleans up whatever data used to belong to this object.
		CECTag temp(rhs);
		std::swap(m_error,	temp.m_error);
		std::swap(m_tagData,	temp.m_tagData);
		std::swap(m_tagName,	temp.m_tagName);
		std::swap(m_dataLen,	temp.m_dataLen);
		std::swap(m_dynamic,	temp.m_dynamic);
		std::swap(m_tagList,	temp.m_tagList);
	}

	return *this;
}

/**
 * Add a child tag to this one.
 *
 * Be very careful that this creates a copy of \e tag. Thus, the following code won't work as expected:
 * \code
 * {
 *	CECPacket *p = new CECPacket(whatever);
 *	CECTag *t1 = new CECTag(whatever);
 *	CECTag *t2 = new CECTag(whatever);
 *	p.AddTag(*t1);
 *	t1.AddTag(*t2);	// t2 won't be part of p !!!
 * }
 * \endcode
 *
 * To get the desired results, the above should be replaced with something like:
 *
 * \code
 * {
 *	CECPacket *p = new CECPacket(whatever);
 *	CECTag *t1 = new CECTag(whatever);
 *	CECTag *t2 = new CECTag(whatever);
 *	t1.AddTag(*t2);
 *	delete t2;	// we can safely delete t2 here, because t1 holds a copy
 *	p.AddTag(*t1);
 *	delete t1;	// now p holds a copy of both t1 and t2
 * }
 * \endcode
 *
 * Then why copying? The answer is to enable simplifying the code like this:
 *
 * \code
 * {
 *	CECPacket *p = new CECPacket(whatever);
 *	CECTag t1(whatever);
 *	t1.AddTag(CECTag(whatever));	// t2 is now created on-the-fly
 *	p.AddTag(t1);	// now p holds a copy of both t1 and t2
 * }
 * \endcode
 *
 * @param tag a CECTag class instance to add.
 * @return \b true on succcess, \b false when an error occured
 */
bool CECTag::AddTag(const CECTag& tag)
{
	// cannot have more than 64k tags
	wxASSERT(m_tagList.size() < 0xffff);

	m_tagList.push_back(tag);
	if (m_tagList.back().m_error == 0) {
		return true;
	} else {
		m_error = m_tagList.back().m_error;
#ifndef KEEP_PARTIAL_PACKETS
		m_tagList.pop_back();
#endif
		return false;
	}
}

CECTag::CECTag(wxSocketBase *sock, ECSocket& socket, void *opaque) : m_dynamic(true)
{
	ec_taglen_t tagLen;
	ec_tagname_t tmp_tagName;

	m_tagData = NULL;
	m_dataLen = 0;
	if (!socket.ReadNumber(sock, &tmp_tagName, sizeof(ec_tagname_t), opaque)) {
		m_error = 2;
		m_tagName = 0;
		return;
	}
	m_tagName = tmp_tagName >> 1;
	if (!socket.ReadNumber(sock, &tagLen, sizeof(ec_taglen_t), opaque)) {
		m_error = 2;
		return;
	}
	if (tmp_tagName & 0x01) {
		if (!ReadChildren(sock, socket, opaque)) {
			return;
		}
	}
	m_dataLen = tagLen - GetTagLen();
	if (m_dataLen > 0) {
		m_tagData = malloc(m_dataLen);
		if (m_tagData != NULL) {
			if (!socket.ReadBuffer(sock, (void *)m_tagData, m_dataLen, opaque)) {
				m_error = 2;
				return;
			}
		} else {
			m_error = 1;
			return;
		}
	} else {
		m_tagData = NULL;
	}
	m_error = 0;
}


bool CECTag::WriteTag(wxSocketBase *sock, ECSocket& socket, void *opaque) const
{
	ec_taglen_t tagLen = GetTagLen();
	ec_tagname_t tmp_tagName = (m_tagName << 1) | (m_tagList.empty() ? 0 : 1);
	
	if (!socket.WriteNumber(sock, &tmp_tagName, sizeof(ec_tagname_t), opaque)) return false;
	if (!socket.WriteNumber(sock, &tagLen, sizeof(ec_taglen_t), opaque)) return false;
	if (!m_tagList.empty()) {
		if (!WriteChildren(sock, socket, opaque)) return false;
	}
	if (m_dataLen > 0) {
		if (m_tagData != NULL) {	// This is here only to make sure everything, it should not be NULL at this point
			if (!socket.WriteBuffer(sock, m_tagData, m_dataLen, opaque)) return false;
		}
	}
	return true;
}


bool CECTag::ReadChildren(wxSocketBase *sock, ECSocket& socket, void *opaque)
{
	uint16 tmp_tagCount;

	if (!socket.ReadNumber(sock, &tmp_tagCount, 2, opaque)) {
		m_error = 2;
		return false;
	}

	m_tagList.clear();
	if (tmp_tagCount > 0) {
		m_tagList.reserve(tmp_tagCount);
		for (int i=0; i<tmp_tagCount; i++) {
			m_tagList.push_back(CECTag(sock, socket, opaque));
			if (m_tagList.back().m_error != 0) {
				m_error = m_tagList.back().m_error;
#ifndef KEEP_PARTIAL_PACKETS
				m_tagList.pop_back();
#endif
				return false;
			}
		}
	}
	return true;
}


bool CECTag::WriteChildren(wxSocketBase *sock, ECSocket& socket, void *opaque) const
{
    uint16 tmp = m_tagList.size();
	if (!socket.WriteNumber(sock, &tmp, sizeof(tmp), opaque)) return false;
	if (!m_tagList.empty()) {
		for (TagList::size_type i=0; i<m_tagList.size(); i++) {
			if (!m_tagList[i].WriteTag(sock, socket, opaque)) return false;
		}
	}
	return true;
}

/**
 * Finds the (first) child tag with given name.
 *
 * @param name TAG name to look for.
 * @return the tag found, or NULL.
 */
const CECTag* CECTag::GetTagByName(ec_tagname_t name) const
{
	for (TagList::size_type i=0; i<m_tagList.size(); i++)
		if (m_tagList[i].m_tagName == name) return &m_tagList[i];
	return NULL;
}

/**
 * Finds the (first) child tag with given name.
 *
 * @param name TAG name to look for.
 * @return the tag found, or NULL.
 */
CECTag* CECTag::GetTagByName(ec_tagname_t name)
{
	for (TagList::size_type i=0; i<m_tagList.size(); i++)
		if (m_tagList[i].m_tagName == name) return &m_tagList[i];
	return NULL;
}

/**
 * Finds the (first) child tag with given name.
 *
 * @param name TAG name to look for.
 * @return the tag found, or a special null-valued tag otherwise.
 *
 * @see s_theNullTag
 */
const CECTag* CECTag::GetTagByNameSafe(ec_tagname_t name) const
{
	const CECTag* result = GetTagByName(name);
	if (result == NULL)
		result = &s_theNullTag;
	return result;
}

/**
 * Query TAG length that is suitable for the TAGLEN field (i.e.\ 
 * without it's own header size).
 *
 * @return Tag length, containing its childs' length.
 */
uint32 CECTag::GetTagLen(void) const
{
	uint32 length = m_dataLen;
	for (TagList::size_type i=0; i<m_tagList.size(); i++) {
		length += m_tagList[i].GetTagLen();
		length += sizeof(ec_tagname_t) + sizeof(ec_taglen_t) + ((m_tagList[i].GetTagCount() > 0) ? 2 : 0);
	}
	return length;
}

/**
 * Returns an EC_IPv4_t class.
 *
 * This function takes care of the enadianness of the port number.
 *
 * @return EC_IPv4_t class.
 *
 * @see CECTag(ec_tagname_t, const EC_IPv4_t&)
 */
EC_IPv4_t CECTag::GetIPv4Data(void) const
{
	EC_IPv4_t p;

	RawPokeUInt32( p.ip, RawPeekUInt32( ((EC_IPv4_t *)m_tagData)->ip ) );
	p.port = ENDIAN_NTOHS(((EC_IPv4_t *)m_tagData)->port);

	return p;
}

/*!
 * \fn CMD4Hash CECTag::GetMD4Data(void) const
 *
 * \brief Returns a CMD4Hash class.
 *
 * This function takes care of converting from MSB to LSB as necessary.
 *
 * \return CMD4Hash class.
 *
 * \sa CECTag(ec_tagname_t, const CMD4Hash&)
 */

/*!
 * \fn CECTag *CECTag::GetTagByIndex(unsigned int index) const
 *
 * \brief Finds the indexth child tag.
 *
 * \param index 0-based index, 0 <= \e index < GetTagCount()
 *
 * \return The child tag, or NULL if index out of range.
 */

/*!
 * \fn CECTag *CECTag::GetTagByIndexSafe(unsigned int index) const
 *
 * \brief Finds the indexth child tag.
 *
 * \param index 0-based index, 0 <= \e index < GetTagCount()
 *
 * \return The child tag, or a special null-valued tag if index out of range.
 */

/*!
 * \fn uint16 CECTag::GetTagCount(void) const
 *
 * \brief Returns the number of child tags.
 *
 * \return The number of child tags.
 */

/*!
 * \fn const void *CECTag::GetTagData(void) const
 *
 * \brief Returns a pointer to the TAG DATA.
 *
 * \return A pointer to the TAG DATA. (As specified with the data field of the constructor.)
*/

/*!
 * \fn uint16 CECTag::GetTagDataLen(void) const
 *
 * \brief Returns the length of the data buffer.
 *
 * \return The length of the data buffer.
 */

/*!
 * \fn ec_tagname_t CECTag::GetTagName(void) const
 *
 * \brief Returns TAGNAME.
 *
 * \return The name of the tag.
 */

/*!
 * \fn wxString CECTag::GetStringData(void) const
 *
 * \brief Returns the string data of the tag.
 *
 * Returns a wxString created from TAGDATA. It is automatically
 * converted from UTF-8 to the internal application encoding.
 * Should be used with care (only on tags created with the
 * CECTag(ec_tagname_t, const wxString&) constructor),
 * becuse it does not perform any check to see if the tag really contains a
 * string object.
 *
 * \return The string data of the tag.
 *
 * \sa CECTag(ec_tagname_t, const wxString&)
 */

/*!
 * \fn uint8 CECTag::GetInt8Data(void) const
 *
 * \brief Returns the uint8 data of the tag.
 *
 * This function takes care of the endianness problems with numbers.
 *
 * \return The uint8 data of the tag.
 *
 * \sa CECTag(ec_tagname_t, uint8)
 */

/*!
 * \fn uint16 CECTag::GetInt16Data(void) const
 *
 * \brief Returns the uint16 data of the tag.
 *
 * This function takes care of the endianness problems with numbers.
 *
 * \return The uint16 data of the tag.
 *
 * \sa CECTag(ec_tagname_t, uint16)
 */

/*!
 * \fn uint32 CECTag::GetInt32Data(void) const
 *
 * \brief Returns the uint32 data of the tag.
 *
 * This function takes care of the endianness problems with numbers.
 *
 * \return The uint32 data of the tag.
 *
 * \sa CECTag(ec_tagname_t, uint32)
 */

/*!
 * \fn uint64 CECTag::GetInt64Data(void) const
 *
 * \brief Returns the uint64 data of the tag.
 *
 * This function takes care of the endianness problems with numbers.
 *
 * \return The uint64 data of the tag.
 *
 * \sa CECTag(ec_tagname_t, uint64)
 */

/**********************************************************
 *							  *
 *	CECPacket class					  *
 *							  *
 **********************************************************/

CECPacket::CECPacket(wxSocketBase *sock, ECSocket& socket, void *opaque) : CECEmptyTag(0)
{
	m_error = 0;
	if (!socket.ReadNumber(sock, &m_opCode, sizeof(ec_opcode_t), opaque)) {
		m_error = 2;
		return;
	}
	ReadChildren(sock, socket, opaque);
}


bool CECPacket::WritePacket(wxSocketBase *sock, ECSocket& socket, void *opaque) const
{
	if (!socket.WriteNumber(sock, &m_opCode, sizeof(ec_opcode_t), opaque)) return false;
	if (!WriteChildren(sock, socket, opaque)) return false;
	return true;
}

/*!
 * \fn CECPacket::CECPacket(ec_opcode_t opCode)
 *
 * \brief Creates a new packet with given OPCODE.
 */

/*!
 * \fn ec_opcode_t CECPacket::GetOpCode(void) const
 *
 * \brief Returns OPCODE.
 *
 * \return The OpCode of the packet.
 */

/*!
 * \fn uint32 CECPacket::GetPacketLength(void) const
 *
 * \brief Returns the length of the packet.
 *
 * \return The length of the packet.
 */
