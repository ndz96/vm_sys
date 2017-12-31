#pragma once

typedef unsigned long PageNum;
typedef unsigned long VirtualAddress;
typedef void* PhysicalAddress;
typedef unsigned long Time;
typedef unsigned PMTEntry;
enum Status { OK, PAGE_FAULT, TRAP, OUT_OF_MEM_PMT, OUT_OF_MEM_CLSTR};
enum AccessType { READ, WRITE, READ_WRITE, EXECUTE};
typedef unsigned ProcessId;
#define PAGE_SIZE 1024


//additional
#define PAGE_BITS 10
#define page(addr) ((addr) >> PAGE_BITS)
#define offset(addr) ((addr) & ((1 << PAGE_BITS) - 1))
#define rbit(type) ((type == READ || type == READ_WRITE) ? 1 : 0)
#define wbit(type) ((type == WRITE || type == READ_WRITE) ? 1 : 0)
#define xbit(type) ((type == EXECUTE) ? 1 : 0)

//mode
//#define NEW_THREAD
#define DIAGNOSTICS
#define TIMESTAMP 1
#define PROBABILITY_AND_STATISTICS