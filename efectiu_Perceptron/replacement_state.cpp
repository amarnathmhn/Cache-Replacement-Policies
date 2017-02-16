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

    // calculate block offset bits and tag bits
    UINT32 bytesperblock = 4*(1<<20)/(numsets*assoc);
    blockOffsetBits = GetBitsInNum(bytesperblock);
    setBits         = GetBitsInNum(numsets);

    // values of all parameters are taken directly from the paper
     // Initialize the prediction bit for each block
    for(UINT32 setIndex=0; setIndex<numsets; setIndex++) 
    {

        for(UINT32 way=0; way<assoc; way++) 
        {
            // initialize stack position (for true LRU)
            repl[ setIndex ][ way ].reusePredictionBit = 0;
        }
    }

    // Initialize Sampler Sets
    samplerSetNum = 64; // no. of sampler sets
    sampler = new Sampler[samplerSetNum];
    samplerSetAssoc = 16; // associativity of sampler sets
    featureNum = 6; // no. of features for perceptron learning
    
    for(UINT32 setIndex=0; setIndex < samplerSetNum; setIndex++){
    
	sampler[setIndex].partialTag = new UINT32[samplerSetAssoc];
	sampler[setIndex].Yout       = new INT32[samplerSetAssoc];
	sampler[setIndex].features   = new UINT32*[samplerSetAssoc];
	sampler[setIndex].LRUstackposition = new UINT32 [samplerSetAssoc];
	sampler[setIndex].valid =      new bool [samplerSetAssoc];
	// Initalize each of these per sampler block data
	for(UINT32 samplerBlock=0; samplerBlock < samplerSetAssoc; samplerBlock++){
	
		sampler[setIndex].partialTag[samplerBlock] = 0;
		sampler[setIndex].Yout[samplerBlock] = 0;
		sampler[setIndex].features[samplerBlock] = new UINT32[featureNum];
		sampler[setIndex].valid[samplerBlock] = false;

		for(UINT32 f=0; f<featureNum; f++){
		  sampler[setIndex].features[samplerBlock][f] = 0;
		}

		sampler[setIndex].LRUstackposition[samplerBlock] = samplerBlock;
		
		
	}
    }

    // Initialize predictor tables
    predictorTableEntryNum = 256;
    predictorTable = new INT32* [featureNum];
    
    for(UINT32 table=0; table<featureNum; table++){
    
    	predictorTable[table] = new INT32 [predictorTableEntryNum];
	
	for(UINT32 entry=0; entry < predictorTableEntryNum; entry++){
	
		predictorTable[table][entry] = 0;
	}	
    }

    // Initialize the array holding the recent PCs 
    recentPCs = new Addr_t [4]; // 4 recent PCs are being used
    for(int r=0; r<4; r++){
 	recentPCs[r] = 0;
    }

    // Initialize Pseudo LRU array
    pseudoLRU_Data = new UINT32* [numsets];
    for(UINT32 set=0; set<numsets; set++){
    
	    pseudoLRU_Data[set] = new UINT32[assoc-1]; // each pseudoLRU state is an assoc-1 bit array
	    for(UINT32 a=0; a<assoc-1; a++){
	    	pseudoLRU_Data[set][a] = 0;
	    }
    }

    // Perceptron parameters
    theta = 68; // Perceptron training threshold
    tau_bypass   = 3; // threshold to decide whether to bypass the block
    tau_replace = 124; // threshold to decide whether to replace the block with incoming block

     
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
	return Get_My_Victim (setIndex, PC, paddr);
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

	// Update recent PCs on every access to the cache
	UpdateRecentPCs(PC);  
        UpdateMyPolicy(setIndex, updateWayID, currLine, PC, cacheHit);	
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

