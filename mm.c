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
#define WSIZE 4          
#define DSIZE 8             
#define CHUNKSIZE (1 << 12)

/* 매크로 */
#define MAX(x, y) ((x) > (y) ? (x) : (y)) 
#define PACK(size, alloc) ((size) | (alloc)) 
#define GET(p) (*(unsigned int *)(p)) 
#define PUT(p, val) (*(unsigned int *)(p) = (val)) 
#define GET_SIZE(p) (GET(p) & ~0x7) 
#define GET_ALLOC(p) (GET(p) & 0x1) 
#define HDRP(bp) ((char *)(bp) - WSIZE) 
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 전역 영역 선언 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *first_fit(size_t asize);

static char *heap_listp;  
static char *next_heap_listp;

/** 메인 함수 **/

/* 
 * mm_init - initialize the malloc package.
 * 최초의 가용블록(4words)을 가지고 힙을 생성하고 할당기를 초기화한다. 
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) 
        return -1;
    PUT(heap_listp, 0);                            
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); 
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));    
    
    heap_listp+= (2*WSIZE);
    next_heap_listp = heap_listp;

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * 새 블록을 할당한다.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) 
        return NULL;

    if (size <= DSIZE) 
        asize = 2 * DSIZE;
    else 
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    bp = first_fit(asize); // Choice fit-method : first_fit, next_fit, best_fit

    if (bp != NULL) {
        place(bp, asize); 
        next_heap_listp = bp;
        return bp; 
    }

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
    size_t size = GET_SIZE(HDRP(bp)); 
    PUT(HDRP(bp),PACK(size,0)); 
    PUT(FTRP(bp), PACK(size,0)); 
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
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




/** 서브 함수 **/

/* 요청받은 words만큼 추가 메모리를 요청한다.*/
static void *extend_heap(size_t words)
{
    char *bp;

    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1) 
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         
    PUT(FTRP(bp), PACK(size, 0));         
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 
    
    return coalesce(bp); 
}


/* fist fit 방식으로 가용 블록을 탐색한다. */
static void *first_fit(size_t asize)
{
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;

    return NULL;
}

/* 찾은 가용 블록에 대해 할당 블록과 가용 블록으로 분할한다. */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        next_heap_listp = bp;
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
    } else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        next_heap_listp = NEXT_BLKP(bp);
    }
}

/* 인접 가용 블록들을 경계 태그 연결 기술을 사용하여 연결한다. */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); 
    size_t size = GET_SIZE(HDRP(bp)); 

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
    next_heap_listp = bp; 
    return bp;
}