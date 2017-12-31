#pragma once

#include "System.h"
#include "Process.h"
#include "part.h"
#include "pmt.h"
#include <set>
#include <iostream>
#include <vector>
#include <mutex>
#include <map>
#include <string>
#include <algorithm>
#include <thread>

class PMTAllocator;
class PMT;

typedef unsigned FrameNum;

struct FrameDesc {
	bool isFree;
	Descriptor* myDesc;
	std::mutex mtx;
	///ProcessId pid; local polytics/thrashing -> for replacement algorithm???
};

class KernelSystem {
public:
	KernelSystem(PhysicalAddress processVMSpace, PageNum processVMSpaceSize,
		PhysicalAddress pmtSpace, PageNum pmtSpaceSize,
		Partition* partition);
	~KernelSystem();
	Process* createProcess();

	Time periodicJob();
	FrameNum getVictim();
	// Hardware job
	Status access(ProcessId pid, VirtualAddress address, AccessType type);
	Process* cloneProcess(ProcessId pid);

	/// My functions
	void refreshFrame(FrameNum frame);

	void erase(ProcessId pid) {
		processes[pid].second = 0;
	}

	PMT* getPMT(ProcessId id);
	PMTAllocator* getPMTAllocator();
	Status pageFault(PMT* pmt, VirtualAddress address);
private:
	int findBestCandidate(bool dirty);
	PhysicalAddress getFrameAddress(FrameNum frame);
	Status eraseSharedSegId(std::string name, ProcessId id);
	Status deleteSharedSegment(const char* name);

	unsigned global_id;
	PhysicalAddress processVMSpace;
	PageNum processVMSpaceSize;
	Partition* partition;
	
	std::recursive_mutex mtx;

	// Memory management
	size_t processes_size = 0;
	std::vector<std::pair<KernelProcess*, bool>> processes;
	std::pair<KernelProcess*, bool>* processesWrap;
	PMTAllocator* pmt_alloc;

	FrameDesc* frames;
	std::set<FrameNum> freeFrames;
	FrameNum clockHand;

	// Shared segments
	std::map<std::string, std::vector<std::pair<ProcessId, VirtualAddress>>> sharedSegIds;
	std::map<std::string, std::pair<VirtualAddress, PageNum>> sharedAddrSeg;
	std::map<std::string, ProcessId> sharedHiddenProcId;
	std::map<std::string, AccessType> sharedFlags;

	// CoW
	bool apply_cow = false;

#ifdef NEW_THREAD
	// Saver thread
	unsigned lastAccessedFrame = -1;
	void startSaverThread();
	std::thread *saverThread;
	static void stfunc(KernelSystem* sys);
	bool end;
#endif

#ifdef DIAGNOSTICS
	unsigned pf_load;
	unsigned pf_save;
#endif

	friend KernelProcess;
};
