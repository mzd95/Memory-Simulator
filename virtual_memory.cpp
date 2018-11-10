#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <time.h>
#include <unistd.h>

using namespace std; 

int pageFaults = 0;	// counter to track page faults
int iTLBMiss = 0;	// counter to track instruction TLB miss
int dTLBMiss = 0;	// counter to track data TLB miss
int TLBMiss = 0;	// counter to track unified TLB miss
int iCacheMiss = 0;	// counter to track L1 instruction cache miss
int dCacheMiss = 0;	// counter to track L1 data cache miss
int L2CacheMiss = 0;	// counter to track L2 cache miss

int PTCount = 0, TLBLRU[128][4], iTLBLRU[16][1];	// stores the tag of the usage time for the implementation of LRU
int memCount = 0, L2cacheLRU[128][16], dcacheLRU[32][7], icacheLRU[64][2];
long long cycles = 0, total_cycles = 0;

struct myinputInstr{	// 40 bit input instruction
    int opcode;
    int opcount;
    int brnCond[6];
    int addr[32];	// opraddr or brnaddr
};
struct myiTlB_PTE{	// instruction TLB's
    int validB;	// if there's data in this position
    int tlbTag[16];	// TLB tag
    int pageNum[16];	// page number
};
struct myTLB_PTE{	// unified TLB
    int validB;
    int tlbTag[9];
    int pageNum[16];
};
struct mypraiTLB{
    struct myiTlB_PTE * way; // n - way associative
};
struct mypraTLB{
    struct myTLB_PTE * way;
};

struct myiCacheblock{
    int validB;
    int dirtyB;
    int addrTag[19];
    int bytes[64];
};
struct mydCacheblock{
    int validB;
    int dirtyB;
    int addrTag[19];
    int bytes[64];
};
struct myL2Cacheblock{
    int validB;
    int dirtyB;
    int addrTag[16];
    int bytes[64];
};
struct myMemory{
    long long tag;
    int address[32];
    int bytes[64];
};
struct myipraCache{
    struct myiCacheblock * way;
};
struct mydpraCache{
    struct mydCacheblock * way;
};
struct myL2praCache{
    struct myL2Cacheblock * way;
};

struct myPagePTE{
    long long tag;
    int index[32];
    int physAddr[32];
};

typedef myinputInstr inputInstr;
typedef myiTlB_PTE iTLBPTE;
typedef myTLB_PTE TLBPTE;
typedef mypraiTLB iTLBtype;
typedef mypraTLB TLBtype;
typedef myiCacheblock iCacheBlock;
typedef myipraCache icache;
typedef mydCacheblock dCacheBlock;
typedef mydpraCache dcache;
typedef myL2Cacheblock L2CacheBlock;
typedef myL2praCache L2cache;
typedef myPagePTE PagePTE;
typedef myMemory memory;
struct myarray{
    vector<iTLBtype> iTLBarray;
    vector<iTLBtype> dTLBarray;
    vector<TLBtype> TLBarray;
    vector<PagePTE> PTarray;
    vector<icache> iCacheSet;
    vector<dcache> dCacheSet;
    vector<L2cache> L2CacheSet;
    vector<memory> memSet;
    myarray(vector<iTLBtype> _iTLBarray,
            vector<iTLBtype> _dTLBarray,
            vector<TLBtype> _TLBarray,
            vector<PagePTE> _PTarray,
            vector<icache> _iCacheSet,
            vector<dcache> _dCacheSet,
            vector<L2cache> _L2cacheset,
            vector<memory> _memoryset):
        iTLBarray(_iTLBarray),
        dTLBarray(_dTLBarray),
        TLBarray(_TLBarray),
        PTarray(_PTarray),
        iCacheSet(_iCacheSet),
        dCacheSet(_dCacheSet),
        L2CacheSet(_L2cacheset),
        memSet(_memoryset){}
};
typedef myarray Array;

bool getInstruction( ifstream & fin , string & buf ) {
    return getline(fin,buf);
}

inputInstr str2InputInstr(string input) {
    inputInstr instr;
    int temp[40];
    for (int i = 0; i < 10; i++) {
    	int t;
        if(input[i] >= 48 && input[i] <= 57) t = input[i] - 48; // 0 - 9
        else if(input[i] >= 65 && input[i] <= 70) t = input[i] - 55; // A - F
        else if(input[i] >= 97 && input[i] <= 102) t = input[i] - 87; // a - f
        else cout << "input error!" << endl;
        temp[i * 4 + 3] = t % 2;
        temp[i * 4 + 2] = (t >> 1) % 2;
        temp[i * 4 + 1] = (t >> 2) % 2;
        temp[i * 4 + 0] = (t >> 3) % 2;
    }
    instr.opcode = (temp[0]) * 2 + (temp[1]);
    instr.opcount = (temp[6]) * 2 + (temp[7]);
    for(int i = 0; i < 6; i++) {
        instr.brnCond[i] = temp[i + 2];
    }
    for(int i = 0; i < 32; i++) {
        instr.addr[i] = temp[i + 8];
    }
    return instr;
}

