#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "buffer_mgr_stat.h"
#include "dt.h"
#include "dberror.h"
#include "stdio.h"
#include "stdlib.h"
#include "pthread.h"

//Mutex Objects for making initialize, pin and unpin methods thread safe
static pthread_mutex_t mutex_init = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_unpinPage = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_pinPage = PTHREAD_MUTEX_INITIALIZER;

//Buffer Structure used for all the implementations
typedef struct Buffer
{
	int buffer_mgr_pageNum;
	int storage_mgr_pageNum;
	BM_PageHandle *ph;
	bool dirty;//marks dirty pages
	int count;
	int freqCount;//counts the number of times the page was called 
	int fixcounts;
	struct Buffer *next;//pointer to the next buffer
} Buffer;

//Buffer Management Structure for all the Buffer to be created
typedef struct BM_BufferMgmt
{
	SM_FileHandle *f;
	Buffer *start;//marks the first buffer of the pool
	Buffer *current;
	Buffer *iterator;
	Buffer *search;
	int numReadIO;//number of Read IO done
	int numWriteIO;//number of Write IO done
} BM_BufferMgmt;

//Returns the lenght of the buffer pool
int lengthofPool(BM_BufferMgmt *mgmt)
{
	int count = 0;

	mgmt->iterator = mgmt->start; //make root equals the first element in the buffer pool

	while(mgmt->iterator != NULL)//until the end of the buffer pool
	{
		count++;//increament the count
		mgmt->iterator = mgmt->iterator->next; //next
	}

	return count;
}

//search the pool for the page
Buffer *searchPrevPos(BM_BufferMgmt *mgmt, PageNumber pNum)
{
	mgmt->iterator = mgmt->start;//marks the start of the buffer pool
	mgmt->search = NULL;

	while (mgmt->iterator != NULL)//check till the end of the list
	{
		if(mgmt->iterator->storage_mgr_pageNum == pNum)//on match exit the block
			break;

		mgmt->search = mgmt->iterator;
		mgmt->iterator = mgmt->iterator->next;//next
	}
	
	return mgmt->search;
}

// returns the postion of a perticular page
Buffer *searchPos(BM_BufferMgmt *mgmt, PageNumber pNum)
{	
	mgmt->iterator = mgmt->start;//mark to the start

	while (mgmt->iterator != NULL)//till the end of buffer pool
	{
		if(mgmt->iterator->storage_mgr_pageNum == pNum)
			break;

		mgmt->iterator = mgmt->iterator->next;//next
	}
	
	return mgmt->iterator;
}
//check for empty frame
int emptyBufferFrame(BM_BufferPool *bm)
{
	int flag = 0;

	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;//initialize to the start of the pool
	while (((BM_BufferMgmt *)bm->mgmtData)->iterator != NULL)
	{
		if(((BM_BufferMgmt *)bm->mgmtData)->iterator->buffer_mgr_pageNum != flag) //if the page is found
			return flag; //store it // get back to it to check POS
		
		flag = flag + 1;
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;//next
	}
	//return
	if(flag < bm->numPages)
		return flag;
	else
		return -1;
}
//function to update the counter
void updateCounter(BM_BufferMgmt *mgmt)
{
	mgmt->iterator = mgmt->start; //make temp equals the first element in the buffer pool
	
	while(mgmt->iterator != NULL)//end of the pool
	{
		mgmt->iterator->count = mgmt->iterator->count + 1;//update the counter
		mgmt->iterator = mgmt->iterator->next;//next
	}
}
//return the buffer with the count values
Buffer *searchCnt(BM_BufferMgmt *mgmt)
{
	int max = 0;

	mgmt->iterator = mgmt->start;//initialize to start
	mgmt->search = mgmt->start;

	while(mgmt->iterator != NULL)// loop till the end
	{
		if(mgmt->iterator->count > max)
		{
			if(mgmt->iterator->fixcounts == 0)
			{
				max = mgmt->iterator->count;
				mgmt->search = mgmt->iterator;// maping the value to the buffer
			}			
		}
		mgmt->iterator = mgmt->iterator->next;//next
	}

	return mgmt->search;
}

