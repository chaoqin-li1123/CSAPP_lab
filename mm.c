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
    /* Team name */
    "ChaoqinTeam",
    /* First member's full name */
    "Chaoqin Li",
    /* First member's email address */
    "chaoqin@uchicago.edu",
    /* Second member's full name (leave blank if none) */
    "Chaoqin Li",
    /* Second member's email address (leave blank if none) */
    "chaoqin@uchicago.edu"
};
/* block structure
free block

*****************************************************
*    (unsigned)size     *    (unsigned)last_size    *
*****************************************************
*                  (Node*) next                     *                      
*****************************************************
*                  (Node*) prev                     *            
*****************************************************
*                   ....  (size - 16) bytes         *                  
*****************************************************


allocated blocks are similliar, but don't have next and prev pointer.

*****************************************************
*    (unsigned)size     *    (unsigned)last_size    *
*****************************************************
*                   (pay load)  size bytes          *  
*****************************************************

Free list organization:
Use segregated list. Maintain 12 doubly linked list. Partition the size of block into power of 2. Calculate the corresponding list index of a block size, insert the free block into the begining of the list. 
When we search a free block with block size > malloc size, also calculate the corresponding list index idx of the malloc size, start from list idx, search for the first block that meet the requirement. Record the index of list that a free block is found and start from this list next time.
(first fit for search inside a list, next fit for search in multiple lists).
*/
typedef struct Node Node;
struct Node
{
    unsigned size; // The size of available memory inside block, set to 0 for dummy head and tail, last bit of size decide whether the block has been used.
    unsigned last_size; // The size of block that come before this block.
    Node * next;
    Node * prev;
};
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
#define LISTN 12
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
// find a block that correspond to an allocated pointer
#define ptr2node(ptr) (Node*)((char*) ptr - HEADER_SIZE)
// Given a pointer to node, return a pointer to its managed memory.
#define node2ptr(node) (void*)((char*)node + HEADER_SIZE)
#define CHUNK_SIZE (27704)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define HEADER_SIZE (2 * sizeof(unsigned))
#define FREE(node) (!(node->size % 2))
// Dummy head and tail for double linked list.
Node* head;
int last_idx = 0;
int first_no_empty = 0;
unsigned alloc_cnt = 0;
const int MIN_BLOCK = 2 << 8;
const int BIG_BLOCK = 622632;
const int PTR_SIZE = sizeof(void*);
void* re_alloc_ptr = NULL;
// Checker function.
int mm_check(void);
int all_free_node_valid(void);
int all_node_in_free_list_free(void);
int all_free_node_in_free_list(void);
int node_in_right_list();
// Helper function
int size2list_idx(int size);
int isfree(Node* node);
void mark_used(Node* node);
void mark_free(Node* node);
int node_size(Node* node); 
Node* next_block(Node* node);
Node* prev_block(Node* node);
void update_next_block(Node* this);

// Helper function for free.
void insert_next(Node* node, Node* this);
void insert_free(Node* node); 
void free_node(Node* node);
void free_ptr(void* ptr);
Node* coalesce(Node* node);

// Helper function for allocate
Node* find_free(int size);
Node* find_in_alist(int idx, int size);
Node* get_chunk_by_size(int size);
void detach_node(Node* node);
void* get_mem(Node* node, int size);

// Mark the node as used, set the last bit of node->size to 1.
void mark_used(Node* node) {
    if (!(node->size % 2)) node->size += 1;
}
// Mark the node as free, set the last bit of node->size to 0.
void mark_free(Node* node) {
    if (node->size % 2) node->size -= 1;

}
// Return the size of memory managed by a node.
int node_size(Node* node) {
    if (node->size % 2) return node->size - 1;    
    return node->size;
}
// Compute the corresponding index of the size in a segregated list.
int size2list_idx(int size) {
    int temp = 1 << 6;
    int idx = 0;
    while (temp < size) {
        temp <<= 1;
        idx++;
    }
    if (idx > LISTN - 1) idx = LISTN - 1;
    return idx;
}