long long add2Long(int *s, int n) {	// convert into long type integer
    long long res = 0;
    for(int i = 0; i < n; i++) {
        res = res * 2 + s[i];
    }
    return res;
}
long long newRand(int k) {
    time_t t;
    srand((unsigned) time(&t));
    long long x = (rand()>>k) % (2);
    return x;
}
int randWord(int k){
    time_t t;
    int x;
    srand((unsigned) time(&t) + k);
    x = rand() % 256;
    return x;
}

void initTLB(vector<iTLBtype> &iTLBarray, vector<iTLBtype> &dTLBarray, vector<TLBtype>  &TLBarray) {
    for(int i = 0; i < 16; i++) {
        iTLBarray[i].way = ( iTLBPTE *) malloc (1 * sizeof(iTLBPTE));	// instruction TLB, 1-way associative
    }
    for(int i = 0; i < 16; i++) {
        dTLBarray[i].way = (iTLBPTE *) malloc (1 * sizeof(iTLBPTE));	 // data TLB, 1-way associative
    }
    for(int i = 0; i < 128; i++) {
        TLBarray[i].way = (TLBPTE *) malloc (4 * sizeof(TLBPTE));	// unified TLB, 4-way associative
    }
    return;
}// initialize all three TLBs
void initCache(vector<icache> &iCacheSet, vector<dcache> &dCacheSet, vector<L2cache> &L2CacheSet) {
    for(int i = 0; i < 64; i++) {
        iCacheSet[i].way = (iCacheBlock *) malloc (2 * sizeof(iCacheBlock));	// instruction cache, 2-way associative
    }
    for(int i = 0; i < 32; i++) {
        dCacheSet[i].way = (dCacheBlock *) malloc (4 * sizeof(dCacheBlock));	// data cache, 2-way associative
    }
    for(int i = 0; i < 128; i++) {
        L2CacheSet[i].way = (L2CacheBlock *) malloc (16 * sizeof(L2CacheBlock));	// L2 cache, 16-way associative
    }
    return;
}// initialize all three caches

bool searchITLB(int * address, vector<iTLBtype> &iTLBarray, iTLBPTE * instr) {
    int virTag[16];
    long long tag, temp;
    iTLBPTE * way;
    for(int i = 0; i < 16; i++) {
        virTag[i] = address[i];
    }
    tag = add2Long(virTag,16);
    for(int i = 0; i < 16; i++) {
    	way = iTLBarray[i].way;
    	temp = add2Long(way[0].tlbTag, 16);
    	if(temp == tag && way[0].validB == 1) {	// if there's data in this position, and virtual page number = TLBtype tag
    		* instr = way[0];
			if(iTLBLRU[i][0] != 16){
				iTLBLRU[i][0] = 16;
                for (int j = 0; j < 16; j++) {
                    if(j != i && iTLBLRU[j][0] != 0)	iTLBLRU[j][0]--;
                }
            }
            return true;	// find 16 bits tlb tag and return true;
		}
	}
    return false;
    // did not find the tlb tag and return false;
}
//also used as data TLB search

void updateITLB(int * address, int * physAddr, vector<iTLBtype> &iTLBarr) {
    int tlbTag[16], pageNum[16];
    iTLBPTE newiPTE;
    int insert = 0, minitlb;
    for (int i = 0; i < 16; i++) {
        tlbTag[i] = address[i];
    }
    for (int i = 0; i < 16; i++) {
        pageNum[i] = physAddr[i];
    }
    newiPTE.validB = 1;
    memcpy(newiPTE.tlbTag, tlbTag, sizeof(int) * 16);
    memcpy(newiPTE.pageNum, pageNum, sizeof(int) * 16);
    minitlb = iTLBLRU[0][0];
    for (int i = 1; i < 16; i++) {
        if(iTLBLRU[i][0] < minitlb){	// find the least recently used one
            insert = i;
            minitlb = iTLBLRU[i][0];
        }
    }
    iTLBarr[insert].way[0] = newiPTE;
    iTLBLRU[insert][0] = 16;
    for (int i = 0; i < 16; i++) {
        if(i != insert && iTLBLRU[i][0] != 0)iTLBLRU[i][0]--;
    }
    return;
}

bool searchTLB(int * address, vector<TLBtype> &TLBarray, TLBPTE * instr) {
    int searchind[7];	// first 7 bit of the virtual address
    int virTag[9];
    long long ind, tag, temp;
    TLBPTE * way;
    for(int i = 0; i < 7; i++) {
        searchind[i] = address[i];
    }
    for(int i = 8; i < 16; i++) {
    	virTag[i - 8] = address[i];
	}
	ind = add2Long(searchind, 7);
    tag = add2Long(virTag, 9);
    way = TLBarray[ind].way;
    for( int i = 0; i < 4; i++) {
        //search in 4 banks
        temp = add2Long(way[i].tlbTag, 9);
        if(temp == tag && way[i].validB == 1) {	// compare virtual tag with TLB tag
            * instr = way[i];
            if(TLBLRU[ind][i] != 4){
                TLBLRU[ind][i] = 4;
                for (int j = 0; j < 4; j++) {
                    if(j != i && TLBLRU[ind][j] != 0)	TLBLRU[ind][j]--;
                }
            }
            return true;
        }
        // find 9 bits TLBtype tag and return true;
    }
    return false;
}

