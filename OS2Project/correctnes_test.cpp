//#include "part.h"
//#include "vm_declarations.h"
//#include "System.h"
//#include "Process.h"
//#include "assert.h"
//
//#include <iostream>
//#include <thread>
//#include <mutex>
//#include <random>
//#include <chrono>
//
//using namespace std;
//
/////MULTITHREADED correctness test
//#define SHARED
//#undef SHARED
//
//#ifdef SHARED
//const PageNum SHARED_SEG_SIZE = 32;
//void* shared_seg_content;
//#endif
//
////fixed
//const PageNum PMT_PAGES = 2000;
//const ClusterNo NUM_CLUSTERS = 30000;
//#define PAGE_BITS 10
//#define ss(page) (page << PAGE_BITS)
//#define PF_TIME
//#define MEASURE
//#undef MEASURE
//
////changeable
//const PageNum MEM_PAGES = 32;
//const PageNum SEG_SIZE = 32;
//const ProcessId NUM_PROC = 5;
//const unsigned REP = 5;
//
//mutex GLOBAL_LOCK;
//
//typedef std::chrono::high_resolution_clock myclock;
//myclock::time_point beginning[2 * NUM_PROC + 1];
//default_random_engine generator;
//uniform_int_distribution<int> distribution(0, 25);
//
//void start_timer(ProcessId pid) {
//	beginning[pid] = myclock::now();
//}
//
//double get_seconds_passed(ProcessId pid) {
//	myclock::duration d = myclock::now() - beginning[pid];
//	return  d.count() * 1.0 / 1000000000;
//}
//
//char rand_letter() {
//	return 'a' + distribution(generator);
//}
//
//void rotate(char* c, int n) { //rotate this character n times
//	int diff = (*c) - 'a';
//	diff = (diff + n) % 26;
//	*c = 'a' + diff;
//}
//
//unsigned page_fault_counter = 0;
//
//long double total_secs_access = 0, total_secs_pf = 0, total_secs_getAddress = 0;
//long double total_iter_access = 0, total_iter_pf = 0, total_iter_getAddress = 0;
//
//void rotate(System* sys, Process* p, ProcessId pid, VirtualAddress vaddr, unsigned rep) {
//	GLOBAL_LOCK.lock();
//	start_timer(pid);
//	Status status = sys->access(pid, vaddr, AccessType::READ_WRITE);
//	total_secs_access += get_seconds_passed(pid);
//	total_iter_access++;
//
//	assert(status != Status::TRAP);
//	if (status == Status::PAGE_FAULT) {
//		start_timer(pid);
//		p->pageFault(vaddr);
//		total_secs_pf += get_seconds_passed(pid);
//		page_fault_counter++;
//		total_iter_pf++;
//	}
//	start_timer(pid);
//	char* paddr = (char*)p->getPhysicalAddress(vaddr);
//	total_secs_getAddress += get_seconds_passed(pid);
//	total_iter_getAddress++;
//	rotate(paddr, 1);
//#ifdef MEASURE
//	if (vaddr == PAGE_SIZE * SEG_SIZE - 1 && rep == 0)
//		cout << pid << " : " << get_seconds_passed(pid) << endl;
//#endif
//	GLOBAL_LOCK.unlock();
//}
//
//
//void f(System* sys) {
//	Process* p = sys->createProcess();
//	ProcessId pid = p->getProcessId();
//
//	//load address space [0-PG_SZ*SG_SZ)
//	void* starting_content = ::operator new(PAGE_SIZE * SEG_SIZE);
//	for (VirtualAddress vaddr = 0; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr)
//		*((char*)starting_content + vaddr) = rand_letter();
//	assert(p->loadSegment(ss(0), SEG_SIZE, 3UL, starting_content) != Status::TRAP);
//
//#ifdef SHARED
//	//connect shared segment
//	p->createSharedSegment(ss(SEG_SIZE), SHARED_SEG_SIZE, "ss", 2UL);
//#endif
//
//	unsigned rep = REP;
//
//#ifdef MEASURE
//	start_timer(pid);
//#endif
//
//	///Rep times rotate address space content
//	while (rep--) {
//#ifdef SHARED
//		//rotate shared seg by 1 -> odd processes
//		if (pid & 1) {
//			for (VirtualAddress vaddr = ss(SEG_SIZE); vaddr < ss(SEG_SIZE) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr)
//				rotate(sys, p, pid, vaddr, rep);
//		}
//#endif
//
//		//rotate standard seg by 1
//		for (VirtualAddress vaddr = 0UL; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr)
//			rotate(sys, p, pid, vaddr, rep);
//
//#ifdef SHARED
//		//rotate shared seg by 1 -> even processes
//		if (!(pid & 1)) {
//			for (VirtualAddress vaddr = ss(SEG_SIZE); vaddr < ss(SEG_SIZE) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr)
//				rotate(sys, p, pid, vaddr, rep);
//		}
//#endif
//		//end rep
//	}
//
//	///CHECK NOW!
//	//read em in the end, check REP times rotated = content now
//	for (VirtualAddress vaddr = 0UL; vaddr < PAGE_SIZE * SEG_SIZE; ++vaddr) {
//		GLOBAL_LOCK.lock();
//		Status status = sys->access(pid, vaddr, AccessType::READ);
//		assert(status != Status::TRAP);
//		if (status == Status::PAGE_FAULT)
//			p->pageFault(vaddr);
//		char* paddr = (char*)p->getPhysicalAddress(vaddr);
//		///
//		char* now = paddr;
//		char* before = (char*)starting_content + vaddr;
//		rotate(before, REP);
//		assert(*now == *before);
//		GLOBAL_LOCK.unlock();
//	}
//	delete p;
//}
//
////int main() {
////	Partition* part = new Partition("p1.ini");
////	void* pmtSpace = ::operator new(PAGE_SIZE * PMT_PAGES);
////	void* memSpace = ::operator new(PAGE_SIZE * MEM_PAGES);
////	System* sys = new System(memSpace, MEM_PAGES, pmtSpace, PMT_PAGES, part);
////#ifdef SHARED
////	Process* dummy = sys->createProcess();
////	dummy->createSharedSegment(ss(SEG_SIZE), SHARED_SEG_SIZE, "ss", 2UL);
////	//write initial values by dummy
////	dummy->createSharedSegment(ss(SEG_SIZE), SHARED_SEG_SIZE, "ss", 2UL);
////	shared_seg_content = ::operator new(PAGE_SIZE * SHARED_SEG_SIZE);
////
////	//write down initial random content to shared_seg_vaddr_space
////	for (VirtualAddress vaddr = ss(SEG_SIZE); vaddr < ss(SEG_SIZE) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr) {
////		*((char*)shared_seg_content + vaddr - ss(SEG_SIZE)) = rand_letter();
////		Status status = sys->access(dummy->getProcessId(), vaddr, AccessType::WRITE);
////		assert(status != Status::TRAP);
////		if (status == Status::PAGE_FAULT)
////			dummy->pageFault(vaddr);
////		char* paddr = (char*)dummy->getPhysicalAddress(vaddr);
////		*paddr = ((char*)shared_seg_content)[vaddr - ss(SEG_SIZE)];
////	}
////
////	//check if ya read it good
////	Process* suka = sys->createProcess();
////	suka->createSharedSegment(ss(SEG_SIZE), SHARED_SEG_SIZE, "ss", 2UL); //connect it for reading
////	for (VirtualAddress vaddr = ss(SEG_SIZE); vaddr < ss(SEG_SIZE) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr) {
////		Status status = sys->access(suka->getProcessId(), vaddr, AccessType::READ);
////		assert(status != Status::TRAP);
////		if (status == Status::PAGE_FAULT)
////			suka->pageFault(vaddr);
////		char* paddr = (char*)suka->getPhysicalAddress(vaddr);
////		char* paddrtest = (char*)dummy->getPhysicalAddress(vaddr);
////		assert(paddr == paddrtest);
////		assert(*paddr == ((char*)shared_seg_content)[vaddr - ss(SEG_SIZE)]);
////	}
////	suka->disconnectSharedSegment("ss");
////#endif
////
////	thread** thr;
////	thr = new thread*[NUM_PROC];
////	for (ProcessId pid = 0; pid < NUM_PROC; ++pid)
////		thr[pid] = new thread(f, sys);
////
////	for (ProcessId pid = 0; pid < NUM_PROC; ++pid)
////		thr[pid]->join();
////
////#ifdef SHARED
////	///CHECK shared content
////	for (VirtualAddress vaddr = ss(SEG_SIZE); vaddr < ss(SEG_SIZE) + SHARED_SEG_SIZE * PAGE_SIZE; ++vaddr) {
////		Status status = sys->access(dummy->getProcessId(), vaddr, AccessType::READ);
////		assert(status != Status::TRAP);
////		if (status == Status::PAGE_FAULT)
////			dummy->pageFault(vaddr);
////		char* paddr = (char*)dummy->getPhysicalAddress(vaddr);
////		///
////		char* now = paddr;
////		char* before = (char*)shared_seg_content + vaddr - ss(SEG_SIZE);
////		rotate(before, REP*NUM_PROC);
////		assert(*now == *before);
////	}
////#endif
////
////	cout << "TOTAL PAGE FAULTS: " << page_fault_counter << endl;
////	cout << "BENCHMARK" << endl;
////	cout << "ACCESS: " << total_secs_access / total_iter_access << endl;
////	cout << "PAGEFAULT: " << total_secs_pf / total_iter_pf << endl;
////	cout << "GETADDRESS: " << total_secs_getAddress / total_iter_getAddress << endl;
////	delete sys;
////	return 0;
////}