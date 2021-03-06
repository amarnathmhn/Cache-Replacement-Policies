#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <map>
#include <iostream>

using namespace std;

#include "replacement_state.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This file is distributed as part of the Cache Replacement Championship     //
// workshop held in conjunction with ISCA'2010.                               //
//                                                                            //
//                                                                            //
// Everyone is granted permission to copy, modify, and/or re-distribute       //
// this software.                                                             //
//                                                                            //
// Please contact Aamer Jaleel <ajaleel@gmail.com> should you have any        //
// questions                                                                  //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

/*
** This file implements the cache replacement state. Users can enhance the code
** below to develop their cache replacement ideas.
**
*/


////////////////////////////////////////////////////////////////////////////////
// The replacement state constructor:                                         //
// Inputs: number of sets, associativity, and replacement policy to use       //
// Outputs: None                                                              //
//                                                                            //
// DO NOT CHANGE THE CONSTRUCTOR PROTOTYPE                                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
CACHE_REPLACEMENT_STATE::CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol )
{

    numsets    = _sets;
    assoc      = _assoc;
    replPolicy = _pol;

    mytimer    = 0;

    InitReplacementState();
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// The function prints the statistics for the cache                           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
ostream & CACHE_REPLACEMENT_STATE::PrintStats(ostream &out)
{

    out<<"=========================================================="<<endl;
    out<<"=========== Replacement Policy Statistics ================"<<endl;
    out<<"=========================================================="<<endl;

    // CONTESTANTS:  Insert your statistics printing here
    
    return out;

}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function initializes the replacement policy hardware by creating      //
// storage for the replacement state on a per-line/per-cache basis.           //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::InitReplacementState()
{
    // Create the state for sets, then create the state for the ways

    repl  = new LINE_REPLACEMENT_STATE* [ numsets ];

    // ensure that we were able to create replacement state

    assert(repl);

    // Create the state for the sets
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++) 
    {
        repl[ setIndex ]  = new LINE_REPLACEMENT_STATE[ assoc ];

        for(UINT32 way=0; way<assoc; way++) 
        {
            // initialize stack position (for true LRU)
            repl[ setIndex ][ way ].LRUstackposition = way;
        }
    }

    if (replPolicy != CRC_REPL_CONTESTANT) return;

    // Contestants:  ADD INITIALIZATION FOR YOUR HARDWARE HERE
    K = 64; // No. Of sets dedicated to each policy
    // generate Set Dedication Types for all sets
    setDedications = new SetDedicationType [numsets];
    
    GenerateSetDedicationTypes();
    
    // Initialize policy selector to 0
    PSEL = 0;
 
    // Inverse Bimodal throttle parameter
    // That is every BIP_frequency'th miss in BIP sets is placed in MRU position
    BIP_frequency = 32;
	
    // Initialize BIP miss counter
    misses = 0; 

    // Set PSEL bits
    PSEL_bits = 10;

}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache on every cache miss. The input        //
// argument is the set index. The return value is the physical way            //
// index for the line being replaced.                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType ) {
    // If no invalid lines, then replace based on replacement policy
    if( replPolicy == CRC_REPL_LRU ) 
    {
        return Get_LRU_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        return Get_Random_Victim( setIndex );
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        // Contestants:  ADD YOUR VICTIM SELECTION FUNCTION HERE
	return Get_My_Victim (setIndex);
    }

    // We should never here here

    assert(0);
    return -1;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function is called by the cache after every cache hit/miss            //
