#include "KernelSystem.h"
#include "KernelProcess.h"
#include "assert.h"

KernelSystem::KernelSystem(PhysicalAddress _processVMSpace, PageNum _processVMSpaceSize, 
		PhysicalAddress _pmtSpace, PageNum _pmtSpaceSize,
		Partition* _partition)
{
	std::lock_guard<std::recursive_mutex> lck(mtx);
	global_id = 0;
	processVMSpace = _processVMSpace;
	processVMSpaceSize = _processVMSpaceSize;
	partition = _partition;
	pmt_alloc = new PMTAllocator(this, _pmtSpace, _pmtSpaceSize, partition);

	frames = new FrameDesc[processVMSpaceSize];
	for (FrameNum fr = 0; fr < processVMSpaceSize; ++fr) {
		freeFrames.insert(fr);	
		frames[fr].isFree = true;
		frames[fr].myDesc = nullptr;
	}
	clockHand = 0;
	processes_size = 0;
	processesWrap = processes.data();
}

KernelSystem::~KernelSystem()
{
	std::lock_guard<std::recursive_mutex> lck(mtx);
	for (ProcessId id = 0; id < processes.size(); ++id) {
		if (processes[id].second) {
			//std::cout << "Proces: " << id << " ostao ziv - unistavam!" << std::endl;
			delete processes[id].first;
		}
	}
	delete pmt_alloc;
}

Process* KernelSystem::createProcess()
{
	std::lock_guard<std::recursive_mutex> lck(mtx);
	Process* proc = new Process(global_id);
	KernelProcess* kproc = proc->pProcess;
	if (!kproc->setup(this, pmt_alloc, partition, &mtx))
		return nullptr;
	processes.push_back(std::make_pair(kproc, true));
	global_id++;
	processes_size++;
	processesWrap = processes.data();
	return proc;
}

Status KernelSystem::access(ProcessId pid, VirtualAddress address, AccessType type)
{
	std::lock_guard<std::recursive_mutex> lck(mtx);
	//dont access like this due to debug
	//std::pair<KernelProcess*, bool> kproc = processes[pid];

	//invalid process id
	if (pid < 0 || pid >= processes_size)
		return Status::TRAP;
	if (!processesWrap[pid].second)
		return Status::TRAP;

	register Descriptor* desc = pmt_alloc->getDescriptor(processesWrap[pid].first->pmt, page(address));
	//invalid address
	if (desc == nullptr)
		return Status::TRAP;

	//invalid access type
	if (type == AccessType::READ && !desc->r)
		return Status::TRAP;
	if (type == AccessType::WRITE && !desc->w)
		return Status::TRAP;
	if (type == AccessType::READ_WRITE && (!desc->r || !desc->w))
		return Status::TRAP;
	if (type == AccessType::EXECUTE && !desc->x)
		return Status::TRAP;

	//if shared propagate to hidden process
	if (desc->shared)
		desc = desc->un.hiddenDesc;

	//set relevant bits
	desc->ref_thrash = 1;
	desc->ref_clock = 1;
	if (type == AccessType::WRITE || type == AccessType::READ_WRITE)
		desc->dirty = 1;
	
	//not in memory
	if (!desc->valid)
		return Status::PAGE_FAULT;
	
	return Status::OK;
}

Process* KernelSystem::cloneProcess(ProcessId pid)
{
	std::lock_guard<std::recursive_mutex> lck(mtx);

	// if not alive
	if (pid < 0 || pid >= processes.size() || !processes[pid].second)
		return nullptr;

	/*Process* p = createProcess();
	ProcessId id = p->getProcessId();*/




	return nullptr;
}

Status KernelSystem::eraseSharedSegId(std::string name, ProcessId id)
{
	if (!sharedSegIds.count(name))
		return Status::TRAP;

	std::vector<std::pair<ProcessId, VirtualAddress>>::iterator it = sharedSegIds[name].begin();
	for (size_t i = 0; i < sharedSegIds[name].size(); ++i, ++it) {
		if (sharedSegIds[name][i].first == id) {
			//process id shares segment named name
			//delete only its pmt space
			assert(pmt_alloc->deleteSegment(id, sharedSegIds[name][i].second)
				== Status::OK);
			//not sharing anymore
			sharedSegIds[name].erase(it);
			return Status::OK;
		}
	}

	return Status::TRAP;
}

