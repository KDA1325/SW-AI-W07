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
    ""
};

// ---  탐색 방식 설정 ---
//#define FIRST_FIT
//#define NEXT_FIT
#define BEST_FIT


/*
전처리기로 연산 수식을 정의하는 이유

전처리기는 컴파일 타임에 치환되는 거라 런타임 중엔 작동하지 않음
-> 포인터는 런타임 중 블록을 이동할 때마다, 혹은 해당 포인터가 필요한 작업을 할 때마다 연산으로 구해져야 하는데 왜 전처리기로 연산 수식을 정의해둘까?
=> 전처리기로 컴파일 타임에 주소값을 미리 계산하는 개념이 아니라서.

전처리기가 컴파일 전에 하는 작업은 단순한 텍스트 치환 뿐
런타임 중 마주하는 NEXT_BLKP(bp)라는 짧은 글자를 복잡한 포인터 계산 수식 텍스트로 바꿔치기만 하고 끝남
-> 런타임에 힙에서 bp 값이 바뀌면 그때그때 바뀐 bp값을 바탕으로 CPU가 수식을 계산하는 실제 연산은 정상적으로 런타임에 수행됨
*/

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
// ---
// 바로 4096으로 하드 코딩 하지 않고 비트 연산을 하는 이유
// 
#define CHUNKSIZE (1 << 12)

// x와 y 중 더 큰 값을 반환하는 매크로
// 사용자가 malloc으로 asize만큼 할당을 요청했으나 현재 힙에 맞는 빈 블록이 없을 때 sbrk로 OS에 힙 영역 늘리기 요청
// -> 매번 필요한 만큼만 늘릭면 시스템 호출 잦아짐 -> 성능 떨어짐
// -> 기본적으로 CHUNKSIZE만큼 넉너과게 늘림
// -> 만약 요청된 asize가 CHUNKSIZE보다 크다면? -> asize만큼 힙을 늘려줘야 함
// -> 사용자가 요청한 블록 크기 asize와 할당기의 기본 힙 확장 크기 CHUNKSIZE를 비교 
// => 둘 중 더 큰 값만큼 힙을 늘림
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// size 비트 값과 alloc(할당 여부) 비트 값을 하나의 값으로 통합하는 매크로
// -> 통합된 값을 헤더, 푸터에 넣음 
// size, alloc 둘 중 하나라도 1이면 1, 둘 다 0이면 0 
#define PACK(size, alloc) ((size) | (alloc))

// 주어진 메모리 주소 p에서 1워드 크기 값을 읽어오는 매크로(unsigned int = 4byte = 1word)
// 할당기에서 다루는 포인터 대부분은 void* 형이라 주소에 있는 값을 바로 가져올 수 없어서 형 변환 후 역참조 해야 값 가져올 수 있음 
#define GET(p) (*(unsigned int *) (p))

// 특정 메모리 주소p에 1워드 크기의 값 val을 직접 넣음 
// GET과 마찬가지로 형 변환 후 역참조 해야 값을 바꿀 수 있음
// 주로 PACK 매크로를 통해 하나로 합쳐진 값을 각 블록 헤더, 푸터에 넣을 때 사용함 
#define PUT(p, val) (*(unsigned int *) (p) = (val)) 

// ---
// OR: True 우선 연산 = 하나라도 True면 다 true
// AND: False 우선 연산 = 하나라도 False면 다 False 
// ---

// GET(p)로 얻어온 각 블록 1워드 헤더, 푸터 값에서 비트 마스킹(&, AND 연산)을 통해 원하는 정보만 깔끔하게 분리
// ~0x7 == 하위 세 비트 0, 나머진 1
// ~: NOT , 0x7: x = 16진수 
// -> 헤더, 푸터 값에서 하위 세 비트 0만 지워지고 온전한 블록 크기 값만 남김
#define GET_SIZE(p) (GET(p) & ~0x7)

// GET(p)로 얻어온 각 블록 1워드 헤더, 푸터 값에서 비트 마스킹(&, AND 연산)을 통해 원하는 정보만 깔끔하게 분리
// 0x1 == 맨 마지막 비트만 1, 나머진 0
// -> 앞의 블록 크기 정보를 지우고 맨 마지막 1비트짜리 할당 정보만 남김  
#define GET_ALLOC(p) (GET(p) & 0x1)