// Return max count value for LFU
int getMaxCount(BM_BufferMgmt *mgmt)
{
	int max = 0;
	mgmt->iterator = mgmt->start;
	mgmt->search = mgmt->start;

	while(mgmt->iterator != NULL)
	{
		if(mgmt->iterator->count > max)
		{
			if(mgmt->iterator->fixcounts == 0)
			{
				max = mgmt->iterator->count;
				mgmt->search = mgmt->iterator;
			}			
		}
		mgmt->iterator = mgmt->iterator->next;
	}

	return mgmt->search->count;

}

//Return Minimum frequency count value for LFU
Buffer *searchMinFreqCnt(BM_BufferMgmt *mgmt)
{
	int min = 99999;
	int maxCount = getMaxCount(mgmt);
	
	mgmt->iterator = mgmt->start;
	mgmt->search = mgmt->start;

	while(mgmt->iterator != NULL)
	{
		if(mgmt->iterator->freqCount <= min)
		{
			if(mgmt->iterator->fixcounts == 0 && mgmt->iterator->count <=  maxCount && mgmt->iterator->count != 1)
			{
				min = mgmt->iterator->freqCount;
				mgmt->search = mgmt->iterator;
			}
		}
		mgmt->iterator = mgmt->iterator->next;
	}

	return mgmt->search;
}