void updateTLB(int * address, int * physAddr, vector<TLBtype> &TLBarray){
    int searchind[7], tlbTag[9], pageNum[16], validB = 1;
    TLBPTE newPTE;
    int insert = 0, mintlb;
	for (int i = 0; i < 7; i++) {
        searchind[i] = address[i];
    }
    for (int i = 0; i < 9; i++) {
        tlbTag[i] = address[i + 7];
    }
    for (int i = 0; i < 16; i++) {
        pageNum[i] = physAddr[i];
    }
    newPTE.validB = validB;
    memcpy(newPTE.tlbTag, tlbTag, sizeof(int) * 9);
    memcpy(newPTE.pageNum, pageNum, sizeof(int) * 16);
    int ind = add2Long(searchind, 7);
    mintlb = TLBLRU[0][0];
    for (int i = 1; i < 4; i++) {
        if(TLBLRU[ind][i] < mintlb){	// find out the least recently used one
            insert = i;
            mintlb = TLBLRU[ind][i];
        }
    }
    TLBarray[ind].way[insert] = newPTE;
    TLBLRU[ind][insert] = 4;
    for (int i = 0; i < 4; i++) {
        if(i != insert && TLBLRU[ind][i] != 0)TLBLRU[ind][i]--;
    }
    return;
}

int searchPT(int * address, vector<PagePTE> &PageTableset) {
    long long  tag;
    tag = add2Long( address, 32);
    for (int i = 0; i < 1024; i++) {
        if(tag == PageTableset[i].tag){
            return i;
        }
    }
    return -1;
}

void updatePT(int * address, vector<PagePTE> &PageTableset){
    int index[32], Phyaddress[32];
    long long row;
    PagePTE newPage;
    for (int i = 0; i < 32; i++) {
        index[i] = address[i];
    }
    for (int i = 0; i < 32; i++) {
        Phyaddress[i] = newRand( i );
    }
    // sometime the random physical address may already in the Page Table
    row = add2Long(index, 32);
    memcpy(&newPage.tag, &row, sizeof(row));
    memcpy(newPage.index, index, sizeof(int) * 32);
    memcpy(newPage.physAddr, Phyaddress, sizeof(int) * 32);
    if(PTCount < 1024){
        PageTableset[PTCount] = newPage;
    }
    else PageTableset.push_back(newPage);
    PTCount++;
    return;
}

bool searchICache(int * address, vector<icache> &iCacheSet, int * word) {
    int cacheindex[6], blockoffset[7], cachetag[19];
    long long ind, tag, temp;
    iCacheBlock * newBlk;
    for (int i = 0; i < 6; i++) {
        cacheindex[i] = address[i + 19];
    }
    for (int i = 0; i < 7; i++) { 
        blockoffset[i] = address[i + 25];
    }
    for (int i = 0; i < 19; i++) {
        cachetag[i] = address[i];
    }
    ind = add2Long(cacheindex, 6);
    int offset = add2Long(blockoffset, 7);
    tag = add2Long(cachetag, 19);
	newBlk = iCacheSet[ind].way;
    for (int i = 0; i < 2; i++) {
        //search in 2 ways
        temp = add2Long(newBlk[i].addrTag, 19);
        if(temp == tag){
            memcpy(word, &newBlk[i].bytes[offset], sizeof(int) * 16);
            if(icacheLRU[ind][i] != 2){
                icacheLRU[ind][i] = 2;
                for (int j = 0; j < 2; j++) {
                    if(j != i && icacheLRU[ind][j] != 0)icacheLRU[ind][j]--;
                }
            }
            return true;
        }
    }
    return false;
}

void updateICache(int * address, vector<icache> &iCacheSet, int * bytes) {
    int validB = 1, dirtyB = 1, minEle, insert = 0;
    int iL1tag[19], iL1ind[6];
    iCacheBlock newBlk;
    int ind;
    for (int i = 0; i < 19; i++) {
        iL1tag[i] = address[i];
    }
    for (int i = 0; i < 6; i++) {
        iL1ind[i] = address[i + 19];
    }
    ind = add2Long(iL1ind, 6);
    memcpy(newBlk.addrTag, iL1tag, sizeof(int) * 19);
    memcpy(newBlk.bytes, bytes, sizeof(int) * 64);
    newBlk.validB = validB;
    newBlk.dirtyB = dirtyB;
    minEle = icacheLRU[ind][0];
    // find out the least recent use, which has number of 1;
    for (int i = 1; i < 2; i++) {
        if(icacheLRU[ind][i] < minEle){
            insert = i;
            minEle = icacheLRU[ind][i];
        }
    }
    iCacheSet[ind].way[insert] = newBlk;
    icacheLRU[ind][insert] = 2;
    for (int i = 0; i < 2; i++) {
        if(i != insert && icacheLRU[ind][i] != 0)icacheLRU[ind][i]--;
    }
    return;
}

