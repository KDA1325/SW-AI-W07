/*
 * memlib.c - a module that simulates the memory system.  Needed because it 
 *            allows us to interleave calls from the student's malloc package 
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* private variables */
static char *mem_start_brk;  /* points to first byte of heap */ // 힙이 어디부터 시작했는지
static char *mem_brk;        /* points to last byte of heap */ // 현재 힙의 어디까지 할당을 시켜놨는지의 주소값
static char *mem_max_addr;   /* largest legal heap address */ // 힙이 최대 할당할 수 있는 메모리 영역 

/* 
 * mem_init - initialize the memory system model
 */
void mem_init(void)
{
    // 교재의 mem_init 함수는 NULL 체크를 진행하지 않음
    // mem_start_brk == NULL -> 공간 자체를 할당하지 못했다
    /* allocate the storage we will use to model the available VM */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
        // 메모리 오류를 반환하고 실행을 종료
    	fprintf(stderr, "mem_init_vm: malloc error\n");
	    exit(1);
    }

    // MAX_HEAP 2의 20승 바이트를 비트 연산으로 구현 = 1mb * 20 = 20mb 
    // mem_max_addr = 최대 힙 공간의 바로 다음 바이트를 가리킴 
    // -> 가상으로 구현된 전체 힙 메모리 구역의 최종 끝 지점 
    // => mem_brk이 mem_max_addr를 넘어가려고 하면 오류 반환 
    mem_max_addr = mem_start_brk + MAX_HEAP;  /* max legal heap address */ // mem_max_addr: 메모리 시작 공간 + max heap => 종료 지점 

    // mem_brk = 현재 힙 영역 꼭대기를 가리키는 포인터
    // 현재 힙 꼭대기 = 힙 시작 주소 => 현재 힙에 할당된 공간이 0 바이트인 텅 빈 상태로 초기화 
    // -> mem_brk 포인터를 위로 이동시키며 힙 늘림 
    mem_brk = mem_start_brk;                  /* heap is empty initially */ // mem_brk 초기화 = 시작 지점으로 넣어둠 
}

/* 
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/* 
 * mem_sbrk - simple model of the sbrk function. Extends the heap 
 *    by incr bytes and returns the start address of the new area. In
 *    this model, the heap cannot be shrunk.
 */
// OS한테 힙 영역을 할당 받음
// mem_start_brk 범위 안에서 추가 가능
// incr 매개변수 만큼 메모리 추가 할당 요청 
void *mem_sbrk(int incr) 
{
    // 1~4까지 썻는데 4부터 메모리를 처음부터 써야 하니까
    // old_brk = 4 
    // old_brk 정의 = mem_brk
    // mem_brk가 증가됨 -> old_brk 주소를 반환해서 old_brk부터 mem_brk를 사용할 수 있게 함 
    char *old_brk = mem_brk;

    // incr 0 이거나 (mem_brk + incr)이 메모리 값보다 크면 -1 반환 
    if ( (incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
	errno = ENOMEM;
	fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
	return (void *)-1;
    }

    // mem_brk를 incr만큼 값 늘리면 메모리가 늘어남 
    mem_brk += incr;

    // old_brk 주소 리턴 
    // old_brk부터! 증가된 mem_brk까지 쓸 수 있게 old_brk 주소를 반환하는 것 
    return (void *)old_brk;
}

/*
 * mem_heap_lo - return address of the first heap byte
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 
 * mem_heap_hi - return address of last heap byte
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