// --- HDRP & FTRP: 메모리 주소를 워드 단위로 연산해서 원하는 주소값을 찾아내는 매크로 ---
// char * 타입 형변환: 주소값을 1바이트 단위로 더하고 빼기 위해 형변환 
// HDRP: HeaDeR Pointer = 헤더 포인터의 약자  
// 범용 포인터인 bp를 char* 타입으로 형 변환 
// bp: 데이터가 들어가는 페이로드의 시작 주소 
// 헤더: bp 바로 앞 1워드에 들어감(WSIZE)
// => bp 주소에서 1워드 크기를 빼면 헤더의 시작 주소
#define HDRP(bp) ((char *)(bp) - WSIZE)

// FTRP: FooTeR Pointer = 푸터 포인터의 약자
// 푸터: 블록 맨 끝 1워드 자리에 들어감
// GET_SIZE: 위에서 구한 HDRP(bp)로 헤더를 찾아 현재 블록의 크기를 알아냄 
// bp 주소에 현재 블록 크기 더함 -> 다음 블록의 헤더를 지나친 페이로드 시작 주소가 됨
// 푸터 시작 주소로 돌아오기 위해 다음 블록의 헤더 크기 1워드 + 푸터 크기 1워드 = 2워드 = DSIZE만큼 빼줌 
// -> 연산이 끝나면 현재 블록의 푸터 시작 주소가 됨 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// --- NEXT_BLKP & PREV_BLKP: 힙을 탐색하기 위해 현재 블록의 페이로드 포인터(bp)에서 인접한 다음이나 이전 블록의 bp로 점프하는 매크로 ---
// NEXT_BLKP와 PREV_BLKP 연산에서 HDRP와 FTRP를 쓰지 않는 이유: NEXT_BLKP 연산에선 HDRP를 쓸 수 있지만 PREV_BLKP 연산에선 FTRP를 쓸 수 없기 때문
// -> 사실 쓰든 안 쓰든 전처리기이기 때문에 성능에 차이가 전혀 없음 
// 현재 블록 bp에서 1워드 빼기 -> 현재 블록의 헤더 위치 => 현재 블록의 크기를 읽음
// 현재 bp 주소에 전체 크기 더함 -> 다음 블록의 페이로드 시작 주소(다음 블록의 bp)로 이동
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char*)(bp) - WSIZE)))

// 현재 블록 bp에서 2워드 빼기 -> 이전 블록의 푸터 => 이전 블록의 크기를 읽음
// 현재 bp에서 이전 블록의 크기만큼 빼기 -> 이전 블록의 페이로드 시작 주소로 이동 
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// heap_listp 선언
// heap_listp: 힙의 시작 부분에 만들어둔 프롤로그 블록을 항상 가리키는 포인터
// -> 힙 영역 전체를 순회할 때, 변하지 않는 출발 지점을 나타내는 역할 
static char *heap_listp;

/*
 * mm_init - initialize the malloc package.
 * mm_init - malloc 패키지를 초기화합니다.
 */