bool searchDCache(int * address, vector<dcache> &dCacheSet, int * word) {
    int cacheindex[5], blockoffset[8], cachetag[19];
    long long tag, temp;
    dCacheBlock * way;
    for (int i = 0; i < 5; i++) {
        cacheindex[i] = address[i + 19];
    }
    for (int i = 0; i < 8; i++) { 
        blockoffset[i] = address[i + 24];
    }
    for (int i = 0; i < 19; i++) {
        cachetag[i] = address[i];
    }
    int ind = add2Long(cacheindex, 5);
    long long offset = add2Long(blockoffset, 8);
    tag = add2Long(cachetag, 19);
    way = dCacheSet[ind].way;
    for (int i = 0; i < 4; i++) {
        //search in 4 ways
        temp = add2Long( way[i].addrTag, 19);
        if(temp == tag){
            memcpy(word, &way[i].bytes[offset], sizeof(int) * 16);
            if(dcacheLRU[ind][i] != 4){
                dcacheLRU[ind][i] = 4;
                for (int j = 0; j < 4; j++) {
                    if(j != i && dcacheLRU[ind][j] != 0)dcacheLRU[ind][j]--;
                }
            }
            return true;
        }
    }
    return false;
}

void updateDCache(int * address, vector<dcache> &dCacheSet, int * bytes) {
    int validB = 1, dirtyB = 1, minEle, insert = 0;
    int dL1tag[19], dL1ind[5];
    dCacheBlock newBlk;
    int ind;
    for (int i = 0; i < 19; i++) {
        dL1tag[i] = address[i];
    }
    for (int i = 0; i < 5; i++) {
        dL1ind[i] = address[i + 19];
    }
    ind = add2Long(dL1ind, 5);
    memcpy(newBlk.addrTag, dL1tag, sizeof(int) * 19);
    memcpy(newBlk.bytes, bytes, sizeof(int) * 64);
    newBlk.validB = validB;
    newBlk.dirtyB = dirtyB;
    minEle = dcacheLRU[ind][0];
    // find out the least recent use, which has number of 1;
    for (int i = 1; i < 4; i++) {
        if(dcacheLRU[ind][i] < minEle){
            insert = i;
            minEle = dcacheLRU[ind][i];
        }
    }
    dCacheSet[ind].way[insert] = newBlk;
    dcacheLRU[ind][insert] = 4;
    for (int i = 0; i < 4; i++) {
        if(i != insert && dcacheLRU[ind][i] != 0)dcacheLRU[ind][i]--;
    }
    //cout << "updated D cache" << endl;
    return;
}

bool searchL2Cache(int * address, vector<L2cache> &L2CacheSet, int * word, int * block) {
    int cacheindex[7], blockoffset[9], cachetag[16];
    long long tag, temp;
    L2CacheBlock * way;
    for (int i = 0; i < 7; i++) {
        cacheindex[i] = address[i + 16];
    }
    for (int i = 0; i < 9; i++) { 
        blockoffset[i] = address[i + 23];
    }
    for (int i = 0; i < 16; i++) {
        cachetag[i] = address[i];
    }
    int ind = add2Long(cacheindex, 7);
    long long offset = add2Long(blockoffset, 9);
    tag = add2Long(cachetag, 16);
    way = L2CacheSet[ind].way;
    for (int i = 0; i < 16; i++) {	//search in 16 ways
        temp = add2Long( way[i].addrTag, 16);
        if(temp == tag){
            memcpy(word, &way[i].bytes[offset], sizeof(int) * 16);
            memcpy(block, &way[i].bytes, sizeof(int) * 64);
            if(L2cacheLRU[ind][i] != 16){
                L2cacheLRU[ind][i] = 16;
                for (int j = 0; j < 16; j++) {
                    if(j != i && L2cacheLRU[ind][j] != 0)L2cacheLRU[ind][j]--;
                }
            }
            return true;
        }
    }
    return false;
}

void updateL2Cache(int * address, vector<L2cache> &L2CacheSet, int * bytes) {
    int validB = 1, dirtyB = 1, minEle, insert = 0;
    int L2tag[16], L2ind[7];
    L2CacheBlock newBlk;
    int ind;
    for (int i = 0; i < 16; i++) {
        L2tag[i] = address[i];
    }
    for (int i = 0; i < 7; i++) {
        L2ind[i] = address[i + 16];
    }
    ind = add2Long(L2ind, 7);
    memcpy(newBlk.addrTag, L2tag, sizeof(int) * 16);
    memcpy(newBlk.bytes, bytes, sizeof(int) * 64);
    newBlk.validB = validB;
    newBlk.dirtyB = dirtyB;
    minEle = L2cacheLRU[ind][0];
    // find out the least recent use, which has number of 1;
    for (int i = 1; i < 16; i++) {
        if(L2cacheLRU[ind][i] < minEle){
            insert = i;
            minEle = L2cacheLRU[ind][i];
        }
    }
    L2CacheSet[ind].way[insert] = newBlk;
    L2cacheLRU[ind][insert] = 16;
    for (int i = 0; i < 16; i++) {
        if(i != insert && L2cacheLRU[ind][i] != 0)L2cacheLRU[ind][i]--;
    }
    //cout << "update L2 cache" << endl;
    return;
}