INT32 CACHE_REPLACEMENT_STATE::Get_My_Victim( UINT32 setIndex, Addr_t PC, Addr_t paddr ) {
	


	// Check if the incoming block is predicted dead and bypass if so
	UINT32 tag = ( paddr >> (blockOffsetBits+setBits) );
	/*
	if( GetPerceptronPredictionBypass(PC, tag ) ){
		// update recent PCs right here because update policy won't be called on a bypass
		UpdateRecentPCs(PC);  
		return -1;
	}	

	// if not bypass, then search for a dead block in the set
	// i.e Search the set for a block predicted not to have reuse
	for(UINT32 blk=0; blk < assoc; blk++){
		// check if this block is dead ie reuse prediction bit is 0 - false
		if(!repl[setIndex][blk].reusePredictionBit){
			// check if it must be replaced
			if(GetPerceptronPredictionReplacement(PC, tag) ){
				return blk;
			}
			//return blk;
		}	
	}

	// if dead block is not found, then get pseudo LRU victim
	UINT32 vic = Get_PseudoLRU_Victim(setIndex);
	repl[setIndex][vic].reusePredictionBit = false;
	return vic;
	//return Get_LRU_Victim(setIndex);
	*/

	// if the set accessed is a sampler set
	if( IsSamplerSet(setIndex) ){
		UINT32 samplerSetIndex = setIndex / (numsets/samplerSetNum);
		// need replacement from the sampler set
		 UINT32 evictedWay = Get_SamplerLRU_Victim(samplerSetIndex);
		 if( ( (sampler[samplerSetIndex].Yout[evictedWay]) < theta ) || 
		     ( repl[setIndex][evictedWay].reusePredictionBit ) ){
		     // misprediction => increment with saturating arithmetic
		     UpdatePredictorFromSamplerEntry(samplerSetIndex, evictedWay, true); // true is for increment
		 }

		return evictedWay;
		 
	}
	// if the set accessed is normal set
	else {
	    if( GetPerceptronPredictionBypass(PC, tag ) ){
		// update recent PCs right here because update policy won't be called on a bypass
		UpdateRecentPCs(PC);  
		return -1;
	    }
		// if not bypass, then search for a dead block in the set
        	// i.e Search the set for a block predicted not to have reuse
	    for(UINT32 blk=0; blk < assoc; blk++){
		// check if this block is dead ie reuse prediction bit is 0 - false
		if(!repl[setIndex][blk].reusePredictionBit){
			// check if it must be replaced
			//if(GetPerceptronPredictionReplacement(PC, tag) ){
				return blk;
			//}
			//return blk;
		}	
  	    }

        	return Get_PseudoLRU_Victim(setIndex);
					
	}
}
INT32 CACHE_REPLACEMENT_STATE::Get_SamplerLRU_Victim( UINT32 samplerSetIndex ){
// Get pointer to replacement state of current set

	INT32   lruWay   = 0;

	// Search for victim whose stack position is assoc-1

	for(UINT32 way=0; way<samplerSetAssoc; way++) {
		if (sampler[samplerSetIndex].LRUstackposition[way] == (samplerSetAssoc-1)) {
			lruWay = way;
			break;
		}
	}

	// return lru way

	return lruWay;
	
}
INT32 CACHE_REPLACEMENT_STATE:: Get_PseudoLRU_Victim(UINT32 setIndex){
	// Getting pseudo lru victim
	UINT32 idx = 0; // index of pseudoLRU array
	while(idx < assoc-1){
		/*
		   If the root is 0, access the left side
		   Otherwise access the right side
		*/
		if( pseudoLRU_Data[setIndex][idx] == 0 ){
			idx = 2*idx + 1;
		}else if(pseudoLRU_Data[setIndex][idx] == 1){
			idx = 2*idx + 2;
		}
	}

	// now idx indicates a line
	return ( idx - (assoc - 1) );
}

