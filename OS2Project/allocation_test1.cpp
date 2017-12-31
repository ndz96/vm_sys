#include "part.h"
#include "vm_declarations.h"
#include "System.h"
#include "Process.h"
#include <iostream>

using namespace std;

const PageNum PMT_PAGES = 20;
const PageNum MEM_PAGES = 20;
const ClusterNo NUM_CLUSTERS = 10000;

#define ss(page) (page << PAGE_BITS)

//int main() {
//	Partition* p = new Partition("p1.ini");
//	void* pmtSpace = ::operator new(PAGE_SIZE * PMT_PAGES);
//	void* memSpace = ::operator new(PAGE_SIZE * MEM_PAGES);
//	System* s = new System(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, p);
//	Process* p1 = s->createProcess();
//	Process* p2 = s->createProcess();
//	p1->createSegment(ss(0), 10, 3UL);
//	p1->createSegment(ss(10), 2, 3UL);
//	p2->createSegment(ss(0), 20, 3UL);
//	p2->createSegment(ss(20), 22, 3UL);
//	p2->deleteSegment(ss(0));
//	p2->deleteSegment(ss(20));
//	p1->deleteSegment(ss(0));
//	p1->deleteSegment(ss(10));
//	delete p2;
//	delete p1;
//	delete s;
//	return 0;
//}