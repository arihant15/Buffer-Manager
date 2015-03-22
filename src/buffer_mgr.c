#include "storage_mgr.c"
#include "buffer_mgr.h"
#include "buffer_mgr_stat.h"
#include "dt.h"
#include "dberror.h"
#include "stdio.h"

//we are using something similar to linked list to define the buffer struct

// defining the buffer
typedef struct Buffer
{
	int buffer_mgr_pageNum; //position of the page frame
	int storage_mgr_pageNum; //page number
	BM_PageHandle *ph;
	bool dirty; //dirty page or not ?
	//char *data; //data of the page
	int count; //general counter to count how old of the page
	int fixcounts;
	struct Buffer *next; // pointer to the next buffer
} Buffer;

//Buffer *start = NULL;
//Buffer *current = NULL;

typedef struct BM_BufferMgmt
{
	SM_FileHandle *f;
	Buffer *start;
	Buffer *current;
}BM_BufferMgmt;

//function to calculate and return the length of the buffer pool
int lengthofPool(BM_BufferMgmt *mgmt)
{
	int count = 0;
	Buffer *temp;
	temp = mgmt->start; //make root equals the first element in the buffer pool

	while(temp != NULL)
	{
		count++;
		temp = temp->next; //next
	}

	return count;
}

//function returns the page frame number 
Buffer *searchPos(BM_BufferMgmt *mgmt, PageNumber pNum)
{
	//int framepos = -1;
	Buffer *search_buffer;	
	search_buffer = mgmt->start;

	while (search_buffer != NULL)
	{
		if(search_buffer->storage_mgr_pageNum == pNum) //if the page is found
		{
			//framepos = search_buffer->buffer_mgr_pageNum; //store it // get back to it to check POS
			break;
		}

		search_buffer = search_buffer->next;
	}
	
	return search_buffer;
}

void updateCounter(BM_BufferMgmt *mgmt)
{
	Buffer *temp;
	temp = mgmt->start; //make temp equals the first element in the buffer pool

	while(temp != NULL)
	{
		temp->count = temp->count + 1;
		temp = temp->next; //next
	}
}

void updateFixCounts(BM_BufferMgmt *mgmt, PageNumber pNum)
{
	Buffer *temp;
	temp = mgmt->start; //make temp equals the first element in the buffer pool

	while(temp != NULL)
	{
		if(temp->storage_mgr_pageNum == pNum) //if the page is found
		{
			temp->fixcounts += 1; //store it // get back to it to check POS
			break;
		}
		temp = temp->next;
	}
}

void resetCounter(BM_BufferMgmt *mgmt, int framepos)
{
	Buffer *temp;
	temp = mgmt->start;

	while (temp != NULL)
	{
		if(temp->buffer_mgr_pageNum == framepos) //if the page is found
		{
			temp->count = 1; //store it // get back to it to check POS
			break;
		}

		temp = temp->next;
	}
}

int emptyBufferFrame(BM_BufferPool *bm)
{
	int flag = 0;
	Buffer *temp;
	temp = ((BM_BufferMgmt *)bm->mgmtData)->start;
	while (temp != NULL)
	{
		if(temp->buffer_mgr_pageNum != flag) //if the page is found
		{
			return flag; //store it // get back to it to check POS
			break;
		}
		
		flag = flag + 1;
		temp = temp->next;
	}

	if(flag < bm->numPages)
		return flag;
	else
		return -10;
}


