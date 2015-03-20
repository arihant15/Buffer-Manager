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
	//int pos; //position of the page frame
	int stroage_mgr_pageNum; //page number
	BM_PageHandle *ph;
	bool dirty; //dirty page or not ?
	//char *data; //data of the page
	int count; //general counter to count how old of the page
	struct Buffer *next; // pointer to the next buffer
} Buffer;

Buffer *start = NULL;

//function to calculate and return the length of the buffer pool
int lengthofPool()
{
	int count = 0;
	Buffer *root;
	root = start; //make root equals the first element in the buffer pool
	while(root != NULL)
	{
		count++;
		root = root->next; //next
	}
	return count;
}

//function returns the page frame number 
int search(PageNumber pNum)
{
	int framepos = -1;
	Buffer *temp;
	temp = start;
	while (temp != NULL)
	{
		if(temp->stroage_mgr_pageNum = pNum) //if the page is found
		{
			framepos = (temp->ph)->pageNum; //store it // get back to it to check POS
			break;
		}
		temp = temp->next;
	}
	return framepos;// return
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
  		pinPage(bm, h, 0);
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
    if(ret == RC_FILE_NOT_FOUND)
		return RC_FILE_NOT_FOUND;

	bm->pageFile = pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;
    bm->mgmtData = fh;

    return RC_OK;
}

RC shutdownBufferPool(BM_BufferPool *const bm)
{

}

RC forceFlushPool(BM_BufferPool *const bm)
{

}

// Buffer Manager Interface Access Pages
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page)
{

}
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page);
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page);

RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
	printf("%d Length of Buffer\n", lengthofPool());
	printf("%d Page Number from storage_mgr\n", pageNum);
	printf("%d Search value\n", search(pageNum));
	if(search(pageNum) == -1)
	{
		if(lengthofPool() == 0)
		{
			Buffer *temp = (Buffer *)malloc(sizeof(Buffer));
			page->pageNum = 0;
			page->data = (char *) malloc(PAGE_SIZE);
			int a = readBlock(pageNum, (SM_FileHandle *)bm->mgmtData, page->data);
			//printf("RC: %d read block\n", a);
			temp->ph = page;
			temp->dirty = 0;
			temp->count = 1;
			temp->stroage_mgr_pageNum = pageNum;
			temp->next = NULL;
			start = temp;
		}
		else if (lengthofPool > 0 && lengthofPool < bm->numPages)
		{
			Buffer *temp = (Buffer *)malloc(sizeof(Buffer));
			page->pageNum = 0;
			page->data = (char *) malloc(PAGE_SIZE);
			int a = readBlock(pageNum, (SM_FileHandle *)bm->mgmtData, page->data);
			//printf("RC: %d read block\n", a);
			temp->ph = page;
			temp->dirty = 0;
			temp->count = 1;
			start->stroage_mgr_pageNum = pageNum;
			temp->next = NULL;
			start->next = temp;
		}
		else
		{
			//FIFO
			//LRU
			printf("Need to implement LRU or FIFO or CLOCK\n");
		}
	}
	else
	{
		printf("Page already present\n");
	}
	
	return RC_OK;
}

// Statistics Interface
PageNumber *getFrameContents (BM_BufferPool *const bm);
bool *getDirtyFlags (BM_BufferPool *const bm);
int *getFixCounts (BM_BufferPool *const bm);
int getNumReadIO (BM_BufferPool *const bm);
int getNumWriteIO (BM_BufferPool *const bm);