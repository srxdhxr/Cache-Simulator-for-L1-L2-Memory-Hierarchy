#include <algorithm>
#include <bitset>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>
using namespace std;


//access state:
#define NA 0 // no action
#define RH 1 // read hit
#define RM 2 // read miss
#define WH 3 // Write hit
#define WM 4 // write miss
#define NOWRITEMEM 5 // no write to memory
#define WRITEMEM 6   // write to memory

struct config
{
    /**
    Holds the Parameters to define the structure of L1 and L2 Caches
    **/
    int L1blocksize;
    int L1setsize;
    int L1size;
    int L2blocksize;
    int L2setsize;
    int L2size;
};



struct Block
{
   /**
    Each Block in a Set has a Valid Bit, Dirty Bit and a Tag.
    Since we are only simulating the mechanism of Cache Data Movement, we are ignoring the actual data
   **/

   int valid_bit = 0; // When Tag of the Block matches with the Address, 1 else 0;
   int dirty_bit = 0; // When Data in a block is not the same as in the memory, 1 else 0;
   int tag = 0; //The integer equivalent address (or index) of the block in a set
};

struct Set
{
    /**
    Set is a Vector of Blocks. Each set has a counter to determine the block which should be evicted
    **/

    vector<Block> block_vec;
    int counter = 0;
};


struct Cache
{
    vector<Set> set_vec; //initialize a Vector of Set

    int set_size; //Size of a Set in a Cache
    int num_blocks; //Total Number of Blocks in a Cache
    int num_sets; //Total Number of Sets in a Cache

    int block_offset_num; //Number of Bits Required to address each byte of a block log2(Block Size)
    int set_index_num; //Number of Bits Required to address each set of a cache log2(num_sets)

    int block_offset = 0; //Index pointing at a byte in a Block
    int set_index = 0; //Index Pointing at a Set in Cache
    int tag = 0; //Index Pointing at a Way in a Set

    //Address = bitset<32> (<Tag><set_index><block_offset>)


    Cache(int blocksize, int setsize, int cachesize)
    {
        /**
        Cache Structure to Initialize Cache Based on the Config.
        **/

        set_size = setsize;

        if(set_size == 0)
        {
            //Handling Fully-Associative Configuration
            set_size = 1024*cachesize/blocksize;
        }

        num_blocks = 1024*cachesize/blocksize;
        num_sets = num_blocks/set_size;
        block_offset_num = log2(blocksize);
        set_index_num = log2(num_sets);

        //Resize the Cache to the given number of Sets
        set_vec.resize(num_sets);

        //Resize each Set to given number of Blocks
        for(int i = 0;i<num_sets;i++)
        {
            set_vec[i].block_vec.resize(set_size);
        }
    }

    int get_index(auto addr)
    {
        /**
        Inputs a bitset<32> addr and parses it to get tag, block_offset and set_index
        **/

        block_offset = ((addr<<(32-block_offset_num))>>(32-block_offset_num)).to_ulong();
        set_index = (((addr>>block_offset_num)<<(32-set_index_num))>>(32-set_index_num)).to_ulong();
        tag = (addr>>(block_offset_num+set_index_num)).to_ulong();
    }
};

class CacheSystem
{
    public:



    Cache L1 = decltype(L1)(1,1,1);
    Cache L2 = decltype(L2)(1,1,1);



    int L1AcceState = NA; // L1 access state variable, can be one of NA, RH, RM, WH, WM;
    int L2AcceState = NA; // L2 access state variable, can be one of NA, RH, RM, WH, WM;
    int MemAcceState = NOWRITEMEM;

    int i = 0;
    int empty_idx = 0; //Used to determine if a set in a cache is full or not;

    CacheSystem(Cache a, Cache b)
    {
        L1 = a;
        L2 = b;
    }

    int check(Cache L, auto addr)
    {
        /** To Check if a Cache is full or empty, in a Set
        Returns -1 if Set is Full else the index of the empty Block
        **/
        L.get_index(addr);
        int empty_idx = -1;
        for(i = 0; i<L.set_size; i++)
        {
            if(L.set_vec[L.set_index].block_vec[i].valid_bit == 0)
            {
                empty_idx = i;
                break;
            }
        }
        return empty_idx;
    }

