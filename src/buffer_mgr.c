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
	//char *data;
	int count;
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
	//Buffer *temp;
	mgmt->iterator = mgmt->start; //make root equals the first element in the buffer pool
	//printf("%i mgmt->iterator\n", mgmt->iterator);
	while(mgmt->iterator != NULL)
	{
		count++;
		//printf("%i count\n", count);
		mgmt->iterator = mgmt->iterator->next; //next
	}

	return count;
}

Buffer *searchPrevPos(BM_BufferMgmt *mgmt, PageNumber pNum)
{
	//Buffer *search_buffer = mgmt->start;
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
	//Buffer *search_buffer;	
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
	//Buffer *temp;
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
	//Buffer *temp;
	//printf("updateCounter Test 1\n");
	mgmt->iterator = mgmt->start; //make temp equals the first element in the buffer pool
	//printf("updateCounter Test 2\n");
	while(mgmt->iterator != NULL)
	{
		//printf("updateCounter Test 3\n");
		mgmt->iterator->count = mgmt->iterator->count + 1;
		//printf("updateCounter Test 4\n");
		mgmt->iterator = mgmt->iterator->next; //next
		//printf("updateCounter Test 5\n");
	}
}

Buffer *searchCnt(BM_BufferMgmt *mgmt)
{
	int max = 0;

	//Buffer *search_buffer = mgmt->start;
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
			//printf(" max %d \n", max);
			
		}
		mgmt->iterator = mgmt->iterator->next;
	}

	return mgmt->search;
}

