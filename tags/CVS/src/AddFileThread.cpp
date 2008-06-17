// This file is part of the aMule project.
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
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


#include <wx/defs.h>		// Needed before any other wx/*.h
#include <wx/event.h>		// Needed for wxCommandEvent
#include <wx/timer.h>		// Needed for wxStopWatch

#if wxCHECK_VERSION(2, 5, 2)
#	include <wx/arrstr.h>		// Needed for wxArrayString
#endif

#include <wx/filename.h>	// Needed for wxFileName::GetPathSeperator()
#include <wx/utils.h>

#include "AddFileThread.h"	// Interface declarations
#include "otherfunctions.h"	// Needed for unicode2char
#include "amule.h"			// Needed for theApp
#include "opcodes.h"		// Needed for TM_HASHTHREADFINISHED
#include "PartFile.h"		// Needed for CKnownFile and CPartFile
#include "CFile.h"			// Needed for CFile
#include "AICHSyncThread.h"	// Needed for CAICHSyncThread

#include "CryptoPP_Inc.h"	// Needed for MD4

//! Size to hash in one go: PARTSIZE/95, which is 100kb.
const unsigned int CRUMBSIZE = PARTSIZE/95;

//! Max number of threads. Change if you have > 1 CPUs ;)
const unsigned int MAXTHREADCOUNT = 1;

/**
 * Container for queued files.
 */
struct QueuedFile
{
	//! Signifies if a thread is currently working on this file.
	bool				m_busy;
	//! The full path to the file.
	wxString			m_path;
	//! The name of the file.
	wxString			m_name;
	//! The PartFile owning this file in case of a final hashing (completing).
	const CPartFile*	m_owner;
};


wxMutex CAddFileThread::s_running_lock;
wxMutex CAddFileThread::s_count_lock;
wxMutex CAddFileThread::s_queue_lock;


std::list<QueuedFile*> CAddFileThread::s_queue;
bool CAddFileThread::s_running;
uint8 CAddFileThread::s_count;


CAddFileThread::CAddFileThread()
	: wxThread(wxTHREAD_DETACHED)
{
	// Ensure that the AICH thread isn't running
	if ( CAICHSyncThread::IsRunning() )
		CAICHSyncThread::Stop();
}


bool CAddFileThread::IsRunning()
{
	wxMutexLocker lock( s_running_lock );
	return s_running;
}


void CAddFileThread::SetRunning(bool running)
{
	wxMutexLocker lock( s_running_lock );
	s_running = running;
}


uint8 CAddFileThread::GetThreadCount()
{
	wxMutexLocker lock( s_count_lock );
	return s_count;
}


void CAddFileThread::ThreadCountInc()
{
	wxMutexLocker lock( s_count_lock );
	s_count++;
}


void CAddFileThread::ThreadCountDec()
{
	s_count_lock.Lock();

	if ( s_count ) {
		// Decrement and make a copy so we can unlock immediatly
		uint8 count = --s_count;
		s_count_lock.Unlock();

		// No threads left? Then let it be known.
		if ( count == 0 ) {
			wxMuleInternalEvent evt(wxEVT_CORE_FILE_HASHING_SHUTDOWN);
			wxPostEvent(&theApp, evt);
		}
	} else {
		// Just unlock
		s_count_lock.Unlock();

		// Some debug info
		printf("Hasher: Warning, ThreadCountDec() called while thread count was zero!\n");
	}
}


void CAddFileThread::CreateNewThread()
{
	printf("Hasher: Creating new thread.\n");

	ThreadCountInc();

	CAddFileThread* th = new CAddFileThread();
	th->Create();

	// The threads shouldn't be hugging the CPU, as it will already be hugging the HD
	th->SetPriority(WXTHREAD_MIN_PRIORITY);
	
	th->Run();
}


void CAddFileThread::Start()
{
	if ( IsRunning() ) {
		printf("Hasher: Warning, Start() called while already running.\n");
	} else {
		SetRunning(true);

		// No threads are running yet, so all files are non-busy.
		uint32 files = GetFileCount();
		// We wont start on more files than the max number of threads.
		if ( files > MAXTHREADCOUNT )
			files = MAXTHREADCOUNT;

		// Start as many threads as we can and as needed
		for ( ; files; files-- ) {
			CreateNewThread();
		}
	}
}


