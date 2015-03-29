#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "buffer_mgr_stat.h"
#include "dt.h"
#include "dberror.h"
#include "stdio.h"
#include "stdlib.h"

typedef struct Buffer
{
	int buffer_mgr_pageNum;
	int storage_mgr_pageNum;
	BM_PageHandle *ph;
	bool dirty;
	int count;
	int freqCount;
	int fixcounts;
	struct Buffer *next;
} Buffer;

typedef struct BM_BufferMgmt
{
	SM_FileHandle *f;
	Buffer *start;
	Buffer *current;
	Buffer *iterator;
	Buffer *search;
	int numReadIO;
	int numWriteIO;
} BM_BufferMgmt;

int lengthofPool(BM_BufferMgmt *mgmt)
{
	int count = 0;

	mgmt->iterator = mgmt->start; //make root equals the first element in the buffer pool

	while(mgmt->iterator != NULL)
	{
		count++;
		mgmt->iterator = mgmt->iterator->next; //next
	}

	return count;
}

Buffer *searchPrevPos(BM_BufferMgmt *mgmt, PageNumber pNum)
{
	mgmt->iterator = mgmt->start;
	mgmt->search = NULL;

	while (mgmt->iterator != NULL)
	{
		if(mgmt->iterator->storage_mgr_pageNum == pNum)
			break;

		mgmt->search = mgmt->iterator;
		mgmt->iterator = mgmt->iterator->next;
	}
	
	return mgmt->search;
}

Buffer *searchPos(BM_BufferMgmt *mgmt, PageNumber pNum)
{	
	mgmt->iterator = mgmt->start;

	while (mgmt->iterator != NULL)
	{
		if(mgmt->iterator->storage_mgr_pageNum == pNum)
			break;

		mgmt->iterator = mgmt->iterator->next;
	}
	
	return mgmt->iterator;
}

int emptyBufferFrame(BM_BufferPool *bm)
{
	int flag = 0;

	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
	while (((BM_BufferMgmt *)bm->mgmtData)->iterator != NULL)
	{
		if(((BM_BufferMgmt *)bm->mgmtData)->iterator->buffer_mgr_pageNum != flag) //if the page is found
			return flag; //store it // get back to it to check POS
		
		flag = flag + 1;
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;
	}

	if(flag < bm->numPages)
		return flag;
	else
		return -1;
}

void updateCounter(BM_BufferMgmt *mgmt)
{
	mgmt->iterator = mgmt->start; //make temp equals the first element in the buffer pool
	
	while(mgmt->iterator != NULL)
	{
		mgmt->iterator->count = mgmt->iterator->count + 1;
		mgmt->iterator = mgmt->iterator->next;
	}
}

Buffer *searchCnt(BM_BufferMgmt *mgmt)
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

	return mgmt->search;
}

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

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData)
{
	if(bm->mgmtData != NULL)
		return RC_BUFFER_POOL_ALREADY_INIT;

	BM_BufferMgmt *mgmt;
    mgmt = (BM_BufferMgmt *)malloc(sizeof(BM_BufferMgmt));
    mgmt->f = (SM_FileHandle *)malloc(sizeof(SM_FileHandle));

    if(mgmt->f == NULL)
    {
    	free(mgmt->f);
    	mgmt->f = NULL;
    	free(mgmt);
    	mgmt = NULL;

        return RC_FILE_HANDLE_NOT_INIT;
    }

    int ret = openPageFile(pageFileName, mgmt->f);

    if(ret == RC_FILE_NOT_FOUND)
		return RC_FILE_NOT_FOUND;

    mgmt->start = NULL;
    mgmt->current = NULL;
    mgmt->iterator = NULL;
    mgmt->search = NULL;
    mgmt->numReadIO = 0;
    mgmt->numWriteIO = 0;

    bm->pageFile = pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = mgmt;

    return RC_OK;
}

