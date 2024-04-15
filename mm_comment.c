/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    "team 4",
    "Yoosun-Chang",
    "yoosun0815@gmail.com",
    "",
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

/* 기본 상수 */
#define WSIZE 4             // word size
#define DSIZE 8             // double word size
#define CHUNKSIZE (1 << 12) // 초기 가용 블록과 힙 확장을 위한 기본 크기 

/* 매크로 */
#define MAX(x, y) ((x) > (y) ? (x) : (y)) // 두 값 중 큰 값을 반환
#define PACK(size, alloc) ((size) | (alloc)) // size와 할당 비트를 결합 -> 헤더와 푸터에 저장
#define GET(p) (*(unsigned int *)(p)) // 주소 p가 가리키는 값을 반환(포인터라서 직접 역참조 불가능 -> 타입 캐스팅)
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // 주소 p에 val을 저장
#define GET_SIZE(p) (GET(p) & ~0x7) // 블록의 크기를 반환(~0x7: ...11111000, '&' 연산으로 뒤에 세자리 없어짐)
#define GET_ALLOC(p) (GET(p) & 0x1) // 블록의 할당 정보를 반환
#define HDRP(bp) ((char *)(bp) - WSIZE) // 블록의 헤더 주소를 반환
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 블록의 푸터 주소를 반환
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 블록의 다음 블록의 주소를 반환
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 블록의 이전 블록의 주소를 반환
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 전역 영역 선언 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *first_fit(size_t asize);
static void *next_fit(size_t asize);
static void *best_fit(size_t asize);
static void place(void *bp, size_t asize);

static char *heap_listp;  
static char *next_heap_listp;

/** 메인 함수 **/

/* 
 * mm_init - initialize the malloc package.
 * 최초의 가용블록(4words)을 가지고 힙b  을 생성하고 할당기를 초기화한다. 
 */
int mm_init(void)
{
    // 1. 초기 빈 힙 생성
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                             // 정렬을 위한 여백 삽입                           
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  // 프롤로그 헤더 설정
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  // 프롤로그 푸터 설정
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      // 에필로그 헤더 설정
    
    heap_listp+= (2*WSIZE); // heap_listp 위치를 프롤로그 헤더 뒤로 옮긴다.
    next_heap_listp = heap_listp; // next_fit에서 사용하기 위해 초기 포인터 위치를 넣어준다.

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) // 2. 이후 일반 블록을 저장하기 위해 힙을 확장한다.
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * 새 블록을 할당한다.
 */
void *mm_malloc(size_t size)
{
    size_t asize; // 조정된 블록 크기
    size_t extendsize; // 적합한 블록을 찾지 못했을 때 확장할 힙의 크기
    char *bp;

    //불필요한 요청 무시
    if (size == 0) 
        return NULL;

    // 더블 워드 정렬 제한 조건을 만족 시키기위해 더블 워드 단위로 크기를 설정한다.    
    if (size <= DSIZE) // 최소 크기인 16바이트(헤더, 푸터, 페이로드 포함)로 설정한다.
        asize = 2 * DSIZE;
    else // 8바이트를 넘는 요청 처리
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    bp = next_fit(asize); // Choice fit-method : first_fit, next_fit, best_fit

    if (bp != NULL) {
        place(bp, asize); // 초과부분을 분할한다.
        next_heap_listp = bp;
        return bp; // 새롭게 할당한 블록을 리턴한다.
    }

    // 적합한 블록을 찾지 못했을 경우, 힙을 확장하여 새로운 블록 할당
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    next_heap_listp = bp;
    return bp;
} 

/*
 * mm_free - Freeing a block does nothing.
 * 요청한 블록을 반환한다.
 */
void mm_free(void *bp){ 
    size_t size = GET_SIZE(HDRP(bp)); // 반환하려는 블록의 사이즈

    //할당 비트를 해제하여 블록을 사용 가능한 빈 블록으로 표시
    PUT(HDRP(bp),PACK(size,0)); 
    PUT(FTRP(bp), PACK(size,0)); 

    coalesce(bp); // 인접 가용 블록들에 대한 연결을 수행
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 * 이전에 할당한 메모리의 크기를 재조정한다.
 */
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) { // 포인터가 NULL인 경우 할당만 수행
        return mm_malloc(size);
    }

    if (size == 0) { // size가 0인 경우 메모리 반환만 수행
        mm_free(ptr);
        return NULL;
    }

    size_t old_size =  GET_SIZE(HDRP(ptr)) - DSIZE; // payload만큼 복사
    // 기존 사이즈가 새 크기보다 더 크면 size로 크기 변경 (기존 메모리 블록보다 작은 크기에 할당하면, 일부 데이터만 복사)
    size_t copy_size = old_size < size ? old_size : size;

    void *new_ptr = mm_malloc(size); // 새로 할당한 블록의 포인터
    if (new_ptr == NULL)
        return NULL;

    memcpy(new_ptr, ptr, copy_size); // 새 블록으로 데이터 복사
    mm_free(ptr);

    return new_ptr;
}