int searchMemory(int * address, vector<memory> &memSet) {
    long long tag;
    int addrTag[32];
    for (int i = 0; i < 32; i++) {
        addrTag[i] = address[i];
    }
    tag = add2Long(addrTag, 32);
    for (int i = 0; i < 1024; i++) {
        if(tag == memSet[i].tag) {
            return i;
        }
    }
    return -1;
}

void updateMemory(int * address, vector<memory> &memSet) {
    memory newMem;
    for(int i = 0; i < 64; i++) {
        newMem.bytes[i] = randWord(i);
    }
    newMem.tag = add2Long(address, 32);
    memcpy(newMem.address, address, sizeof(int) * 32);
    if(memCount < 1024) {
    	memSet[memCount] = newMem;
	}
    else memSet.push_back(newMem);
    memCount++;
    //cout<<"update memory" << endl;
    return;
}

void phy(int * address, int * pageoff, int * result) {	// get the 32 bit physical address (page number, page offset)
    for (int i = 0; i < 16; i++) {
        result[i] = address[i];
    }
    for (int i = 0; i < 16; i++) {
        result[i + 16] = pageoff[i];
    }
    return;
}

int *word2Bit(int word) {
    int * res = (int * ) malloc ( 8 * sizeof(int) );
    for (int i = 7; i >= 0; i--) {
        res[i] = word % 2;
        word = word / 2;
    }
    return res;
}

vector<int> word2FullBit(int * word) {
    int *temp = NULL, j = 0;
    vector<int> res;
    for (int i = 0; i < 16; i++) {
        temp = word2Bit(word[j]);
        res.push_back(temp[0]);
        res.push_back(temp[1]);
        res.push_back(temp[2]);
        res.push_back(temp[3]);
        res.push_back(temp[4]);
        res.push_back(temp[5]);
        res.push_back(temp[6]);
        res.push_back(temp[7]);
        j++;
    }
    return res;
}

vector<int> getInst(Array arrays, inputInstr instr1) {
	//cout << "get instruction" << endl;
    bool iTLBmiss, TLBmiss, PTmiss;
    bool icachemiss, L2cachemiss, memorymiss;
    iTLBPTE instriTLB;
    TLBPTE instrTLB;
    int instrPT, instrMem, word[16], targetAddr[32], block[64];
    int rVPMiss = newRand(1), rVMemoryMiss = newRand(0);
    vector<int> operand;
    if(searchITLB(instr1.addr, arrays.iTLBarray, & instriTLB) == true){	// if hits in instruction TLB
        iTLBmiss = false;
        cout << "iTLB hit" << ";";
        phy(instriTLB.pageNum, instr1.addr, targetAddr);
    }
    else {
        cout << "iTLB miss" << "->";
        iTLBMiss++;
        iTLBmiss = true;
        if(searchTLB(instr1.addr, arrays.TLBarray, & instrTLB) == true){	// if hits in unified TLB
            TLBmiss = false;
            cout << "TLB hit" << ";";
            phy(instrTLB.pageNum, instr1.addr, targetAddr);
            updateITLB(instr1.addr, instrTLB.pageNum, arrays.iTLBarray);
        }
        else {
            cout << "TLBtype miss" << "->";
            TLBMiss++;
            TLBmiss = true;
            instrPT = searchPT(instr1.addr, arrays.PTarray);
            if(instrPT != -1){	// if hits in page table
                PTmiss = false;
                cout <<"Page table hit"<< ";";
                phy(arrays.PTarray[instrPT].physAddr, instr1.addr, targetAddr);
                updateTLB(arrays.PTarray[instrPT].index, arrays.PTarray[instrPT].physAddr, arrays.TLBarray);
                updateITLB(arrays.PTarray[instrPT].index, arrays.PTarray[instrPT].physAddr, arrays.iTLBarray);
            }
            else {
                cout << "Page Table miss" << "->";
                if(rVPMiss == 1){
                    cout << "Page Table fault" <<";";
                    pageFaults++;
                }
                else {
                    cout << "Page Table hit"<< ";";
                }
                updatePT(instr1.addr, arrays.PTarray);
                updateTLB(instr1.addr, arrays.PTarray[PTCount-1].physAddr, arrays.TLBarray);
                updateITLB(instr1.addr, arrays.PTarray[PTCount-1].physAddr, arrays.iTLBarray);
                instrPT = searchPT(instr1.addr, arrays.PTarray);
                phy(arrays.PTarray[instrPT].physAddr, instr1.addr, targetAddr);
            }
        }
    }
    cout << endl;
    cycles = cycles + 4;
    if(searchICache(targetAddr, arrays.iCacheSet, word) == true){
        cout << "icache hit" << ";";
        icachemiss = false;
    }
    else {
        cout << "icache miss"<< "->";
        iCacheMiss++;
        cycles = cycles + 8;
        if(searchL2Cache(targetAddr, arrays.L2CacheSet, word, block) == true){
            cout << "L2cache hit" << ";";
            L2cachemiss = false;
            updateICache(targetAddr, arrays.iCacheSet, block);
        }
        else {
            cout << "L2cache miss" << "->";
            L2CacheMiss++;
            cycles = cycles + 100;
            instrMem = searchMemory(targetAddr, arrays.memSet);
            if(instrMem != -1){
                cycles = cycles + 10000;
                cout << "Main Memory hit" << ";";
                updateL2Cache(targetAddr, arrays.L2CacheSet, arrays.memSet[instrMem].bytes);
                updateICache(targetAddr, arrays.iCacheSet, arrays.memSet[instrMem].bytes);
            }
            else {
                memorymiss = true;
                //cout << "L3cache miss" << "->";
                if(rVMemoryMiss == 1)cout << "Main Memory Miss" << ";";
                else cout << "Main Memory Hit;";
                updateMemory(targetAddr, arrays.memSet);
                //cout << "instr now" << endl;
                updateL2Cache(targetAddr, arrays.L2CacheSet, arrays.memSet[memCount - 1].bytes);
                //cout << "instr now" << endl;
                updateICache(targetAddr, arrays.iCacheSet, arrays.memSet[memCount - 1].bytes);
                //cout << "instr now" << endl;
            }
        }
    }
    cout << endl;
    if(searchICache(targetAddr, arrays.iCacheSet, word) == true){
        operand = word2FullBit(word);
    }
    return operand;
}

