#include "storage_mgr.h"
#include <sys/stat.h>
#include <stdio.h>
// the Main function that was implemented to Check all the test cases
/* manipulating page files */
/*
int main()
{
	SM_FileHandle fh;
	SM_PageHandle ph;
	int a,i;
	ph = (SM_PageHandle) malloc(PAGE_SIZE);
	
	initStorageManager();

	a = createPageFile("Hello.txt");
	printf("RC: %d Create Page file\n",a);
	
	a = openPageFile("Hello.txt", &fh);
	printf("RC: %d open Page file\n",a);

	a = closePageFile(&fh);
	printf("RC: %d file closed\n", a);

	//a = destroyPageFile("Hello.txt");
	//printf("RC: %d file destroyPageFile\n", a);

	a = readBlock(0,&fh,ph);
	printf("RC: %d read block\n", a);

	a = getBlockPos(&fh);
	printf("RC: %d block Position\n", a);

	a = readFirstBlock(&fh, ph);
	printf("RC: %d read First block\n", a);

	a = readPreviousBlock(&fh, ph);
	printf("RC: %d read Previous block\n", a);

	a = readCurrentBlock(&fh, ph);
	printf("RC: %d read Current block\n", a);

	a = readNextBlock(&fh, ph);
	printf("RC: %d read Next block\n", a);

	a = readLastBlock(&fh, ph);
	printf("RC: %d read Last block\n", a);
	
	for (i=0; i < PAGE_SIZE; i++)
    	ph[i] = (i % 10) + '0';
	a = writeBlock(0,&fh,ph);
	printf("RC: %d write block\n", a);

	for (i=0; i < PAGE_SIZE; i++)
    	ph[i] = '0';
	a = writeCurrentBlock(&fh,ph);
	printf("RC: %d write current block\n", a);

	a = readNextBlock(&fh, ph);
	printf("RC: %d read Next block\n", a);

	a = appendEmptyBlock(&fh);
	printf("RC: %d Append Empty Block!!\n", a);

	a = readNextBlock(&fh, ph);
	printf("RC: %d read Next block\n", a);

	//a = ensureCapacity(10,&fh);
	//printf("RC: %d Ensure Capacity!!\n", a);

	a = readNextBlock(&fh, ph);
	printf("RC: %d read Next block\n", a);

	a = appendEmptyBlock(&fh);
	printf("RC: %d Append Empty Block!!\n", a);

	for (i=0; i < PAGE_SIZE; i++)
    	ph[i] = '1';
	a = writeCurrentBlock(&fh,ph);
	printf("RC: %d write current block\n", a);

}*/
//just a minor initialization function, doesn't do anything yet
void initStorageManager (void)
{
	printf("Storage Manager Initialized!!\n");

}
//Creating a page in the file
RC createPageFile (char *fileName)
{
	FILE *fp;
	int i;
	char ops;
	fp = fopen(fileName, "r");//open the file 
	if(fp != NULL)
	{
		printf("File Already Present!!\nDo you want to over-write the file (y/n): ");
		scanf("%c",&ops);
		fclose(fp);
	}
	if(ops == 'y' || fp == NULL)
	{
		fp = fopen(fileName, "w");	
		fseek(fp , 0 , SEEK_SET);//seeking to the beginning of the file
		//filling the page of page size 4096 bytes
		for(i = 0; i < PAGE_SIZE; i++)
		{
			fwrite("\0",1, 1,fp);
			fseek(fp,0,SEEK_END);
		}

		fclose(fp);
		return RC_OK;
	}
	else
	{
		return RC_FILE_PRESENT;
	}//returning respective errors
}
//open page function
RC openPageFile(char *fileName, SM_FileHandle *fHandle)
{
	//FILE *fp;
	fHandle->mgmtInfo = fopen(fileName, "r");

	if(fHandle->mgmtInfo == NULL)// if file not present i.e NULL
		return RC_FILE_NOT_FOUND;
	//finding the file size
	struct stat fileStat;
	stat(fileName,&fileStat);
	int fileSize = fileStat.st_size;
	//initializing the structure variables
	fHandle->fileName = fileName;
	fHandle->totalNumPages = fileSize/PAGE_SIZE;
	fHandle->curPagePos = ftell(fHandle->mgmtInfo)/PAGE_SIZE;
	//fHandle->mgmtInfo = fp;
	fclose(fHandle->mgmtInfo);
	return RC_OK;

}
//closing the file
RC closePageFile (SM_FileHandle *fHandle)
{
	if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
	switch(fclose(fHandle->mgmtInfo))//close the file
	{// returning respective errors
		case 0: return RC_OK;
		default : return RC_FILE_NOT_FOUND;
	}
}
//destroying the page file
RC destroyPageFile (char *fileName)
{
	if(remove(fileName) == 0)
		return RC_OK;
	else
		return RC_FILE_NOT_FOUND;
}