/** 서브 함수 **/

/* 요청받은 words만큼 추가 메모리를 요청한다. */
static void *extend_heap(size_t words)
{
    char *bp;

    // 요청된 워드 수를 더블 워드 정렬 제한 조건에 적용하기 위해 짝수로 만든다. 
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 
    // mem_sbrk return : 이전 brk(epilogue block 뒤 포인터) 반환
    if ((long)(bp = mem_sbrk(size)) == -1) 
        return NULL;
    
    // 새로운 블록의 헤더와 푸터 설정
    PUT(HDRP(bp), PACK(size, 0));         // 블록 헤더 설정     
    PUT(FTRP(bp), PACK(size, 0));         // 블록 푸터 설정
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새로운 에필로그 헤더 설정
    
    return coalesce(bp); // 이전 블록이 가용블록이라면, 새로운 블록과 병합 
}


/* fist fit 방식으로 가용 블록을 탐색한다. */
static void *first_fit(size_t asize)
{
    void *bp;

    // 에필로그 블록의 헤더를 0으로 넣어줬으므로 에필로그 블록을 만날 때까지 탐색을 진행한다.
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;

    return NULL;
}

/* next fit 방식으로 가용 블록을 탐색한다. */
static void *next_fit(size_t asize)
{
    char *bp;

    // next_fit 포인터에서 탐색을 시작한다.
    for (bp = NEXT_BLKP(next_heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;

    for (bp = heap_listp; bp <= next_heap_listp; bp = NEXT_BLKP(bp))
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;

    return NULL;
}

/* best-fit 방식으로 가용 블록을 탐색한다. */
static void *best_fit(size_t asize)
{
    void *bp;
    void *best_fit = NULL;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            // 기존에 할당하려던 공간보다 더 최적의 공간이 나타났을 경우 리턴 블록 포인터 갱신
            if (!best_fit || GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(best_fit)))
                best_fit = bp;

    return best_fit;
}

/* 찾은 가용 블록에 대해 할당 블록과 가용 블록으로 분할한다. */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp)); // 가용 블록의 사이즈

    if ((csize - asize) >= (2 * DSIZE)) {
        // asize만큼의 할당 블록을 생성한다.
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        next_heap_listp = bp; // 분할 이후 그 다음 블록
        // 새로운 할당 블록(전체 가용 블록 - 할당 블록)의 뒷 부분을 가용 블록으로 만든다.
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        next_heap_listp = NEXT_BLKP(bp); // 분할 이후 그 다음 블록
    }
}

/* 인접 가용 블록들을 경계 태그 연결 기술을 사용하여 연결한다. */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록 푸터에서 할당 여부 파악
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록 헤더에서 할당 여부 파악
    size_t size = GET_SIZE(HDRP(bp)); // 블록 사이즈

    // @pb : 이전 블록, @cb : 현재 블록, @nb : 다음 블록
    // [CASE 1] : pb, nb - 둘 다 할당 상태
    if (prev_alloc && next_alloc)
    {
        return bp;
    }
    // [CASE 2] : pb - 할당 상태 / nb - 가용 상태
    // resize block size : cb + nb
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 현재 블록 사이즈에 다음 블록 사이즈 더함
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // [CASE 3] : pb - 가용 상태 / nb - 할당 상태
    // resize block size : pb + cb
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // [CASE 4] : pb - 가용 상태 / nb - 가용 상태
    // resize block size : pb + nb
    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    // next_heap_listp가 속해있는 블록이 이전 블록과 합쳐진다면 
    // next_heap_listp에 해당하는 블록을 찾아갈 수 없으므로
    // 새로 next_heap_listp를 이전 블록 위치로 지정해준다.
    next_heap_listp = bp; 
    return bp;
}