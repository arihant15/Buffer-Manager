			              Assignment 2: Buffer Manager

Project Modules
--------------------------------------------------------------------------------------
C source files: buffer_mgr.c,storage_mgr.c,buffer_mgr_stat.c, dberror.c,test_assign2_1.c,test_assign2_2.c
Header files:buffer_mgr.h,storage_mgr.h,buffer_mgr_stat.h dberror.h, test_helper.h,dt.h

Team members
---------------------------------------
1)Anuja Velankar  (avelanka@hawk.iit.edu, CWID: A20315513)
2)Arihant Nagarajan (anagara6@hawk.iit.edu, CWID: A20334121)
3)Syed Sufiyan Sher (ssher@hawk.iit.edu, CWID:A20318494)
4)Sonaki Bengali (sbengal1@hawk.iit.edu, CWID: A20279865)


Aim:
----------------------------------------------------------------------------------------
The goal of this assignment is to implement a buffer manager. It manages a fixed number of pages in memory that represent pages from a page file managed by the storage manager implemented in assignment 1. The memory pages managed by the buffer manager are called page frames or simply frames. A buffer pool is a combination of a page file and the page frames storing pages.The Buffer manager handles more than one open buffer pool at the same time. However, there is only one buffer pool for each page file. Each buffer pool uses a page replacement strategy. The strategy is determined when the buffer pool is initialized. We have implemented 3 replacement strategies FIFO,LRU, LFU and the program is thread safe.

With the help of functions buffer manager transfers the pages from/to disk from/to buffer manager, allows user to read,write the pages and once a page is written by user, updates it on the disk. The functions are categorized as page management functions,buffer manager interface functions and statistics functions.

Instructions to run the code
----------------------------
1) Extract the source code from zip folder "assign2_avelanka.zip"
2) Open the command prompt.
3) Go to the path where the extracted files are present.
4) Run the following commands:
   make clean
   make
5) Run the below command for testing test_assign2_1.c:
   ./test_assign2
6) To remove object files, run following command:
   make clean
7) For additional test cases in test_assign2_2.c (LFU implementation and thread safety):
   ./test_assign2_2

Data structures used:

In addition to BM_BufferPool and BM_PageHandle we have defined following data structures:

Buffer: Stores pagenumbers from storage manager and buffer manager, boolean variable to check if a page is dirty, 

BM_BufferMgmt: Stores file pointers for files in storage manager, buffer manager and variables to maintain statistics of pages in buffer manager.

  
Functions:
------------
Functions used to implement a buffer manager are described below:

lengthofPool()
------------------------------
Returns length of pool. A counter is initialized to zero. Sets the iterator to starting position of buffer pool, increments the count
till the end of pool. Return the counter.

Buffer *searchPrevPos()
------------------------------
Searches and returns the position of a previous block with respect to page frame that is currently accessed by buffer manager. Initialises search in buffer_mgmt to NULL. Sets the iterator to the start of buffer manager. Traverses the buffer pool till the match with expected PageNumber is found. Once found, returns the  previous block.

Buffer *searchPos()
-------------------------------
Searches and returns the position of a required page frame that is currently accessed by buffer manager. Initialises search in buffer_mgmt to NULL. Sets the iterator to the start of buffer manager. Traverses the buffer pool till the page with expected PageNumber is found. Once found, returns the pointer to current position.

int emptyBufferFrame()
------------------------------
Set the iterator to start of buffer pool. Increments flag till the required page is found.Returns -1 if the buffer is full , required page is not found and therefore a page replacement will be needed.

void updateCounter()
------------------------------
Traverses through the buffer pool to check the frame which was entered first and not been accessed for the longest time.

Buffer *searchCnt()
------------------------------
Searches and returns a frame with fix count 0 i.e. which is not being accessed and accessed minimum number of times.

int replacementStrategy()
------------------------------
Replaces the page in buffer using FIFO or LRU. If the page to be replaced is dirty, write it to disk, reset flags, else just read, reset flags and replace it.

int getMaxCount()
------------------------------
Searches for a frame who is 2nd in the row to be least accessed.

Buffer *searchMinFreqCnt()
------------------------------
Searches the buffer pool for the page frame having which is accessed least number of times.

int LFU()
------------------------------
Searches and returns a frame which is least frequently used. If it is dirty, then writes it to disk, resets dirty flag, increments numWriteIO flag. Else if it is not dirty, read the block and reset the related flags and replace it with required page.This is LFU strategy.

