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
#ifdef NEW_THREAD
	end = false;
	startSaverThread();
#endif
#ifdef DIAGNOSTICS
	pf_save = 0;
	pf_load = 0;
#endif
}

KernelSystem::~KernelSystem()
{
	std::lock_guard<std::recursive_mutex> lck(mtx);
#ifdef NEW_THREAD
	end = true;
	saverThread->join();
#endif
	for (ProcessId id = 0; id < processes.size(); ++id) {
		if (processes[id].second) {
			//std::cout << "Proces: " << id << " ostao ziv - unistavam!" << std::endl;
			delete processes[id].first;
		}
	}
	delete pmt_alloc;
#ifdef DIAGNOSTICS
	std::cout << std::endl << "DIAGNOSTICS: " << std::endl;
	std::cout << "PAGE_FAULT_SAVES: " << pf_save << std::endl;
	std::cout << "PAGE_FAULT_LOADS: " << pf_load << std::endl;
#endif
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

	//if cloned propagate to cloned descriptor
	if (desc->cloned) {
		desc = &(desc->un.clonedDesc->desc);
		desc->ref_thrash = 1;
		desc->ref_clock = 1;
		if (type == AccessType::WRITE || type == AccessType::READ_WRITE) {
			//apply CoW optimization
			apply_cow = true;
			desc->dirty = 1;
			return Status::PAGE_FAULT;
		}
		if (!desc->valid)
			return Status::PAGE_FAULT;
		return Status::OK;
	}

	//if shared propagate to hidden process
	if (desc->shared)
		desc = desc->un.hiddenDesc;

	//set relevant bits
	desc->ref_thrash = 1;
	desc->ref_clock = 1;

	//not in memory
	if (!desc->valid) {
		if (type == AccessType::WRITE || type == AccessType::READ_WRITE)
			desc->dirty = 1;
		return Status::PAGE_FAULT;
	}
	

	if (type == AccessType::WRITE || type == AccessType::READ_WRITE) {
#ifdef NEW_THREAD
#ifdef PROBABILITY_AND_STATISTICS
		std::lock_guard<std::mutex> lk(frames[desc->frame].mtx);
#endif
#endif
		desc->dirty = 1;
#ifdef NEW_THREAD
		lastAccessedFrame = desc->frame;
#endif
	}
	return Status::OK;
}