int mm_init(void)
{
    // heap_listp 초기화
    // -> 이 변수에 실제 힙의 시작 주소값(프롤로그 블록)을 넣는 작업

    // 4 워드를 가져와서 빈 가용 리스트를 만듦 
    // -> sbrk로 힙이 만들어졌으면 반환값으로 heap_listp 초기화 => 실제 힙의 시작 주소값(프롤로그 블록)을 넣음
    // -> sbrk 힙 생성 실패로 (void *)-1 반환하면
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        // mm_init도 -1 반환 및 종료
        return -1;
    }

    // ---  생성된 힙의 4 워드 공간 초기화 ---
    // 정렬 패딩
    // 힙 시작 부분의 빈 공간
    // 첫 번째 워드 
    PUT(heap_listp + (0 * WSIZE), 0);
    
    // 프롤로그 헤더
    // -> 프롤로그는 헤더(1워드) + 푸터(1워드)로 2 word 짜리 블록
    // -> 할당 정보 비트를 1로 해줘야 다음에 들어오는 블록이 첫번째 블록이라는 것을 알 수 있음
    // 두 번째 워드 
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));

    // 프롤로그 푸터
    // -> 헤더에서 두 번째 워드를 사용했으니 세 번째 워드를 head_listp로 해줌
    // -> PACK 하는 값은 헤더와 동일(푸터는 헤더의 복사본)
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    
    // 페이로드는 초기화 하지 않음
    // -> 실제 데이터를 저장할 블록이 아니라 힙 탐색을 위한 경계선을 만들어주는 단계이기 때문 
    // => 묵시적 가용 리스트의 불변하는 형식 

    // 헤더 - 페이로드 - 푸터 형식
    // -> 초기화 단계에선 페이로드가 없기 때문에 헤더 - 푸터 형식이 되었고, 이걸 프롤로그라고 묶어서 부르는 것 
    // => 프롤로그 블럭 = 페이로드 크기가 0인 블럭 
    // => 실질적으로 프롤로그 푸터 위치가 페이로드의 시작 주소가 됨 

    // 에필로그 
    // 크기가 0인 할당 상태만 표시하는 특수 블록 
    // 프롤로그 블록에서 세 번째 워드까지 사용했으니 에필로그는 네 번째 워드 
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));

    // heap_listp를 프롤로그 푸터 위치(페이로드 시작 위치 == bp)를 가리키게 함 
    heap_listp += (2 * WSIZE);

    // extend_heap 함수로 힙을 CHUNKSIZE로 확장
    // 초기 가용 블록 생성
    // 8 바이트 정렬 조건을 맞추기 위해 CHUNKSIZE를 WORD 크기로 
    extend_heap(CHUNKSIZE/WSIZE);

    // extend_heap(CHUNKSIZE)
    // extend_heap 구현
    return 0;
}

