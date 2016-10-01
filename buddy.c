/**
 * Buddy Allocator
 *
 * For the list library usage, see http://www.mcs.anl.gov/~kazutomo/list/
 */

/**************************************************************************
 * Conditional Compilation Options
 **************************************************************************/
#define USE_DEBUG 0

/**************************************************************************
 * Included Files
 **************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "buddy.h"
#include "list.h"

/**************************************************************************
 * Public Definitions
 **************************************************************************/
#define MIN_ORDER 12
#define MAX_ORDER 20

#define PAGE_SIZE (1<<MIN_ORDER)
/* page index to address */
#define PAGE_TO_ADDR(page_idx) (void *)((page_idx*PAGE_SIZE) + g_memory)

/* address to page index */
#define ADDR_TO_PAGE(addr) ((unsigned long)((void *)addr - (void *)g_memory) / PAGE_SIZE)

/* find buddy address */
#define BUDDY_ADDR(addr, o) (void *)((((unsigned long)addr - (unsigned long)g_memory) ^ (1<<o)) \
									 + (unsigned long)g_memory)

#if USE_DEBUG == 1
#  define PDEBUG(fmt, ...) \
	fprintf(stderr, "%s(), %s:%d: " fmt,			\
		__func__, __FILE__, __LINE__, ##__VA_ARGS__)
#  define IFDEBUG(x) x
#else
#  define PDEBUG(fmt, ...)
#  define IFDEBUG(x)
#endif

/**************************************************************************
 * Public Types
 **************************************************************************/
typedef struct {
	struct list_head list;
	/* TODO: DECLARE NECESSARY MEMBER VARIABLES */
	int id;
	void* address;
	int order;
	int used;
} page_t;

int max_free = MAX_ORDER;

/**************************************************************************
 * Global Variables
 **************************************************************************/
/* free lists*/
struct list_head free_area[MAX_ORDER+1];

/* memory area */
char g_memory[1<<MAX_ORDER];

/* page structures */
page_t g_pages[(1<<MAX_ORDER)/PAGE_SIZE];

/**************************************************************************
 * Public Function Prototypes
 **************************************************************************/

/**************************************************************************
 * Local Functions
 **************************************************************************/

/**
 * Initialize the buddy system
 */
void buddy_init()
{
	int i;
	int n_pages = (1<<MAX_ORDER) / PAGE_SIZE;
	for (i = 0; i < n_pages; i++) {
		/* TODO: INITIALIZE PAGE STRUCTURES */
		g_pages[i].id = i;
		g_pages[i].order = MAX_ORDER;
		g_pages[i].address = PAGE_TO_ADDR(i);
		g_pages[i].used = 0;
		INIT_LIST_HEAD(&g_pages[i].list);
	}

	/* initialize freelist */
	for (i = MIN_ORDER; i <= MAX_ORDER; i++) {
		INIT_LIST_HEAD(&free_area[i]);
	}

	/* add the entire memory as a freeblock */
	list_add(&g_pages[0].list, &free_area[MAX_ORDER]);
}

/**
 * Allocate a memory block.
 *
 * On a memory request, the allocator returns the head of a free-list of the
 * matching size (i.e., smallest block that satisfies the request). If the
 * free-list of the matching block size is empty, then a larger block size will
 * be selected. The selected (large) block is then splitted into two smaller
 * blocks. Among the two blocks, left block will be used for allocation or be
 * further splitted while the right block will be added to the appropriate
 * free-list.
 *
 * @param size size in bytes
 * @return memory block address
 */
void *buddy_alloc(int size)
{
	/* TODO: IMPLEMENT THIS FUNCTION */
	if(size > (1 << MAX_ORDER)) {
		return NULL;
	}

	int o = MIN_ORDER;
	while((o < MAX_ORDER) && ((1 << o) < size)){
		o++;
	}		

	int i;
	for(i = o; i < MAX_ORDER; i++){
		if(!list_empty(&free_area[i])){
			break;
		}
	}

	if((i == MAX_ORDER) && (list_empty(&free_area[i]))){ 
		return NULL;
	}

	while(i != o){
		page_t* tmp = list_entry(free_area[i].next, page_t, list);
		i--;
		list_add(&g_pages[(ADDR_TO_PAGE(BUDDY_ADDR(tmp->address,i)))].list, &free_area[i]);
		g_pages[(ADDR_TO_PAGE(BUDDY_ADDR(tmp->address,i)))].order = i;
		list_move(free_area[i+1].next, &free_area[i]);
	}

	page_t* tmp = list_entry(free_area[i].next, page_t, list);
	tmp->order = i;
	tmp->used = 1;
	list_del_init(free_area[i].next);
	return tmp->address;	
}

/**
 * Free an allocated memory block.
 *
 * Whenever a block is freed, the allocator checks its buddy. If the buddy is
 * free as well, then the two buddies are combined to form a bigger block. This
 * process continues until one of the buddies is not free.
 *
 * @param addr memory block address to be freed
 */
void buddy_free(void *addr)
{
	/* TODO: IMPLEMENT THIS FUNCTION */
	page_t *tmp = &g_pages[ADDR_TO_PAGE(addr)];
	tmp->used = 0;

	if(list_empty(&free_area[tmp->order])) {
		list_add(&tmp->list, &free_area[tmp->order]);
	} else {
		struct list_head *pos;
		list_for_each(pos, &free_area[tmp->order]) {		
			page_t* tmp2 = list_entry(pos, page_t, list);
			if((tmp->id) < (tmp2->id)) {
				break;
			}
		}
		list_add(&tmp->list, pos->prev);
	}

	int buddyID = ADDR_TO_PAGE(BUDDY_ADDR(tmp->address, tmp->order));
	
	while((tmp->order < MAX_ORDER) && (g_pages[buddyID].used == 0) && tmp->order == g_pages[buddyID].order){
		if(buddyID >= tmp->id){
			tmp->order++;
			g_pages[buddyID].order = MAX_ORDER;
			list_del_init(&g_pages[buddyID].list);
		} else {
			g_pages[buddyID].order = tmp->order + 1;
			tmp->order = MAX_ORDER;
			list_del_init(&tmp->list);
			tmp = &g_pages[buddyID];
		}

		if(list_empty(&free_area[tmp->order])) {
			list_move(&tmp->list, &free_area[tmp->order]);
		} else {
			list_del_init(&tmp->list);
			struct list_head *pos;
			list_for_each(pos, &free_area[tmp->order]) {		
				page_t* tmp2 = list_entry(pos, page_t, list);
				if((tmp->id) < (tmp2->id)) {
					break;
				}
			}
			list_add(&tmp->list, pos->prev);
		}
		
		buddyID = ADDR_TO_PAGE(BUDDY_ADDR(tmp->address, tmp->order));
	}
}

/**
 * Print the buddy system status---order oriented
 *
 * print free pages in each order.
 */
void buddy_dump()
{
	int o;
	for (o = MIN_ORDER; o <= MAX_ORDER; o++) {
		struct list_head *pos;
		int cnt = 0;
		list_for_each(pos, &free_area[o]) {
			cnt++;
		}
		printf("%d:%dK ", cnt, (1<<o)/1024);
	}
	printf("\n");
}
