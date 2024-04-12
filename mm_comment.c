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

/* 기본 상수  */
#define WSIZE 4             // word size
#define DSIZE 8             // double word size
#define CHUNKSIZE (1 << 12) // 힙 확장을 위한 기본 크기 (= 초기 빈 블록의 크기, 4kb)

/* 매크로 */
#define MAX(x, y) ((x) > (y) ? (x) : (y)) // x,y 중 큰 값

#define PACK(size, alloc) ((size) | (alloc)) // 크기와 할당 비트를 통합 -> 헤더와 푸터에 저장
// alloc : 가용여부 (ex. 000) / size : block size를 의미. =>합치면 온전한 주소가 나온다.

#define GET(p) (*(unsigned int *)(p)) // p가 참조하는 워드 리턴 / 인자 p는 (void*) 이므로 역참조는 불가하다.
#define PUT(p, val) (*(unsigned int *)(p) = (val)) // p에 val을 저장

#define GET_SIZE(p) (GET(p) & ~0x7) // 헤더 또는 푸터의 사이즈 리턴
#define GET_ALLOC(p) (GET(p) & 0x1) // 가용 여부 리턴

#define HDRP(bp) ((char *)(bp) - WSIZE) // 블록 헤더를 가리키는 포인터 리턴
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 블록 푸터를 가리키는 포인터 리턴
// ↳ HDRP(bp)에 헤더 사이즈(w)도 포함되어 있어서 DSIZE를 빼준다.

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) // 다음 블록의 포인터
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) // 이전 블록의 포인터

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static void *extend_heap(size_t words);
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void){ // 처음에 heap을 시작할 때 0부터 시작. 완전 처음.(start of heap)
    /* create 초기 빈 heap*/
    char *heap_listp;
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1){ // old brk에서 4만큼 늘려서 mem brk로 늘림.
        return -1;
    }
    PUT(heap_listp,0); // 블록생성시 넣는 padding을 한 워드 크기만큼 생성. heap_listp 위치는 맨 처음.
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE,1)); // prologue header 생성. pack을 해석하면, 
    // 할당을(1) 할건데 8만큼 줄거다(DSIZE). -> 1 WSIZE 늘어난 시점부터 PACK에서 나온 사이즈를 줄거다.)
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); // prologue footer생성.
    PUT(heap_listp + (3*WSIZE), PACK(0,1)); // epilogue block header를 처음에 만든다. 그리고 뒤로 밀리는 형태.
    heap_listp+= (2*WSIZE); // prologue header와 footer 사이로 포인터로 옮긴다. header 뒤 위치. 다른 블록 가거나 그러려고.

    if (extend_heap(CHUNKSIZE/WSIZE)==NULL)
        return -1;
    // 주어진 워드 수(CHUNKSIZE/WSIZE) 만큼 힙을 확장하고, 성공하면 확장된 힙의 시작 주소를 반환한다. 
    // 만약 확장이 실패하면 NULL을 반환한다.
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp; // 블록 포인터
    size_t size;

    // 더블 워드 정렬 제한 조건을 적용하기 위해 반드시 짝수 사이즈(8byte)의 메모리만 할당한다.(반올림)
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    bp = mem_sbrk(size); // mem_sbrk return : 이전 brk(epilogue block 뒤 포인터) 반환
    // ↳ 실제 brk : 확장 후 힙의 맨 끝 포인터

    if ((long)bp ==  -1) // mem_sbrk err return
        return NULL;

    PUT(HDRP(bp), PACK(size, 0)); // size 만큼의 가용 블록의 헤더를 생성한다.
    PUT(FTRP(bp), PACK(size, 0)); // size 만큼의 가용 블록의 푸터를 생성한다.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새로운 에필로그 헤더를 생성한다.

    return coalesce(bp); // 이전 힙이 가용 블록이라면 연결 수행
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
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














