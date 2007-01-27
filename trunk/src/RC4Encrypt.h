//
// This file is part of the aMule Project.
//
// Copyright (c) 2003-2006 aMule Team ( admin@amule.org / http://www.amule.org )
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
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//

#ifndef __RC4ENCRYPT_H__
#define __RC4ENCRYPT_H__

#include "Types.h"
#include <vector>

// Helper class

class CRC4EncryptableBuffer : public std::vector<uint8>
{
public:
	// Create, empty
	CRC4EncryptableBuffer();

	// Clear memory
	~CRC4EncryptableBuffer();

	// Obvious
	bool IsEmpty();

	// Appends to the end
	void Append(uint8* buffer);

	// RC4 encrypts the internal buffer. Marks it as encrypted, any other further call 
	// to add data, as Append(), must assert if the inner data is encrypted.
	void Encrypt();

	// Obvious
	size_t GetSize();
	
	// Returns a uint8* buffer with the internal data, and clears the internal one.
	// Don't forget to clear the encryption flag.
	uint8* Detach();


};

#endif // __RC4ENCRYPT_H__