vector<int> getD(Array arrays, inputInstr instr1) {
    bool iTLBmiss, TLBmiss, PTmiss;
    bool dcachemiss, L2cachemiss, memorymiss;
    iTLBPTE instriTLB;
    TLBPTE instrTLB;
    int instrPT, instrMem, word[16], targetAddr[32], block[64];
    int rVPMiss = randWord(1), rVMemoryMiss = newRand(0);
    vector<int> operand;
    instr1.addr[0] = 0;
    if(searchITLB(instr1.addr, arrays.dTLBarray, & instriTLB) == true){
        iTLBmiss = false;
        cout << "dTLB hit" << ";";
        phy(instriTLB.pageNum, instr1.addr, targetAddr);
    }
    else {
        cout << "dTLB miss" << "->";
        dTLBMiss++;
        iTLBmiss = true;
        if(searchTLB(instr1.addr, arrays.TLBarray, & instrTLB) == true){
            TLBmiss = false;
            cout << "TLB hit" << ";";
            phy(instrTLB.pageNum, instr1.addr, targetAddr);
            updateITLB(instr1.addr, instrTLB.pageNum, arrays.iTLBarray);
        }
        else {
            cout << "TLBtype miss" << "->";
            TLBmiss = true;
            instrPT = searchPT(instr1.addr, arrays.PTarray);
            if(instrPT != -1){
                PTmiss = false;
                cout <<"page table hit"<< ";";
                phy(arrays.PTarray[instrPT].physAddr, instr1.addr, targetAddr);
                updateTLB(arrays.PTarray[instrPT].index, arrays.PTarray[instrPT].physAddr, arrays.TLBarray);
                updateITLB(arrays.PTarray[instrPT].index, arrays.PTarray[instrPT].physAddr, arrays.iTLBarray);
            }
            else {
                cout << "page Table miss" <<"->";
                PTmiss = true;
                if(rVPMiss == 1){
                    cout << "Page Table fault" << ";";
                    pageFaults++;
                }
                else {
                    cout << "Page Table Hit;";
                }
                updatePT(instr1.addr, arrays.PTarray);
                updateTLB(instr1.addr, arrays.PTarray[PTCount-1].physAddr, arrays.TLBarray);
                updateITLB(instr1.addr, arrays.PTarray[PTCount-1].physAddr, arrays.iTLBarray);
                instrPT = searchPT(instr1.addr, arrays.PTarray);
                phy(arrays.PTarray[instrPT].physAddr, instr1.addr, targetAddr);
            }
        }
    }
    cout << endl;
    if(searchDCache(targetAddr, arrays.dCacheSet, word) == true){
        cout << "dcache hit" << ";";
        cycles = cycles + 4;
        dcachemiss = false;
    }
    else {
        cout << "dcache miss"<< "->";
        dCacheMiss++;
        cycles = cycles + 8;
        if(searchL2Cache(targetAddr, arrays.L2CacheSet, word, block) == true){
            cout << "L2cache hit" << ";";
            L2cachemiss = false;
            updateDCache(targetAddr, arrays.dCacheSet, block);
        }
        else {
            cycles = cycles + 100;
            instrMem = searchMemory(targetAddr, arrays.memSet);
            L2CacheMiss++;
            cout << "L2cache miss" << "->";
            if(instrMem != -1){
                cout << "Main Memory hit" << ";";
                updateL2Cache(targetAddr, arrays.L2CacheSet, arrays.memSet[instrMem].bytes);
                updateDCache(targetAddr, arrays.dCacheSet, arrays.memSet[instrMem].bytes);
            }
            else {
                cycles = cycles + 10000;
                memorymiss = true;
                if(rVMemoryMiss == 1) {
                	cout << "Main Memory Miss" << ";";
				} else {
					cout << "Main Memory Hit;";
				}
                updateMemory(targetAddr, arrays.memSet);
                //cout << "now" << endl;
                updateL2Cache(targetAddr, arrays.L2CacheSet, arrays.memSet[memCount - 1].bytes);
                //cout << "now" << endl;
                updateDCache(targetAddr, arrays.dCacheSet, arrays.memSet[memCount - 1].bytes);
                //cout << "now" << endl;
            }
        }
    }
    cout << endl;
    if(searchDCache(targetAddr, arrays.dCacheSet, word) == true){
        operand = word2FullBit(word);
    }
    return operand;
}

