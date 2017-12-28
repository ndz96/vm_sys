#include "pmt.h"
#include "KernelSystem.h"
#include <iostream>

PMTAllocator::PMTAllocator(KernelSystem* _sys, PhysicalAddress _pmtSpace, PageNum _pmtSpaceSize, Partition* _partition)
{
	sys = _sys;
	pmtSpace = _pmtSpace;
	pmtSpaceSize = _pmtSpaceSize;
	partition = _partition;
	for (PageNum i = 0; i < pmtSpaceSize; ++i)
		freePages.push(i);
	for (ClusterNo c = 0; c < partition->getNumOfClusters(); ++c)
		freeClusters.push(c);
}

PMT* PMTAllocator::allocatePMT(ProcessId id)
{
	if (freePages.empty())
		return nullptr;

	PageNum page = freePages.front();
	freePages.pop();

	PMT* pmt = (PMT*)align_page(page);
	for (unsigned i = 0; i < (1 << PMT_L1_BITS); ++i) {
		pmt->pageL2[i] = -1;
		pmt->cntChildren[i] = 0;
	}

	idpmt_page[id] = page;
	return pmt;
}

void PMTAllocator::deallocatePMT(ProcessId id)
{
	if (segments.count(id)) {
		//deallocate remaining segments
		while(segments[id].size()){
			//std::cout << "deallocatePMT :: Unistavam preostali segment: " << segments[id][0].first << std::endl;
			deleteSegment(id, segments[id][0].first << PAGE_BITS);
		}
		segments.erase(id);
	}

	///success - free single pmt page
	freePages.push(idpmt_page[id]);
	idpmt_page.erase(id);
}

Status PMTAllocator::allocateSegment(ProcessId id, VirtualAddress startAddress, PageNum segmentSize, AccessType flags,
									 bool shared, bool cloned, ProcessId hiddenProcessId, VirtualAddress sharedStartAddress)
{
	if (offset(startAddress) != 0)
		return Status::TRAP; //does not align

	if (segmentSize == 0)
		return Status::TRAP; //empty segment

	//check for overlaping segments
	PageNum pgStart = page(startAddress);
	PageNum pgEnd = pgStart + segmentSize - 1;
	for (size_t i = 0; i < segments[id].size(); ++i)
		if (!(pgEnd < segments[id][i].first || pgStart > segments[id][i].second))
			return Status::TRAP; //overlaps

	PMT* pmt = sys->getPMT(id);
	PMTEntry pmtEntryStart = pmt_entry(pgStart), pmtEntryEnd = pmt_entry(pgEnd);

	//check for pmt allocation
	size_t numPgRequired = 0;
	for (PMTEntry entry = pmtEntryStart; entry <= pmtEntryEnd; ++entry)
		if (pmt->pageL2[entry] == -1)
			numPgRequired++;
	if (numPgRequired > freePages.size())
		return Status::OUT_OF_MEM_PMT; //not enough pmt memory

	if (!shared && !cloned) {
	//check for parition allocation
		if (segmentSize > freeClusters.size())
			return Status::OUT_OF_MEM_CLSTR; //not enough cluster memory
	}


	///all fine -> allocate
	//allocate pages
	for (PMTEntry entry = pmtEntryStart; entry <= pmtEntryEnd; ++entry) {
		if (pmt->pageL2[entry] == -1) {
			pmt->pageL2[entry] = freePages.front();
			freePages.pop();

			//set descriptor valid desc bits to 0 for all descriptors in new page
			PMTL2* pmtL2 = (PMTL2*)align_page(pmt->pageL2[entry]);
			for (unsigned i = 0; i < (1 << PMT_L2_BITS); ++i)
				pmtL2->desc[i].desc_valid = 0;
		}
	}

	//get shared pmt
	PMT* pmtShared = nullptr;
	PageNum sharedPgStart = 0;
	if (shared) {
		pmtShared = sys->getPMT(hiddenProcessId);
		sharedPgStart = page(sharedStartAddress);
	}
		

	//set descriptors for each page
	for (PageNum pg = pgStart; pg <= pgEnd; ++pg) {
		//update children count
		pmt->cntChildren[pmt_entry(pg)]++;

		//get descriptor(page should already be allocated)
		unsigned pgL2 = pmt->pageL2[pmt_entry(pg)];
		PMTL2* pmtL2 = (PMTL2*)align_page(pgL2);
		Descriptor* desc = &pmtL2->desc[pmtL2_entry(pg)];

		///update descriptor
		desc->desc_valid = 1;
		desc->r = (rbit(flags));
		desc->w = (wbit(flags));
		desc->x = (xbit(flags));
		desc->shared = shared;

		if (!shared) {
			desc->valid = 0;
			desc->ref_thrash = 0;
			desc->ref_clock = 0;
			desc->dirty = 0;
			desc->frame = 0;
			unsigned cluster = freeClusters.front();
			freeClusters.pop();
			desc->un.cluster = cluster;
		}
		else
			desc->un.hiddenDesc = getDescriptor(pmtShared, sharedPgStart + (pg - pgStart));
	}

	//insert segment into vector
	segments[id].push_back(std::make_pair(pgStart, pgEnd));
	return Status::OK;
}

Status PMTAllocator::deleteSegment(ProcessId id, VirtualAddress startAddress)
{
	PageNum pgStart = page(startAddress), pgEnd;
	bool found = false;
	std::vector<std::pair<PageNum, PageNum>>::iterator it = segments[id].begin();
	for (size_t i = 0; i < segments[id].size(); ++i, ++it)
		if (segments[id][i].first == pgStart) {
			found = true;
			pgEnd = segments[id][i].second;
			break;
		}

	if (!found)
		return Status::TRAP; //not aligned on segment start

	//erase segment
	segments[id].erase(it);
	PMT* pmt = sys->getPMT(id);

	for (PageNum pg = pgStart; pg <= pgEnd; ++pg) {
		///clusters/frames
		Descriptor* desc = getDescriptor(pmt, pg);
		if (desc == nullptr)
			std::cout << "deleteSegment :: HUGEERRROR";
		desc->desc_valid = 0;

		//if not shared descriptor free relevant space
		if (!desc->shared && !desc->cloned) {
			// if in memory 
			if (desc->valid)
				sys->refreshFrame(desc->frame);
			freeClusters.push(desc->un.cluster);
		}

		///pmt deallocation
		//update children count
		pmt->cntChildren[pmt_entry(pg)]--;
		if (pmt->cntChildren[pmt_entry(pg)] == 0) {
			//deallocate page if free
			freePages.push(pmt->pageL2[pmt_entry(pg)]);
			pmt->pageL2[pmt_entry(pg)] = -1;
		}
	}
	return Status::OK;
}

void* PMTAllocator::align_page(PageNum pg_num)
{
	return (char*)pmtSpace + pg_num * PAGE_SIZE;
}

Descriptor* PMTAllocator::getDescriptor(PMT* pmt, PageNum pg)
{
	unsigned pgL2 = pmt->pageL2[pg >> PMT_L2_BITS];
	if (pgL2 == -1) 
		return nullptr;
	PMTL2* pmtL2 = (PMTL2*)((char*)pmtSpace + (pgL2 << PAGE_BITS));
	Descriptor* desc = &pmtL2->desc[pg & ((1 << PMT_L2_BITS) - 1)];
	if (!desc->desc_valid) 
		return nullptr;
	//all OK
	return desc;
}