    auto writeL1(auto addr){
       /** Returns WH or WM **/

        //select the set in our L1 cache using set index bits
        L1.get_index(addr);
        L1AcceState = WM;
        //iterate through each way in the current set
        for(i = 0; i<L1.set_size; i++)
        {
            if(L1.set_vec[L1.set_index].block_vec[i].tag == L1.tag && L1.set_vec[L1.set_index].block_vec[i].valid_bit == 1)
            {
                // - If Matching tag and Valid Bit High -> WriteHit and DirtyBit = High
                L1.set_vec[L1.set_index].block_vec[i].dirty_bit = 1;
                L1AcceState = WH;
                L2AcceState = NA;
                MemAcceState = NOWRITEMEM;
                break;
            }
        }
        //Otherwise? -> WriteMiss!
    }

    auto writeL2(auto addr){
        /**
        return {WM or WH, WRITEMEM or NOWRITEMEM}
        **/

        //Get the Corresponding Tag and SetIndex from the Address
        L2.get_index(addr);
        L2AcceState = WM;
        MemAcceState = WRITEMEM;

        for(i = 0; i<L2.set_size; i++)
        {
            if(L2.set_vec[L2.set_index].block_vec[i].tag == L2.tag && L2.set_vec[L2.set_index].block_vec[i].valid_bit == 1)
            {

                /*
                   iterate through each way in the current set
                        - If Matching tag and Valid Bit High -> WriteHit!
                                                             -> Dirty Bit High
                */

                L2.set_vec[L2.set_index].block_vec[i].dirty_bit = 1;
                L2AcceState = WH;
                MemAcceState = NOWRITEMEM;
                break;
            }
        }
        //Otherwise? -> WriteMiss!
    }

    auto readL1(auto addr){
        /**
        return RH or RM
        **/

        //Find the corresponding Tag and SetIndex in L1 for given Address
        L1.get_index(addr);
        L1AcceState = RM;
        for(i = 0; i<L1.set_size; i++)
        {
            if(L1.set_vec[L1.set_index].block_vec[i].tag == L1.tag && L1.set_vec[L1.set_index].block_vec[i].valid_bit == 1)
            {
                //If Matching Tag Found and ValidBit = 1 in L1, READ HIT !

                L1AcceState = RH;
                L2AcceState = NA;
                MemAcceState = NOWRITEMEM;
                break;
            }
        }
        //Otherwise? -> ReadMiss!
    }

