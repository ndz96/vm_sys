#include <chrono>
#include <cstdint>
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>
#include "System.h"
#include "part.h"
#include "vm_declarations.h"
#include "ProcessTest.h"
#include "SystemTest.h"

#define VM_SPACE_SIZE (100)
#define PMT_SPACE_SIZE (3000)
#define N_PROCESS (2)
#define PERIODIC_JOB_COST (1)

typedef std::chrono::high_resolution_clock myclock;
myclock::time_point x;

void start_timer() {
	x = myclock::now();
}

double get_seconds_passed() {
	myclock::duration d = myclock::now() - x;
	return  d.count() * 1.0 / 1000000000;
}

PhysicalAddress alignPointer(PhysicalAddress address) {
    uint64_t addr = reinterpret_cast<uint64_t> (address);

    addr += PAGE_SIZE;
    addr = addr / PAGE_SIZE * PAGE_SIZE;

    return reinterpret_cast<PhysicalAddress> (addr);
}

//int main() {
//    Partition part("p1.ini");
//
//	start_timer();
//
//    uint64_t size = (VM_SPACE_SIZE + 2) * PAGE_SIZE;
//    PhysicalAddress vmSpace = (PhysicalAddress ) new char[size];
//    PhysicalAddress alignedVmSpace = alignPointer(vmSpace);
//
//    size = (PMT_SPACE_SIZE + 2) * PAGE_SIZE;
//    PhysicalAddress pmtSpace = (PhysicalAddress ) new char[size];
//    PhysicalAddress alignedPmtSpace = alignPointer(pmtSpace);
//
//    System system(alignedVmSpace, VM_SPACE_SIZE, alignedPmtSpace, PMT_SPACE_SIZE, &part);
//    SystemTest systemTest(system, alignedVmSpace, VM_SPACE_SIZE);
//    ProcessTest* process[N_PROCESS];
//    std::thread *threads[N_PROCESS];
//
//    std::mutex globalMutex;
//
//    for (int i = 0; i < N_PROCESS; i++) {
//		process[i] = new ProcessTest(system, systemTest);
//    }
//	
//	for (int i = 0; i < N_PROCESS; i++) {
//		std::cout << "Create process " << i << std::endl;
//        threads[i] = new std::thread(&ProcessTest::run, process[i]);
//    }
//
//    Time time;
//    while ((time = system.periodicJob())) {
//        std::this_thread::sleep_for(std::chrono::microseconds(time));
//
//        std::lock_guard<std::mutex> guard(systemTest.getGlobalMutex());
//
//        std::cout << "Doing periodic job\n";
//
//        std::this_thread::sleep_for(std::chrono::microseconds(PERIODIC_JOB_COST));
//
//        bool finished = true;
//        for (int i = 0; i < N_PROCESS; i++) {
//            finished = finished && process[i]->isFinished();
//        }
//
//        if (finished) {
//            break;
//        }
//    }
//
//    for (int i = 0; i < N_PROCESS; i++) {
//        threads[i]->join();
//        delete threads[i];
//        delete process[i];
//    }
//
//    delete [] vmSpace;
//    delete [] pmtSpace;
//
//	std::cout << "Test finished\n" << std::endl;
//	std::cout << get_seconds_passed();
//}