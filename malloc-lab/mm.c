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

// 매크로: #define으로 만드는 문자열 치환 규칙, 컴파일 전에 코드를 바꿔주는 치환 규칙임 
// 함수 X, 상수 정의, 짧은 코드 표현, 조건부 컴파일 등에 쓰임 
// size를 ALIGNMENT 단위로 올림해서 메모리 정렬된 크기(특정 바이트 단위 경계에 맞춘 크기)로 만드는 매크로 
// (size) + (ALIGNMENT - 1): 필요한 크기를 정렬 경계보다 조금 크게 더함 -> 올림 
// --- 
// 0x7: 이진수 0000 0111
// ~: 비트를 뒤집는 연산자 
// -> ~0x7 = 1111 1000(0을 1로 바꾸고 1을 0으로 바꾸는 것, 순서를 뒤집는 것이 X) 
// &~0x7 => 어떤 수의 맨 아래 3비트를 0으로 만드는 비트 마스크
// 8 = 2^3 = 마지막 3비트가 000임 = 마지막 3비트가 000이면 무조건 8의 배수 
// & 연산자: x & ~0x7 -> x의 비트와 ~0x7 비트 한자리씩 비교해서 둘 다 1이면 1, 둘 다 1이 아니거나/둘 중 하나만 1이면 0
// -> ~0x7 마지막 3 비트가 000이라 & 연산하면 나오는 값도 무조건 마지막 3비트 000임 => 특정 비트만 남기고, 특정 비트는 지우는 역할
// => size를 8바이트 배수로 맞춤 
// --- 
// ex) size = 27 = 0001 1011
// 0001 1011 // 27 이진수 (size) + (ALIGNMENT - 1)
// 1111 1000 // ~0x7 이진수 ~0x7
// --------- // & 연산
// 0001 1000 // 최종 size
// ex) size = 20 -> size + (ALIGNMENT - 1) = 27 -> 27과 가장 가까운 8의 배수 = 24
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 1 word == 4byte
// header 1 byte, footer 1 byte, pred pointer 1 byte, succ pointer 1 byte = total 4 byte

// WSIZE = 4byte -> CPU가 한 번에 읽고 쓰는 기본 덩어리가 4byte라서 1 word = 4 byte
#define WSIZE 4

// DSIZE = 8byte -> 기본 단위 워드를 두 개를 합친 8 byte도 CPU가 한 번에 읽고 쓰기 쉽기 때문에 DSIZE로 8 byte 정의 
// -> DSIZE = 2 word 
#define DSIZE 8

// OS에게 추가로 요청할 힙 메모리의 기본 크기
// -> 할당기 첫 기작 때 CHUNKSIZE 만큼 힙을 늘려 초기 빈 블록 세팅
// -> 사용자의 malloc 요청 할당할 공간이 없을 때 sbrk 함수 호출해서 CHUNKSIZE 만큼 힙을 늘려줌
// => 사용자의 요청 때마다 작은 공간들을 바로바로 늘려줌 X, 분할해서 쓰도록 아예 큰 뭉텅이 하나로 늘려줌 
// ---
// 1<<12: 비트 연산, 1을 왼쪽으로 12칸 이동 
// 1: 0000 0001
// 1을 왼쪽으로 12칸 이동: 0001 0000 0000 0000 = 2^12 = 10진수 4096 = 4096 bytes = 4KB 
#define CHUNKSIZE (1 << 12)

// size 비트 값과 alloc(할당 여부) 비트 값을 하나의 값으로 통합하는 매크로
// -> 통합된 값을 헤더, 푸터에 넣음 
// size, alloc 둘 중 하나라도 1이면 1, 둘 다 0이면 0 
#define PACK(size, alloc) ((size) | (alloc))
/*
 * mm_init - initialize the malloc package.
 * mm_init - malloc 패키지를 초기화합니다.
 */
int mm_init(void)
{
    // 힙 영역 만들기
    // 힙 생성 실패로 -1 반환하면 mm_init도 -1 반환 및 종료
    if(sbrk(WSIZE) == -1)
    {
        return -1;
    }

    // PACK 매크로
    // GET 매크로 
    // PUT 매크로

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