// Insert a Node after prev
void insert_next(Node* prev, Node* this) {
    Node* next = prev->next;
    prev->next = this;
    if (next != NULL) next->prev = this;
    this->next = next;
    this->prev = prev;
}
// Insert a free node into the beginning of a free list.
void insert_free(Node* node) {
    int idx = size2list_idx(node->size);
    if (idx >= last_idx && idx < first_no_empty) first_no_empty = idx;
    insert_next(&head[idx], node);
}
// Get a chunk of memory using mem_sbrk, make a free block and insert the node into the list, return pointer to node.
Node* get_chunk_by_size(int size) {
    alloc_cnt++;
    size += 48;
    if (size < MIN_BLOCK) size = MIN_BLOCK;
    if (alloc_cnt > 70) {
        if (size < 1 << 14) size = (1 << 15);
	else size = 4 * size;
    }    
    Node* this = mem_sbrk(HEADER_SIZE + size);
    this->size = size;
    this->last_size = 0;
    mark_free(this);
    insert_free(this);
    return this;
}
// Get next block
Node* next_block(Node* node) {
    return (void*)node + HEADER_SIZE + node_size(node);
}
// Get previous block
Node* prev_block(Node* node) {
    if (node->last_size == 0) return NULL;
    return (void*) node - HEADER_SIZE - node->last_size;
}
// Set the node->last_size field of next block.
void update_next_block(Node* this) {
    Node* next = next_block(this);
    if ((void*)next < mem_heap_hi()) next->last_size = node_size(this);
}
/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    head = (Node*)mem_sbrk(sizeof(Node) * LISTN);
    for (int i = 0; i < LISTN; i++) {
        head[i].size = 1;
        head[i].next = NULL;
    }
    return 0;
}
// Detach a node from the free list.
void detach_node(Node* node) {
    Node* prev = node->prev;
    Node* next = node->next;
    prev->next = next;
    if (next != NULL) next->prev = prev;
}
// Find from begin of a free list and return a free node that has size bigger than required size.
Node* find_in_alist(int idx, int size) {
    Node* ahead = &head[idx];
    Node* this = ahead->next;
    for (; this != NULL; this = this->next) {
        if (this->size >= size) return this;
    }
    return NULL;    
}
// Find a free block with size bigger than given size, return NULL if no free block in the list meet the requirement.
Node* find_free(int size) {
    int idx = size2list_idx(size);
    Node* node = NULL;
    int i = idx;
    if (last_idx >= idx && first_no_empty > i) i = first_no_empty; 
    for (; i < LISTN; i++) {
        node = find_in_alist(i, size);
        if (node != NULL) {
            first_no_empty = i;
            break;
        }
    }
    last_idx = idx;
    return node;
}
// Coalesce the node with its adjacent nodes, return a Node* to the new coalesced node.
Node* coalesce(Node* node) {
    Node* next = next_block(node);    
    Node* prev = prev_block(node);
    int cnt = 0;
    if (mem_heap_hi() >= (void*)next && isfree(next)) {
        detach_node(next);
        node->size = node_size(node) + node_size(next) + HEADER_SIZE;
        cnt++;
    }
    if (prev && isfree(prev)) {
        detach_node(prev);
        prev->size = node_size(prev) + node_size(node) + HEADER_SIZE;
        node = prev;
        cnt++;
    } 
    if (cnt) update_next_block(node);  
    return node;
}
// Free a node and insert it into list.
void free_node(Node* node) {
    if (alloc_cnt > 1) alloc_cnt -= 1;
    update_next_block(node);
    node = coalesce(node);
    mark_free(node);
    insert_free(node);
}
// Find a block that correspond to the pointer to be freed, free the block.
void free_ptr(void* ptr) {
    Node* node = ptr2node(ptr);
    free_node(node);
}
// Get memory from a block in the free list, split the block if the space left can hold another node.
void* get_mem(Node* node, int size) {
    int unused = node->size - size;
    // Split the block.
    if (unused >= 2 * PTR_SIZE + HEADER_SIZE) {
        // If the size to be allocated is big, use the right half of the block, otherwise use the left half.
    	if (size > 72) {
            node->size = (unused) - HEADER_SIZE;
            Node* right = (Node*)((void*) node + HEADER_SIZE + node_size(node));
            right->size = size + 1;
            //mark_used(right);
            update_next_block(node);
            update_next_block(right);
	    detach_node(node);
	    insert_free(node);
            return node2ptr(right);
    	}
    	else {
            Node* right = (Node*) ((void*) node + HEADER_SIZE + size);
            right->size = unused - HEADER_SIZE;
            mark_free(right);
            node->size = size + 1;
            //mark_used(node);
            update_next_block(node);
            update_next_block(right);
            detach_node(node);
            insert_free(right);
            return node2ptr(node);
    	}
    }
    mark_used(node);
    detach_node(node);
    return node2ptr(node);
}
// Allocate a big chunk of memory if the ptr has been reallocated for many times.
void *get_chunk_realloc(void *ptr, size_t size) {
    int temp = 1756;
    if (size > CHUNK_SIZE) temp = BIG_BLOCK;
    while (temp < size) temp <<= 4;
    Node* node = ptr2node(ptr);
    Node* next = next_block(node);
    // If this node is on top of heap, use mem_sbrk to allocate new memory.
    if ((void*)next > mem_heap_hi()) {
        int remain = temp - size;
        mem_sbrk(remain + 8);
        node->size = temp;
        mark_used(node);
        node->last_size = 0;
        return ptr;
    }
    Node* newnode = (Node*)mem_sbrk(HEADER_SIZE + temp);
    newnode->size = temp;
    mark_used(newnode);
    newnode->last_size = 0;
    Node* newptr = node2ptr(newnode);
    free_node(node);
    memcpy(newptr, ptr, node->size);
    return newptr;    
}
/* 
 * mm_malloc - Search the free list start from index that correspond to malloc size, if not block in the free list meet the requirement, use mem_sbrk() to get a big block. Get the memory from the free block found in the free list.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (!size) return NULL;
    size = ALIGN(size);
    if (size < 2 * PTR_SIZE) size = 2 * PTR_SIZE;
    // Find a free block that the allocated memory can fit in.
    Node* node = find_free(size);
    // Can't get a block, use mem_sbrk()
    if (!node) node = get_chunk_by_size(size);
    // return a pointer to the memory managed by the node. 
    void* ptr = get_mem(node, size);
    //mm_check();
    return ptr;
}

/*
 * mm_free - Caculate the corresponding node of a given pointer, insert the node into the free list.
 */