    auto readL2(auto addr)
    {

        //Get SetIndex and BlockOffset in L2 to Read Data
        L2.get_index(addr);
        L2AcceState = RM;
        //Iterate Each Block in L2 to find the Matching Tag
        for(i = 0; i<L2.set_size; i++)
        {
            if(L2.set_vec[L2.set_index].block_vec[i].tag == L2.tag && L2.set_vec[L2.set_index].block_vec[i].valid_bit == 1)
            {
                //If Read Hit in L2, Move the data from L2 Block to L1  -> Its a ReadHit!
                //If Read Hit, then the data from L2 should be Moved to L2

                L2AcceState = RH;

                //-> Set the Hit Blocks Valid Bit to 0
                L2.set_vec[L2.set_index].block_vec[i].valid_bit = 0;
                //Check if L1 is Full or Not before bringing L2 block to L1
                empty_idx = check(L1, addr);
                if(empty_idx == -1)
                {
                    //L1 is Full
                    //Reconstruct the Address of the <TO BE EVICTED BLOCK> to Move it to L2
                    bitset<32> tag_binary = L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].tag;
                    bitset<32> set_binary = L1.set_index;
                    bitset<32> stored_address(0);

                    stored_address = (tag_binary<<(L1.block_offset_num + L1.set_index_num))|(set_binary<<L1.block_offset_num);

                    //Store the L1 evicted block Dirty Bit to copy in L2.DirtyBit after moving L1 to L2
                    int stored_dirty_bit = L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].dirty_bit;

                    //Place the L2 Block in the Evicted Block and Increment Counter of the Set of L1 (Round Robin Eviction Policy)
                    L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].tag = L1.tag;
                    L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].valid_bit = 1;
                    L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].dirty_bit = L2.set_vec[L2.set_index].block_vec[i].dirty_bit;
                    L1.set_vec[L1.set_index].counter = (L1.set_vec[L1.set_index].counter+1) % L1.set_size;

                    //Before Moving the Evicted L1 Block to L2, check if L2 is full or not
                    L2.get_index(stored_address);
                    empty_idx = check(L2, stored_address);

                    if(empty_idx == -1)
                    {
                        //L2 Is full
                        //If <tobeEvicted> Dirty Bit of L2 is Dirty, Then Write in MEM else Dont Write
                        if(L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].dirty_bit == 1)
                        {
                            MemAcceState = WRITEMEM;
                        }
                        else
                        {
                            MemAcceState = NOWRITEMEM;
                        }
                         //Move the Evicted Data From L1 to the Block in L2 based on Round Robin Policy
                        L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].tag = L2.tag;
                        L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].valid_bit = 1;
                        L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].dirty_bit = stored_dirty_bit;
                        L2.set_vec[L2.set_index].counter = (L2.set_vec[L2.set_index].counter+1) % L2.set_size;
                    }

                    else
                    {
                        //L2 is Not full -> Place the L1 Evicted Block in the Empty Block of L2
                        L2.set_vec[L2.set_index].block_vec[empty_idx].tag = L2.tag;
                        L2.set_vec[L2.set_index].block_vec[empty_idx].valid_bit = 1;
                        L2.set_vec[L2.set_index].block_vec[empty_idx].dirty_bit = stored_dirty_bit;
                        MemAcceState = NOWRITEMEM;
                    }
                }

                else
                {
                    //L1 Not Full -> Place the HIT BLOCK of L2 in the empty block of L1
                    L1.set_vec[L1.set_index].block_vec[empty_idx].tag = L1.tag;
                    L1.set_vec[L1.set_index].block_vec[empty_idx].valid_bit = 1;
                    L1.set_vec[L1.set_index].block_vec[empty_idx].dirty_bit = L2.set_vec[L2.set_index].block_vec[i].dirty_bit;
                    MemAcceState = NOWRITEMEM;
                }
                break;
            }
        }


        if(L2AcceState == RM)
        {
            //No Tag found in L2 to Read -> READ MISS

            //Check if L1 block is empty or not to place the READ data from memory
            empty_idx = check(L1, addr);
            if(empty_idx == -1)
            {
                //L1 is FUll

                //Reconstruct the Address of the <TO BE EVICTED BLOCK> to Move it to L2
                bitset<32> tag_binary = L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].tag;
                bitset<32> set_binary = L1.set_index;
                bitset<32> stored_address(0);
                stored_address = (tag_binary<<(L1.block_offset_num + L1.set_index_num))|(set_binary<<L1.block_offset_num);
                int stored_dirty_bit = L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].dirty_bit;

                //Place the READ Data from Memory in L1 Based on Round-Robin Policy
                L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].tag = L1.tag;
                L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].valid_bit = 1;
                L1.set_vec[L1.set_index].block_vec[L1.set_vec[L1.set_index].counter].dirty_bit = 0;
                L1.set_vec[L1.set_index].counter = (L1.set_vec[L1.set_index].counter+1) % L1.set_size;

                //Get L2 SetIndex and Tag to Place the L1 Evicted Block
                L2.get_index(stored_address);

                //Check if L2 is Full or Not
                empty_idx = check(L2, stored_address);

                if(empty_idx == -1)
                {
                    //L2 is FULL

                    //Write the Data from the <TobeEvictedBlock> in memory if Dirty bit =1
                    if(L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].dirty_bit == 1)
                    {
                        MemAcceState = WRITEMEM;
                    }
                    else
                    {
                        MemAcceState = NOWRITEMEM;
                    }

                    //Place the EvictedL1Block data in L2 based on round robin method
                    L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].tag = L2.tag;
                    L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].valid_bit = 1;
                    L2.set_vec[L2.set_index].block_vec[L2.set_vec[L2.set_index].counter].dirty_bit = stored_dirty_bit;
                    L2.set_vec[L2.set_index].counter = (L2.set_vec[L2.set_index].counter+1) % L2.set_size;
                }

                else
                {
                    //L2 has empty block where EvictedL1Block can be placed
                    L2.set_vec[L2.set_index].block_vec[empty_idx].tag = L2.tag;
                    L2.set_vec[L2.set_index].block_vec[empty_idx].valid_bit = 1;
                    L2.set_vec[L2.set_index].block_vec[empty_idx].dirty_bit = stored_dirty_bit;
                    MemAcceState = NOWRITEMEM;
                }


            }
            else
            {
                //L1 is Empty -> Place the READ DATA From memory in the Empty Block of L1
                L1.set_vec[L1.set_index].block_vec[empty_idx].tag = L1.tag;
                L1.set_vec[L1.set_index].block_vec[empty_idx].valid_bit = 1;
                L1.set_vec[L1.set_index].block_vec[empty_idx].dirty_bit = 0;
                MemAcceState = NOWRITEMEM;
            }

        }
    }
};