RC shutdownBufferPool(BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	if(((BM_BufferMgmt *)bm->mgmtData)->start == NULL)
	{
		free(((BM_BufferMgmt *)bm->mgmtData)->f);
		((BM_BufferMgmt *)bm->mgmtData)->f = NULL;
		free(bm->mgmtData);
		bm->mgmtData = NULL;

		return RC_OK;
	}
	
	((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
	
	while(((BM_BufferMgmt *)bm->mgmtData)->current != NULL)
	{
		((BM_BufferMgmt *)bm->mgmtData)->current->fixcounts = 0;
		((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->current->next;
	}
	
	int a = forceFlushPool(bm);

	free(((BM_BufferMgmt *)bm->mgmtData)->f);
	((BM_BufferMgmt *)bm->mgmtData)->f = NULL;
	free(bm->mgmtData);
	bm->mgmtData = NULL;
	
	return RC_OK;
}

RC forceFlushPool(BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;
	
	int a = RC_OK;
	
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
	
	if(((BM_BufferMgmt *)bm->mgmtData)->iterator == NULL)
		return RC_BUFFER_POOL_EMPTY;
	
	while (((BM_BufferMgmt *)bm->mgmtData)->iterator != NULL)
	{
		if(((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty)
		{
			if(((BM_BufferMgmt *)bm->mgmtData)->iterator->fixcounts == 0)
			{
				a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->iterator->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->iterator->ph->data);
				if(a == RC_OK)
				{
					((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty = 0;
					((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
				}
			}
		}
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;
	}

	return a;
}

// Buffer Manager Interface Access Pages
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, page->pageNum);

	if(((BM_BufferMgmt *)bm->mgmtData)->search != NULL)
	{
		((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 1;

		return RC_OK;
	}

	return RC_BUFFER_POOL_MARKDIRTY_ERROR;
}

RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;
	
	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, page->pageNum);

	if(((BM_BufferMgmt *)bm->mgmtData)->search != NULL)
	{
		((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts -= 1;
		
		return RC_OK;
		//return RC_BUFFER_POOL_PAGE_INUSE;
	}

	return RC_BUFFER_POOL_UNPINPAGE_ERROR;
}

RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, page->pageNum);

	if(((BM_BufferMgmt *)bm->mgmtData)->search != NULL)
	{
		int a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, page->data);

		if (a == RC_OK)
		{
			((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;
			((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;

			return RC_OK;
		}
	}

	return RC_OK;
	//return RC_BUFFER_POOL_FORCEPAGE_ERROR;
}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

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

		return RC_OK;
		//return RC_BUFFER_POOL_PINPAGE_ALREADY_PRESENT;
	}
	
	((BM_BufferMgmt *)bm->mgmtData)->numReadIO += 1;
	return RC_OK;
}

// Statistics Interface
PageNumber *getFrameContents (BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	int i = 0;
	PageNumber *pn;//array that should be return
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
	
	if(((BM_BufferMgmt *)bm->mgmtData)->iterator == NULL)
		return NO_PAGE;

	pn = (PageNumber *)malloc(sizeof(PageNumber)*bm->numPages);

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
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;
	}

	return pn;
}

bool *getDirtyFlags (BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	int i = 0, n;
	bool *dirt;//array that should be return
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
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
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;
	}

	return dirt;
}

int *getFixCounts (BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	int i = 0, n;
    int *fix;//array that should be return
    ((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
    n = bm->numPages;

    fix = (int *)malloc(sizeof(int)*n);

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
        ((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;
    }

    return fix;
}

int getNumReadIO (BM_BufferPool *const bm)
{
	if(bm->mgmtData != NULL)
		return ((BM_BufferMgmt *)bm->mgmtData)->numReadIO;
	else
		return 0;
}

int getNumWriteIO (BM_BufferPool *const bm)
{
	if(bm->mgmtData != NULL)
		return ((BM_BufferMgmt *)bm->mgmtData)->numWriteIO;
	else
		return 0;
}