static void *extend_heap(size_t words)
{
    // 블록 포인터
    // 페이로드의 첫 바이트를 가리키는 포인터 
    char *bp;
    
    // size_t: 메모리의 크기나 데이터의 길이를 바이트 단위로 나타낼 때 사용하는 부호 없는 정수(unsigned int) 자료형
    size_t size;

    // 8바이트 정렬을 조건 맞추기 위한 구문
    // words: OS에게 요청할 메모리 크기의 워드 개수(byte X)
    // words % 2: 홀수, 짝수 판별 -> 8바이트 정렬 조건 만족 = 8의 배수 = 짝수
    // 1 워드 = 4 바이트, 2워드 = 8 바이트 ... -> 워드가 짝수면 무조건 8바이트 이상이 됨  
    // 홀수라면(정렬 조건 만족 X): (words + 1) * WSIZE -> 워드 개수를 짝수로 하나 늘린 후 WISE 곱 -> 바이트 단위로 변환
    // 짝수라면(정렬 조건 만족): 바로 words * WSIZE -> 바이트 단위로 변환 
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    
    // long을 붙이는 이유: sbrk 실패 반환 값은 그냥 숫자 -1이 아닌, 메모리 시작 주소인 포인터 (void *)-1
    // -> 포인터 타입과 일반 정수를 비교하려면 포인터 크기를 온전히 담을 수 있는 넉넉한 크기의 부호 있는 정수 자료형 long으로 형 변환 필요
    if((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    // 메모리가  늘어남에 따라 빈 공간에 새로운 경계선을 만들어줌
    // => mm_init 함수와 같은 역할을 함 
    // 프롤로그 헤더 초기화
    PUT(HDRP(bp), PACK(size, 0));

    // 프롤로그 푸터 초기화 
    PUT(FTRP(bp), PACK(size, 0));
   
    // 에필로그 블록 초기화 
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}


/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * mm_malloc - brk 포인터를 증가시켜 블록을 할당합니다.
 *     Always allocate a block whose size is a multiple of the alignment.
 *     항상 정렬 단위의 배수 크기를 갖는 블록을 할당합니다.
 */
// 요청이 들어올 때마다 무조건 mem_sbrk 호출해서 늘려주는 단순한 형태의 할당기
// -> free 된 빈 공간을 재사용할 수 없음 
// void *mm_malloc(size_t size)
// {
//     int newsize = ALIGN(size + SIZE_T_SIZE);

//     void *p = mem_sbrk(newsize);

//     if (p == (void *)-1)
//         return NULL;
//     else
//     {
//         *(size_t *)p = size;
//         return (void *)((char *)p + SIZE_T_SIZE);
//     }
// }
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    // 의미 없는 요청 무시 
    if(size == 0)
    {
        return NULL;
    }

    if(size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // find_fit 
    if((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }
    
    extendsize = MAX(asize, CHUNKSIZE);

    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
    {
        return NULL;
    }

    place(bp, asize);

    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 * mm_free - 블록을 해제해도 아무 작업도 하지 않습니다.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));

    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

// 병합 함수 => 앞뒤 블록 가용 상태를 확인하고 빈 블록끼리 병합 
// 반환형 void X, void * 보이드 포인터 O
// -> 어떤 타입의 데이터든 가리킬 수 있는 범용 포인터 주소 
// -> 매개변수로 전달받은 bp(페이로드 시작 주소)를 그대로 반환
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 01, 이전/다음 모두 할당된 상태라 병합 X
    if(prev_alloc && next_alloc)
    {
        return bp;
    }
    // case 02, 이전 블록만 할당 상태 다음 블록은 가용 상태 
    else if(prev_alloc && !next_alloc)
    {
        // 현재 블록 사이즈 + 다음 블록 사이즈(헤더로 접근)를 총 사이즈로 계산
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 

        // 현재 블록 헤더에 총 사이즈 값을 넣어서 한 블록 크기를 늘림
        PUT(HDRP(bp), PACK(size, 0));
        
        // 다음 블록의 푸터(새로 병합된 블록의 푸터)에 헤더와 같은 값을 넣어서 블록 크기를 늘림 
        // 왜 FTRP(NEXT_BLKP(bp))가 아닐까?
        // #define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
        // -> FTRP 매크로는 고정된 거리를 이동하는 것이 아닌, 현재 블록 헤더에 적힌 크기를 읽어와서 그만큼 뒤로 점프해서 푸터 위치를 찾음
        // -> GET_SIZE(HDRP(bp))로 위에서 커진 헤더의 사이즈 값을 읽어옴 => 현재 블록 bp에서 다음 블록 푸터까지 점프 
        // 바로 위에서 헤더를 미리 늘려놨기 떄문에 NEXT_BLKP 연산을 하지 않아도 됨 
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 03, 이전 블록은 가용 상태, 다음 블록만 할당 상태
    else if(!prev_alloc && next_alloc)
    {
        // 현재 블록 사이즈 + 이전 블록 사이즈(푸터로 접근)를 총 사이즈로 계산
        // size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        // -> 위 로직도 맞지만 교재에서 아래와 같이 사용하는 이유
        // -> 일관성 유지 때문. 
        // -> PREV_BLKP(bp): 이전 블록의 페이로드 시작 주소로 포인터 이동
        // -> PREV_BLKP(bp) 이후엔 이전 블록 푸터보다 헤더가 더 가까움
        // -> 이전 블록의 bp를 구한 뒤, 그 블록의 헤더에서 크기를 가져오는 정형화된 논리적 흐름을 따르는 것이 할당기 구현에서의 표준 패턴
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        // 현재 블록의 푸터를 병합된 블록의 푸터로 보고 총 사이즈 값을 넣어서 블록 크기 늘림
        PUT(FTRP(bp), PACK(size, 0));

        // 이전 블록의 헤더를 병합된 블록의 헤더로 보고 푸터와 동일한 값을 넣어서 블록 크기 늘림
        // -> 이전 블록 + 현재 블록 = 병합 블록으로 통합
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        // bp를 이전 블록으로 옮겨줌 
        bp = PREV_BLKP(bp);
    }
    // case 04, 이전/다음 블록 모두 가용 상태 
    else if(!prev_alloc && !next_alloc)
    {
        // 현재 블록 사이즈 + 이전 블록 사이즈 + 다음 블록 사이즈
        size += (GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp))));

        // 이전 블록의 헤더를 병합 블록의 헤더로 보고 총 사이즈 값을 넣어서 블록 크기 늘림
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));

        // 다음 블록의 위치로 간 뒤, 그곳의 원래 헤더 크기를 읽어 다음 블록의 푸터를 찾음 
        //PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        // 위와 아래는 동일한 결과가 나옴
        // -> 케이스 2에서의 FTRP와 동일한 점프 동작으로 인해 같은 결과가 나옴 
        // FTRP는 증가된 헤더의 사이즈값을 기준으로 푸터를 찾기 때문에 바로 이전 블록의 bp를 넣어 계산해도 됨 
        PUT(FTRP(PREV_BLKP(bp)), PACK(size, 0));

        // bp를 이전 블록으로 옮겨줌 
        bp = PREV_BLKP(bp);
    }

    return bp;
}