void CACHE_REPLACEMENT_STATE::UpdateMyPolicy( UINT32 setIndex, INT32 updateWayID,const LINE_STATE *currLine, Addr_t PC, bool cacheHit ) {
	
	UINT32 tag = currLine->tag ;

	

	// Training the predictor
	// Check if this is sampler set
	if( IsSamplerSet(setIndex) ){

		/*	
		INT32 updateWay = 0;
		INT32 samplerHitWay = HitInSampler(samplerSetIndex, tag);
		// if hit in sampler
		if(samplerHitWay != -1){
			//printf("##################### Hit in Sampler ######################\n");
			// if the Yout for this entry exceeds a threshold, the predictor table entries
			// indexed by hashes in the accessed sampler entry are decremented with saturating arithmetic
			if( ((sampler[samplerSetIndex].Yout[samplerHitWay]) > -theta )){
				//printf("Updating on sampler hit\n");
				UpdatePredictorFromSamplerEntry(samplerSetIndex, samplerHitWay, false); // false is for decrement
				//UpdatePredictorFromRecentPCs(tag, false);

			}
			updateWay = samplerHitWay;

		} // if miss in the sampler
		else{
			//printf("############## Miss in the sampler set %u ##############\n",samplerSetIndex);
			UINT32 evictedWay = 0;// Get_SamplerLRU_Victim(samplerSetIndex);
			bool needReplacement = true;
			for(UINT32 way=0; way < samplerSetAssoc; way++){
				if( !sampler[samplerSetIndex].valid[way] ){
				  	evictedWay = way;
					needReplacement = false;
					break;
				}
			}
 			
			if(needReplacement){
			    evictedWay = Get_SamplerLRU_Victim(samplerSetIndex);
			    if( ( (sampler[samplerSetIndex].Yout[evictedWay]) < theta ) || 
			        ( repl[setIndex][updateWayID].reusePredictionBit ) ){
				// misprediction => increment with saturating arithmetic
				UpdatePredictorFromSamplerEntry(samplerSetIndex, evictedWay, true); // true is for increment
			    }


			

			}
			//repl[setIndex][updateWayID].reusePredictionBit = false;
			updateWay = evictedWay;


		   
			

		}
		*/
		UINT32 samplerSetIndex = setIndex/(numsets/samplerSetNum);
		if( cacheHit ){
			if( ((sampler[samplerSetIndex].Yout[updateWayID]) > -theta )){
				//printf("Updating on sampler hit\n");
				UpdatePredictorFromSamplerEntry(samplerSetIndex, updateWayID, false); // false is for decrement

			}

		}else{
			
			// Place the new features in the sampled entry
			UINT32 mask15 = (1 << 15) - 1;
			sampler[samplerSetIndex].partialTag[updateWayID] = (tag & mask15 );
			// fill the feature list -- recentPCs is up to date now
			UINT32 hmask = ((1 << 8) - 1 );

			sampler[samplerSetIndex].features[updateWayID][0] = ( ( (recentPCs[0] >> 2) & (hmask) ) ^ (PC & hmask)  ); // 1st feature PC0
			sampler[samplerSetIndex].features[updateWayID][1] = ( ( (recentPCs[1] >> 1) & (hmask) ) ^ (PC & hmask)  ); // 2nd feature PC1
			sampler[samplerSetIndex].features[updateWayID][2] = ( ( (recentPCs[2] >> 2) & (hmask) ) ^ (PC & hmask)  ); // 3rd feature PC2
			sampler[samplerSetIndex].features[updateWayID][3] = ( ( (recentPCs[3] >> 3) & (hmask) ) ^ (PC & hmask)  ); // 4th feature PC3
			sampler[samplerSetIndex].features[updateWayID][4] = ( ( (tag >> 4) & (hmask) ) ^ (PC & hmask) ); // 5th feature current tag>>4
			sampler[samplerSetIndex].features[updateWayID][5] = ( ( (tag >> 7) & (hmask) ) ^ (PC & hmask) ); // 6th feature current tag>>7

			// calculate yout
			sampler[samplerSetIndex].Yout[updateWayID] = GetPredictionFromSamplerEntry(samplerSetIndex, updateWayID);
			sampler[samplerSetIndex].valid[updateWayID] = true;
		}	
			
		// Update sampler lru now
		UpdateSamplerLRU(samplerSetIndex, updateWayID);

	} // a normal set
	else{
		UpdatePseudoLRU(setIndex, updateWayID);
	}
        
	repl[setIndex][updateWayID].reusePredictionBit = GetPerceptronPredictionBitRecentPCs(tag);
	
}
void CACHE_REPLACEMENT_STATE::UpdateSamplerLRU(UINT32 samplerSetIndex, INT32 samplerWayID ){

	// Determine current LRU stack position
	UINT32 currLRUstackposition = sampler[samplerSetIndex].LRUstackposition[ samplerWayID ];

	// Update the stack position of all lines before the current line
	// Update implies incremeting their stack positions by one

	for(UINT32 way=0; way<samplerSetAssoc; way++) {
		if( sampler[samplerSetIndex].LRUstackposition[ way ] < currLRUstackposition ) {
			sampler[samplerSetIndex].LRUstackposition[ way ]++;
		}
	}

	// Set the LRU stack position of new line to be zero
	sampler[samplerSetIndex].LRUstackposition[ samplerWayID ] = 0;


}

