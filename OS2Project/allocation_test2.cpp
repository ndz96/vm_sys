#include "part.h"
#include "vm_declarations.h"
#include "System.h"
#include "Process.h"
#include "assert.h"
#include <iostream>

using namespace std;

const PageNum PMT_PAGES = 10;
const PageNum MEM_PAGES = 20;
const ClusterNo NUM_CLUSTERS = 30000;

#define ss(page) (page << PAGE_BITS)

const PageNum PAGE_GAP = 5;
const unsigned REP = 224;

///Simple allocation and deallocation

//int main() {
//	Partition* p = new Partition("p1.ini");
//	void* pmtSpace = ::operator new(PAGE_SIZE * PMT_PAGES);
//	void* memSpace = ::operator new(PAGE_SIZE * MEM_PAGES);
//	System* s = new System(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, p);
//	Process* process[5];
//	unsigned ps = 0;
//	for (int i = 0; i < 5; ++i)
//		process[i] = s->createProcess();
//	for (int k = 0; k < REP; ++k) {
//		for (int i = 0; i < 5; ++i) {
//			if (process[i]->createSegment(ss(ps), PAGE_GAP, 3UL) != Status::OK) {
//				cout << "NOT OK at REP: " << k << "process: " << i << endl;
//				goto END;
//			}
//		}
//		ps += PAGE_GAP;
//	}
//END:
//	ps = 0;
//	for (int k = 0; k < 25; ++k) {
//		for (int i = 0; i < 5; ++i)
//			assert(process[i]->deleteSegment(ss(ps)) == Status::OK);
//		ps += PAGE_GAP;
//	}
//
//
//	for (int i = 0; i < 5; ++i)
//		delete process[i];
//
//	delete s;
//	return 0;
//}