int replacementStrategy(BM_BufferPool *bm, BM_PageHandle *page, PageNumber pageNum)
{
	int a;
	((BM_BufferMgmt *)bm->mgmtData)->search = searchCnt(bm->mgmtData);
	//printf("ReplacementStrategy , ((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum %i\n", ((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum);
	//printf("ReplacementStrategy ((BM_BufferMgmt *)bm->mgmtData)->search->dirty %i\n", ((BM_BufferMgmt *)bm->mgmtData)->search->dirty);
	if(((BM_BufferMgmt *)bm->mgmtData)->search->dirty == 1)
	{
		a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);

		if (a == RC_OK)
		{
			((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;
			//rintf("ReplacementStrategy numWriteIO Inc : %i\n", ((BM_BufferMgmt *)bm->mgmtData)->numWriteIO);
			((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
		}
	}
	
	updateCounter(bm->mgmtData);
	//printf("%d total Num Pages \n", ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages);
	//printf("%i Page Num reading\n", pageNum);
	a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);
	
	if(a == RC_OK)
	{
		((BM_BufferMgmt *)bm->mgmtData)->search->ph->pageNum = pageNum;
		((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum = pageNum;
		((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;
		((BM_BufferMgmt *)bm->mgmtData)->search->count = 1;
		((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts = 1;

		page->data = ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data;
		page->pageNum = pageNum;
		/*
		printf("-------------------------\n");
		printf("Inside LRU\n");
		printf("-------------------------\n");
		printf("%i temp->buffer_mgr_pageNum \n", temp->buffer_mgr_pageNum);
		printf("%i temp->stroage_mgr_pageNum \n", temp->storage_mgr_pageNum);
		printf("%d temp->count \n", temp->count);
		printf("%i temp->fixcounts \n", temp->fixcounts);
		printf("%i temp->dirty \n", temp->dirty);
		printf("-------------------------\n");
		*/
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
    //mgmt->start = (Buffer *)malloc(sizeof(Buffer));
    //mgmt->current = (Buffer *)malloc(sizeof(Buffer));

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

	//printf("\n=================== SHUTDOWN========================\n");
	//printf("%i Length of Pool\n", lengthofPool(bm->mgmtData));
	if(((BM_BufferMgmt *)bm->mgmtData)->start == NULL)
	{
		free(((BM_BufferMgmt *)bm->mgmtData)->f);
		((BM_BufferMgmt *)bm->mgmtData)->f = NULL;
		free(bm->mgmtData);
		bm->mgmtData = NULL;

		return RC_OK;
	}
	//printf("Shutdown Test 1\n");
	((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
	//printf("Shutdown Test 2\n");
	while(((BM_BufferMgmt *)bm->mgmtData)->current != NULL)
	{
		((BM_BufferMgmt *)bm->mgmtData)->current->fixcounts = 0;
		((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->current->next;
	}
	//printf("Shutdown Test 3\n");
	int a = forceFlushPool(bm);
	//printf("Shutdown Test 4\n");
	/*
	while(lengthofPool(bm->mgmtData) != 0)
	{
		((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
		((BM_BufferMgmt *)bm->mgmtData)->search = NULL;

		while (((BM_BufferMgmt *)bm->mgmtData)->current != NULL)
		{
			((BM_BufferMgmt *)bm->mgmtData)->search = ((BM_BufferMgmt *)bm->mgmtData)->current;
			((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->current->next;
		}

		free(((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);
		((BM_BufferMgmt *)bm->mgmtData)->search->ph->data = NULL;
		free(((BM_BufferMgmt *)bm->mgmtData)->search->ph);
		((BM_BufferMgmt *)bm->mgmtData)->search->ph = NULL;
		free(((BM_BufferMgmt *)bm->mgmtData)->search);
		((BM_BufferMgmt *)bm->mgmtData)->search = NULL;
		((BM_BufferMgmt *)bm->mgmtData)->current = NULL;
	}
	*/
	//printf("Shutdown Test 5\n");
	free(((BM_BufferMgmt *)bm->mgmtData)->f);
	((BM_BufferMgmt *)bm->mgmtData)->f = NULL;
	free(bm->mgmtData);
	bm->mgmtData = NULL;
	//printf("shutdownBufferPool complete\n");
	return RC_OK;
}

RC forceFlushPool(BM_BufferPool *const bm)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;
	//printf("Force Flush Pool Test 1\n");
	int a = RC_OK;
	//Buffer *temp;
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
	//printf("Force Flush Pool Test 2\n");
	if(((BM_BufferMgmt *)bm->mgmtData)->iterator == NULL)
		return RC_BUFFER_POOL_EMPTY;
	//printf("Force Flush Pool Test 3\n");
	while (((BM_BufferMgmt *)bm->mgmtData)->iterator != NULL)
	{
		//printf("Force Flush Pool ((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty : %i\n", ((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty);
		if(((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty)
		{
			//printf("Force Flush Pool Test 5\n");
			if(((BM_BufferMgmt *)bm->mgmtData)->iterator->fixcounts == 0)
			{
				//printf("Force Flush Pool Test 6\n");
				a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->iterator->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->iterator->ph->data);
				if(a == RC_OK)
				{
					//printf("Force Flush Pool Test 7\n");
					((BM_BufferMgmt *)bm->mgmtData)->iterator->dirty = 0;
					//printf("forceFlushPool numWriteIO Inc\n");
					((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
				}
			}
		}
		//printf("Force Flush Pool Test 8\n");
		((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->iterator->next;
	}
	//printf("Force Flush Pool Test 9\n");
	//free(temp);
	//temp = NULL;

	return a;
}

// Buffer Manager Interface Access Pages
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	//printf("\n=================== Mark Dirty Start ========================\n");
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, page->pageNum);

	if(((BM_BufferMgmt *)bm->mgmtData)->search != NULL)
	{
		((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 1;
		//printf("\n=================== Mark Dirty END ========================\n");
		return RC_OK;
	}

	//free(bufferPagePos);
	//bufferPagePos = NULL;

	return RC_BUFFER_POOL_MARKDIRTY_ERROR;
}

RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	//printf("\n=================== Un Pin Page Start ========================\n");
	//printf("Unpin Test 0\n");
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;
	//printf("Unpin Test 1\n");
	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, page->pageNum);
	//printf("Unpin Test 2\n");
	//printf("%i ((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum\n", ((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum);
	//printf("un pin page ((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts : %i\n", ((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts);
	if(((BM_BufferMgmt *)bm->mgmtData)->search != NULL)
	{
		//printf("Unpin Test 3\n");
		((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts -= 1;
		//((BM_BufferMgmt *)bm->mgmtData)->search->ph->data = "\0";
		/*
		if(((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts == 0)
		{
			//printf("Unpin Test 4\n");
			printf("%i ((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum\n", ((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum);
			printf("%i ((BM_BufferMgmt *)bm->mgmtData)->search->dirty\n", ((BM_BufferMgmt *)bm->mgmtData)->search->dirty);
			if(((BM_BufferMgmt *)bm->mgmtData)->search->dirty)
			{
				//printf("Unpin Test 5\n");
				//Dirty page : need to write back to disk
				int a = writeBlock(((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);
				//printf("RC: %d write block\n", a);
				//printf("Unpin Test 6\n");
				if(a != RC_OK)
				{
					((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts += 1;
					return a;
				}
				printf("unpinPage numWriteIO Inc: %i\n", ((BM_BufferMgmt *)bm->mgmtData)->numWriteIO);
				((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
				//printf("Unpin Test 7\n");
			}
			/*
			if(lengthofPool(bm->mgmtData) == 1)
			{
				/*
				free(((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);
				((BM_BufferMgmt *)bm->mgmtData)->search->ph->data = NULL;
				free(((BM_BufferMgmt *)bm->mgmtData)->search->ph);
				((BM_BufferMgmt *)bm->mgmtData)->search->ph = NULL;
				free(((BM_BufferMgmt *)bm->mgmtData)->search);
				((BM_BufferMgmt *)bm->mgmtData)->search = NULL;*/
				//free(((BM_BufferMgmt *)bm->mgmtData)->start->ph->data);
				//((BM_BufferMgmt *)bm->mgmtData)->start->ph->data = NULL;
				//free(((BM_BufferMgmt *)bm->mgmtData)->start->ph);
				//((BM_BufferMgmt *)bm->mgmtData)->start->ph = NULL;
				//free(((BM_BufferMgmt *)bm->mgmtData)->start);
				/*((BM_BufferMgmt *)bm->mgmtData)->start = NULL;
			}
			else
			{
				Buffer *bufferPagePrevPos = searchPrevPos(bm->mgmtData, page->pageNum);

				if(bufferPagePrevPos != NULL && ((BM_BufferMgmt *)bm->mgmtData)->search!= NULL)
				{
					free(((BM_BufferMgmt *)bm->mgmtData)->search->ph->data);
					((BM_BufferMgmt *)bm->mgmtData)->search->ph->data = NULL;
					free(((BM_BufferMgmt *)bm->mgmtData)->search->ph);
					((BM_BufferMgmt *)bm->mgmtData)->search->ph = NULL;
					
					((BM_BufferMgmt *)bm->mgmtData)->search->next->buffer_mgr_pageNum -= 1;
					bufferPagePrevPos->next = ((BM_BufferMgmt *)bm->mgmtData)->search->next;

					free(((BM_BufferMgmt *)bm->mgmtData)->search);
					((BM_BufferMgmt *)bm->mgmtData)->search = NULL;
				}
			}		
			
			//printf("\n=================== Un Pin Page End ========================\n");
			//printf("%d Length of Buffer\n", lengthofPool(bm->mgmtData));
			return RC_OK;
		}*/
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
		//int a = writeBlock(page->pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, page->data);

		if (a == RC_OK)
		{
			((BM_BufferMgmt *)bm->mgmtData)->search->dirty = 0;
			//printf("forcePage numWriteIO Inc : %i\n", ((BM_BufferMgmt *)bm->mgmtData)->numWriteIO);
			((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
			//free(bufferPagePos);
			//bufferPagePos = NULL;
			return RC_OK;
		}
	}

	return RC_OK;
	//return RC_BUFFER_POOL_FORCEPAGE_ERROR;
}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
	//printf("\n=================== Pin Page Start ========================\n");
	//printf("Pin Page Test 1\n");
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;
	//printf("Buffer Mgmr Already Init\n");
	//printf("%i ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages\n", ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages);
	//printf("%i pageNum\n", pageNum);
	//printf("%i lengthofPool(bm->mgmtData)\n", lengthofPool(bm->mgmtData));
	//printf("Pin Page Test 2\n");
	if(pageNum >= ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages)
	{
		printf("Creating missing page %i\n", pageNum);
		int a = ensureCapacity(pageNum + 1, ((BM_BufferMgmt *)bm->mgmtData)->f);
		//printf("\nensureCapacity %i\n", a);
	}
	//printf("Pin Page Test 3\n");
	//printf("------------ %i Total Number of Pages -------------\n", ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages);
	((BM_BufferMgmt *)bm->mgmtData)->search = searchPos(bm->mgmtData, pageNum);
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
	//printf("Pin Page Test 4\n");
	while(((BM_BufferMgmt *)bm->mgmtData)->search)
	{
		//printf("Pin Page Test 5\n");
		if(((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum == pageNum)
		{
			//printf("Pin Page Test 6\n");
			((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->search;
			break;
		}
		((BM_BufferMgmt *)bm->mgmtData)->search = ((BM_BufferMgmt *)bm->mgmtData)->search->next;
	}
	//printf("Pin Page Test 7\n");
	if( ((BM_BufferMgmt *)bm->mgmtData)->search != ((BM_BufferMgmt *)bm->mgmtData)->iterator || ((BM_BufferMgmt *)bm->mgmtData)->search == 0)
	{
		//printf("Pin Page Test 8\n");
		int emptyFrame = emptyBufferFrame(bm);
		//printf("emptyBufferFrame @ %d\n", emptyFrame);
		//printf("%i Length of Pool\n", lengthofPool(bm->mgmtData));
		//printf("Pin Page Test 9\n");
		if (emptyFrame != -1)
		{
			//printf("Pin Page Test 10\n");
			if(lengthofPool(bm->mgmtData) == 0)
			{
				//printf("Pin Page Test 11\n");
				((BM_BufferMgmt *)bm->mgmtData)->start = (Buffer *)malloc(sizeof(Buffer));
				((BM_BufferMgmt *)bm->mgmtData)->start->ph = MAKE_PAGE_HANDLE();
				((BM_BufferMgmt *)bm->mgmtData)->start->ph->data = (char *) malloc(PAGE_SIZE);
				//printf("%i pageNum\n", pageNum);
				int a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->start->ph->data);
				
				if(a == RC_OK)
				{
					updateCounter(bm->mgmtData);
					
					page->data = ((BM_BufferMgmt *)bm->mgmtData)->start->ph->data;
					page->pageNum = pageNum;
					//printf(" page->data\n", *page->data);
					//printf("%i page->pageNum\n", page->pageNum);
					((BM_BufferMgmt *)bm->mgmtData)->start->ph->pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->start->buffer_mgr_pageNum = emptyFrame;
					((BM_BufferMgmt *)bm->mgmtData)->start->dirty = 0;
					((BM_BufferMgmt *)bm->mgmtData)->start->count = 1;
					((BM_BufferMgmt *)bm->mgmtData)->start->fixcounts = 1;
					((BM_BufferMgmt *)bm->mgmtData)->start->storage_mgr_pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->start->next = NULL;

					((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
					
				}
				else
				{
					//printf("Pin Page failed due to: %d \n", a);
					return RC_BUFFER_POOL_PINPAGE_ERROR;
				}
			}
			else
			{
				//printf("Pin Page Test 12\n");
				((BM_BufferMgmt *)bm->mgmtData)->current->next = (Buffer *)malloc(sizeof(Buffer));
				//printf("Pin Page Test 13\n");
				((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->current->next;
				
				//printf("Pin Page Test 14\n");
				((BM_BufferMgmt *)bm->mgmtData)->current->ph = MAKE_PAGE_HANDLE();
				//printf("Pin Page Test 15\n");
				((BM_BufferMgmt *)bm->mgmtData)->current->ph->data = (char *) malloc(PAGE_SIZE);
				//printf("Pin Page Test 16\n");
				((BM_BufferMgmt *)bm->mgmtData)->current->next = (Buffer *)malloc(sizeof(Buffer));

				int a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->current->ph->data);
				//printf("Pin Page Test 17\n");
				if(a == RC_OK)
				{
					//printf("Pin Page Test 18\n");
					//((BM_BufferMgmt *)bm->mgmtData)->current->count = 1;
					//updateCounter(bm->mgmtData);
					//printf("Pin Page Test 19\n");
					page->data = ((BM_BufferMgmt *)bm->mgmtData)->current->ph->data;
					page->pageNum = pageNum;

					((BM_BufferMgmt *)bm->mgmtData)->current->ph->pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->current->buffer_mgr_pageNum = emptyFrame;
					((BM_BufferMgmt *)bm->mgmtData)->current->dirty = 0;
					((BM_BufferMgmt *)bm->mgmtData)->current->count = 1;
					((BM_BufferMgmt *)bm->mgmtData)->current->fixcounts = 1;
					((BM_BufferMgmt *)bm->mgmtData)->current->storage_mgr_pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->current->next = NULL;
					//printf("Pin Page Test 20\n");
					updateCounter(bm->mgmtData);
					((BM_BufferMgmt *)bm->mgmtData)->current->count = 1;
					//printf("Pin Page Test 21\n");
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
			//printf("Pin Page Test 22\n");
			//printf("-------Implementing Replacement Strategy------\n");
			int a = replacementStrategy(bm, page, pageNum);

			if( a != RC_OK)
			{
				printf("Pin Page failed due to: %d \n", a);
				return RC_BUFFER_POOL_PINPAGE_ERROR;
			}
		}
	}
	else
	{
		//printf("%i ((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum\n", ((BM_BufferMgmt *)bm->mgmtData)->search->storage_mgr_pageNum);
		//printf("pin Page ((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts %i \n", ((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts);
		((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts += 1;
		//((BM_BufferMgmt *)bm->mgmtData)->search->fixcounts
		page->data = ((BM_BufferMgmt *)bm->mgmtData)->search->ph->data;
		page->pageNum = pageNum;
		//updateFixCounts(bm->mgmtData, pageNum);
		if(bm->strategy == RS_LRU)
		{
			updateCounter(bm->mgmtData);
			//resetCounter(bm->mgmtData, bufferPagePos);
			((BM_BufferMgmt *)bm->mgmtData)->search->count = 1;
		}
		printf("Page already present @ Buffer location: %d\n", ((BM_BufferMgmt *)bm->mgmtData)->search->buffer_mgr_pageNum);
		return RC_OK;
		//return RC_BUFFER_POOL_PINPAGE_ALREADY_PRESENT;
	}
	
	((BM_BufferMgmt *)bm->mgmtData)->numReadIO += 1;
	//free(temp1);
	//temp1 = NULL;
	//free(bufferPagePos);
	//bufferPagePos = NULL;
	//printf("\n=================== Pin Page End ========================\n");
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

	//free(temp);
	//temp = NULL;

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

	//free(temp);
    //temp = NULL;

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

    //free(temp);
    //temp = NULL;

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