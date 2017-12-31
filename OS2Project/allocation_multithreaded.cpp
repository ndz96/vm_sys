#include "part.h"
#include "vm_declarations.h"
#include "System.h"
#include "Process.h"
#include "assert.h"
#include <iostream>

#include <thread>
#include <mutex>

using namespace std;

const PageNum PMT_PAGES = 50;
const PageNum MEM_PAGES = 20;
const ClusterNo NUM_CLUSTERS = 30000;

#define ss(page) (page << PAGE_BITS)

const PageNum PAGE_GAP = 20;
const unsigned REP = 64;

///MULTITHREADED allocation/deallocation

using namespace std;
const unsigned ITER = 107;
const unsigned PAGES = 6260;

void thread_allocator(Process* p) {
	unsigned i = ITER, pg = 0;
	while (i--) {
		(p->createSegment(ss(pg), PAGE_GAP, 3UL) == Status::OK) ? cout<<"allocator: OK" : cout <<"allocator: TRAP";
		cout << i << endl;
		pg += PAGE_GAP;
		if (pg == 6260)
			pg = 0;
	}
}

void thread_deallocator(Process* p) {
	unsigned i = ITER, pg = 0;
	while (i--) {
		(p->deleteSegment(ss(pg)) == Status::OK) ? cout << "delete: OK" : cout << "delete: TRAP";
		cout << i << endl;
		pg += PAGE_GAP;
		if (pg == 6260)
			pg = 0;
	}
}

//int main() {
//	Partition* part = new Partition("p1.ini");
//	void* pmtSpace = ::operator new(PAGE_SIZE * PMT_PAGES);
//	void* memSpace = ::operator new(PAGE_SIZE * MEM_PAGES);
//	System* sys = new System(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, part);
//	Process* p = sys->createProcess();
//
//	/*unsigned ps = 0;
//	while (p->createSegment(ss(ps), PAGE_GAP, 3UL) == Status::OK)
//		ps += PAGE_GAP;*/
//
//	thread t1(thread_allocator, p);
//	thread t2(thread_deallocator, p);
//
//	t1.join();
//	t2.join();
//
//	delete sys;
//	return 0;
//}