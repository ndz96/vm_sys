#pragma once

#include "Process.h"
#include "part.h"
#include "pmt.h"
#include <mutex>

class KernelProcess {
public:
	KernelProcess(ProcessId pid);
	~KernelProcess();
	ProcessId getProcessId() const;
	Status createSegment(VirtualAddress startAddress, PageNum segmentSize,
		AccessType flags);
	Status loadSegment(VirtualAddress startAddress, PageNum segmentSize,
		AccessType flags, void* content);
	Status deleteSegment(VirtualAddress startAddress);
	Status pageFault(VirtualAddress address);
	PhysicalAddress getPhysicalAddress(VirtualAddress address);

	/// My functions
	bool setup(KernelSystem* _sys, PMTAllocator* _pmt_alloc, 
			   Partition* _partition,
			   std::recursive_mutex* _mtx) {
					mtx = _mtx;
					sys = _sys;
					pmt_alloc = _pmt_alloc;
					pmt = pmt_alloc->allocatePMT(id);
					partition = _partition;
					if (pmt == nullptr)
						return false;
					return true;
				}

	Process* clone(ProcessId pid);
	Status createSharedSegment(VirtualAddress startAddress,
		PageNum segmentSize, const char* name, AccessType flags);
	Status disconnectSharedSegment(const char* name);
	Status deleteSharedSegment(const char* name);
private:
	std::recursive_mutex* mtx;
	KernelSystem* sys;
	PMTAllocator* pmt_alloc;
	Partition* partition;
	PMT* pmt;
	unsigned id;
	std::vector<std::pair<VirtualAddress, std::string>> sharedSegs;

	friend KernelSystem;
};