/* reading blocks from disc */
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	if (fHandle == NULL)//checking for initialization
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)//checking if the file is present or not
		return RC_FILE_NOT_FOUND;

	if(pageNum > fHandle->totalNumPages)//checking if its an invalid page number
		return RC_READ_NON_EXISTING_PAGE;
	
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
	
	fseek(fHandle->mgmtInfo , pageNum*PAGE_SIZE , SEEK_SET);//seeking to a particular block with respect to the page number

	if(fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE)// reading a block of size 4096 bytes
		return RC_FILE_READ_ERROR;
	//setting the current pointer position
	
	fHandle->curPagePos = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
	fclose(fHandle->mgmtInfo);
	
	return RC_OK;
}
// returns the current pointer position
int getBlockPos (SM_FileHandle *fHandle)
{
	if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;

	return fHandle->curPagePos;//returning position of the pointer
}
//read first block
RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;

	fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
	
	fseek(fHandle->mgmtInfo , 0 , SEEK_SET);// seeking to the beginning of the file
	//reading the first 4096bytes 
	if(fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE)
		return RC_FILE_READ_ERROR;
	
	//setting the pointer position 
	fHandle->curPagePos = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
	fclose(fHandle->mgmtInfo);
	
	return RC_OK;
}
//reads previous block
RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;
	//checking for errors and returning error codes
	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;

	if(fHandle->curPagePos < 1)
		return RC_READ_NON_EXISTING_PAGE;
	
	//seeking to the previous page
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
	
	fseek(fHandle->mgmtInfo , -PAGE_SIZE , SEEK_CUR);
	
	//reading a block of data
	if(fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE)
		return RC_FILE_READ_ERROR;
	
	//setting the current pointer position
	fHandle->curPagePos = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
	fclose(fHandle->mgmtInfo);
	
	return RC_OK;
}
//reading current block form the file
RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;
	
	//returning required errors after checking the conditions
	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;

	if((fHandle->curPagePos) > (fHandle->totalNumPages-1))
		return RC_READ_NON_EXISTING_PAGE;
	
	//seeking to the current page with respect to the start of the file
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
	fseek(fHandle->mgmtInfo , (fHandle->curPagePos)*PAGE_SIZE , SEEK_SET);
	
	//reading a block of file
    if(fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE)
    	return RC_FILE_READ_ERROR;

	fHandle->curPagePos = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
	fclose(fHandle->mgmtInfo);
    
    return RC_OK;
}
//read next block of data
RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;

	if(fHandle->curPagePos > (fHandle->totalNumPages-1))
		return RC_READ_NON_EXISTING_PAGE;
	
	//seeking to the next block 
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
    fseek(fHandle->mgmtInfo , fHandle->curPagePos * PAGE_SIZE , SEEK_SET);
	
	//reading from the file
    if(fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE)
    	return RC_FILE_READ_ERROR;
	
	//setting the current position of the pointer
	fHandle->curPagePos = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
	fclose(fHandle->mgmtInfo);
    
    return RC_OK;
}
//reads the last block of the file
RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;
	
	//seeks to the beginning of the last page
	
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
    fseek(fHandle->mgmtInfo , -PAGE_SIZE , SEEK_END);
	
	//reads the last page to a buffer
    if(fread(memPage, 1, PAGE_SIZE, fHandle->mgmtInfo) != PAGE_SIZE)
    	return RC_FILE_READ_ERROR;
	
	//sets the current position of the pointer
	fHandle->curPagePos = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
	fclose(fHandle->mgmtInfo);
    
    return RC_OK;
}

