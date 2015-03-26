#include "storage_mgr.h"
#include "buffer_mgr.h"
#include "buffer_mgr_stat.h"
#include "dt.h"
#include "dberror.h"
#include "stdio.h"

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
	int numReadIO;
	int numWriteIO;
} BM_BufferMgmt;

int lengthofPool(BM_BufferMgmt *mgmt)
{
	int count = 0;
	//Buffer *temp;
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
	//Buffer *search_buffer = mgmt->start;
	mgmt->iterator = mgmt->start;
	Buffer *temp = NULL;

	while (mgmt->iterator != NULL)
	{
		if(mgmt->iterator->storage_mgr_pageNum == pNum)
			break;

		temp = mgmt->iterator;
		mgmt->iterator = mgmt->iterator->next;
	}
	
	return temp;
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
	mgmt->iterator = mgmt->start; //make temp equals the first element in the buffer pool

	while(mgmt->iterator != NULL)
	{
		mgmt->iterator->count = mgmt->iterator->count + 1;
		mgmt->iterator = mgmt->iterator->next; //next
	}
}

Buffer *searchCnt(BM_BufferMgmt *mgmt)
{
	int max = 0;

	//Buffer *search_buffer = mgmt->start;
	mgmt->iterator = mgmt->start;
	Buffer *temp = mgmt->start;

	while(mgmt->iterator != NULL)
	{
		if(mgmt->iterator->count > max)
		{
			max = mgmt->iterator->count;
			//printf(" max %d \n", max);
			temp = mgmt->iterator;
		}
		mgmt->iterator = mgmt->iterator->next;
	}

	return temp;
}