Status KernelSystem::deleteSharedSegment(const char* _name)
{
	std::string name(_name);
	if (!sharedSegIds.count(name))
		return Status::TRAP;

	// disconnect all remaining processes
	for (size_t i = 0; i < sharedSegIds[name].size(); ++i) {
		ProcessId pid = sharedSegIds[name][i].first;
		assert(processes[pid].first->disconnectSharedSegment(_name) == Status::OK);
	}

	// delete hidden proc alongside its only standard segment
	ProcessId hiddenProc = sharedHiddenProcId[name];
	delete processes[hiddenProc].first;

	// update structs
	sharedSegIds.erase(name);
	sharedHiddenProcId.erase(name);
	sharedAddrSeg.erase(name);
	sharedFlags.erase(name);
	return Status::OK;
}

PMT* KernelSystem::getPMT(ProcessId id) {
	if (id < 0 || id >= processes.size())
		return nullptr;
	if (!processes[id].second)
		return nullptr;
	//OK, process is alive
	return processes[id].first->pmt;
}

PMTAllocator* KernelSystem::getPMTAllocator()
{
	return pmt_alloc;
}

Status KernelSystem::pageFault(PMT* pmt, VirtualAddress address)
{
	PageNum pg = page(address);
	//get its descriptor and cluster
	Descriptor* myDesc = pmt_alloc->getDescriptor(pmt, pg);
	if (myDesc->shared)
		myDesc = myDesc->un.hiddenDesc;
	
	//select frame of page to swap out
	FrameNum frame = getVictim();

	if (!frames[frame].isFree) {
		Descriptor* desc = frames[frame].myDesc;
		desc->valid = 0; //not in memory anymore
		if (desc->dirty) {
			//is dirty
			desc->dirty = 0; //not dirty anymore
			//write new version to its cluster 
			ClusterNo cluster = desc->un.cluster;
			if (!partition->writeCluster(cluster, (char*)getFrameAddress(frame))) {
				//should not happen
				std::cout << "PARTITION SAVE FAILED!";
				desc->dirty = 1;
				desc->valid = 1;
				return Status::TRAP;
			}
		}
	}

	//load page from address to frame
	ClusterNo cluster = myDesc->un.cluster;

	//load page from cluster
	if (!partition->readCluster(cluster, (char*)getFrameAddress(frame))) {
		//should not happen
		std::cout << "PARTITION LOAD FAILED!";
		return Status::TRAP;
	}

	//update its descriptor
	myDesc->valid = 1; //is in memory
	myDesc->frame = frame; //set its frame
	
	//update frame structure
	frames[frame].isFree = false; //is taken
	frames[frame].myDesc = myDesc; //point its descriptor
	return Status::OK; //all OK, huh
}

int KernelSystem::findBestCandidate(bool dirty)
{
	FrameNum initClockHand = clockHand;
	do {
		if (frames[clockHand].myDesc->dirty == dirty) {
			if (frames[clockHand].myDesc->ref_clock == false) //found victim
				return 1;
			frames[clockHand].myDesc->ref_clock = 0; //reset bit, give second chance
		}
		clockHand = (clockHand + 1) % processVMSpaceSize;
	} while (clockHand != initClockHand);
	return 0; //no corresponding class victims
}

PhysicalAddress KernelSystem::getFrameAddress(FrameNum frame)
{
	return (char*)processVMSpace + frame*PAGE_SIZE;
}

void KernelSystem::refreshFrame(FrameNum frame)
{
	freeFrames.insert(frame);
	frames[frame].isFree = true;
	return;
}

Time KernelSystem::periodicJob()
{
	std::lock_guard<std::recursive_mutex> lck(mtx);
	///TO DO
	return 0;
}

FrameNum KernelSystem::getVictim() {
	FrameNum victim;
	if (!freeFrames.empty()) {
		victim = *freeFrames.begin();
		freeFrames.erase(freeFrames.begin());
		return victim;
	}

	//all frames taken, select one of them
	//enchanced clock-hand algorithm used
	if (findBestCandidate(0)) {
		victim = clockHand;
		clockHand = (clockHand + 1) % processVMSpaceSize;
		return victim;
	}
	if (findBestCandidate(1)) {
		victim = clockHand;
		clockHand = (clockHand + 1) % processVMSpaceSize;
		return victim;
	}
	if (findBestCandidate(0)) {
		victim = clockHand;
		clockHand = (clockHand + 1) % processVMSpaceSize;
		return victim;
	}
	if (findBestCandidate(1)) {
		victim = clockHand;
		clockHand = (clockHand + 1) % processVMSpaceSize;
		return victim;
	}
	return -1;
}