inputInstr decode1(vector<int> operand){
    inputInstr instr;
    for (int i = 0; i < 6; i++) instr.brnCond[i] = operand[i + 2];
    for (int i = 0; i < 32; i++) instr.addr[i] = operand[i + 8];
    return instr;
}
inputInstr decode2(vector<int> operand){
    inputInstr instr;
    for (int i = 0; i < 6; i++) instr.brnCond[i] = operand[i + 2];
    for (int i = 0; i < 32; i++) instr.addr[i] = operand[i + 8];
    return instr;
}

void getVirAddr(int * virtualAddr, string s) {
    int temp[32], t;
    for (int i = 2; i < 10; i++) {
        if(s[i] >= 48 && s[i] <= 57) t = s[i] - 48;
        else if(s[i] >= 65 && s[i] <= 70) t = s[i] - 55;
        else if(s[i] >= 97 && s[i] <= 102) t = s[i] - 87;
        else cout << "input error!" << endl;
        temp[i * 4 + 3 - 8] = t % 2;
        temp[i * 4 + 2 - 8] = (t >> 1) % 2;
        temp[i * 4 + 1 - 8] = (t >> 2) % 2;
        temp[i * 4 + 0 - 8] = (t >> 3) % 2;
    }
    memcpy(virtualAddr, temp, sizeof(int) * 32);
    return;
}

void putDataInPT(vector<PagePTE> & PageTableset, int * virtualAddr, int * address) {
    long long row;
    PagePTE newPage;
    // sometime the random physical address may already in the Page Table
    row = add2Long(virtualAddr, 32);
    newPage.tag = row;
    memcpy(newPage.index, virtualAddr, sizeof(int) * 32);
    memcpy(newPage.physAddr, address, sizeof(int) * 32);
    if(PTCount < 1024){
        PageTableset[PTCount] = newPage;
    }
    else PageTableset.push_back(newPage);
    PTCount++;
    return; 
}

void putDataInMemory(vector<memory> &memSet, string a, int * address, int * offset) {
    int memorytag[32], bytes[64];
    memory newMem;
    int i = 0;
    for (int j= 0; j < 64; j++, i+=2) {
        
        if(i < 32){
            bytes[j] = (a[i] - '0') * 16 + (a[i + 1] - '0');
        }
        if(i >= 32 && i < 64){
            bytes[j] = (a[i - 32] - '0') * 16 + (a[i - 31] - '0');
        }
        if(i >= 64 && i < 96){
            bytes[j] = (a[i - 64] - '0') * 16 + (a[i - 63] - '0');
        }
        if(i >= 96 && i < 128){
            bytes[j] = (a[i - 96] - '0') * 16 + (a[i - 95] - '0');
        }
    }
    memcpy(newMem.bytes, bytes, sizeof(int) * 64);
    for (i = 0; i < 32; i++) {
        if(i < 16) memorytag[i] = address[i];
        else memorytag[i] = offset[i - 16];
    }
    memcpy(newMem.address, memorytag, sizeof(int) * 32);
    newMem.tag = add2Long(memorytag, 32);
    if(memCount < 1024) {
    	memSet[memCount] = newMem;
	}
    else memSet.push_back(newMem);
    memCount++;
    return;
}

void getOffset(int * offset, string s) {
    int temp[16], t;
    for (int i = 6; i < 10; i++) {
        if(s[i] >= 48 && s[i] <= 57) t = s[i] - 48;
        else if(s[i] >= 65 && s[i] <= 70) t = s[i] - 55;
        else if(s[i] >= 97 && s[i] <= 102) t = s[i] - 87;
        else cout << "input error!" << endl;
        temp[i * 4 + 3 - 24] = t % 2;
        temp[i * 4 + 2 - 24] = (t >> 1) % 2;
        temp[i * 4 + 1 - 24] = (t >> 2) % 2;
        temp[i * 4 + 0 - 24] = (t >> 3) % 2;
    }
    memcpy(offset, temp, sizeof(int) * 16);
    return;
}

