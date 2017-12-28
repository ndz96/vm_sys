#pragma once

#include "vm_declarations.h"
#include "part.h"
#include <set>
#include <vector>
#include <queue>
#include <map>
#include <iostream>

#define PMT_L1_BITS 7 //higher 7 bits
#define PMT_L2_BITS 7 //lower 7 bits

#define VALID_POS 31
#define R_POS 30
#define W_POS 29
#define X_POS 28
#define REF_THRASHING_POS 27
#define REF_CLOCKHAND_POS 26
#define DIRTY_POS 25
#define SHARED_POS 24
#define CLONED_POS 23
#define DESC_VALID_POS 22

struct Descriptor;
union UN{
	unsigned cluster; //if standard segment
	Descriptor* hiddenDesc; //if shared segment
};

struct Descriptor {
	UN un;
	unsigned frame : 22;
	unsigned valid : 1;
	unsigned r : 1;
	unsigned w : 1;
	unsigned x : 1;
	unsigned ref_thrash : 1;
	unsigned ref_clock : 1;
	unsigned dirty : 1;
	unsigned shared : 1;
	unsigned cloned : 1;
	unsigned desc_valid : 1;
	//helper
	inline bool getbit(short pos) const{ return (frame & (1 << pos)) > 0; }
	inline void setbit(short pos, bool b) { b ? (frame |= (1 << pos)) : (frame &= ~(1 << pos)); }
public:
	//getters
	//bool getValid() const{ return getbit(VALID_POS); }
	//bool getR() const{ return getbit(R_POS); }
	//bool getW() const{ return getbit(W_POS); }
	//bool getX() const{ return getbit(X_POS); }
	//bool getThrashingRef() const{ return getbit(REF_THRASHING_POS); }
	//bool getClockHandRef() const { return getbit(REF_CLOCKHAND_POS); }
	//bool getDirty() const { return getbit(DIRTY_POS); }
	//bool getShared() const { return getbit(SHARED_POS); }
	//bool getCloned() const { return getbit(CLONED_POS); }
	//bool getDescValid() const{ return getbit(DESC_VALID_POS); }
	//unsigned getFrame() const{ return frame & ((1 << DESC_VALID_POS) - 1); }
	//unsigned getCluster() const{ return un.cluster; }
	//ProcessId getHiddenId() const { return un.hiddenId; }

	////setters
	//void setValid(bool b) { setbit(VALID_POS, b); }
	//void setR(bool b) { setbit(R_POS, b); }
	//void setW(bool b) { setbit(W_POS, b); }
	//void setX(bool b) { setbit(X_POS, b); }
	//void setThrashingRef(bool b) { setbit(REF_THRASHING_POS, b); }
	//void setClockHandRef(bool b) { setbit(REF_CLOCKHAND_POS, b); }
	//void setDirty(bool b) { setbit(DIRTY_POS, b); }
	//void setShared(bool b) { setbit(SHARED_POS, b); }
	//void setCloned(bool b) { setbit(CLONED_POS, b); }
	//void setDescValid(bool b) { setbit(DESC_VALID_POS, b); }
	//void setFrame(unsigned _frame) {
	//	//reset frame
	//	frame &= ~((1 << DESC_VALID_POS) - 1);
	//	//set frame
	//	frame |= _frame;
	//}
	//void setCluster(unsigned _cluster) {
	//	un.cluster = _cluster;
	//}
	//void setHiddenId(ProcessId hiddenId) {
	//	un.hiddenId = hiddenId;
	//}
	
	/*friend std::ostream& operator<<(std::ostream& out, Descriptor& desc) {
		using namespace std;
		cout << "VALID: " << desc.getValid() << endl;
		cout << "Klaster: " << desc.getCluster() << endl;
		cout << "Frame: " << desc.getFrame() << endl;
		cout << "R: " << desc.getR() << endl;
		cout << "W: " << desc.getW() << endl;
		cout << "X: " << desc.getX() << endl;
		cout << "Ref: " << desc.getThrashingRef() << endl;
		cout << "DescVALID: " << desc.getDescValid() << endl;
		return out;
	}*/
};

struct PMTL2 {
	Descriptor desc[1 << PMT_L2_BITS];
};

class PMT{
public:
	unsigned pageL2[1 << PMT_L1_BITS];
	unsigned cntChildren[1 << PMT_L1_BITS];
};

class KernelSystem;

class PMTAllocator {
public:
	PMTAllocator(KernelSystem* _sys, PhysicalAddress _pmtSpace, PageNum _pmtSpaceSize, 
			Partition* _partition);
	PMT* allocatePMT(ProcessId id);
	void deallocatePMT(ProcessId id);
	Status allocateSegment(ProcessId id, VirtualAddress startAddress, PageNum segmentSize, AccessType flags,
						   bool shared, bool cloned, ProcessId hiddenProcessId, VirtualAddress sharedStartAddress);
	Status deleteSegment(ProcessId id, VirtualAddress startAddress);
	Descriptor* getDescriptor(PMT* pmt, PageNum pg);

	//debug
	inline unsigned numFreePages() { return freePages.size(); }
	inline unsigned numFreeClusters() { return freeClusters.size(); }
private:
	void* align_page(PageNum);
	inline static unsigned pmt_entry(PageNum page) {
		return page >> PMT_L2_BITS;
	}
	inline static unsigned pmtL2_entry(PageNum page) {
		return page & ((1 << PMT_L2_BITS) - 1);
	}
	PhysicalAddress pmtSpace;
	PageNum pmtSpaceSize;
	Partition* partition;
	KernelSystem* sys;

	std::queue<PageNum> freePages;
	std::queue<ClusterNo> freeClusters;
	std::map<ProcessId, std::vector<std::pair<PageNum, PageNum>>> segments;
	std::map<ProcessId, PageNum> idpmt_page;
};