void mm_free(void *ptr)
{
    free_ptr(ptr);
    //mm_check();
}
/*
 * mm_realloc - For null ptr. malloc, for 0 size, free, if the pointer has benn extended many times, give it a big chunk of memory when it call realloc. If the size required by realloc is smaller than the current size of the block, do nothing. When extending a block, if it is on top of heap, just call mem_sbrk directly to extend.
 */
void *mm_realloc(void *ptr, size_t size)
{
    // If ptr is NULL, just malloc.
    if (!ptr) return mm_malloc(size);
    // If size == 0, free the pointer.
    if (!size) {
        mm_free(ptr);
        return NULL;
    }
    re_alloc_ptr = ptr;
    void *oldptr = ptr;
    Node* node = ptr2node(oldptr);
    // If reallocate the node to be smaller, don't require more space.
    if (node_size(node) >= size) return ptr;
    size = ALIGN(size);
    size_t copy_size = node_size(node);
    if (ptr == re_alloc_ptr) return get_chunk_realloc(ptr, size);
    void *newptr = mm_malloc(size); 
    free_node(node); 
    memcpy(newptr, oldptr, copy_size);
    //mm_check();
    return newptr;
}
// Return 0 if the block is free, 1 if the node is allocated.
int isfree(Node* node) {
    return !(node->size % 2);
}
// Count the total number of nodes in the free list.
int cnt_free_node_in_list(void) {
    int cnt = 0;
    for (int i = 0; i < LISTN; i++) {
        Node* ahead = &head[i];
        Node* free_node = ahead->next;
        for (; free_node != NULL; free_node = free_node->next) {
            cnt++;
        }
    }
    return cnt;    
}
// Count the total number of nodes on heap.
int cnt_all_free_nodes(void) {
    Node* node = &head[LISTN]; 
    int cnt = 0;
    while ((void*)node < mem_heap_hi()) {
        if (isfree(node)) cnt++;        
        node = next_block(node);
    }
    return cnt;    
}
// Check whether all nodes in the free list is free.
int all_node_in_free_list_free(void) {
    for (int i = 0; i < LISTN; i++) {
        Node* ahead = &head[i];
        Node* free_node = ahead->next;
        for (; free_node != NULL; free_node = free_node->next) {
            if (!isfree(free_node)) {
                printf("Node in free list not free, pointer: %p, size: %d", node2ptr(free_node), node_size(free_node));
                return 0;
            }
        }
    }
    return 1;
}
// All free node in the free list?
int all_free_node_in_free_list(void) {
    int in_list_cnt = cnt_free_node_in_list();
    int all_cnt = cnt_all_free_nodes();
    if (in_list_cnt == all_cnt) return 1;
    printf("Total number of free nodes:%d, free nodes in free list:%d.\n", all_cnt, in_list_cnt);
    return 0;
}
// Check whether all nodes in free list inside the range of heap.
int all_free_node_valid(void) {
    void* ptr;
    for (int i = 0; i < LISTN; i++) {
        Node* ahead = &head[i];
        Node* free_node = ahead->next;
        for (; free_node != NULL; free_node = free_node->next) {
            ptr = node2ptr(free_node);
            if (ptr < mem_heap_lo() || ptr > mem_heap_hi()) {
                printf("Out of head: %p, heap hi: %p, heap lo: %p", ptr, mem_heap_hi(), mem_heap_lo());
                return 0;
            }
        }
    }
    return 1;
}
// Check if all nodes in the right index of free list.
int node_in_right_list(void) {
    for (int i = 0; i < LISTN; i++) {
        Node* ahead = &head[i];
        Node* free_node = ahead->next;
        for (; free_node != NULL; free_node = free_node->next) {
            int idx = size2list_idx(free_node->size);
            if (idx != i) {
                printf("Block not in the right list, address: %p, size %d bytes, should be in list %d, actually in list %d.\n", node2ptr(free_node), free_node->size, idx, i);
                return 0;
            }
        }
    }
    return 1;
}
// Return 0 if heap is not consistent.
int mm_check(void) {
    return all_free_node_in_free_list() && all_node_in_free_list_free() && all_free_node_valid() && node_in_right_list();
}

