/*
   The following description of Tree Pseudo LRU policy is taken from an internet search.
   The method that follows is my own implementation
   TREE PSEUDO LRU Policy
              are all 4 lines valid?
                   /       \
                 yes        no, use an invalid line
                  |
                  |
                  |
             bit_0 == 0?            state | replace      ref to | next state
              /       \             ------+--------      -------+-----------
             y         n             00x  |  line_0      line_0 |    11_
            /           \            01x  |  line_1      line_1 |    10_
     bit_1 == 0?    bit_2 == 0?      1x0  |  line_2      line_2 |    0_1
       /    \          /    \        1x1  |  line_3      line_3 |    0_0
      y      n        y      n
     /        \      /        \        ('x' means       ('_' means unchanged)
   line_0  line_1  line_2  line_3 
    (3)      (4)     (5)    (6)


    the tree is represented as pseudoLRU array in this implementation(for a 4 way associativity)
    pseudoLRU_Data = [bit_0, bit_1, bit_2] 
    Leaves are the lines to be evicted = [line_0, line_1, line_2, line_3]
    The lines can be thought of as parts of the tree with continued indices:
    line_0 = 3, line_1 = 4 and so on.
*/
// Updates Pseudo LRU data for non sampler sets
void CACHE_REPLACEMENT_STATE::UpdatePseudoLRU(UINT32 setIndex, INT32 updateWayID ){

	INT32 idx = assoc - 1 + updateWayID; // index of the leaf node which represents a line
	
	/*
	   If a line is accessed, then its parent is updated according
	   to whether it is the left child or right child of its parent.
	*/
	while( idx > 0 ){
		// if the node is even numbered
		if( !(idx & 1) ){
			// then parent must be 0
			idx = (idx - 2)/2;
			pseudoLRU_Data[setIndex][idx] = 0;
		}
		// if the node is odd numbered
		else {
			// then parent must be 1
			idx = (idx - 1)/2;
			pseudoLRU_Data[setIndex][idx] = 1;
			
		}
	}


	// print for debug
	/*
	printf("******************* PSEUDO LRU for SET = %u, line = %d *******************\n",setIndex, updateWayID);
	for(UINT32 a=0; a<assoc-1; a++){
	
		printf("%u ",pseudoLRU_Data[setIndex][a]);
	}
	printf("\n");
	*/


}