int main(int argc, char *argv[])
{

    config cacheconfig;
    ifstream cache_params;
    string dummyLine;
    cache_params.open(argv[1]);
    while (!cache_params.eof())                   // read config file
    {
        cache_params >> dummyLine;                // L1:
        cache_params >> cacheconfig.L1blocksize;  // L1 Block size
        cache_params >> cacheconfig.L1setsize;    // L1 Associativity
        cache_params >> cacheconfig.L1size;       // L1 Cache Size
        cache_params >> dummyLine;                // L2:
        cache_params >> cacheconfig.L2blocksize;  // L2 Block size
        cache_params >> cacheconfig.L2setsize;    // L2 Associativity
        cache_params >> cacheconfig.L2size;       // L2 Cache Size
    }

    ifstream traces; //Read Input Trace File consisting of R/W Instructions
    ofstream tracesout;
    string outname;
    outname = string(argv[2]) + ".out";
    ofstream debugger;
    string debugger_name = "debugger_outstream.txt";
    //debugger.open(debugger_name.c_str());
    traces.open(argv[2]);
    tracesout.open(outname.c_str());
    string line;
    string accesstype;     // the Read/Write access type from the memory trace;
    string xaddr;          // the address from the memory trace store in hex;
    unsigned int addr;     // the address from the memory trace store in unsigned int;
    bitset<32> accessaddr; // the address from the memory trace store in the bitset;



    int i = 1;
    if (cacheconfig.L1blocksize!=cacheconfig.L2blocksize){
        printf("please test with the same block size\n");
        return 1;
    }

    int L1AcceState = NA; // L1 access state variable, can be one of NA, RH, RM, WH, WM;
    int L2AcceState = NA; // L2 access state variable, can be one of NA, RH, RM, WH, WM;
    int MemAcceState = NOWRITEMEM; // Main Memory access state variable, can be either NA or WH;

    Cache L1(cacheconfig.L1blocksize, cacheconfig.L1setsize, cacheconfig.L1size); //Set UP L1 Based on L1 config Params
    Cache L2(cacheconfig.L2blocksize, cacheconfig.L2setsize, cacheconfig.L2size); //Set UP L1 Based on L1 config Params

    CacheSystem myCache(L1,L2); //Initialize Cache System


    if (traces.is_open() && tracesout.is_open())
    {
        while (getline(traces, line))
        {

            istringstream iss(line);
            if (!(iss >> accesstype >> xaddr)){
                break;
            }

            stringstream saddr(xaddr);
            saddr >> std::hex >> addr;
            accessaddr = bitset<32>(addr);

            // access the L1 and L2 Cache according to the trace;
            if (accesstype.compare("R") == 0)  // Read request
            {
                //Read L1
                myCache.readL1(accessaddr);
                if(myCache.L1AcceState == RM)
                {
                    //IF L1 ReadMiss, Read L2
                    myCache.readL2(accessaddr);
                }
            }

            else{

                myCache.writeL1(accessaddr); // Write request
                //Write L1
                if(myCache.L1AcceState == WM)
                {
                    //If L1 write Miss, Write in L2
                    myCache.writeL2(accessaddr);

                }
            }
            //Update Access States
            L1AcceState = myCache.L1AcceState;
            L2AcceState = myCache.L2AcceState;
            MemAcceState = myCache.MemAcceState;

            //Write the Access State Pattern in output trace file.
            tracesout << L1AcceState << " " << L2AcceState << " " << MemAcceState << endl; // Output hit/miss results for L1 and L2 to the output file;
            i++;
        }
        traces.close();
        tracesout.close();
    }
    else
        cout << "Unable to open trace or traceout file ";

    return 0;
}