/* writing blocks to a page file */
RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
	if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;
	//handling respective error checking 
	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;

	if(pageNum > ((fHandle->totalNumPages)))
		return RC_WRITE_OUT_OF_BOUND_INDEX;

	//FILE *fp;
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r+");
	fseek(fHandle->mgmtInfo , pageNum*PAGE_SIZE , SEEK_SET);
	//seeks to the particular page with respect to the start of the file
	if(fwrite(memPage,PAGE_SIZE,1,fHandle->mgmtInfo) != 1)
		return RC_WRITE_FAILED;
	//writes the data
	//sets all the file handle variables

	//fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
	fseek(fHandle->mgmtInfo , (pageNum+1)*PAGE_SIZE , SEEK_SET);
	//fHandle->mgmtInfo = fp;
	fHandle->curPagePos = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
	//fHandle->totalNumPages = (ftell(fp)/PAGE_SIZE);
	fclose(fHandle->mgmtInfo);
	
	return RC_OK;
}
        //writes to the current page block
RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;

	if(fHandle->curPagePos > (fHandle->totalNumPages -1))
		return RC_WRITE_OUT_OF_BOUND_INDEX;

    //FILE *fp;
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r+");
	fseek(fHandle->mgmtInfo, (fHandle->curPagePos)*PAGE_SIZE , SEEK_SET);// seeks to the current page w.r.t to start of the file
    
 	if(fwrite(memPage,PAGE_SIZE,1, fHandle->mgmtInfo) != 1)//writes the data from the buffer to the file
		return RC_WRITE_FAILED;
 	//sets the fHandle variables
 	fHandle->curPagePos = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
 	//fHandle->mgmtInfo = fopen(fHandle->fileName,"r");
 	fseek(fHandle->mgmtInfo, (fHandle->curPagePos)*PAGE_SIZE , SEEK_SET);
 	//fHandle->mgmtInfo = fp;
 	fclose(fHandle->mgmtInfo);
    return RC_OK;
}
//appends the empty block
RC appendEmptyBlock (SM_FileHandle *fHandle)
{
	int i;
	//printf("\n=================== Append Empty Block ========================\n");
	if (fHandle == NULL)
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;
	//printf("\n=================== Append Empty Block Opening File========================\n");
    //FILE *fp;
	fHandle->mgmtInfo = fopen(fHandle->fileName,"r+");
	//printf("\n=================== Append Empty Block File opened ========================\n");
	//seek to the end of the file
	fseek(fHandle->mgmtInfo, 0, SEEK_END);
	//printf("\n=================== Append Empty Block Seek end ========================\n");
	//writes to the empty page
	for(i = 0; i < PAGE_SIZE; i++)
	{
		fwrite("\0",1, 1,fHandle->mgmtInfo);
		fseek(fHandle->mgmtInfo,0,SEEK_END);
	}

	//fHandle->mgmtInfo = fp;
	fHandle->totalNumPages = (ftell(fHandle->mgmtInfo)/PAGE_SIZE);
	fclose(fHandle->mgmtInfo);
	//printf("\n=================== Append Empty Block Complete ========================\n");
	return RC_OK;
}
RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle)
{
	
	if (fHandle == NULL)   // If file handle is null, throw error
		return RC_FILE_HANDLE_NOT_INIT;

	if(fHandle->mgmtInfo == NULL)
		return RC_FILE_NOT_FOUND;
	
	if (fHandle->totalNumPages < numberOfPages) //Check if total number of pages is less than number of pages
	{
		while (fHandle->totalNumPages != numberOfPages) 
		{
			//printf("\n=================== Ensure Capacity calling Append Empty Block ========================\n");
			appendEmptyBlock(fHandle); // Append empty block
		}
	}

	return RC_OK;
}

