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

/*
 Implementing "Perceptron Learning for Reuse Prediction" Paper (E. Teran, Z. Wang, D.A Jimenez)
*/
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
    // Re use prediction bit per block 
    // false if predicted dead, true if reuse
    bool reusePredictionBit; 

} LINE_REPLACEMENT_STATE;

// Structure for each sampler set
struct Sampler {

	// Partial Tag Array-  Lower 15 bits of tag
	UINT32* partialTag;
	// Perceptron output for each sampled line
	INT32*  Yout; 
	// Sequence of Hashed Features
	UINT32** features;
	// LRU data
	UINT32* LRUstackposition;
	// valid bit
	bool* valid;
	
};

// The implementation for the cache replacement policy
class CACHE_REPLACEMENT_STATE
{
public:
    LINE_REPLACEMENT_STATE   **repl;
  private:

    UINT32 numsets;
    UINT32 assoc;
    UINT32 replPolicy;
   
    UINT32 blockOffsetBits;
    UINT32 setBits;

    COUNTER mytimer;  // tracks # of references to the cache

    // CONTESTANTS:  Add extra state for cache here
	
    Addr_t* recentPCs; // 4 recent program counters. index 0 has current, 1 has previous PC and so on.
    UINT32 samplerSetNum; // Number of sampler sets
    UINT32 samplerSetAssoc; // associativity of sampler sets
    UINT32 featureNum; // Number of features for perceptron learning
    Sampler* sampler; // list of sampler sets
    
    INT32** predictorTable; // predictor Tables
    UINT32  predictorTableEntryNum; // number of entries in each predictor table

    INT32 theta; // perceptron threshold for training
    INT32 tau_bypass;
    INT32 tau_replace;
    // Pseudo LRU data for each set -
    // assoc-1 bits per set. state is taken as assoc-1 bit array for each set
    UINT32** pseudoLRU_Data;     
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
    INT32  Get_My_Victim( UINT32 setIndex, Addr_t PC, Addr_t paddr );
    INT32  Get_SamplerLRU_Victim( UINT32 samplerSetIndex );
    INT32  Get_PseudoLRU_Victim(UINT32 setIndex);
    void   UpdateLRU( UINT32 setIndex, INT32 updateWayID );
    void   UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID,const LINE_STATE *currLine, Addr_t PC, bool cacheHit );
    void   UpdateSamplerLRU(UINT32 samplerSetIndex, INT32 samplerWayID );
    void   UpdatePseudoLRU(UINT32 setIndex, INT32 updateWayID );
    void   UpdateRecentPCs(Addr_t PC);
    bool   IsSamplerSet(UINT32 setIndex);
    bool   GetPerceptronPredictionBypass(Addr_t PC, UINT32 tag);
    bool   GetPerceptronPredictionReplacement(Addr_t PC, UINT32 tag);
    bool   GetPerceptronPredictionReplacementRecentPCs(Addr_t PC, UINT32 tag);
    bool   GetPerceptronPredictionBypassRecentPCs(Addr_t PC, UINT32 tag);
    bool   GetPerceptronPredictionBitRecentPCs(UINT32 PC);
    INT32  GetPredictionFromSamplerEntry(UINT32 samplerSetIndex, INT32 way);
    //void   UpdatePredictorEntries(bool increment, Addr_t PC);
    void   UpdatePredictorFromSamplerEntry(UINT32 samplerSetIndex, INT32 way, bool increment);
    void   UpdatePredictorFromRecentPCs(UINT32 tag, bool increment);
    void   UpdateTableEntrySaturatingArithmetic(UINT32 tableNo, UINT32 feature, bool increment);

    INT32   HitInSampler(UINT32 samplerSetIndex, UINT32 tag);
    	
    UINT32 GetBitsInNum(UINT32 num);
};

#endif
