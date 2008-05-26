//
// This file is part of the aMule Project.
//
// Copyright (c) 2008 aMule Team ( admin@amule.org / http://www.amule.org )
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

#ifndef PLATFORMSPECIFIC_H
#define PLATFORMSPECIFIC_H

#include <common/Path.h>
#include "Types.h"


namespace PlatformSpecific {

/**
 * Create sparse file.
 *
 * This function will create the named file sparse if possible.
 *
 * @param name The file to be created (sparse if possible).
 * @param size The desired size of the file.
 * @return true, if creating the file succeeded, false otherwise.
 */
bool CreateSparseFile(const CPath& name, uint64_t size);

/**
 * Returns the max number of connections the current OS can handle.
 *
 * Currently anything but windows will return the default value (-1);
 */
#ifdef __WXMSW__
int GetMaxConnections();
#else
inline int GetMaxConnections() { return -1; }
#endif

}; /* namespace PlatformSpecific */

#endif /* PLATFORMSPECIFIC_H */