// shifts the recentPCs array to higher indices
// a[3] = a[2], a[3] = a[2],.. so on. a[0] = PC
void CACHE_REPLACEMENT_STATE::UpdateRecentPCs(Addr_t PC){
	// shift the PCs to higher index
	for(int i=3; i > 0; i--){
		recentPCs[i] = recentPCs[i-1];
	}
	// shift in the lates PC passed as argument to this function
	recentPCs[0] = PC;

	// Print them for debug
	/*
	for(int i=0; i < 4; i++){
		printf("recentPCs[%d] = %llx\n",i,recentPCs[i]);
	}*/

}
// returns true if setIndex is a sampler set,otherwise return false
bool CACHE_REPLACEMENT_STATE::IsSamplerSet(UINT32 setIndex){
	
	return (setIndex % (numsets / samplerSetNum ) == 0 );
	//return (setIndex < samplerSetNum ) ;
}
// Calculates perceptron output by accessing the 6 prediction tables
bool CACHE_REPLACEMENT_STATE::GetPerceptronPredictionBypass(Addr_t PC, UINT32 tag){
	//UINT32 tag = (PC >> (blockOffsetBits + setBits));
	UINT32 hmask = ((1<<8)-1);
	INT32 yout = predictorTable[0][ ( (PC>>2)& hmask )^ (PC & hmask) ] + 
		     predictorTable[1][ ( (recentPCs[0] >>1)& hmask )^(PC & hmask) ] + 
		     predictorTable[2][ ( (recentPCs[1] >>2)& hmask )^(PC & hmask) ] + 
		     predictorTable[3][ ( (recentPCs[2] >>3)& hmask )^(PC & hmask) ] +
		     predictorTable[4][ ( (tag >> 4)& hmask)^(PC & hmask) ]         +
		     predictorTable[5][ ( (tag >> 7)& hmask)^(PC & hmask) ];

	
	return (yout > tau_bypass);
	
	
}
// Returns the prediction for replacement
bool  CACHE_REPLACEMENT_STATE::GetPerceptronPredictionReplacement(Addr_t PC, UINT32 tag){
	//UINT32 tag = (PC >> (blockOffsetBits + setBits));
	UINT32 hmask = ((1<<8)-1);
	INT32 yout = predictorTable[0][ ( (PC>>2)& hmask )^ (PC & hmask) ] + 
		     predictorTable[1][ ( (recentPCs[0] >>1)& hmask )^(PC & hmask) ] + 
		     predictorTable[2][ ( (recentPCs[1] >>2)& hmask )^(PC & hmask) ] + 
		     predictorTable[3][ ( (recentPCs[2] >>3)& hmask )^(PC & hmask) ] +
		     predictorTable[4][ ( (tag >> 4)& hmask)^(PC & hmask) ]         +
		     predictorTable[5][ ( (tag >> 7)& hmask)^(PC & hmask) ];	

	return (yout > tau_replace);

}
bool  CACHE_REPLACEMENT_STATE:: GetPerceptronPredictionReplacementRecentPCs(Addr_t PC, UINT32 tag){
	INT32 yout = 0;
	UINT32 hmask = ( (1 <<8 ) -1 );
	//UINT32 PC = recentPCs[0];

	yout += predictorTable[0][ ( (recentPCs[0] >> 2) & (hmask ) )^( PC & hmask) ];
	for(int pc=1; pc < 4; pc++){
		yout += predictorTable[pc][ ( (recentPCs[pc] >> pc) & (hmask ) )^( PC & hmask) ];
	}
	yout += predictorTable[4][ ( (tag >> 4) & hmask )^(PC & hmask) ] + predictorTable[5][ ( (tag >> 7) & hmask )^(PC & hmask) ];

	return  (yout < tau_replace);	
	
}
// returns true if predicted to be live
bool  CACHE_REPLACEMENT_STATE:: GetPerceptronPredictionBypassRecentPCs(Addr_t PC, UINT32 tag){
	INT32 yout = 0;
	UINT32 hmask = ( (1 <<8 ) -1 );
	//UINT32 PC = recentPCs[0];

	yout += predictorTable[0][ ( (recentPCs[0] >> 2) & (hmask ) )^( PC & hmask) ];
	for(int pc=1; pc < 4; pc++){
		yout += predictorTable[pc][ ( (recentPCs[pc] >> pc) & (hmask ) )^( PC & hmask) ];
	}
	yout += predictorTable[4][ ( (tag >> 4) & hmask )^(PC & hmask) ] + predictorTable[5][ ( (tag >> 7) & hmask )^(PC & hmask) ];

	return  (yout < tau_bypass);	
	
}
// returns true if the block is predicted reuse and false if predicted dead
bool  CACHE_REPLACEMENT_STATE::GetPerceptronPredictionBitRecentPCs(UINT32 tag){
	INT32 yout = 0;
	UINT32 hmask = ( (1 <<8 ) -1 );
	UINT32 PC = recentPCs[0];

	yout += predictorTable[0][ ( (recentPCs[0] >> 2) & (hmask ) )^( PC & hmask) ];
	for(int pc=1; pc < 4; pc++){
		yout += predictorTable[pc][ ( (recentPCs[pc] >> pc) & (hmask ) )^( PC & hmask) ];
	}
	yout += predictorTable[4][ ( (tag >> 4) & hmask )^(PC & hmask) ] + predictorTable[5][ ( (tag >> 7) & hmask )^(PC & hmask) ];

	return  (yout < tau_replace);	
}
// Calculates Yout for the features a sampler set entry
INT32 CACHE_REPLACEMENT_STATE::GetPredictionFromSamplerEntry(UINT32 samplerSetIndex, INT32 way){

	INT32 yout = 0;
	for(UINT32 f=0; f < featureNum; f++){
		yout += predictorTable[f][ sampler[samplerSetIndex].features[way][f]  ];
	}
	//printf("sample yout = %d\n",yout);
	return yout;
}

