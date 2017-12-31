#include "part.h"
#include "vm_declarations.h"
#include "System.h"
#include "KernelSystem.h"
#include "pmt.h"
#include "Process.h"
#include "assert.h"
#include <iostream>

using namespace std;

const PageNum PMT_PAGES = 50;
const PageNum MEM_PAGES = 20;
const ClusterNo NUM_CLUSTERS = 30000;

#define ss(page) (page << PAGE_BITS)

const PageNum PAGE_GAP = 20;
const unsigned REP = 64;

///Check for Descriptor content & access

//int main() {
//	Partition* p = new Partition("p1.ini");
//	void* pmtSpace = ::operator new(PAGE_SIZE * PMT_PAGES);
//	void* memSpace = ::operator new(PAGE_SIZE * MEM_PAGES);
//	KernelSystem* sys = new KernelSystem(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, p);
//	Process* proc = sys->createProcess();
//	// PMTAllocator* alloc = sys->getPMTAllocator();
//	AccessRight read = 1UL;
//	AccessRight write = 2UL;
//	AccessRight execute = 4UL;
//
//	proc->createSegment(ss(0), PAGE_GAP, read+write);
//	proc->createSegment(ss(PAGE_GAP + 1), PAGE_GAP, read);
//
//	assert(sys->access(proc->getProcessId()+1, ss(0) + 512, AccessType::READ_WRITE) == Status::TRAP); //illegal process
//	assert(sys->access(proc->getProcessId(), ss(PAGE_GAP) + 512, AccessType::READ) == Status::TRAP); //illegal address
//	assert(sys->access(proc->getProcessId(), ss(0) + 512, AccessType::EXECUTE) == Status::TRAP); //illegal access_right
//	assert(sys->access(proc->getProcessId(), ss(0) + 512, AccessType::READ_WRITE) == Status::PAGE_FAULT); //legal access
//
//	/*cout << endl << endl;
//	cout << *(alloc->getDescriptor(0, 15));
//	cout << *(alloc->getDescriptor(0, PAGE_GAP+1));*/
//	//cout << alloc->numFreeClusters() << " " << alloc->numFreePages();
//
//	delete proc;
//	delete sys;
//	return 0;
//}