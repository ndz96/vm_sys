#include "KernelProcess.h"
#include "KernelSystem.h"

KernelProcess::KernelProcess(ProcessId pid)
{
	id = pid;
}

KernelProcess::~KernelProcess()
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	pmt_alloc->deallocatePMT(id);
	sys->erase(id);
}

ProcessId KernelProcess::getProcessId() const
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	return id;
}

Status KernelProcess::createSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	return pmt_alloc->allocateSegment(id, startAddress, segmentSize, flags, 0, 0, 0, 0);
}

Status KernelProcess::loadSegment(VirtualAddress startAddress, PageNum segmentSize, AccessType flags, void* content)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	Status status = pmt_alloc->allocateSegment(id, startAddress, segmentSize, flags, 0, 0, 0, 0);
	if (status != Status::OK)
		return status;
	
	//all OK -> write initial content to clusters
	PageNum pgStart = page(startAddress);
	PageNum pgEnd = pgStart + segmentSize - 1;
	for (PageNum pg = pgStart; pg <= pgEnd; ++pg) {
		Descriptor* desc = pmt_alloc->getDescriptor(pmt, pg);
		if (desc == nullptr) {
			std::cout << "loadSegment :: BIG ERRORRR";
			return Status::TRAP;
		}
		ClusterNo cluster = desc->un.cluster;
		if (!partition->writeCluster(cluster, (char*)content + (pg - pgStart) * PAGE_SIZE))
			return Status::TRAP;
	}

	return Status::OK;
}

Status KernelProcess::deleteSegment(VirtualAddress startAddress)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	return pmt_alloc->deleteSegment(id, startAddress);
}

Status KernelProcess::pageFault(VirtualAddress address)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	return sys->pageFault(pmt, address);
}

PhysicalAddress KernelProcess::getPhysicalAddress(VirtualAddress address)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	PageNum pg = page(address);
	const Descriptor* desc = pmt_alloc->getDescriptor(pmt, pg);
	//bad address
	if (desc == nullptr)
		return nullptr;

	if (desc->shared)
		desc = desc->un.hiddenDesc;
	if (desc->cloned)
		desc = &(desc->un.clonedDesc->desc);

	//not in memory
	if (!desc->valid)
		return nullptr;

	//is in memory currently
	unsigned frame = desc->frame;
	PhysicalAddress processVMSpace = sys->processVMSpace;
	return (char*)processVMSpace + (frame << PAGE_BITS) + offset(address);
}

Process * KernelProcess::clone(ProcessId pid)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	return sys->cloneProcess(pid);
}

Status KernelProcess::createSharedSegment(VirtualAddress startAddress, PageNum segmentSize, const char * _name, AccessType flags)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	std::string name(_name);

	if (!sys->sharedHiddenProcId.count(name)) {
		//first time -> shared segment creation
		//create new hidden process
		ProcessId pid = sys->createProcess()->getProcessId();

		//allocate standard segment for hidden process
		Status status = pmt_alloc->allocateSegment(pid, startAddress, segmentSize, flags, 0, 0, 0, 0);
		if (status != Status::OK)
			return status;
		
		//update structs
		sys->sharedHiddenProcId[name] = pid;
		sys->sharedAddrSeg[name] = std::make_pair(startAddress, segmentSize);
		sys->sharedFlags[name] = flags;
	}
	
	///connect to existing shared segment
	if (offset(startAddress) != 0)
		return Status::TRAP; //does not align
	if (segmentSize != sys->sharedAddrSeg[name].second)
		return Status::TRAP; //different segment size
	AccessType sflags = sys->sharedFlags[name];
	if ((rbit(flags) && !rbit(sflags)) || (wbit(flags) && !wbit(sflags)) || (xbit(flags) & !xbit(sflags)))
		return Status::TRAP;

	//id of hidden process -> to make connection
	ProcessId pid = sys->sharedHiddenProcId[name];
	//allocate shared segment(only pmt) for this process
	Status status = pmt_alloc->allocateSegment(id, startAddress, segmentSize, flags, 1, 0, pid, sys->sharedAddrSeg[name].first);
	if (status != Status::OK)
		return status;

	//allocated shared segment successfully -> now points to hidden process
	//update structs
	sys->sharedSegIds[name].push_back(std::make_pair(id, startAddress));
	sharedSegs.push_back(std::make_pair(startAddress, name));
	return Status::OK;
}

Status KernelProcess::disconnectSharedSegment(const char * name)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	return sys->eraseSharedSegId(name, id);
}

Status KernelProcess::deleteSharedSegment(const char * name)
{
	std::lock_guard<std::recursive_mutex> lck(*mtx);
	return sys->deleteSharedSegment(name);
}