char *testName;
int main()
{
	int a,i;
	BM_BufferPool *bm = MAKE_POOL();
  	testName = "Creating and Reading Back Dummy Pages";
  	a = createPageFile("testbuffer.bin");
  	BM_PageHandle *h = MAKE_PAGE_HANDLE();
  	

  	a = initBufferPool(bm, "testbuffer.bin", 3, RS_FIFO, NULL);
  	printf("RC: %d Buffer Pool Initialization \n",a);

  	//printf("%s Page File\n", bm->pageFile);
  	//printf("%d Num Pages\n", bm->numPages);
  	//printf("%i strategy\n", bm->strategy);
  	//printf("%d mgmtData\n", (*(SM_FileHandle *)bm->mgmtData).curPagePos);
  	//printf("%d mgmtData\n", ((SM_FileHandle *)bm->mgmtData)->curPagePos);
  	for(i = 0; i < 3; i++)
  	{
  		printf("pinPage %i\n", i);
  		a = pinPage(bm, h, i);
  		printf("RC: %d Pin Page \n",a);
  		//sprintf(h->data, "%s-%i", "Page", h->pageNum);
  		//printf("h->data ---------> %s\n", h->data);
  		//printf("h->pageNum --------> %i\n", h->pageNum);
  		//a = forcePage(bm, h);
  		//printf("RC: %d Force Page \n",a);
  		a = markDirty(bm,h);
  		printf("RC: %d Mark Dirty \n",a);
  		//a = forceFlushPool(bm);
  		//printf("RC: %d Force Flush Pool \n",a);
  		a = unpinPage(bm,h);
  		printf("RC: %d UnPin Page \n",a);
  	}
  	a = forcePage(bm, h);
  	printf("RC: %d Force Page \n",a);
  	//printBuffer(bm);
  	a = forceFlushPool(bm);
  	printf("RC: %d Force Flush Pool \n",a);
  	PageNumber *b;
  	b = getFrameContents(bm);
  	printf("Frame Contents : ");
  	if(b != NO_PAGE)
  		for(i = 0; i < 3; i++)
  			printf(" %d",b[i]);
  	else
  		printf(" %i", NO_PAGE);
  	printf("\n");
  	printBuffer(bm);
}

void printBuffer(BM_BufferPool *bm)
{
	Buffer *temp;
	temp = ((BM_BufferMgmt *)bm->mgmtData)->start;
	printf("-------------------------\n");
	printf("BUFFER POOL\n");
	printf("-------------------------\n");
	while (temp != NULL)
	{
		printf("%i temp->buffer_mgr_pageNum \n", temp->buffer_mgr_pageNum);
		printf("%i temp->storage_mgr_pageNum \n", temp->storage_mgr_pageNum);
		printf("%i temp->count \n", temp->count);
		printf("%i temp->fixcounts \n", temp->fixcounts);
		printf("%i temp->dirty \n", temp->dirty);
		temp = temp->next;
	}
	printf("-------------------------\n");
}

// Buffer Manager Interface Pool Handling
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData)
{
	if(bm->mgmtData != NULL)
		return RC_BUFFER_POOL_ALREADY_INIT;

	SM_FileHandle *fh;
	fh=(SM_FileHandle *)malloc(sizeof(SM_FileHandle));

    if(fh == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    int ret = openPageFile(pageFileName, fh);
    ensureCapacity(10,fh); // extra line of code
    if(ret == RC_FILE_NOT_FOUND)
		return RC_FILE_NOT_FOUND;

    BM_BufferMgmt *mgmt;
    mgmt = (BM_BufferMgmt *)malloc(sizeof(BM_BufferMgmt));

    mgmt->f = fh;
    mgmt->start = NULL;
    mgmt->current = NULL;

    bm->pageFile = pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = mgmt;

    return RC_OK;
}

RC shutdownBufferPool(BM_BufferPool *const bm)
{

}

RC forceFlushPool(BM_BufferPool *const bm)
{
	int a = RC_OK;
	Buffer *temp;
	temp = ((BM_BufferMgmt *)bm->mgmtData)->start;

	if(temp == NULL)
		return RC_BUFFER_POOL_EMPTY;

	while (temp != NULL)
	{
		if(temp->dirty)
		{
			if(temp->fixcounts == 0)
			{
				a = writeBlock(temp->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, temp->ph->data);
				if(a == RC_OK)
					temp->dirty = 0;
			}
		}

		temp = temp->next;
	}

	return a;

}

// Buffer Manager Interface Access Pages
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	Buffer *bufferPagePos = searchPos(bm->mgmtData, page->pageNum);

	if(bufferPagePos != NULL)
	{
		bufferPagePos->dirty = 1;
		return RC_OK;
	}

	return RC_BUFFER_POOL_MARKDIRTY_ERROR;
}

RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	//printf("Unpin Page\n");
	//printf("%i page->pageNum\n", page->pageNum);
	Buffer *bufferPagePos = searchPos(bm->mgmtData, page->pageNum);

	if(bufferPagePos != NULL)
	{
		bufferPagePos->fixcounts -= 1;
		if(bufferPagePos->fixcounts == 0)
		{
			if(bufferPagePos->dirty)
			{
				//Dirty page : need to write back to disk
				int a = writeBlock(bufferPagePos->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, page->data);
				//printf("RC: %d write block\n", a);
				if(a != RC_OK)
				{
					bufferPagePos->fixcounts += 1;
					return a;
				}
			}

			bufferPagePos->buffer_mgr_pageNum = -1;
			bufferPagePos->storage_mgr_pageNum = -1;
			bufferPagePos->ph = NULL;
			bufferPagePos->count = -1;
			//free(bufferPagePos);
			
			if(lengthofPool(bm->mgmtData) == 1)
			{
				bufferPagePos = NULL;
				((BM_BufferMgmt *)bm->mgmtData)->start = NULL;
			}
			
			return RC_OK;
		}

		return RC_BUFFER_POOL_PAGE_INUSE;
	}

	return RC_BUFFER_POOL_UNPINPAGE_ERROR;
	
}

RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page)
{
	Buffer *bufferPagePos = searchPos(bm->mgmtData, page->pageNum);

	if(bufferPagePos != NULL)
	{
		int a = writeBlock(bufferPagePos->storage_mgr_pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, page->data);
		printf("RC: %d write block\n", a);
		if (a == RC_OK)
			return RC_OK;
	}

	return RC_BUFFER_POOL_FORCEPAGE_ERROR;
}

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
	//printf("%d Length of Buffer\n", lengthofPool());
	//printf("%d Buffer Pool length\n", bm->numPages);
	//printf("%d Page Number from storage_mgr\n", pageNum);
	//printf("%d Search value\n", searchPos(pageNum));
	//Buffer *bufferPagePos = (Buffer *)malloc(sizeof(Buffer));

	Buffer *bufferPagePos = searchPos(bm->mgmtData, pageNum);
	//printf("%i bufferPagePos\n", bufferPagePos);
	//printf("%i ((BM_BufferMgmt *)bm->mgmtData)->start\n", ((BM_BufferMgmt *)bm->mgmtData)->start);
	if( bufferPagePos != ((BM_BufferMgmt *)bm->mgmtData)->start || bufferPagePos == 0)
	{
		int emptyFrame = emptyBufferFrame(bm);
		printf("emptyBufferFrame @ %d\n", emptyFrame);
		if (emptyFrame != -10)
		{
			Buffer *temp = (Buffer *)malloc(sizeof(Buffer));
			page->data = (char *) malloc(PAGE_SIZE);
			int a = readBlock(pageNum, ((BM_BufferMgmt *)bm->mgmtData)->f, page->data);
			//printf("RC: %d read block\n", a);
			if(a == RC_OK)
			{
				updateCounter(bm->mgmtData);
				page->pageNum = pageNum;
				temp->buffer_mgr_pageNum = emptyFrame;
				temp->ph = page;
				temp->dirty = 0;
				temp->count = 1;
				temp->fixcounts = 1;
				temp->storage_mgr_pageNum = pageNum;
				temp->next = NULL;
				//printf("%i lengthofPool(bm->mgmtData)\n", lengthofPool(bm->mgmtData));
				if(emptyFrame == 0)//lengthofPool(bm->mgmtData) == 0)
				{
					((BM_BufferMgmt *)bm->mgmtData)->start = temp;
					((BM_BufferMgmt *)bm->mgmtData)->current = ((BM_BufferMgmt *)bm->mgmtData)->start;
				}
				else
				{
					((BM_BufferMgmt *)bm->mgmtData)->current->next = temp;
					((BM_BufferMgmt *)bm->mgmtData)->current = temp;
				}
			}
			else
			{
				printf("Pin Page failed due to: %d \n", a);
				return RC_BUFFER_POOL_PINPAGE_ERROR;
			}
		}
		else
		{
			printf("Need to implement LRU or FIFO or CLOCK\n");
			switch(bm->strategy)
			{
				case RS_FIFO:
					//Call FIFO
					printf("FIFO\n");
					break;

				case RS_LRU:
					//Call LRU
					printf("LRU\n");
					break;

				default:
					printf("Buffer strategy not implemented\n");

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
	
	return RC_OK;
}

// Statistics Interface
PageNumber *getFrameContents (BM_BufferPool *const bm)
{
	int i=0;
	PageNumber *pn;//array that should be return
	Buffer *temp = ((BM_BufferMgmt *)bm->mgmtData)->start;
	
	if(temp == NULL)
		return NO_PAGE;

	pn = (PageNumber *)malloc(sizeof(PageNumber)*bm->numPages);

	while (temp!=NULL)//going to each node
	{
		pn[i]=temp->storage_mgr_pageNum;//checking if page handle has a value
		i++;
		temp=temp->next;
	}
	return pn;
}

bool *getDirtyFlags (BM_BufferPool *const bm);
int *getFixCounts (BM_BufferPool *const bm);
int getNumReadIO (BM_BufferPool *const bm);
int getNumWriteIO (BM_BufferPool *const bm);