void CAddFileThread::Stop()
{
	if ( IsRunning() ) {
		// Are there any threads to kill?
		if ( GetThreadCount() ) {		
			printf("Hasher: Signaling for remaining threads to terminate.\n");
		
			SetRunning( false );

			// Wait for all threads to die
			while ( GetThreadCount() ) {
				// Sleep for 1/100 of a second to avoid clobbering the mutex
				// By doing this we ensure that this function only returns
				// once the thread has died.
#if wxCHECK_VERSION(2, 5, 3)
				// wxUsleep has been deprecated in 2.5.3 and above
				wxMilliSleep(10);
#else
				wxUsleep(10);
#endif
			}
			
			printf("Hasher: Threads terminated.\n");
		} else {
			// No threads to kill, just stay quiet.
			SetRunning( false );
		}
	} else {
		printf("Hasher: Warning, attempted to stop already stopped hasher!\n");
	}
}


int	CAddFileThread::GetFileCount()
{
	wxMutexLocker lock( s_queue_lock );
	return s_queue.size();
}


QueuedFile* CAddFileThread::GetNextFile()
{
	wxMutexLocker lock( s_queue_lock );

	QueuedFile* file = NULL;

	// Get the first non-busy file
	FileQueue::iterator it = s_queue.begin();
	for ( ; it != s_queue.end(); ++it ) {
		if ( !(*it)->m_busy ) {
			file = (*it);
			// Mark the file as busy to avoid having multiple threads working on it
			file->m_busy = true;
			break;					
		}
	}
			
	return file;
}


void CAddFileThread::RemoveFromQueue(QueuedFile* file)
{
	// Some debugging information
	if ( !file ) {
		printf("Hasher: Error, tried to pass NULL pointer to RemoveFromQueue().\n");
		return;
	}

	// Some moredebugging information
	if ( !file->m_busy ) {
		printf("Hasher: Warning, non-busy file passed to RemoveFromQueue().\n");
	}

	// Remove a queued file which has been completed
	FileQueue::iterator it = std::find( s_queue.begin(), s_queue.end(), file );
			
	if ( it != s_queue.end() ) {
		s_queue.erase( it );
	} else {
		// Some debug info
		printf("Hasher: Warning, attempted to remove file from queue, but didn't find it.\n");
	}
			
	delete file;
}



void CAddFileThread::AddFile(const wxString& path, const wxString& name, const CPartFile* part)
{
	QueuedFile* hashfile = new QueuedFile;
	hashfile->m_busy = false;
	hashfile->m_path = path;
	hashfile->m_name = name;
	hashfile->m_owner = part;


	// New scope so that I can use wxMutexLocker ;) 
	{
		wxMutexLocker lock( s_queue_lock );

		// Avoid duplicate files
		FileQueue::iterator it = s_queue.begin();
		for ( ; it != s_queue.end(); ++it )
			if ( ( name == (*it)->m_name ) && ( path == (*it)->m_path ) )
				return;

		
		// If it's a partfile (part != NULL), then add it to the front so that
		// it gets hashed sooner, otherwise add it to the back
		if ( part == NULL ) {
			s_queue.push_back( hashfile );
		} else {
			s_queue.push_front( hashfile );
		}
	}


	// Should we start another thread?
	if ( GetThreadCount() < MAXTHREADCOUNT )
		CreateNewThread();
}