string int2Str(vector<int> &arr, int n) {
    char temp;
    string res;
    int sum;
    if(n == 1){
        for(int i = 0; i < 10; i++) {
            sum = arr[i] * 8 + arr[i + 1] * 4 + arr[i + 2] * 2 + arr[i + 3];
            if(sum >= 10)temp = sum + 87;
            else temp = sum + 48;
            res += temp;
        }
    }
    if(n == 2){
        for(int i = 0; i < 10; i++) {
            sum = arr[i + 40] * 8 + arr[i + 41] * 4 + arr[i + 42] * 2 + arr[i + 43];
            if(sum >= 10)temp = sum + 87;
            else temp = sum + 48;
            res += temp;
        }
    }
    return res;
}

void exeInstr(inputInstr instr1, Array & arrays) {
    inputInstr a, b;
    vector<int> operand, data;
    int opcode, second;
    string PC;
    cout << "*******************************************************************"<<endl;
    operand = getInst(arrays, instr1);
    opcode = operand[0] * 2 + operand[1];
    second = operand[40] * 2 + operand[41];
    a = decode1(operand);
    b = decode2(operand);
    cout << "*******************************************************************"<<endl;
    if(opcode == 1) {
        cout << "load data;" << endl;
        data = getD(arrays, a);
        cout << "-------------------------------------------------------------------"<< endl;
    }
    else if(opcode == 0){
        cout << "store data;" << endl;
        data = getD(arrays, a);
        cout << "-------------------------------------------------------------------"<< endl;
    }
    else if(opcode == 2) {
        cout << "fetch branch address;"<< endl;
        PC = int2Str(operand, 1);
        cout << "PC is: "<< PC << endl;
        exeInstr(a, arrays);
        return;
    }
    else {
        cout << "excute another kind of instruction;"<< endl;
        cout << "-------------------------------------------------------------------"<< endl;
    }
    if(opcode != 2){
        if(second == 1) {
            cout << "load data;" << endl;
            data = getD(arrays, b);
            return;
        }
        else if(second == 0) {
            cout << "store data;" << endl;
            data = getD(arrays, b);
            return;
        }
        else if(second == 2) {
            cout << "fetch branch address;"<< endl;
            PC = int2Str(operand, 2);
            cout << "PC is: "<< PC << endl;
            exeInstr(b, arrays);
            return;
        }
        else {
            cout << "excute another kind of instruction;"<< endl;
            return;
        }
    }
}

int main() {
    string filename1 = "input";
    string filename2 = "program";
    ifstream fin, fpro, fex;
    fin.open(filename1.c_str());
    fpro.open(filename2.c_str());
    fex.open(filename1.c_str());
    string instr, PCcount;
    string line1;
    inputInstr instr1;
    int address[32], offset[16], virtualAddr[32];
    
    // get the instruction
    // initialize the caches and TLBs
    vector<iTLBtype> iTLBarr(16);	// instruction TLB
    vector<iTLBtype> dTLBarray(16);	// data TLB
    vector<TLBtype> TLBarray(128);	// unified TLB
    vector<PagePTE> PTarray(1024);	// global page table
    vector<icache> iCacheSet(64);	// instruction cache
    vector<dcache> dCacheSet(32);	// data cache
    vector<L2cache> L2CacheSet(128);	// L2 cache
    vector<memory> memSet(4096);	//main memory
    Array arrays(iTLBarr, dTLBarray, TLBarray, PTarray, iCacheSet, dCacheSet, L2CacheSet, memSet);
    initTLB(arrays.iTLBarray,arrays.dTLBarray,arrays.TLBarray);
    initCache(arrays.iCacheSet,arrays.dCacheSet,arrays.L2CacheSet); 
    
    vector<L2cache> testL2cache;
    while(getInstruction(fpro, instr)) {
        line1 = instr;
        if(getInstruction(fin, PCcount)) {
            getOffset(offset, PCcount);
            getVirAddr(virtualAddr, PCcount);
        }
        for (int i = 0; i < 32; i++) {
            address[i] = newRand(i);
        }
        putDataInPT(arrays.PTarray, virtualAddr, address);
        putDataInMemory(arrays.memSet, line1, address, offset);
    }
    while(getInstruction(fex, instr)) {
        instr1 = str2InputInstr(instr);
        cout << "PC is: "<< instr << endl;
        exeInstr(instr1, arrays);
        cout << "cycles is: " << cycles << endl;
        total_cycles = total_cycles + cycles;
        cout << "Total cycles is: " << total_cycles << endl;
        cycles = 0;
        cout << "===================================================================" << endl;
    }
    
    cout << "# of page fault is: " << pageFaults << endl;
    cout << "# of instruction TLBtype miss is: " << iTLBMiss << endl;
    cout << "# of data TLB miss is: " << dTLBMiss << endl;
    cout << "# of unified TLB miss is: " << TLBMiss << endl;
    cout << "# of instruction cache miss is: " << iCacheMiss << endl;
    cout << "# of data cache miss is: " << dCacheMiss << endl;
    cout << "# of L2 cache miss is: " << L2CacheMiss << endl;
    getchar();
    return 0;
}