//Implementation of LFU (Least frequently used) algorithm
int LFU(BM_BufferPool *bm, BM_PageHandle *page, PageNumber pageNum)
{
	int a;

	((BM_BufferMgmt *)bm->mgmtData)->search = searchMinFreqCnt(bm->mgmtData);

	if(((BM_BufferMgmt *)bm->mgmtData)->search->dirty == 1)
	{
		a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);

		if (a == RC_OK)
		{
			((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;
			((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
		}
	}

	updateCounter(bm->mgmtData);

	a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);

	if(a == RC_OK)
	{
		((BM_BufferMgmt *)bm->mgmtData)->search->ph->pageNum = pageNum;
		((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum = pageNum;
		((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;
		((BM_BufferMgmt *)bm->mgmtData)->search->count = 1;
		((BM_BufferMgmt *)bm->mgmtData)->search->freqCount = 1;
		((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts = 1;

		page->data = ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data;
		page->pageNum = pageNum;

		return RC_OK;
	}

	return a;

}

// For LRU and FIFO replace the buffer according to the strategy
int replacementStrategy(BM_BufferPool *bm, BM_PageHandle *page, PageNumber pageNum)
{
	int a;

	((BM_BufferMgmt *)bm->mgmtData)->search = searchCnt(bm->mgmtData);

	if(((BM_BufferMgmt *)bm->mgmtData)->search->dirty == 1)
	{
		a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);

		if (a == RC_OK)
		{
			((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;
			((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
		}
	}
	
	updateCounter(bm->mgmtData);

	a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);
	
	if(a == RC_OK)
	{
		((BM_BufferMgmt *)bm->mgmtData)->search->ph->pageNum = pageNum;
		((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum = pageNum;
		((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;
		((BM_BufferMgmt *)bm->mgmtData)->search->count = 1;
		((BM_BufferMgmt *)bm->mgmtData)->search->freqCount = 1;
		((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts = 1;

		page->data = ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data;
		page->pageNum = pageNum;

		return RC_OK;
	}

	return a;
}

//Initializing the buffer pool
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData)
{
    if(bm->mgmtData != NULL)//error check
        return RC_BUFFER_POOL_ALREADY_INIT;

    pthread_mutex_lock(&mutex_init); // Lock the mutex when entering critical regoin
    BM_BufferMgmt *mgmt;
    mgmt = (BM_BufferMgmt *)malloc(sizeof(BM_BufferMgmt));
    mgmt->f = (SM_FileHandle *)malloc(sizeof(SM_FileHandle));
	//initiazing the buffer to NULL
    if(mgmt->f == NULL)
    {
    	free(mgmt->f);
    	mgmt->f = NULL;
    	free(mgmt);
    	mgmt = NULL;

	pthread_mutex_unlock(&mutex_init);//Unlock mutex while returning
        return RC_FILE_HANDLE_NOT_INIT;
    }

    int ret = openPageFile(pageFileName, mgmt->f);

    if(ret == RC_FILE_NOT_FOUND)
    {
	pthread_mutex_unlock(&mutex_init);//Unlock mutex while returning
	return RC_FILE_NOT_FOUND;
    }
    
    //initializing the buffer structure variables to NULLs and Zero
    mgmt->start = NULL;
    mgmt->current = NULL;
    mgmt->iterator = NULL;
    mgmt->search = NULL;
    mgmt->numReadIO = 0;
    mgmt->numWriteIO = 0;

    //initializing values to buffer variables
    bm->pageFile = pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = mgmt;

    pthread_mutex_unlock(&mutex_init); //Unlock mutex while returning
    return RC_OK;
}

//function to shut down buffer pool
RC shutdownBufferPool(BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)//error check
		return RC_BUFFER_POOL_NOT_INIT;
	//if the head of the pool is NULL we free the memeory of the buffer pool
	if(((BM_BufferMgmt *)bm->mgmtData)->start == NULL)
	{
		free(((BM_BufferMgmt *)bm->mgmtData)->f);
		((BM_BufferMgmt *)bm->mgmtData)->f = NULL;
		free(bm->mgmtData);
		bm->mgmtData = NULL;

		return RC_OK;
	}
	//initializing the pointer to the head of the pool
	((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
	
	while(((BM_BufferMgmt *)bm->mgmtData)->current != NULL)//loop until the end
	{
		((BM_BufferMgmt *)bm->mgmtData)->current->fixcounts = 0;//initializing the fixcount to zero
		((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->current->next;//next
	}
	
	int a = forceFlushPool(bm);//force write to the storage befor shutting down
	//free memory
	free(((BM_BufferMgmt *)bm->mgmtData)->f);
	((BM_BufferMgmt *)bm->mgmtData)->f = NULL;
	free(bm->mgmtData);
	bm->mgmtData = NULL;
	
	return RC_OK;
}

//function to force write to the storage befor shutting down
RC forceFlushPool(BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)//error check
		return RC_BUFFER_POOL_NOT_INIT;
	
	int a = RC_OK;
	//initialing the iritator to the start
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
	
	if(((BM_BufferMgmt *)bm->mgmtData)->iterator == NULL)//check if the buffer pool is empty
		return RC_BUFFER_POOL_EMPTY;
	
	while (((BM_BufferMgmt *)bm->mgmtData)->iterator != NULL)//loop until the end of the pool
	{
		if(((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty)//check if page is dirty
		{
			if(((BM_BufferMgmt *)bm->mgmtData)->iterator->fixcounts == 0)//check if fix count is zero
			{	//write the the content of the memory to the storage
				a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->iterator->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->iterator->ph->data);
				if(a == RC_OK)
				{
					((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty = 0;//change the dirty
					((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;//increase the number of write IO done
				}
			}
		}
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;//next
	}

	return a;
}

// Buffer Manager Interface Access Pages
//marks a page dirty
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	if(bm->mgmtData == NULL)//check if the buffer pool i not initialized
		return RC_BUFFER_POOL_NOT_INIT;

	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, page->pageNum);//look for the page and store the value

	if(((BM_BufferMgmt *)bm->mgmtData)->search != NULL)//check if the postion was NULL
	{
		((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 1;//mark dirty

		return RC_OK;
	}

	return RC_BUFFER_POOL_MARKDIRTY_ERROR;
}

//function to unpin a page
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	if(bm->mgmtData == NULL)//check if the buffer pool is initialized
		return RC_BUFFER_POOL_NOT_INIT;

	pthread_mutex_lock(&mutex_unpinPage); // Lock mutex before entering critical region	
	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, page->pageNum);//look for the page and store the value


	if(((BM_BufferMgmt *)bm->mgmtData)->search != NULL)//check if the postion was NULL

	{
		((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts -= 1;//decreament the fix count by 1
		pthread_mutex_unlock(&mutex_unpinPage); // Unlock mutex while returning	
		return RC_OK;
		//return RC_BUFFER_POOL_PAGE_INUSE;
	}

	pthread_mutex_unlock(&mutex_unpinPage); // Unlock mutex while returning
	return RC_BUFFER_POOL_UNPINPAGE_ERROR;
}

RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	if(bm->mgmtData == NULL)//check if the buffer pool is initialized

		return RC_BUFFER_POOL_NOT_INIT;

	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, page->pageNum);//look for the page and store the value


	if(((BM_BufferMgmt *)bm->mgmtData)->search != NULL)//check if the postion was NULL

	{
		int a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, page->data);

		if (a == RC_OK)
		{
			((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;//reset the dirty page variable
			((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;//increament the number of write IO

			return RC_OK;
		}
	}

	return RC_OK;
	//return RC_BUFFER_POOL_FORCEPAGE_ERROR;
}

// Pin Page function
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
	if(bm->mgmtData == NULL)//error check
		return RC_BUFFER_POOL_NOT_INIT;

	pthread_mutex_lock(&mutex_pinPage);
	if(pageNum >= ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages)
	{
		printf("Creating missing page %i\n", pageNum);
		int a = ensureCapacity(pageNum + 1, ((BM_BufferMgmt *)bm->mgmtData)->f); 
	}

	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, pageNum);
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
	
	while(((BM_BufferMgmt *)bm->mgmtData)->search)
	{
		if(((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum == pageNum)
		{
			((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->search;
			break;
		}
		((BM_BufferMgmt *)bm->mgmtData)->search = ((BM_BufferMgmt *)bm->mgmtData)->search->next;
	}
	
	if( ((BM_BufferMgmt *)bm->mgmtData)->search != ((BM_BufferMgmt *)bm->mgmtData)->iterator || ((BM_BufferMgmt *)bm->mgmtData)->search == 0)
	{
		
		int emptyFrame = emptyBufferFrame(bm);

		if (emptyFrame != -1)
		{
			if(lengthofPool(bm->mgmtData) == 0)
			{
				((BM_BufferMgmt *)bm->mgmtData)->start = (Buffer *)malloc(sizeof(Buffer));
				((BM_BufferMgmt *)bm->mgmtData)->start->ph = MAKE_PAGE_HANDLE();
				((BM_BufferMgmt *)bm->mgmtData)->start->ph->data = (char *) malloc(PAGE_SIZE);

				int a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->start->ph->data);
				
				if(a == RC_OK)
				{
					page->data = ((BM_BufferMgmt *)bm->mgmtData)->start->ph->data;
					page->pageNum = pageNum;

					((BM_BufferMgmt *)bm->mgmtData)->start->ph->pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->start->buffer_mgr_pageNum = emptyFrame;
					((BM_BufferMgmt *)bm->mgmtData)->start->dirty = 0;
					((BM_BufferMgmt *)bm->mgmtData)->start->count = 1;
					((BM_BufferMgmt *)bm->mgmtData)->start->freqCount = 1;
					((BM_BufferMgmt *)bm->mgmtData)->start->fixcounts = 1;
					((BM_BufferMgmt *)bm->mgmtData)->start->storage_mgr_pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->start->next = NULL;

					((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
					
				}
				else
				{
					printf("Pin Page failed due to: %d \n", a);
					pthread_mutex_unlock(&mutex_pinPage); // Unlock mutex while returning
					return RC_BUFFER_POOL_PINPAGE_ERROR;
				}
			}
			else
			{
				((BM_BufferMgmt *)bm->mgmtData)->current->next = (Buffer *)malloc(sizeof(Buffer));
				((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->current->next;
				
				((BM_BufferMgmt *)bm->mgmtData)->current->ph = MAKE_PAGE_HANDLE();
				((BM_BufferMgmt *)bm->mgmtData)->current->ph->data = (char *) malloc(PAGE_SIZE);
				((BM_BufferMgmt *)bm->mgmtData)->current->next = (Buffer *)malloc(sizeof(Buffer));

				int a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->current->ph->data);
				
				if(a == RC_OK)
				{
					page->data = ((BM_BufferMgmt *)bm->mgmtData)->current->ph->data;
					page->pageNum = pageNum;

					((BM_BufferMgmt *)bm->mgmtData)->current->ph->pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->current->buffer_mgr_pageNum = emptyFrame;
					((BM_BufferMgmt *)bm->mgmtData)->current->dirty = 0;
					((BM_BufferMgmt *)bm->mgmtData)->current->count = 1;
					((BM_BufferMgmt *)bm->mgmtData)->current->freqCount = 1;
					((BM_BufferMgmt *)bm->mgmtData)->current->fixcounts = 1;
					((BM_BufferMgmt *)bm->mgmtData)->current->storage_mgr_pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->current->next = NULL;
					
					updateCounter(bm->mgmtData);
					((BM_BufferMgmt *)bm->mgmtData)->current->count = 1;
				}
				else
				{
					printf("Pin Page failed due to: %d \n", a);
					pthread_mutex_unlock(&mutex_pinPage); // Unlock mutex while returning
					return RC_BUFFER_POOL_PINPAGE_ERROR;
				}
			}
		}
		else
		{
			int a;
			if(bm->strategy == RS_LFU)
				a = LFU(bm, page, pageNum);
			else
				a = replacementStrategy(bm, page, pageNum);

			if( a != RC_OK)
			{
				printf("Pin Page failed due to: %d \n", a);
				pthread_mutex_unlock(&mutex_pinPage); // Unlock mutex while returning
				return RC_BUFFER_POOL_PINPAGE_ERROR;
			}
		}
	}
	else
	{
		((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts += 1;

		page->data = ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data;
		page->pageNum = pageNum;

		if(bm->strategy == RS_LRU)
		{
			updateCounter(bm->mgmtData);
			((BM_BufferMgmt *)bm->mgmtData)->search->count = 1;
		}

		if(bm->strategy == RS_LFU)
			((BM_BufferMgmt *)bm->mgmtData)->search->freqCount += 1;

		//printf("Page already present @ Buffer location: %d\n", ((BM_BufferMgmt *)bm->mgmtData)->search->buffer_mgr_pageNum);
		pthread_mutex_unlock(&mutex_pinPage); // Unlock mutex while returning
		return RC_OK;
		//return RC_BUFFER_POOL_PINPAGE_ALREADY_PRESENT;
	}
	
	((BM_BufferMgmt *)bm->mgmtData)->numReadIO += 1;
	pthread_mutex_unlock(&mutex_pinPage); // Unlock mutex while returning
	return RC_OK;
}

// Statistics Interface
// returns the values in the page frame
PageNumber *getFrameContents (BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)// error check
		return RC_BUFFER_POOL_NOT_INIT;

	int i = 0;
	PageNumber *pn;//array that should be return
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;// initialize to the head
	
	if(((BM_BufferMgmt *)bm->mgmtData)->iterator == NULL)
		return NO_PAGE;

	pn = (PageNumber *)malloc(sizeof(PageNumber)*bm->numPages);
	//inititalizing for default value
	while(i < bm->numPages)
	{
		pn[i] = -1;
		i++;
	}
	i = 0;

	while (((BM_BufferMgmt *)bm->mgmtData)->iterator != NULL)//going to each node
	{
		pn[i] = ((BM_BufferMgmt *)bm->mgmtData)->iterator->storage_mgr_pageNum;//checking if page handle has a value
		i++;
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;//next
	}

	return pn;
}
//returns whether the page is dirty or not
bool *getDirtyFlags (BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)//error check
		return RC_BUFFER_POOL_NOT_INIT;

	int i = 0, n;
	bool *dirt;//array that should be return
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;//inititalization to the start of the buffer
	n = bm->numPages;

	dirt = (bool *)malloc(sizeof(bool)*n);
	while(i < n)
	{
		dirt[i] = FALSE;
		i++;
	}
	i = 0;
	while (((BM_BufferMgmt *)bm->mgmtData)->iterator != NULL)//going to each node
	{	
		if(((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty)
			dirt[i] = TRUE;//storing the dirty values in the array
		i++;
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next; //next
	}

	return dirt;
}
//returns the number of present request on a perticular page
int *getFixCounts (BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)//error check
		return RC_BUFFER_POOL_NOT_INIT;

	int i = 0, n;
    int *fix;//array that should be return
    ((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
    n = bm->numPages;

    fix = (int *)malloc(sizeof(int)*n);
	//setting all fix as zero
    while(i < n)
    {
    	fix[i] = 0;
    	i++;
    }

    i = 0;

    while (((BM_BufferMgmt *)bm->mgmtData)->iterator != NULL)//going to each node
    {
    	if(((BM_BufferMgmt *)bm->mgmtData)->iterator->fixcounts > 0)
        	fix[i] = ((BM_BufferMgmt *)bm->mgmtData)->iterator->fixcounts;//storing the dirty values in the array
        i++;
        ((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;//next
    }

    return fix;
}
//returs the number of read IO done
int getNumReadIO (BM_BufferPool *const bm)
{
	if(bm->mgmtData != NULL)
		return ((BM_BufferMgmt *)bm->mgmtData)->numReadIO;
	else
		return 0;
}
// returns the number of writes IO done
int getNumWriteIO (BM_BufferPool *const bm)
{
	if(bm->mgmtData != NULL)
		return ((BM_BufferMgmt *)bm->mgmtData)->numWriteIO;
	else
		return 0;
}