// The arguments are: the set index, the physical way of the cache,           //
// the pointer to the physical line (should contestants need access           //
// to information of the line filled or hit upon), the thread id              //
// of the request, the PC of the request, the accesstype, and finall          //
// whether the line was a cachehit or not (cacheHit=true implies hit)         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
void CACHE_REPLACEMENT_STATE::UpdateReplacementState( 
    UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
    UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit )
{
	//fprintf (stderr, "ain't I a stinker? %lld\n", get_cycle_count ());
	//fflush (stderr);
    // What replacement policy?
    if( replPolicy == CRC_REPL_LRU ) 
    {
        UpdateLRU( setIndex, updateWayID );
    }
    else if( replPolicy == CRC_REPL_RANDOM )
    {
        // Random replacement requires no replacement state update
    }
    else if( replPolicy == CRC_REPL_CONTESTANT )
    {
        // Contestants:  ADD YOUR UPDATE REPLACEMENT STATE FUNCTION HERE
        // Feel free to use any of the input parameters to make
        // updates to your replacement policy
	UpdateMyPolicy(setIndex, updateWayID, cacheHit);
    }
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//////// HELPER FUNCTIONS FOR REPLACEMENT UPDATE AND VICTIM SELECTION //////////
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds the LRU victim in the cache set by returning the       //
// cache block at the bottom of the LRU stack. Top of LRU stack is '0'        //
// while bottom of LRU stack is 'assoc-1'                                     //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_LRU_Victim( UINT32 setIndex )
{
	// Get pointer to replacement state of current set

	LINE_REPLACEMENT_STATE *replSet = repl[ setIndex ];
	INT32   lruWay   = 0;

	// Search for victim whose stack position is assoc-1

	for(UINT32 way=0; way<assoc; way++) {
		if (replSet[way].LRUstackposition == (assoc-1)) {
			lruWay = way;
			break;
		}
	}

	// return lru way

	return lruWay;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function finds a random victim in the cache set                       //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
INT32 CACHE_REPLACEMENT_STATE::Get_Random_Victim( UINT32 setIndex )
{
    INT32 way = (rand() % assoc);
    
    return way;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This function implements the LRU update routine for the traditional        //
// LRU replacement policy. The arguments to the function are the physical     //
// way and set index.                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

void CACHE_REPLACEMENT_STATE::UpdateLRU( UINT32 setIndex, INT32 updateWayID )
{
	// Determine current LRU stack position
	UINT32 currLRUstackposition = repl[ setIndex ][ updateWayID ].LRUstackposition;

	// Update the stack position of all lines before the current line
	// Update implies incremeting their stack positions by one

	for(UINT32 way=0; way<assoc; way++) {
		if( repl[setIndex][way].LRUstackposition < currLRUstackposition ) {
			repl[setIndex][way].LRUstackposition++;
		}
	}

	// Set the LRU stack position of new line to be zero
	repl[ setIndex ][ updateWayID ].LRUstackposition = 0;
}

INT32 CACHE_REPLACEMENT_STATE::Get_My_Victim( UINT32 setIndex ) {
	// return first way always
	// No matter what set, get the LRU victim
	return Get_LRU_Victim(setIndex);
	//return 0;
}

void CACHE_REPLACEMENT_STATE::UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID, bool cacheHit ) {
	
	if(!cacheHit){
		misses++;
		if(misses > BIP_frequency){
		  misses = 0;
		}
	}
	//update LRU policy for the set if the set is dedicated to LRU
	if(setDedications[setIndex] == DIP_SD_LRU){	
		UpdateLRU(setIndex, updateWayID);
		// Increment PSEL on a miss in LRU dedicated set
		if(!cacheHit){	
			if(PSEL < (unsigned int)( (1<<PSEL_bits)-1 ) ){
				PSEL++;
			}
		}
	}
	// update BIP policy for the set if the set is dedicated to BIP
	else if(setDedications[setIndex] == DIP_SD_BIP ){
		
		
		// Decrement PSEL on a miss in BIP dedicated set
		if(!cacheHit){
			// decrement BIP miss counter
			if(PSEL > 0){
				PSEL--;
			
			}

			// Update the block metadata to be at MRU position at frequency of BIP_frequency misses - LRU policy
			if( misses == BIP_frequency){
				misses = 0; // reset the misses
				UpdateLRU(setIndex, updateWayID);
			
			}
			// else use LIP. avoiding the receny update here.
		}
		// if the access is a hit on an LRU block, then update LRU for the set
		else {
			if( repl[setIndex][updateWayID].LRUstackposition == (assoc - 1) ){
				UpdateLRU(setIndex, updateWayID);
			}
		}
		
	}

	// update Follower set metadata
	else if(setDedications[setIndex] == DIP_SD_FOLLOWER){
	
		// select the policy depending on the MSB of PSEL
		if( (((PSEL >> (PSEL_bits - 1))&1) == 1)){
		  	// MSB is 1, use BIP policy
			if(!cacheHit){
				
				// Update the block metadata to be at MRU position at frequency of BIP_frequency misses - LRU policy
				if( misses == BIP_frequency){
					misses = 0; // reset the misses
					UpdateLRU(setIndex, updateWayID);
				
				}
				// else use LIP. avoiding the receny update here.
			}
			// if the access is a hit on an LRU block, then update LRU for the set
			else {
				if( repl[setIndex][updateWayID].LRUstackposition == (assoc - 1) ){
					UpdateLRU(setIndex, updateWayID);
				}
			}

		  
		
		}else{
		  // MSB is 0, use LRU policy
		  
		   UpdateLRU(setIndex, updateWayID);

		}
	}
	

	
}

CACHE_REPLACEMENT_STATE::~CACHE_REPLACEMENT_STATE (void) {
}

/*
 As described in the associated paper by Qureshi et. al.
 Implements the Complement Set Selection Policy for DIP-SD algorithm.
 Let N be the number of sets in the cache,
 Let K be the number of sets dedicated to each policy
 Divide the sets into K equally sized regions each containing N/K sets.
 Each of these region is called a constituency.
 In each consituency, one set is dedicated to each of the competing policies . (LRU and BIP)
 
 
*/

void CACHE_REPLACEMENT_STATE:: GenerateSetDedicationTypes(){
	// find number of bits in the setIndex field of the address
	UINT32 setbits = floor( log( (float)numsets ) / log(2.0) );
	UINT32 Kbits   = floor( log( (float)K ) / log(2.0) ); // most significant Kbits of setbits identify constituency
	UINT32 offsetbits = setbits-Kbits;
	//printf("numsets = %d, setbits = %d, Kbits = %d, offsetbits = %d\n",numsets,setbits,Kbits,offsetbits);	
	// assign set dedications
	for(UINT32 s = 0; s < numsets; s++){
		UINT32 constituency = (s>>(offsetbits)) ;
		UINT32 offset       = (s & ( (1<<offsetbits) - 1 ));
		UINT32 offset_comp  = ((~s) & ( (1<<offsetbits) - 1 ));

		// if constituency bits are equal to offset bits, the dedicate the set to LRU
		if( constituency == offset ) {
			setDedications[s] = DIP_SD_LRU;
		}
		// if the complement of offset equals the constituency identifying bits, dedicate it to BIP
		else if( constituency == offset_comp ){
			setDedications[s] = DIP_SD_BIP;
		}
		else {
			setDedications[s] = DIP_SD_FOLLOWER;	
		}

	//	printf("setDedications[%d] = %d\n",s,setDedications[s]);
	}


	
	
}
