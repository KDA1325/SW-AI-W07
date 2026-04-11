/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * mm-naive.c - 가장 빠르지만 메모리 효율은 가장 낮은 malloc 패키지입니다.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * 이 단순한 접근 방식에서는 brk 포인터를 단순히 증가시켜 블록을 할당합니다.
 * the brk pointer.  A block is pure payload. There are no headers or
 * 블록은 순수한 페이로드만으로 이루어지며 헤더나
 * footers.  Blocks are never coalesced or reused. Realloc is
 * 푸터가 없습니다. 블록은 병합되거나 재사용되지 않으며, realloc은
 * implemented directly using mm_malloc and mm_free.
 * mm_malloc과 mm_free를 사용해 직접 구현됩니다.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * 학생들에게 알림: 이 헤더 주석은 여러분의 해결 방법을 높은 수준에서 설명하는
 * comment that gives a high level description of your solution.
 * 자체 헤더 주석으로 교체하세요.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * 학생들에게 알림: 다른 작업을 하기 전에 먼저
 * provide your team information in the following struct.
 * 아래 구조체에 팀 정보를 입력하세요.
 ********************************************************/
team_t team = {
    /* Team name */ /* 팀 이름 */
    "다때",
    /* First member's full name */ /* 첫 번째 팀원의 전체 이름 */
    "Daae Kim",
    /* First member's email address */ /* 첫 번째 팀원의 이메일 주소 */
    "pb.kimdaae@gmail.com",
    /* Second member's full name (leave blank if none) */ /* 두 번째 팀원의 전체 이름 (없으면 비워두기) */
    "",
    /* Second member's email address (leave blank if none) */ /* 두 번째 팀원의 이메일 주소 (없으면 비워두기) */
    ""};

/* single word (4) or double word (8) alignment */
/* 워드(4바이트) 또는 더블 워드(8바이트) 정렬 */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
/* ALIGNMENT의 가장 가까운 배수로 올림합니다 */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*
 * mm_init - initialize the malloc package.
 * mm_init - malloc 패키지를 초기화합니다.
 */
int mm_init(void)
{
    mem_sbrk(4 * newsize);

    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * mm_malloc - brk 포인터를 증가시켜 블록을 할당합니다.
 *     Always allocate a block whose size is a multiple of the alignment.
 *     항상 정렬 단위의 배수 크기를 갖는 블록을 할당합니다.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 * mm_free - 블록을 해제해도 아무 작업도 하지 않습니다.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * mm_realloc - mm_malloc과 mm_free를 이용해 단순하게 구현됩니다.
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}