int replacementStrategy(BM_BufferPool *bm, BM_PageHandle *page, PageNumber pageNum)
{
	int a;
	Buffer *temp = searchCnt(bm->mgmtData);

	if(temp->dirty == 1)
	{
		a = writeBlock(temp->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, temp->ph->data);

		if (a == RC_OK)
		{
			temp->dirty = 0;
			((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
		}
	}
	
	updateCounter(bm->mgmtData);
	//printf("%d total Num Pages \n", ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages);
	//printf("%i Page Num reading\n", pageNum);
	a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, temp->ph->data);
	
	if(a == RC_OK)
	{
		temp->ph->pageNum = pageNum;
		temp->storage_mgr_pageNum = pageNum;
		temp->dirty = 0;
		temp->count = 1;
		temp->fixcounts = 1;

		page->data = temp->ph->data;
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

	printf("\n=================== SHUTDOWN========================\n");
	printf("%i Length of Pool\n", lengthofPool(bm->mgmtData));
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

	while(lengthofPool(bm->mgmtData) != 0)
	{
		((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
		Buffer *temp = NULL;

		while (((BM_BufferMgmt *)bm->mgmtData)->current != NULL)
		{
			temp = ((BM_BufferMgmt *)bm->mgmtData)->current;
			((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->current->next;
		}

		free(temp->ph->data);
		temp->ph->data = NULL;
		free(temp->ph);
		temp->ph = NULL;
		free(temp);
		temp = NULL;
		((BM_BufferMgmt *)bm->mgmtData)->current = NULL;
	}
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
	//Buffer *temp;
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

	//free(temp);
	//temp = NULL;

	return a;
}

// Buffer Manager Interface Access Pages
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	printf("\n=================== Mark Dirty Start ========================\n");
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	Buffer *bufferPagePos = searchPos(bm->mgmtData, page->pageNum);

	if(bufferPagePos != NULL)
	{
		bufferPagePos->dirty = 1;
		printf("\n=================== Mark Dirty END ========================\n");
		return RC_OK;
	}

	//free(bufferPagePos);
	//bufferPagePos = NULL;

	return RC_BUFFER_POOL_MARKDIRTY_ERROR;
}

RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	printf("\n=================== Un Pin Page Start ========================\n");
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	Buffer *bufferPagePos = searchPos(bm->mgmtData, page->pageNum);

	if(bufferPagePos != NULL)
	{
		bufferPagePos->fixcounts -= 1;
		if(bufferPagePos->fixcounts == 0)
		{
			if(bufferPagePos->dirty)
			{
				//Dirty page : need to write back to disk
				int a = writeBlock(bufferPagePos->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, bufferPagePos->ph->data);
				//printf("RC: %d write block\n", a);
				if(a != RC_OK)
				{
					bufferPagePos->fixcounts += 1;
					return a;
				}
				((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
			}

			if(lengthofPool(bm->mgmtData) == 1)
			{
				free(bufferPagePos->ph->data);
				bufferPagePos->ph->data = NULL;
				free(bufferPagePos->ph);
				bufferPagePos->ph = NULL;
				free(bufferPagePos);
				bufferPagePos = NULL;
				//free(((BM_BufferMgmt *)bm->mgmtData)->start->ph->data);
				//((BM_BufferMgmt *)bm->mgmtData)->start->ph->data = NULL;
				//free(((BM_BufferMgmt *)bm->mgmtData)->start->ph);
				//((BM_BufferMgmt *)bm->mgmtData)->start->ph = NULL;
				//free(((BM_BufferMgmt *)bm->mgmtData)->start);
				((BM_BufferMgmt *)bm->mgmtData)->start = NULL;
			}
			else
			{
				Buffer *bufferPagePrevPos = searchPrevPos(bm->mgmtData, page->pageNum);

				if(bufferPagePrevPos != NULL && bufferPagePos!= NULL)
				{
					free(bufferPagePos->ph->data);
					bufferPagePos->ph->data = NULL;
					free(bufferPagePos->ph);
					bufferPagePos->ph = NULL;
					
					bufferPagePos->next->buffer_mgr_pageNum -= 1;
					bufferPagePrevPos->next = bufferPagePos->next;

					free(bufferPagePos);
					bufferPagePos = NULL;
				}
			}		

			printf("\n=================== Un Pin Page End ========================\n");
			//printf("%d Length of Buffer\n", lengthofPool(bm->mgmtData));
			return RC_OK;
		}

		return RC_BUFFER_POOL_PAGE_INUSE;
	}

	return RC_BUFFER_POOL_UNPINPAGE_ERROR;
}

RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;

	Buffer *bufferPagePos = searchPos(bm->mgmtData, page->pageNum);

	if(bufferPagePos != NULL)
	{
		int a = writeBlock(bufferPagePos->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, page->data);
		//int a = writeBlock(page->pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, page->data);

		if (a == RC_OK)
		{
			bufferPagePos->dirty = 0;
			((BM_BufferMgmt *)bm->mgmtData)->numWriteIO += 1;
			//free(bufferPagePos);
			//bufferPagePos = NULL;
			return RC_OK;
		}
	}

	return RC_BUFFER_POOL_FORCEPAGE_ERROR;
}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
	printf("\n=================== Pin Page Start ========================\n");
	if(bm->mgmtData == NULL)
		return RC_BUFFER_POOL_NOT_INIT;
	printf("Buffer Mgmr Already Init\n");
	printf("%i ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages\n", ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages);
	printf("%i pageNum\n", pageNum);
	if(pageNum >= ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages)
	{
		printf("Increaing the number of Pages in file\n");
		int a = ensureCapacity(pageNum + 1, ((BM_BufferMgmt *)bm->mgmtData)->f);
		printf("\nensureCapacity %i\n", a);
	}

	printf("------------ %i Total Number of Pages -------------\n", ((BM_BufferMgmt *)bm->mgmtData)->f->totalNumPages);
	Buffer *bufferPagePos = searchPos(bm->mgmtData, pageNum);
	((BM_BufferMgmt *)bm->mgmtData)->iterator = ((BM_BufferMgmt *)bm->mgmtData)->start;
	
	while(bufferPagePos)
	{
		if(bufferPagePos->storage_mgr_pageNum == pageNum)
		{
			((BM_BufferMgmt *)bm->mgmtData)->iterator = bufferPagePos;
			break;
		}
		bufferPagePos = bufferPagePos->next;
	}

	if( bufferPagePos != ((BM_BufferMgmt *)bm->mgmtData)->iterator || bufferPagePos == 0)
	{

		int emptyFrame = emptyBufferFrame(bm);
		printf("emptyBufferFrame @ %d\n", emptyFrame);
		printf("%i Length of Pool\n", lengthofPool(bm->mgmtData));
		if (emptyFrame != -1)
		{
			if(lengthofPool(bm->mgmtData) == 0)
			{
				printf("Test1\n");((BM_BufferMgmt *)bm->mgmtData)->start = (Buffer *)malloc(sizeof(Buffer));
				printf("Test2\n");((BM_BufferMgmt *)bm->mgmtData)->start->ph = MAKE_PAGE_HANDLE();
				printf("Test3\n");((BM_BufferMgmt *)bm->mgmtData)->start->ph->data = (char *) malloc(PAGE_SIZE);
				printf("Test4\n");
				int a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, ((BM_BufferMgmt *)bm->mgmtData)->start->ph->data);
				printf("Test5\n");
				if(a == RC_OK)
				{
					printf("Test6\n");
					updateCounter(bm->mgmtData);
					printf("Test7\n");
					page->data = ((BM_BufferMgmt *)bm->mgmtData)->start->ph->data;
					printf("Test8\n");
					page->pageNum = pageNum;
					printf("Test9\n");
					((BM_BufferMgmt *)bm->mgmtData)->start->ph->pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->start->buffer_mgr_pageNum = emptyFrame;
					((BM_BufferMgmt *)bm->mgmtData)->start->dirty = 0;
					((BM_BufferMgmt *)bm->mgmtData)->start->count = 1;
					((BM_BufferMgmt *)bm->mgmtData)->start->fixcounts = 1;
					((BM_BufferMgmt *)bm->mgmtData)->start->storage_mgr_pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->start->next = NULL;

					((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
					printf("Test10\n");
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
					updateCounter(bm->mgmtData);
					page->data = ((BM_BufferMgmt *)bm->mgmtData)->current->ph->data;
					page->pageNum = pageNum;

					((BM_BufferMgmt *)bm->mgmtData)->current->ph->pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->current->buffer_mgr_pageNum = emptyFrame;
					((BM_BufferMgmt *)bm->mgmtData)->current->dirty = 0;
					((BM_BufferMgmt *)bm->mgmtData)->current->count = 1;
					((BM_BufferMgmt *)bm->mgmtData)->current->fixcounts = 1;
					((BM_BufferMgmt *)bm->mgmtData)->current->storage_mgr_pageNum = pageNum;
					((BM_BufferMgmt *)bm->mgmtData)->current->next = NULL;
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
			printf("-------Implementing Replacement Strategy------\n");
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
		bufferPagePos->fixcounts += 1;
		//updateFixCounts(bm->mgmtData, pageNum);
		if(bm->strategy == RS_LRU)
		{
			updateCounter(bm->mgmtData);
			//resetCounter(bm->mgmtData, bufferPagePos);
			bufferPagePos->count = 1;
		}
		printf("Page already present @ Buffer location: %d\n", bufferPagePos->buffer_mgr_pageNum);
		return RC_BUFFER_POOL_PINPAGE_ALREADY_PRESENT;
	}
	
	((BM_BufferMgmt *)bm->mgmtData)->numReadIO += 1;
	//free(temp1);
	//temp1 = NULL;
	//free(bufferPagePos);
	//bufferPagePos = NULL;
	printf("\n=================== Pin Page End ========================\n");
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