wxThread::ExitCode CAddFileThread::Entry()
{
	// Temporary file to contain the result
	CKnownFile* knownfile 		= NULL;
	// Current queued file
	QueuedFile*	current 		= NULL;

	// Cached value of filesize, to avoid constantly calling GetFileSize()
	uint32		filesize 		= 0;

	// The position in the current part.
	uint32		current_pos 	= 0;

	// The file currently getting hashed
	CFile		file;
	
	// MD4 hashing class.
	CryptoPP::MD4	hasher;
	

	// Continue to loop until there's nothing to do, or someone kills the threads
	while ( IsRunning() ) {
		// Haven't we started on hashing the file?
		if ( !file.IsOpened() ) {

			// Try to get the next file on queue
			current = GetNextFile();
			
			if ( !current ) {
				// Nothing to do, break
				printf("Hasher: No non-busy files on queue, stopping thread.\n");
				break;
			}
			
			wxString filename = current->m_path + wxFileName::GetPathSeparator() + current->m_name;
		
			// Attempt to open the file
			if ( !file.Open( filename, CFile::read ) ) {
				printf("Hasher: Warning, failed to open file, skipping: %s\n", unicode2char(filename));
				
				// Whoops, something's wrong. Delete the item and continue
				RemoveFromQueue( current );
				current = NULL;
				continue;
			}


			// We only support files <= 4gigs
			if ( (uint64)file.GetLength() >= (uint64)(4294967295U) ) {
				printf("Hasher: Warning, file is bigger than 4GB, skipping: %s\n", unicode2char(filename));
				
				// Close the current file so we can continue with the next
				file.Close();

				// Delete and continue 
				RemoveFromQueue( current );
				current = NULL;
				continue;
			}


			// Create a CKnownFile to contain the result
			knownfile = new CKnownFile();
			
			// Set initial values
			knownfile->m_strFilePath = current->m_path;
			knownfile->m_strFileName = current->m_name;
		
			filesize = file.GetLength();
			knownfile->SetFileSize( filesize );
			knownfile->date = wxFileModificationTime( filename );
			knownfile->m_AvailPartFrequency.Insert(0, 0, knownfile->GetPartCount() );
	
			// Starting from scratch
			current_pos = 0;

			printf("Hasher: Starting to hash file: %s\n", unicode2char(current->m_name));
		}


		// Well read at most CRUMBSIZE bytes per cycle
		uint32 cur_length = CRUMBSIZE;
		if ( file.GetPosition() + CRUMBSIZE > filesize ) {
			// Less than CRUMBSIZE left, reduce read-length		
			cur_length = filesize - file.GetPosition();
		}

		byte* data = new byte[cur_length];
		
		// Check for read errors
		if ( file.Read(data, cur_length) != cur_length ) {
			printf("Hasher: Error while reading file, skipping: %s\n", unicode2char(current->m_name));
			
			file.Close();
			
			// Delete and continue 
			RemoveFromQueue( current );
			current = NULL;
			delete [] data;
			continue;
		}

		// Update hash
		hasher.Update(data, cur_length);

		delete[] data;

		// Move to next crumb
		current_pos++;		


		// Finished a part?
		if ( current_pos >= PARTSIZE/CRUMBSIZE ) {
			byte hash[16];

			// Finalize hash and reset
			hasher.Final(hash);
		
			knownfile->hashlist.Add( (uchar*)hash );

			current_pos = 0;
		}
	
	
		// Done hashing?
		if ( file.GetPosition() >= filesize ) {
			// If there was a part < PARTSIZE, then create the hash for that part now
			if ( current_pos ) {
				byte hash[16];

				// Finalize hash and reset
				hasher.Final(hash);
		
				knownfile->hashlist.Add( (uchar*)hash );
			}

	
			// If the file is >= PARTSIZE, then the filehash is that one hash,
			// otherwise, the filehash is the hash of the parthashes
			if ( knownfile->hashlist.GetCount() == 1 ) {
				knownfile->m_abyFileHash = knownfile->hashlist[0];
				knownfile->hashlist.Clear();
				
			} else {
				hasher.Restart();
				
				for (size_t i = 0; i < knownfile->hashlist.GetCount(); i++)
					hasher.Update( knownfile->hashlist[i], 16 );

				byte hash[16];
				hasher.Final( hash );
	
				knownfile->m_abyFileHash.SetHash( (uchar*)hash );
			}

			// Nothing more to do with this file ...
			file.Close();
			
			
			// Pass on the completion
			wxMuleInternalEvent evt(wxEVT_CORE_FILE_HASHING_FINISHED);
			evt.SetClientData( knownfile );
			evt.SetExtraLong( (long)current->m_owner );

			printf("Hasher: Finished hashing file: %s\n", unicode2char(current->m_name));
			
			knownfile = NULL;
			RemoveFromQueue( current );
			current = NULL;
		
			wxPostEvent(&theApp, evt);
		}
	}

	// Ensure consistancy so we can start properly after a Stop()
	if ( current ) {
		delete knownfile;

		// Reset the busy-flag so the file will be hashed again
		current->m_busy = false;
	}
	
	// Notify that the thread has died
	printf("Hasher: A thread has died.\n");

	ThreadCountDec();

	return 0;
}

