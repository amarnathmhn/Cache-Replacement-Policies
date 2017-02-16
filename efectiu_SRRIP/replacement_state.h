#ifndef REPL_STATE_H
#define REPL_STATE_H
 
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

#include <cstdlib>
#include <cassert>
#include "utils.h"
#include "crc_cache_defs.h"
#include <iostream>

using namespace std;

// Replacement Policies Supported
typedef enum 
{
    CRC_REPL_LRU        = 0,
    CRC_REPL_RANDOM     = 1,
    CRC_REPL_CONTESTANT = 2
} ReplacemntPolicy;

// Replacement State Per Cache Line
typedef struct
{
    UINT32  LRUstackposition;

    // CONTESTANTS: Add extra state per cache line here
    /*
	Implementing Static Re-Reference Interval Prediction (A.Jaleel, K.B. Theobald, S.C Steely Jr., J. Emer)
    */

    /* Re-reference prediction value	
       if RRPV = 2^M-1, then the block is assumed to be referenced in distant future
       if RRPV = 2^M-2, then the block is assumed to be referenced in a long interval
       if RRPV = 0, then the block is assumed to be referenced in a near immediate future
       Higher the value of RRPV, further in the future is it assumed to be referenced.
       The no of bits M is defined under CACHE_REPLACEMENT_STATE class
    */
    UINT32 RRPV;  

} LINE_REPLACEMENT_STATE;

typedef enum {
    RRIP_SD_SRRIP = 0,
    RRIP_SD_BRRIP = 1,
    RRIP_SD_FOLLOWER = 2,
    
    
} SetDedicationType;
struct sampler; // Jimenez's structures

// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{
public:
    LINE_REPLACEMENT_STATE   **repl;
  private:

    UINT32 numsets;
    UINT32 assoc;
    UINT32 replPolicy;

    COUNTER mytimer;  // tracks # of references to the cache

    // CONTESTANTS:  Add extra state for cache here

    UINT32 M;      // RRIP parameter M. M Bits are used to store RRPV value for each cache block
    UINT32 DIST_RRPV; // equals 2^M - 1
    UINT32 LONG_RRPV; // equals 2^M - 2
    UINT32 misses; // counts number of misses
    UINT32 BRRIP_frequency;

    // parameters for DRRIP
    // No. of Sets dedicated to each policy in Set Dueling
    UINT32 K; 
    // Set Dedication Type for each set
    SetDedicationType* setDedications;

    /*Follower Set Eviction Policy Selector: if MSB(PSEL) is 1, then BIP, otherwise LRU
      If Miss incurred in LRU dedicated sets, increment PSEL.
      If Miss incurred in BIP dedicated sets, decrement PSEL.
    */	
    UINT32 PSEL;
    UINT32 PSEL_bits; // no of bits for PSEL counter

  public:
    ostream & PrintStats(ostream &out);

    // The constructor CAN NOT be changed
    CACHE_REPLACEMENT_STATE( UINT32 _sets, UINT32 _assoc, UINT32 _pol );

    INT32 GetVictimInSet( UINT32 tid, UINT32 setIndex, const LINE_STATE *vicSet, UINT32 assoc, Addr_t PC, Addr_t paddr, UINT32 accessType );

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID );

    void   SetReplacementPolicy( UINT32 _pol ) { replPolicy = _pol; } 
    void   IncrementTimer() { mytimer++; } 

    void   UpdateReplacementState( UINT32 setIndex, INT32 updateWayID, const LINE_STATE *currLine, 
                                   UINT32 tid, Addr_t PC, UINT32 accessType, bool cacheHit );

    ~CACHE_REPLACEMENT_STATE(void);

  private:
    
    void   InitReplacementState();
    INT32  Get_Random_Victim( UINT32 setIndex );

    INT32  Get_LRU_Victim( UINT32 setIndex );
    INT32  Get_My_Victim( UINT32 setIndex );
    void   UpdateLRU( UINT32 setIndex, INT32 updateWayID );
    void   UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID, bool cacheHit ); // adding cacheHit argument
    void   UpdateSRRIP(UINT32 setIndex, INT32 updateWayID, bool cacheHit); // SRRIP update
    void   UpdateBRRIP(UINT32 setIndex, INT32 updateWayID, bool cacheHit); // BRRIP update
    void   GenerateSetDedicationTypes();
};

#endif
