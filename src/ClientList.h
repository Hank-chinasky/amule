//
// This file is part of the aMule Project
//
// Copyright (c) 2003-2004 aMule Project ( http://www.amule-project.net )
// Copyright (C) 2002 Merkur ( merkur-@users.sourceforge.net / http://www.emule-project.net )
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

#ifndef CLIENTLIST_H
#define CLIENTLIST_H

#include "types.h"		// Needed for uint16 and uint32
#include "CMD4Hash.h"	// Needed for CMD4Hash

#include <list>
#include <map>


class CUpDownClient;
class CClientReqSocket;
class CDeletedClient;
class CMD4Hash;


#define BAN_CLEANUP_TIME	1200000 // 20 min


/**
 * This class takes care of managing existing clients.
 *
 * This class tracks a number of attributes related to existing and deleted
 * clients. Among other things, it keeps track of existing, banned, dead and
 * dying clients, as well as offers support for matching new client-instances
 * against already exist clients to avoid duplicates.
 */
class CClientList
{
public:
	/**
	 * Constructor.
	 */
	CClientList();

	/**
	 * Destructor.
	 */
	~CClientList();
	
	
	/**
	 * Adds a client to the global list of clients.
	 *
	 * @param toadd The new client.
	 */
	void	AddClient( CUpDownClient* toadd );

	/**
	 * Schedules a client for deletion.
	 *
	 * @param client The client to be deleted.
	 *
	 * Call this function whenever a client is to be deleted, rather than 
	 * directly deleting the client. If the client is on the global client-
	 * list, then it will be scheduled for deletion, otherwise it will be 
	 * deleted immediatly. Please check CUpDownClient::Safe_Delete for the
	 * proper way to do this.
	 */
	void	AddToDeleteQueue( CUpDownClient* client );


	/**
	 * Updates the recorded IP of the specified client.
	 *
	 * @param client The client to have its entry updated.
	 * @param newIP The new IP adress of the client.
	 *
	 * This function is to be called before the client actually changes its
	 * IP-address, and will update the old entry with the new value. There 
	 * will only be added an entry if the new IP isn't zero.
	 */
	void	UpdateClientIP( CUpDownClient* client, uint32 newIP );
	
	/**
	 * Updates the recorded ID of the specified client.
	 *
	 * @param client The client to have its entry updated.
	 * @param newID The new ID of the client.
	 *
	 * This function is to be called before the client actually changes its
	 * ID, and will update the old entry with the new value. Unlike the other 
	 * two functions, this function will always ensure that there is an entry
	 * for the client, regardless of the value of newID.
	 */
	void	UpdateClientID( CUpDownClient* client, uint32 newID );

	/**
	 * Updates the recorded hash of the specified client.
	 *
	 * @param client The client to have its entry updated.
	 * @param newHash The new user-hash.
	 *
	 * This function is to be called before the client actually changes its
	 * user-hash, and will update the old entry with the new value. There will 
	 * only be added an entry if the new hash is valid.
	 */
	void	UpdateClientHash( CUpDownClient* client, const CMD4Hash& newHash );


	/**
	 * Returns the number of banned clients.
	 */
	uint16	GetBannedCount() const;

	/**
	 * Returns the number of listed clients.
	 */
	uint32	GetClientCount() const;

	
	/**
	 * Deletes all tracked clients.
	 */
	void	DeleteAll();


	/**
	 * Replaces a new client-instance with the an already existing client, if one such exist.
	 *
	 * @param client A pointer to the pointer of the new instance.
	 * @param sender The socket assosiated with the new instance.
	 *
	 * Call this function when a new client-instance has been created. This function will then 
	 * compare it against all existing clients and see if we already have an instance matching 
	 * the new one. If that is the case, it will delete the new instance and set the pointer to
	 * the existing one.
	 */
	bool	AttachToAlreadyKnown( CUpDownClient** client, CClientReqSocket* sender );


	/**
	 * Finds a client with the specified ip and port.
	 *
	 * @param clientip The IP of the client to find.
	 * @param port The port used by the client.
	 */
	CUpDownClient* FindClientByIP( uint32 clientip, uint16 port );


	//! The list-type used to store clients IPs and other information
	typedef std::map<uint32, uint32> ClientMap;
	
	/**
	 * Function for generating statistics about the current clients.
	 */
	void	GetStatistics(uint32 &totalclient, uint32 stats[], ClientMap *clientVersionEDonkey, ClientMap *clientVersionEDonkeyHybrid, ClientMap *clientVersionEMule, ClientMap *clientVersionAMule); 


	/**
	 * Adds a client to the list of tracked clients.
	 * 
	 * @param toadd The client to track.
	 *
	 * This function is used to keep track of clients after they 
	 * have been deleted and makes it possible to spot port or hash
	 * changes.
	 */
	void	AddTrackClient(CUpDownClient* toadd);