initBufferPool()
------------------------------
Initialises buffer pool with  its parameters like number of pages,replacement strategy and page file to be accessed. If the buffer pool is not empty returns buffer pool already initialised RC. If buffer pool is empty then allocate it a memory. Allocates memory for file.
Opens the page file . If pages file is not able to open returns appropriate RC, else initilaises buffer manager paramemtrs. Statistic interface parameters all will be set to 0.

shutdownBufferPool()
--------------------------
Destroys a buffer pool. It will free up all resources allocated to buffer pool.It will also ensure that dirty pages are written to disk before resources are freed. 


forceFlushPool()
--------------------
forceFlushPool causes all dirty pages (with fix count 0) from the buffer pool to be written to disk.
On successful initialization of buffer this function checks if page is dirty and fix count is zero. If both the conditions are satisfied write the page to the disk.

Page Management Functions:

pinPage()
--------------------
pinPage pins the page with page number pageNum. Checks if buffer pool is initialized.Then it pins the page with page number pageNum. The buffer manager is responsible to set the pageNum field of the page handle passed to the function. The data field should point to the page frame the page is stored in the area in memory storing the content of the page.

markDirty()
---------------
markDirty marks a page as dirty. If buffer is successfully initialized, look for the desired page and checks if it dirty.


unpinPage()
-------------------
unpins the page. The pageNum field of page should be used to figure out which page to unpin. If buffer is successfully initialized, look for the desired page, reduces the fix count by 1 and unpins it.

forcePage()
----------------
forcePage writes the current content of the page back to the page file on disk. This fnction searches for the required page, writes it to disk using writeBlock() and reset the dirty page variable and WriteIO

Buffer pool statistics interface

getFrameContents()
----------------------
The getFrameContents function returns an array of PageNumbers (of size numPages) where the ith element is the number of the page in storage manager stored in the ith page frame. An empty page frame is represented using the constant NO_PAGE.
Checks if buffer pool is initialized.If initialized,sets the iterator to starting position of buffer pool.If the frame is empty, return RC accrodingly. Else creates array of PageNumbers of size numPages. This array is intialized to -1. The entire page frame is traversed, to find number of the page, pageNum, and updating the array. Return this array of pageNum.

getDirtyFlags ()
------------------------

The getDirtyFlags function returns an array of bools (of size numPages) where the ith element is TRUE if the page stored in the ith page frame is dirty. Empty page frames are considered as clean thus dirty flag 0.
Checks if the buffer pool is not empty. If it has pages, buffer manager pointer is set to start.
Memory is allocated for the array of size numPages and dirt array is initialized to 0. Each node of buffer pool is visited, to check status of dirty flag. If the page is dirty , set the value to TRUE.The buffer pool is traversed till the end.The updated array with boolean values for all pages in buffer is returned.

getFixCounts ()
----------------------
The getFixCounts function returns an array of ints (of size numPages) where the ith element is the fix count of the page stored in the ith page frame. Return 0 for empty page frames.
Checks if the buffer pool is not empty. If it has pages, buffer manager pointer is set to start.
Memory is allocated for the array of size numPages and entire array is initialized to 0. Each node of buffer pool is visited, to check fix count. If the fix count of i th page is greater than 0,fix count is stored into ith position of array 'fix'.The buffer pool is traversed till the end.The array of fix count for pages in buffer is returned.

getNumReadIO ()
--------------------
The getNumReadIO function returns the number of pages that have been read from disk since a buffer pool has been initialized. This value is intitialized to 0, when the pool is created and increments whenever a page is read from the page file into a page frame.

getNumWriteIO ()
-------------------
Returns the numWriteIO parameter in mgmtdata, which is the number of pages written to the page file since the buffer pool was initialized.This value is intitialized to 0, when the pool is created and increments whenever a page is read from the page file into a page frame.

Additional Error codes
--------------------
We have defined some new error codes and corresponding error 
messages in  dberror.h file which are mentioned below:

These codes were added in Assignment 1:

1) RC_FILE_PRESENT 5
2) RC_FILE_READ_ERROR 6
3) RC_WRITE_OUT_OF_BOUND_INDEX 7

These codes are added in assignment 2

RC_BUFFER_POOL_NOT_INIT 8  
RC_BUFFER_POOL_ALREADY_INIT 9 
RC_BUFFER_POOL_PINPAGE_ERROR 10
RC_BUFFER_POOL_PINPAGE_ALREADY_PRESENT 11
RC_BUFFER_POOL_PAGE_INUSE 12
RC_BUFFER_POOL_UNPINPAGE_ERROR 13
RC_BUFFER_POOL_FORCEPAGE_ERROR 14
RC_BUFFER_POOL_MARKDIRTY_ERROR 15 
RC_BUFFER_POOL_EMPTY 16 