// Updates the predictor entries using the features in sampler set entry
// increment = true => entries are incremented and decremented otherwise
void CACHE_REPLACEMENT_STATE::UpdatePredictorFromSamplerEntry(UINT32 samplerSetIndex, INT32 way, bool increment){
	
	for(UINT32 f=0; f < featureNum; f++){
		UpdateTableEntrySaturatingArithmetic(f, sampler[samplerSetIndex].features[way][f], increment);
	}

}
// Takes the current input features and updates the accessed predictor weights
void CACHE_REPLACEMENT_STATE::UpdatePredictorFromRecentPCs(UINT32 tag, bool increment){

	UINT32 hmask = ( (1<<8)-1 );	
	UINT32 PC = recentPCs[0];
//	predictorTable[0][ ( (recentPCs[0] >> 2) & (hmask ) )^( PC & hmask) ]++;
	UpdateTableEntrySaturatingArithmetic(0, ( (recentPCs[0] >> 2) & (hmask ) )^( PC & hmask), increment);
	for(int pc=1; pc < 4; pc++){
//		predictorTable[pc][ ( (recentPCs[pc] >> pc) & (hmask ) )^( PC & hmask) ]++;
		UpdateTableEntrySaturatingArithmetic(pc, ( (recentPCs[pc] >> pc) & (hmask ) )^( PC & hmask) , increment);
	}
//	predictorTable[4][ ( (tag >> 4) & hmask )^(PC & hmask) ];
//	predictorTable[5][ ( (tag >> 7) & hmask )^(PC & hmask) ];
	UpdateTableEntrySaturatingArithmetic(4,( (tag >> 4) & hmask )^(PC & hmask)  , increment);
	UpdateTableEntrySaturatingArithmetic(5,( (tag >> 7) & hmask )^(PC & hmask)  , increment);
	
}
// Increments table entries with saturating arithmetic
// if increment=true, increments the entry if < 31, otherwise decrements if > -32
void CACHE_REPLACEMENT_STATE::UpdateTableEntrySaturatingArithmetic(UINT32 tableNo, UINT32 feature, bool increment){
	if(increment){
		if(predictorTable[tableNo][feature] < 31){
			predictorTable[tableNo][feature]++;
		}
	}else{
		if(predictorTable[tableNo][feature] > -32){
			predictorTable[tableNo][feature]--;
		}
	}
}
UINT32 CACHE_REPLACEMENT_STATE::GetBitsInNum(UINT32 num){
	UINT32 res = 0;
	while(num > 1){
		res++;
		num = num>>1;
	}

	return res;
}

// Returns -1 if there is a miss in the sampler and returns way number if there is a hit
INT32 CACHE_REPLACEMENT_STATE:: HitInSampler(UINT32 samplerSetIndex, UINT32 tag){
	UINT32 mask15 = ((1<<15)-1);
	for(UINT32 way=0; way < samplerSetAssoc; way++){
		if(sampler[samplerSetIndex].valid[way] && ( sampler[samplerSetIndex].partialTag[way] == (tag & mask15) ) ){
			return way;
		}
	}

	return -1;
}

CACHE_REPLACEMENT_STATE::~CACHE_REPLACEMENT_STATE (void) {
}