Process* KernelSystem::cloneProcess(ProcessId pid)
{
	std::lock_guard<std::recursive_mutex> lck(mtx);

	// if not alive
	if (pid < 0 || pid >= processes.size() || !processes[pid].second)
		return nullptr;

	//create new process
	Process* newProc = createProcess();

	//for each segment
	for (size_t i = 0; i < pmt_alloc->segments[pid].size(); ++i) {
		PageNum pgStart = pmt_alloc->segments[pid][i].first;
		PageNum pgEnd = pmt_alloc->segments[pid][i].second;

		bool sharedSegExists = false;
		//check if segment is *shared* -> connect new process
		for (size_t i = 0; i < processes[pid].first->sharedSegs.size(); ++i) {
			if (processes[pid].first->sharedSegs[i].first == (pgStart << PAGE_BITS)) {
				//seg is shared
				sharedSegExists = true;
				std::string name = processes[pid].first->sharedSegs[i].second;
				AccessType type = sharedFlags[name];
				//connect it to shared segment
				if (newProc->createSharedSegment((pgStart << PAGE_BITS),
					pgEnd - pgStart + 1, name.c_str(), type) != Status::OK) {
						delete newProc;
						return nullptr;
					}
				break;
			}
		}

		if (sharedSegExists)
			continue; //no need to inspect this seg anymore

		AccessType atype;
		//for each page in *standard* segment
		PMT* pmt = getPMT(pid);
		for (PageNum pg = pgStart; pg <= pgEnd; ++pg) {
			Descriptor* desc = pmt_alloc->getDescriptor(pmt, pg);
			if (pg == pgStart) {
				//get segment atype
				if (desc->r && desc->w)
					atype = READ_WRITE;
				else if (desc->r)
					atype = READ;
				else if (desc->w)
					atype = WRITE;
				else atype = EXECUTE;
			}
			if (!desc->cloned) {
				///first time cloned page - new cloned descriptor for it
				ClonedDescriptor* cdesc = pmt_alloc->getFreeClonedDescriptor();
				if (cdesc == nullptr) {
					delete newProc;
					return nullptr;
				}
				cdesc->numSharing++;

				//copy descriptor to cloned one
				cdesc->desc = *desc;
				cdesc->desc.un.cluster = desc->un.cluster; //just in case for union copyc

				//update frame to point to cloned one
				if (desc->valid) {
#ifdef NEW_THREAD
					frames[desc->frame].mtx.lock();
#endif
					frames[desc->frame].myDesc = &(cdesc->desc);
#ifdef NEW_THREAD
					frames[desc->frame].mtx.unlock();
#endif
				}

				//update this one to point to cloned one
				desc->cloned = 1;
				desc->un.clonedDesc = cdesc;
			}
		}

		//now all are pages in this segment are *cloned* ones
		//->allocate *cloned* segment for new process
		if (pmt_alloc->allocateSegment(newProc->getProcessId(), pgStart << PAGE_BITS, pgEnd - pgStart + 1,
			atype, 0, 1, pid, 0) != Status::OK) {
			delete newProc;
			return nullptr;
		}
	}
	//all fine
	return newProc;
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
			///not sharing anymore
			//delete from process structs
			processes[id].first->sharedSegs.erase(std::find(processes[id].first->sharedSegs.begin(),
				processes[id].first->sharedSegs.end(),
				std::make_pair(sharedSegIds[name][i].second, name)));
			
			//delete from system structs
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

#ifdef NEW_THREAD
void KernelSystem::stfunc(KernelSystem* sys) {
	while (1) {
		std::this_thread::sleep_for(std::chrono::milliseconds(TIMESTAMP));
		for (FrameNum fr = 0; fr < sys->processVMSpaceSize; ++fr) {
			if (sys->end)
				goto END;
			sys->frames[fr].mtx.lock();
			if (!sys->frames[fr].isFree && fr != sys->lastAccessedFrame) {
				Descriptor* desc = sys->frames[fr].myDesc;
				if (desc->dirty) {
					//dirty page -> write to its cluster
					desc->dirty = 0;
					ClusterNo cluster = desc->un.cluster;
					if (!sys->partition->writeCluster(cluster, (char*)sys->getFrameAddress(fr))) {
						std::cout << "PARTITION SAVE FAILED!";
						desc->dirty = 1;
					}
				}
			}
			sys->frames[fr].mtx.unlock();
		}
	}
END:
	return;
}


void KernelSystem::startSaverThread()
{
	saverThread = new std::thread(stfunc, this);
}
#endif

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

	if (apply_cow) {
		///CoW routine was called
		apply_cow = false;
		ClonedDescriptor* cdesc = myDesc->un.clonedDesc;
		Descriptor* clonedDesc = &(myDesc->un.clonedDesc->desc);
		ClusterNo new_cluster;
		char* phyAddress;
		void* temp = nullptr;

		///copy cloned page to new cluster
		if (clonedDesc->valid) {
			//cloned page in memory 
			unsigned frame = clonedDesc->frame;
			//get address
			phyAddress = (char*)processVMSpace + (frame << PAGE_BITS);
		}
		else {
			//cloned page out of memory
			temp = ::operator new(PAGE_SIZE);
			ClusterNo clust = clonedDesc->un.cluster;
			partition->readCluster(clust, (char*)temp);
			phyAddress = (char*)temp;
		}
		//get new cluster
		if (!pmt_alloc->freeClusters.size())
			return Status::TRAP;
		new_cluster = pmt_alloc->freeClusters.front();
		pmt_alloc->freeClusters.pop();
		partition->writeCluster(new_cluster, phyAddress);
		if (temp)
			delete temp;


		///modify myDesc to point on new cluster
		myDesc->cloned = 0;
		myDesc->valid = 0;
		myDesc->ref_thrash = 1;
		myDesc->ref_clock = 1;
		myDesc->dirty = 1;
		myDesc->un.cluster = new_cluster;

		///update cloned descriptor
		cdesc->numSharing--;
		if (cdesc->numSharing == 0) {
			//page not needed anymore
			if (cdesc->desc.valid)
				refreshFrame(cdesc->desc.frame);
			pmt_alloc->freeClusters.push(cdesc->desc.un.cluster);

			pmt_alloc->pvFree[cdesc->pvidx].numFree++;
			if (pmt_alloc->pvFree[cdesc->pvidx].numFree == CLONED_PAGE_SIZE) {
				pmt_alloc->pvFree[cdesc->pvidx].cpage = nullptr;
				pmt_alloc->freePages.push(pmt_alloc->pvFree[cdesc->pvidx].pageNumber);
			}
		}
	}

	if (myDesc->shared)
		myDesc = myDesc->un.hiddenDesc;
	if (myDesc->cloned) {
		myDesc = &(myDesc->un.clonedDesc->desc);
		//std::cout << myDesc->cloned;
	}
	assert(myDesc->cloned == 0);
	
	//select frame of page to swap out
	FrameNum frame = getVictim();
#ifdef NEW_THREAD
	lastAccessedFrame = frame;

	frames[frame].mtx.lock();
#endif
	if (!frames[frame].isFree) {
		Descriptor* desc = frames[frame].myDesc;
		desc->valid = 0; //not in memory anymore
		if (desc->dirty) {
			//is dirty
			desc->dirty = 0; //not dirty anymore
			//write new version to its cluster
			assert(desc->cloned == 0);
			ClusterNo cluster = desc->un.cluster;
			//std::cout << desc->cloned;
#ifdef DIAGNOSTICS
			pf_save++;
#endif
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
#ifdef DIAGNOSTICS
	pf_load++;
#endif
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
#ifdef NEW_THREAD
	frames[frame].mtx.unlock();
#endif
	return Status::OK; //all OK, huh
}

int KernelSystem::findBestCandidate(bool dirty)
{
	FrameNum initClockHand = clockHand;
	do {
//#ifdef NEW_THREAD
//		std::lock_guard<std::mutex> lck(frames[clockHand].mtx);
//#endif
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
#ifdef NEW_THREAD
	frames[frame].mtx.lock();
#endif
	frames[frame].isFree = true;
#ifdef NEW_THREAD
	frames[frame].mtx.unlock();
#endif
	return;
}

Time KernelSystem::periodicJob()
{
	std::lock_guard<std::recursive_mutex> lck(mtx);
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

#ifdef NEW_THREAD
	if (findBestCandidate(0)) {
		victim = clockHand;
		clockHand = (clockHand + 1) % processVMSpaceSize;
		return victim;
	}
#endif
	return -1;
}