	/**
	 * Returns the number of tracked client.
	 *
	 * @param dwIP The IP-adress which of the clients.
	 * @return The number of clients tracked at the specifed IP.
	 */
	uint16	GetClientsFromIP(uint32 dwIP);

	/**
	 * Checks if a client has changed its user-hash.
	 * 
	 * @param dwIP The IP of the client.
	 * @param nPort The port of the client.
	 * @param pNewHash The userhash assosiated with the client.
	 * 
	 */
	bool	ComparePriorUserhash( uint32 dwIP, uint16 nPort, void* pNewHash );


	/**
	 * Bans an IP address for 2 hours.
	 *
	 * @param dwIP The IP from which all clients will be banned.
	 */
	void	AddBannedClient(uint32 dwIP);

	/**
	 * Checks if a client has been banned.
	 *
	 * @param dwIP The IP to check.
	 * @return True if the IP is banned, false otherwise.
	 */
	bool	IsBannedClient(uint32 dwIP);

	/**
	 * Unbans an IP address, if it has been banned.
	 *
	 * @param dwIP The IP address to unban.
	 */
	void	RemoveBannedClient(uint32 dwIP);


	/**
	 * Main loop.
	 *
	 * This function takes care of cleaning the various lists and deleting 
	 * pending clients on the deletion-queue.
	 */
	void	Process();


	/**
	 * This function removes all clients filtered by the current IPFilter.
	 *
	 * Call this function after changing the current IPFiler list, to ensure
	 * that no client-connections to illegal IPs exist. These would otherwise
	 * be allowed to exist, bypassing the IPFilter.
	 */
	void	FilterQueues();


	//! The type of the list used to store client-pointers for a couple of tasks.
	typedef std::list<CUpDownClient*> SourceList;
	

	/**
	 * Returns a list of clients with the specified user-hash.
	 *
	 * @param hash The userhash to search for.
	 *
	 * This function will return a list of clients with the specified userhash,
	 * provided that the hash is a valid non-empty userhash. Empty hashes will
	 * simply result in nothing being found.
	 */
	SourceList	GetClientsByHash( const CMD4Hash& hash );
	
	/**
	 * Returns a list of clients with the specified IP.
	 *
	 * @param ip The IP-address to search for.
	 *
	 * This function will return a list of clients with the specified IP,
	 * provided that the IP is a non-zero value. A value of zero will not 
	 * result in any results.
	 */
	SourceList	GetClientsByIP( unsigned long ip );


	//! The type of the lists used to store IPs and IDs.
	typedef std::multimap<uint32, CUpDownClient*> IDMap;
	//! The pairs of the IP/ID list.
	typedef std::pair<uint32, CUpDownClient*> IDMapPair;


	/**
	 * Returns a list of all clients.
	 *
	 * @return The complete list of clients.
	 */
	const IDMap& GetClientList();

	
private:
	/**
	 * Helperfunction which finds a client matching the specified client.
	 *
	 * @param client The client to search for.
	 * @return The matching client or NULL.
	 *
	 * This functions searches through the list of clients and finds the first match
	 * using the same checks as CUpDownClient::Compare, but without the overhead.
	 */
	CUpDownClient* FindMatchingClient( CUpDownClient* client );
	
	
	/**
	 * Helperfunction which removes the client from the IP-list.
	 */
	void	RemoveIPFromList( CUpDownClient* client );
	/**
	 * Helperfunction which removes the client from the ID-list.
	 */
	bool	RemoveIDFromList( CUpDownClient* client );
	/**
	 * Helperfunction which removes the client from the hash-list.
	 */
	void	RemoveHashFromList( CUpDownClient* client );


	//! The type of the list used to store user-hashes.
	typedef std::multimap<CMD4Hash, CUpDownClient*> HashMap;
	//! The pairs of the Hash-list.
	typedef std::pair<CMD4Hash, CUpDownClient*> HashMapPair;


	//! The map of clients with valid hashes
	HashMap	m_hashList;
	
	//! The map of clients with valid IPs
	IDMap	m_ipList;
	
	//! The full lists of clients
	IDMap	m_clientList;



	//! This is the lists of clients that should be deleted
	SourceList m_delete_queue;
	
	//! This is the map of banned clients.
	ClientMap m_bannedList;
	//! This variable is used to keep track of the last time the banned-list was pruned.
	uint32	m_dwLastBannCleanUp;

	//! This is the map of tracked clients.
	std::map<uint32, CDeletedClient*> m_trackedClientsList;
	//! This keeps track of the last time the tracked-list was pruned.
	uint32	m_dwLastTrackedCleanUp;
};

#endif