// 묵시적 가용 리스트에서 사용자의 요청 크기 asize에 맞는 빈 블록을 찾음
static void *find_fit(size_t size)
{
    void *bp;

#ifdef FIRST_FIT
    // 힙 처음부터 탐색 시작
    // 에필로그 블록을 만날 때까지(-> 크기가 0인 블록을 만날 때까지)
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        // 할당되지 않은 블복 + 요청한 크기보다 블록의 크기가 크같다면
        if(!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= size)
        {
            // 조건에 맞는 블록의 주소를 반환
            return bp;
        }
    }
#endif
    
#ifdef NEXT_FIT
    
#endif
    
#ifdef BEST_FIT
    
#endif

    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2 * DSIZE))
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        bp = NEXT_BLKP(bp);
        
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * mm_realloc - mm_malloc과 mm_free를 이용해 단순하게 구현됩니다.
 */
// 기본으로 제공되는 realloc 함수 
// 예외 처리 누락 -> prt이 NULL일 때 예외 처리를 하지 않아서 Segmentatoin fault 발생해 프로그램 죽음 
// 잘못된 헤더 크기 계산 -> copySize: 포인터 연산으로 헤더 값을 그대로 읽어옴 
// -> 헤더엔 크기 뿐 아니라 맨 끝에 할당 상태 비트 1도 함께 있어서 이걸 걸러내는 매크로를 사용해야 기존 블록 크기를 정확하게 구할 수 있음 
// 비효율적인 무조건 복사 -> 호출되면 무조건 새 공간을 할당받고 데이터 복사해 처리 속도와 메모리 공간 모두 낭비 
// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;

//     newptr = mm_malloc(size);
//     if (newptr == NULL)
//         return NULL;
//     copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
//     if (size < copySize)
//         copySize = size;
//     memcpy(newptr, oldptr, copySize);
//     mm_free(oldptr);
//     return newptr;
// }

void *mm_realloc(void *ptr, size_t size)
{
    char *bp;
    size_t copySize;

    // ptr NULL 확인
    if(ptr == NULL)
    {
        return mm_malloc(size);
    }

    if(size == 0)
    {
        mm_free(ptr);
        
        return NULL; 
    }

    // 새 공간 할당
    if((bp = mm_malloc(size)) == NULL)
    {
        return NULL;
    }

    // 기존 블록에 들어있던 데이터 크기와 새로 요청한 크기 중 더 작은 값만큼만 기존 데이터를 새 공간으로 복사 
    // GET_SIZE: 헤더와 푸터를 포함한 전체 블록 크기 반환
    // DSIZE: 2워드 = 헤더 + 푸터 합산 크기 
    // -> GET_SIZE - DSIZE = 실제 데이터가 들어있는 페이로드의 크기
    if((GET_SIZE(HDRP(ptr)) - DSIZE) > size)
    {
        copySize = size;
    }
    else
    {
        copySize = (GET_SIZE(HDRP(ptr)) - DSIZE);
    }
    
    // 데이터 복사
    // memcpy 활용 
    memcpy(bp, ptr, copySize);    

    // 기존 공간 해제 
    mm_free(ptr);

    // 새 블록의 주소 반환 
    return bp;
}