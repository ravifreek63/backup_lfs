/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2011 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "yaffsfs.h"
#include "yaffs_guts.h"
#include "yaffscfg.h"
#include "yportenv.h"
#include "yaffs_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>         /* for memset */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

#define sync_transaction_enable 1
#define YAFFSFS_MAX_SYMLINK_DEREFERENCES 5

#ifndef NULL
#define NULL ((void *)0)
#endif


/* YAFFSFS_RW_SIZE must be a power of 2 */
#define YAFFSFS_RW_SHIFT (13)
#define YAFFSFS_RW_SIZE  (1<<YAFFSFS_RW_SHIFT)

/*------------------------------------ Externally Defined Methods    ---------------------------*/
extern struct yaffs_obj *yaffs_create_file_default (struct yaffs_obj *parent, const YCHAR *name, u32 mode, u32 uid, u32 gid, bool flushToDisk);
extern struct yaffs_obj *yaffs_sync_file_to_disk (char *pathname);
extern void print_committed_transactions(void);
extern struct yaffs_obj *get_journal_obj (void);
int log_init (void);
int result_log_init (void);
/* Some forward references */
static struct yaffs_obj *yaffsfs_FindObject(struct yaffs_obj *relativeDirectory,
			const YCHAR *path,
			int symDepth, int getEquiv,
			struct yaffs_obj **dirOut,
			int *notDir, int *loop);

static void yaffsfs_RemoveObjectCallback(struct yaffs_obj *obj);
int transaction_support = 1;
int journal_write_tran_id = 1;
enum ROLLBACK_TYPE rollback_type = YAFFS_COW ;
extern int yflash2_GetNumberOfBlocks(void);
FILE *resultFileFd;
FILE *logFileFd;


unsigned int yaffs_wr_attempts;

/*
 * Handle management.
 * There are open inodes in yaffsfs_Inode.
 * There are open file descriptors in yaffsfs_FileDes.
 * There are open handles in yaffsfs_FileDes.
 *
 * Things are structured this way to be like the Linux VFS model
 * so that interactions with the yaffs guts calls are similar.
 * That means more common code paths and less special code.
 * That means better testing etc.
 *
 * We have 3 layers because:
 * A handle is different than an fd because you can use dup()
 * to create a new handle that accesses the *same* fd. The two
 * handles will use the same offset (part of the fd). We only close
 * down the fd when there are no more handles accessing it.
 *
 * More than one fd can currently access one file, but each fd
 * has its own permsiions and offset.
 */

typedef struct {
	int count;	/* Number of handles accessing this inode */
	struct yaffs_obj *iObj;
} yaffsfs_Inode;



typedef struct {
	short int fdId;
	short int useCount;
} yaffsfs_Handle;

static yaffsfs_Inode yaffsfs_inode[YAFFSFS_N_HANDLES];
static yaffsfs_FileDes yaffsfs_fd[YAFFSFS_N_HANDLES];
static yaffsfs_Handle yaffsfs_handle[YAFFSFS_N_HANDLES];

static int yaffsfs_handlesInitialised;


unsigned yaffs_set_trace(unsigned  tm)
{
	yaffs_trace_mask = tm;
	return yaffs_trace_mask;
}

unsigned yaffs_get_trace(void)
{
	return yaffs_trace_mask;
}

/*
 * yaffsfs_InitHandle
 * Inilitalise handle management on start-up.
 */

static void yaffsfs_InitHandles(void)
{
	int i;
	if(yaffsfs_handlesInitialised)
                return;

	memset(yaffsfs_inode,0,sizeof(yaffsfs_inode));
	memset(yaffsfs_fd,0,sizeof(yaffsfs_fd));
	memset(yaffsfs_handle,0,sizeof(yaffsfs_handle));

	for(i = 0; i < YAFFSFS_N_HANDLES; i++)
		yaffsfs_fd[i].inodeId = -1;
	for(i = 0; i < YAFFSFS_N_HANDLES; i++)
		yaffsfs_handle[i].fdId = -1;
}

static yaffsfs_Handle *yaffsfs_HandleToPointer(int h)
{
	if(h >= 0 && h <= YAFFSFS_N_HANDLES)
		return &yaffsfs_handle[h];
	return NULL;
}

static yaffsfs_FileDes *yaffsfs_HandleToFileDes(int handle)
{
	yaffsfs_Handle *h = yaffsfs_HandleToPointer(handle);

	if(h && h->useCount > 0 && h->fdId >= 0 && h->fdId < YAFFSFS_N_HANDLES)
		return  &yaffsfs_fd[h->fdId];

	return NULL;
}

static yaffsfs_Inode *yaffsfs_HandleToInode(int handle)
{
	yaffsfs_FileDes *fd = yaffsfs_HandleToFileDes(handle);

	if(fd && fd->handleCount > 0 &&
		fd->inodeId >= 0 && fd->inodeId < YAFFSFS_N_HANDLES)
		return  &yaffsfs_inode[fd->inodeId];

	return NULL;
}

static struct yaffs_obj *yaffsfs_HandleToObject(int handle)
{
	//printf("yaffsfs_HandleToObject called on handle = %d\n", handle);
	yaffsfs_Inode *in = yaffsfs_HandleToInode(handle);

	if(in)
		{
			//printf ("Object Found\n");
			return in->iObj;
		}

	return NULL;
}

/*
 * yaffsfs_FindInodeIdForObject
 * Find the inode entry for an object, if it exists.
 */

static int yaffsfs_FindInodeIdForObject(struct yaffs_obj *obj)
{
	int i;
	int ret = -1;

	if(obj)
		obj = yaffs_get_equivalent_obj(obj);

	/* Look for it in open inode table*/
	for(i = 0; i < YAFFSFS_N_HANDLES && ret < 0; i++){
		if(yaffsfs_inode[i].iObj == obj)
			ret = i;
	}
	return ret;
}

/*
 * yaffsfs_GetInodeIdForObject
 * Grab an inode entry when opening a new inode.
 */
static int yaffsfs_GetInodeIdForObject(struct yaffs_obj *obj)
{
	int i;
	int ret;
	yaffsfs_Inode *in = NULL;

	if(obj)
		obj = yaffs_get_equivalent_obj(obj);

        ret = yaffsfs_FindInodeIdForObject(obj);

	for(i = 0; i < YAFFSFS_N_HANDLES && ret < 0; i++){
		if(!yaffsfs_inode[i].iObj)
			ret = i;
	}

	if(ret>=0){
		in = &yaffsfs_inode[ret];
		if(!in->iObj)
			in->count = 0;
		in->iObj = obj;
		in->count++;
	}


	return ret;
}


static int yaffsfs_CountHandles(struct yaffs_obj *obj)
{
	int i = yaffsfs_FindInodeIdForObject(obj);

	if(i >= 0)
		return yaffsfs_inode[i].count;
	else
		return 0;
}

static void yaffsfs_ReleaseInode(yaffsfs_Inode *in)
{
	struct yaffs_obj *obj;

	obj = in->iObj;

	if(obj->unlinked)
		yaffs_del_obj(obj);

	obj->my_inode = NULL;
	in->iObj = NULL;

}

static void yaffsfs_PutInode(int inodeId)
{
	if(inodeId >= 0 && inodeId < YAFFSFS_N_HANDLES){
		yaffsfs_Inode *in = & yaffsfs_inode[inodeId];
		in->count--;
		if(in->count <= 0){
			yaffsfs_ReleaseInode(in);
			in->count = 0;
		}
	}
}



static int yaffsfs_NewHandle(yaffsfs_Handle **hptr)
{
	int i;
	yaffsfs_Handle *h;

	for(i = 0; i < YAFFSFS_N_HANDLES; i++){
		h = &yaffsfs_handle[i];
		if(h->useCount < 1){
			memset(h,0,sizeof(yaffsfs_Handle));
			h->fdId=-1;
			h->useCount=1;
			if(hptr)
				*hptr = h;
			return i;
		}
	}
	return -1;
}

static int yaffsfs_NewHandleAndFileDes(void)
{
	int i;
	yaffsfs_FileDes *fd;
	yaffsfs_Handle  *h = NULL;
	int handle = yaffsfs_NewHandle(&h);

	if(handle < 0)
		return -1;

	for(i = 0; i < YAFFSFS_N_HANDLES; i++){
		fd = &yaffsfs_fd[i];
		if(fd->handleCount < 1){
			memset(fd,0,sizeof(yaffsfs_FileDes));
			fd->inodeId=-1;
			fd->handleCount=1;
			h->fdId = i;
			return handle;
		}
	}

	/* Dump the handle because we could not get a fd */
	h->useCount = 0;
	return -1;
}

/*
 * yaffs_get_handle
 * Increase use of handle when reading/writing a file
 * Also gets the file descriptor.
 */

static int yaffsfs_GetHandle(int handle)
{
	yaffsfs_Handle *h = yaffsfs_HandleToPointer(handle);

	if(h && h->useCount > 0){
		h->useCount++;
		return 0;
	}
	return -1;
}

/*
 * yaffs_put_handle
 * Let go of a handle when closing a file or aborting an open or
 * ending a read or write.
 */

static int yaffsfs_PutFileDes(int fdId)
{
	yaffsfs_FileDes *fd;

	if(fdId >= 0 && fdId < YAFFSFS_N_HANDLES){
		fd = &yaffsfs_fd[fdId];
		fd->handleCount--;
		if(fd->handleCount < 1){
			if(fd->inodeId >= 0){
				yaffsfs_PutInode(fd->inodeId);
				fd->inodeId = -1;
			}
		}
	}
	return 0;
}
static int yaffsfs_PutHandle(int handle)
{
	yaffsfs_Handle *h = yaffsfs_HandleToPointer(handle);

	if(h && h->useCount > 0){
		h->useCount--;
		if(h->useCount < 1){
			yaffsfs_PutFileDes(h->fdId);
			h->fdId = -1;
		}
	}

	return 0;
}

static void yaffsfs_BreakDeviceHandles(struct yaffs_dev *dev)
{
	yaffsfs_FileDes *fd;
	yaffsfs_Handle *h;
	struct yaffs_obj *obj;
	int i;
	for(i = 0; i < YAFFSFS_N_HANDLES; i++){
		h = yaffsfs_HandleToPointer(i);
		fd = yaffsfs_HandleToFileDes(i);
		obj = yaffsfs_HandleToObject(i);
		if(h && h->useCount > 0){
			h->useCount = 0;
			h->fdId = 0;
		}
		if(fd && fd->handleCount>0 && obj && obj->my_dev == dev){

			fd->handleCount = 0;
			yaffsfs_PutInode(fd->inodeId);
			fd->inodeId = -1;
		}
	}
}




/*
 *  Stuff to handle names.
 */


int yaffsfs_Match(YCHAR a, YCHAR b)
{
	/* case sensitive */
	return (a == b);
}

int yaffsfs_IsPathDivider(YCHAR ch)
{
	const YCHAR *str = YAFFS_PATH_DIVIDERS;

	while(*str){
		if(*str == ch)
			return 1;
		str++;
	}

	return 0;
}

int yaffsfs_CheckNameLength(const char *name)
{
	int retVal = 0;

	int nameLength = strnlen(name,YAFFS_MAX_NAME_LENGTH+1);

	if(nameLength == 0){
		yaffsfs_SetError(-ENOENT);
		retVal = -1;
	} else if (nameLength > YAFFS_MAX_NAME_LENGTH){
		yaffsfs_SetError(-ENAMETOOLONG);
		retVal = -1;
	}

	return retVal;
}


static int yaffsfs_alt_dir_path(const YCHAR *path, YCHAR **ret_path)
{
	YCHAR *alt_path = NULL;
	int path_length;
	int i;

	/*
	 * We don't have a definition for max path length.
	 * We will use 3 * max name length instead.
	 */
	*ret_path = NULL;
	path_length = strnlen(path,(YAFFS_MAX_NAME_LENGTH+1)*3 +1);

	/* If the last character is a path divider, then we need to
	 * trim it back so that the name look-up works properly.
	 * eg. /foo/new_dir/ -> /foo/newdir
	 * Curveball: Need to handle multiple path dividers:
	 * eg. /foof/sdfse///// -> /foo/sdfse
	 */
	if(path_length > 0 &&
		yaffsfs_IsPathDivider(path[path_length-1])){
		alt_path = kmalloc(path_length + 1, 0);
		if(!alt_path)
			return -1;
		strcpy(alt_path, path);
		for(i = path_length-1;
			i >= 0 && yaffsfs_IsPathDivider(alt_path[i]);
			i--)
			alt_path[i] = (YCHAR) 0;
	}
	*ret_path = alt_path;
	return 0;
}


LIST_HEAD(yaffsfs_deviceList);

/*
 * yaffsfs_FindDevice
 * yaffsfs_FindRoot
 * Scan the configuration list to find the device
 * Curveballs: Should match paths that end in '/' too
 * Curveball2 Might have "/x/ and "/x/y". Need to return the longest match
 */
static struct yaffs_dev *yaffsfs_FindDevice(const YCHAR *path, YCHAR **restOfPath)
{

	struct list_head *cfg;
	const YCHAR *leftOver;
	const YCHAR *p;
	struct yaffs_dev *retval = NULL;
	struct yaffs_dev *dev = NULL;
	int thisMatchLength;
	int longestMatch = -1;
	int matching;
	//printf("In yaffsfs_FindDevice path=%s\n", path);
	/*
	 * Check all configs, choose the one that:
	 * 1) Actually matches a prefix (ie /a and /abc will not match
	 * 2) Matches the longest.
	 */
	list_for_each(cfg, &yaffsfs_deviceList)
        {
		dev = list_entry(cfg, struct yaffs_dev, dev_list);
		//printf("chunks_per_block = %d\n", dev->param.chunks_per_block);
		leftOver = path;
		p = dev->param.name_;
		//printf("Device Parameter Name %s\n", dev->param.name_);
		//if (!p) continue;
		thisMatchLength = 0;
		matching = 1;


		while(matching && *p && *leftOver){
			/* Skip over any /s */
			while(yaffsfs_IsPathDivider(*p))
			      p++;

			/* Skip over any /s */
			while(yaffsfs_IsPathDivider(*leftOver))
		              leftOver++;

			/* Now match the text part */
		        while(matching &&
		              *p && !yaffsfs_IsPathDivider(*p) &&
		              *leftOver && !yaffsfs_IsPathDivider(*leftOver)){
			      	if(yaffsfs_Match(*p,*leftOver)){
			      		p++;
			      		leftOver++;
			      		thisMatchLength++;
				} else {
					matching = 0;
				}
			}
		}

		/* Skip over any /s in leftOver */
		while(yaffsfs_IsPathDivider(*leftOver))
	              leftOver++;

		// Skip over any /s in p
		while(yaffsfs_IsPathDivider(*p))
	              p++;

		// p should now be at the end of the string (ie. fully matched)
		if(*p)
			matching = 0;

		if( matching && (thisMatchLength > longestMatch))
		{
			// Matched prefix
			*restOfPath = (YCHAR *)leftOver;
			retval = dev;
			longestMatch = thisMatchLength;
		}

	}
	//printf("Returning Device Parameter Name %s\n", retval->param.name_);
	return retval;
}

static int yaffsfs_CheckPath(const YCHAR *path)
{
	int n=0;
	int divs=0;
	while(*path && n < YAFFS_MAX_NAME_LENGTH && divs < 100){
		if(yaffsfs_IsPathDivider(*path)){
			n=0;
			divs++;
		} else
			n++;
		path++;
	}

	return (*path) ? -1 : 0;
}

/* FindMountPoint only returns a dev entry if the path is a mount point */
static struct yaffs_dev *yaffsfs_FindMountPoint(const YCHAR *path)
{
	//printf("within yaffsfs_FindMountPoint\n");
	struct yaffs_dev *dev;
	YCHAR *restOfPath=NULL;
	dev = yaffsfs_FindDevice(path,&restOfPath);
        if (dev != NULL)
	{
		;//printf ("yaffsfs_FindMountPoint dev not NULL dev path = %s\n", dev->param.name_);
	}
	else 
 	{
		;//printf ("yaffsfs_FindMountPoint dev NULL\n", dev->param.name);
	}	

	if(dev && restOfPath && *restOfPath)
		dev = NULL;
	return dev;
}

static struct yaffs_obj *yaffsfs_FindRoot(const YCHAR *path, YCHAR **restOfPath)
{

	struct yaffs_dev *dev;
//	printf("In yaffsfs_FindRoot\n");
	dev= yaffsfs_FindDevice(path,restOfPath);
        if (dev == NULL)
	{
		printf("Error yaffsfs_FindRoot dev is NULL\n");
	}
	else
	if (!dev->is_mounted)
	{
		printf("Error yaffsfs_FindRoot dev is not mounted\n");
	}
	if(dev && dev->is_mounted)
	{
		
		return dev->root_dir;
	}
	return NULL;
}

static struct yaffs_obj *yaffsfs_FollowLink(struct yaffs_obj *obj,
					int symDepth, int *loop)
{

	if(obj)
		obj = yaffs_get_equivalent_obj(obj);

	while(obj && obj->variant_type == YAFFS_OBJECT_TYPE_SYMLINK){
		YCHAR *alias = obj->variant.symlink_variant.alias;

		if(yaffsfs_IsPathDivider(*alias))
			/* Starts with a /, need to scan from root up */
			obj = yaffsfs_FindObject(NULL,alias,symDepth++,
						1,NULL,NULL,loop);
		else
			/* Relative to here, so use the parent of the symlink as a start */
			obj = yaffsfs_FindObject(obj->parent,alias,symDepth++,
						1,NULL,NULL,loop);
	}
	return obj;
}


/*
 * yaffsfs_FindDirectory
 * Parse a path to determine the directory and the name within the directory.
 *
 * eg. "/data/xx/ff" --> puts name="ff" and returns the directory "/data/xx"
 */
static struct yaffs_obj *yaffsfs_DoFindDirectory(struct yaffs_obj *startDir,
				const YCHAR *path, YCHAR **name, int symDepth,
				int *notDir,int *loop)
{
       // printf("yaffsfs_DoFindDirectory called on %s path\n", path);
	struct yaffs_obj *dir;
	YCHAR *restOfPath;
	YCHAR str[YAFFS_MAX_NAME_LENGTH+1];
	int i;

	if(symDepth > YAFFSFS_MAX_SYMLINK_DEREFERENCES){
		
		if(loop)
			*loop = 1;
		return NULL;
	}

	if(startDir)
	{
		
		dir = startDir;
		restOfPath = (YCHAR *)path;
	}
	else
		{
			//printf("Calling Find Root\n");
			dir = yaffsfs_FindRoot(path,&restOfPath);
			if (dir == NULL) printf ("Error yaffsfs_DoFindDirectory :: root directory structure is NULL\n");
			//printf("Rest of path %s\n", restOfPath); // rest of path transaction_bitmap
		}


	while(dir){
		/*
		 * parse off /.
		 * curve ball: also throw away surplus '/'
		 * eg. "/ram/x////ff" gets treated the same as "/ram/x/ff"
		 */
		while(yaffsfs_IsPathDivider(*restOfPath))
			restOfPath++; /* get rid of '/' */

		*name = restOfPath;
		i = 0;

		while(*restOfPath && !yaffsfs_IsPathDivider(*restOfPath)){
			if (i < YAFFS_MAX_NAME_LENGTH){
				str[i] = *restOfPath;
				str[i+1] = '\0';
				i++;
			}
			restOfPath++;
		}

		if(!*restOfPath)
			{
				//printf("Exit\n");
			/* got to the end of the string */
				return dir;
			}
		else{
			if(strcmp(str,_Y(".")) == 0){
				/* Do nothing */
			} else if(strcmp(str,_Y("..")) == 0) {
				dir = dir->parent;
			} else{
				dir = yaffs_find_by_name(dir,str);

				dir = yaffsfs_FollowLink(dir,symDepth,loop);

				if(dir && dir->variant_type !=
					YAFFS_OBJECT_TYPE_DIRECTORY){
					if(notDir)
						*notDir = 1;
					dir = NULL;
				}

			}
		}
	}
	/* directory did not exist. */
	return NULL;
}

struct yaffs_obj *yaffsfs_FindDirectory(struct yaffs_obj *relDir,
					const YCHAR *path,
					YCHAR **name,
					int symDepth,
					int *notDir,
					int *loop)
{
	return yaffsfs_DoFindDirectory(relDir,path,name,symDepth,notDir,loop);
}

/*
 * yaffsfs_FindObject turns a path for an existing object into the object
 */
static struct yaffs_obj *yaffsfs_FindObject(struct yaffs_obj *relDir,
			const YCHAR *path,int symDepth, int getEquiv,
			struct yaffs_obj **dirOut, int *notDir,int *loop)
{
	//printf("in yaffsfs_FindObject \n");
        fflush (stdout);

	struct yaffs_obj *dir;
	struct yaffs_obj *obj;
	YCHAR *name;

	dir = yaffsfs_FindDirectory(relDir,path,&name,symDepth,notDir,loop);
	if (dir == NULL)	
	{
		printf ("Error yaffsfs_FindObject dir is null");
	}
	
	if(dirOut)
		*dirOut =  dir;

	if(dir && *name)
		{
			//printf ("find object by name called from yaffsfs_FindObject \n");
			obj = yaffs_find_by_name(dir,name);
		}
	else
		obj = dir;

	if(getEquiv)
		obj = yaffs_get_equivalent_obj(obj);
if (!obj)	printf("returning object null\n");
//else		printf("returning object not null\n");
	return obj;
}


/*************************************************************************
 *	Start of yaffsfs visible functions.
 *************************************************************************/

int yaffs_dup(int handle)
{
	int newHandleNumber = -1;
	yaffsfs_FileDes *existingFD = NULL;
	yaffsfs_Handle *existingHandle = NULL;
	yaffsfs_Handle *newHandle = NULL;

	yaffsfs_Lock();
	existingHandle = yaffsfs_HandleToPointer(handle);
	existingFD = yaffsfs_HandleToFileDes(handle);
	if(existingFD)
		newHandleNumber = yaffsfs_NewHandle(&newHandle);
	if(newHandle){
		newHandle->fdId = existingHandle->fdId;
		existingFD->handleCount++;
	}

	yaffsfs_Unlock();

	if(!existingFD)
		yaffsfs_SetError(-EBADF);
	else if (!newHandle)
		yaffsfs_SetError(-ENOMEM);

	return newHandleNumber;

}



int yaffs_open_sharing(const YCHAR *path, int oflag, int mode, int sharing)
{
        //printf("In yaffs_open_sharing\n");
	struct yaffs_obj *obj = NULL;
	struct yaffs_obj *dir = NULL;
	YCHAR *name;
	int handle = -1;
	yaffsfs_FileDes *fd = NULL;
	int openDenied = 0;
	int symDepth = 0;
	int errorReported = 0;
	int rwflags = oflag & ( O_RDWR | O_RDONLY | O_WRONLY);
	u8 shareRead = (sharing & YAFFS_SHARE_READ) ?  1 : 0;
	u8 shareWrite = (sharing & YAFFS_SHARE_WRITE) ? 1 : 0;
	u8 sharedReadAllowed;
	u8 sharedWriteAllowed;
	u8 alreadyReading;
	u8 alreadyWriting;
	u8 readRequested;
	u8 writeRequested;
	int notDir = 0;
	int loop = 0;

	if(!path) {
		yaffsfs_SetError(-EFAULT);
		printf("Error In yaffs_open_sharing Path Null\n");
		return -1;
	}
	

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		printf("Error In yaffs_open_sharing Name too long\n");
		return -1;
	}
	//printf ("yaffs_open_sharing :: Checkpath Done\n");
	/* O_EXCL only has meaning if O_CREAT is specified */
	if(!(oflag & O_CREAT))
		oflag &= ~(O_EXCL);

	/* O_TRUNC has no meaning if (O_CREAT | O_EXCL) is specified */
	if( (oflag & O_CREAT) & (oflag & O_EXCL))
		oflag &= ~(O_TRUNC);

	/* Todo: Are there any more flag combos to sanitise ? */

	/* Figure out if reading or writing is requested */

	readRequested = (rwflags == O_RDWR || rwflags == O_RDONLY) ? 1 : 0;
	writeRequested = (rwflags == O_RDWR || rwflags == O_WRONLY) ? 1 : 0;
	//printf("here\n");fflush(stdout);
	yaffsfs_Lock();
	//printf("Calling yaffsfs_NewHandleAndFileDes\n");
	handle = yaffsfs_NewHandleAndFileDes();
	//printf ("handle == %d\n", handle);
	if(handle < 0){
		yaffsfs_SetError(-ENFILE);
		errorReported = 1;
	} else {

		fd = yaffsfs_HandleToFileDes(handle);
		//printf("Position Initial = %d\n", fd->position);
		//printf("open_file_sharing :: file-descriptor returned %d\n", fd);
		/* try to find the exisiting object */
		obj = yaffsfs_FindObject(NULL,path,0,1,NULL,NULL,NULL);
		if(!obj) printf("obj null\n");
		obj = yaffsfs_FollowLink(obj,symDepth++,&loop);

		if(obj &&
			obj->variant_type != YAFFS_OBJECT_TYPE_FILE &&
			obj->variant_type != YAFFS_OBJECT_TYPE_DIRECTORY)
			obj = NULL;


		if(obj){

			/* The file already exists or it might be a directory */

			/* If it is a directory then we can't open it as a file */
			if(obj->variant_type == YAFFS_OBJECT_TYPE_DIRECTORY){
				openDenied = 1;
				yaffsfs_SetError(-EISDIR);
				errorReported = 1;
			}

			/* Open should fail if O_CREAT and O_EXCL are specified since
			 * the file exists
			 */
			if(!errorReported && (oflag & O_EXCL) && (oflag & O_CREAT)){
				openDenied = 1;
				yaffsfs_SetError(-EEXIST);
				errorReported = 1;
			}

			/* Check file permissions */
			if( readRequested && !(obj->yst_mode & S_IREAD))
				openDenied = 1;

			if( writeRequested && !(obj->yst_mode & S_IWRITE))
				openDenied = 1;

			if( !errorReported && writeRequested &&
				obj->my_dev->read_only){
				openDenied = 1;
				yaffsfs_SetError(-EROFS);
				errorReported = 1;
			}

			if(openDenied && !errorReported ) {
				/* Error if the file exists but permissions are refused. */
				yaffsfs_SetError(-EACCES);
				errorReported = 1;
			}

			/* Check sharing of an existing object. */
			if(!openDenied){
				yaffsfs_FileDes *fdx;
				int i;

				sharedReadAllowed = 1;
				sharedWriteAllowed = 1;
				alreadyReading = 0;
				alreadyWriting = 0;
				for( i = 0; i < YAFFSFS_N_HANDLES; i++){
					fdx = &yaffsfs_fd[i];
					if(fdx->handleCount > 0 &&
						fdx->inodeId >= 0 &&
						yaffsfs_inode[fdx->inodeId].iObj == obj){
						if(!fdx->shareRead)
							sharedReadAllowed = 0;
						if(!fdx->shareWrite)
							sharedWriteAllowed = 0;
						if(fdx->reading)
							alreadyReading = 1;
						if(fdx->writing)
							alreadyWriting = 1;
					}
				}



				if((!sharedReadAllowed && readRequested)||
					(!shareRead  && alreadyReading) ||
					(!sharedWriteAllowed && writeRequested) ||
					(!shareWrite && alreadyWriting)){
					openDenied = 1;
					yaffsfs_SetError(-EBUSY);
					errorReported=1;
				}
			}

		}

		/* If we could not open an existing object, then let's see if
		 * the directory exists. If not, error.
		 */
		if(!obj && !errorReported){
			dir = yaffsfs_FindDirectory(NULL,path,&name,0,&notDir,&loop);
			if (!dir)
				{
					printf("Dir not found\n");
				}
				else
				{
					;//printf("Dir found\n");
				}
			if(!dir && notDir){
				yaffsfs_SetError(-ENOTDIR);
				errorReported = 1;
			} else if(loop){
				yaffsfs_SetError(-ELOOP);
				errorReported = 1;
			} else	if(!dir){
				yaffsfs_SetError(-ENOENT);
				errorReported = 1;
			}
		}

		if(!obj && dir && !errorReported && (oflag & O_CREAT)) {
			//printf("file_open_sharing() :: trying to create the file \n");
			/* Let's see if we can create this file if it does not exist. */
			if(dir->my_dev->read_only){
				yaffsfs_SetError(-EROFS);
				errorReported = 1;
			} else
				obj = yaffs_create_file(dir,name,mode,0,0);

			if(!obj && !errorReported){
				yaffsfs_SetError(-ENOSPC);
				errorReported = 1;
			}
		}

		if(!obj && dir && !errorReported && !(oflag & O_CREAT)) {
			/* Error if the file does not exist and CREAT is not set. */
			yaffsfs_SetError(-ENOENT);
			errorReported = 1;
		}

		if(obj && !openDenied) {
			int inodeId = yaffsfs_GetInodeIdForObject(obj);

			if(inodeId<0) {
				/*
				 * Todo: Fix any problem if inodes run out, though that
				 * can't happen if the number of inode items >= number of handles.
				 */
			}

			fd->inodeId = inodeId;
			fd->reading = readRequested;
			fd->writing = writeRequested;
			fd->append =  (oflag & O_APPEND) ? 1 : 0;
			fd->position = 0;
			fd->shareRead = shareRead;
			fd->shareWrite = shareWrite;

			/* Hook inode to object */
                        obj->my_inode = (void*) &yaffsfs_inode[inodeId];

                        if((oflag & O_TRUNC) && fd->writing)
                                yaffs_resize_file(obj,0);
		} else {
			yaffsfs_PutHandle(handle);
			if(!errorReported)
				yaffsfs_SetError(0); /* Problem */
			handle = -1;
		}
	}

	yaffsfs_Unlock();

	return handle;
}

int yaffs_open(const YCHAR *path, int oflag, int mode)
{
	return yaffs_open_sharing(path, oflag, mode,
			YAFFS_SHARE_READ | YAFFS_SHARE_WRITE);
}

int yaffs_Dofsync(int handle,int datasync)
{
	int retVal = -1;
	struct yaffs_obj *obj;

	yaffsfs_Lock();

	obj = yaffsfs_HandleToObject(handle);

	if(!obj)
		yaffsfs_SetError(-EBADF);
	else if(obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else {
		yaffs_flush_file(obj,1,datasync);
		retVal = 0;
	}

	yaffsfs_Unlock();

	return retVal;
}

int yaffs_fsync(int handle)
{
	return yaffs_Dofsync(handle,0);
}

int yaffs_flush(int handle)
{
	return yaffs_fsync(handle);
}

int yaffs_fdatasync(int handle)
{
	return yaffs_Dofsync(handle,1);
}

int yaffs_close(int handle)
{
	yaffsfs_Handle *h = NULL;
	struct yaffs_obj *obj = NULL;
	int retVal = -1;

	yaffsfs_Lock();

	h = yaffsfs_HandleToPointer(handle);
	if (h == NULL) 
		printf ("handle NULL\n");

	obj = yaffsfs_HandleToObject(handle);
	if (obj == NULL)
		printf ("yaffs_close :: Object NULL\n");

	if(!h  || !obj)
		yaffsfs_SetError(-EBADF);
	else {
		/* clean up */
		yaffs_flush_file(obj,1,0);
		yaffsfs_PutHandle(handle);
		retVal = 0;
	}

	yaffsfs_Unlock();

	return retVal;
}

int yaffs_abort_close(int handle)
{
	yaffsfs_Handle *h = NULL;
	struct yaffs_obj *obj = NULL;
	int retVal = -1;

	yaffsfs_Lock();

	h = yaffsfs_HandleToPointer(handle);
	if (h == NULL) 
		printf ("handle NULL\n");

	obj = yaffsfs_HandleToObject(handle);
	if (obj == NULL)
		printf ("Object NULL\n");

	if(!h  || !obj)
		yaffsfs_SetError(-EBADF);
	else {
		/* clean up */
		yaffs_flush_file_abort(obj,1,0);
		yaffsfs_PutHandle(handle);
		retVal = 0;
	}

	yaffsfs_Unlock();

	return retVal;
}


// Pread is for reading from a specific offset of the file 
int yaffsfs_do_read(int handle, void *vbuf, unsigned int nbyte, int isPread, int offset)
{
	
	yaffsfs_FileDes *fd = NULL;
	struct yaffs_obj *obj = NULL;
	int pos = 0;
	int startPos = 0;
	int endPos = 0;
	int nRead = 0;
	int nToRead = 0;
	int totalRead = 0;
	unsigned int maxRead;
	u8 *buf = (u8 *)vbuf;
	//printf("nByte=%d\n", nbyte);
	if(!vbuf){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	yaffsfs_Lock();
	fd = yaffsfs_HandleToFileDes(handle);
	if (fd == NULL)
	{
		printf("Error :: yaffsfs_do_read :: fd NULL\n");	
	}
	obj = yaffsfs_HandleToObject(handle);
	if (obj == NULL)
	{
		printf("Error :: yaffsfs_do_read :: obj NULL");
	}
	//printf("Object Length = %d\n", yaffs_get_obj_length(obj));
	if(!fd || !obj){
		/* bad handle */
		yaffsfs_SetError(-EBADF);
		totalRead = -1;
	} else if(!fd->reading){
		/* Not a reading handle */
		yaffsfs_SetError(-EINVAL);
		totalRead = -1;
	} else if(nbyte > YAFFS_MAX_FILE_SIZE){
		yaffsfs_SetError(-EINVAL);
		totalRead = -1;
	} else {
		if(isPread)
			startPos = offset;
		else
			startPos = fd->position;
		//printf("In Yaffsfs_do_read, handle =%d, nbytes=%d, offset=%d\n", handle, nbyte, startPos);
		pos = startPos;

		if(yaffs_get_obj_length(obj) > pos)
			maxRead = yaffs_get_obj_length(obj) - pos;
		else
			maxRead = 0;

		if(nbyte > maxRead)
			nbyte = maxRead;


		yaffsfs_GetHandle(handle);

		endPos = pos + nbyte;

		if(pos < 0 || pos > YAFFS_MAX_FILE_SIZE ||
			nbyte > YAFFS_MAX_FILE_SIZE ||
			endPos < 0 || endPos > YAFFS_MAX_FILE_SIZE){
			totalRead = -1;
			nbyte = 0;
		}
		//printf("nByte=%d\n", nbyte);
		while(nbyte > 0) {
			nToRead = YAFFSFS_RW_SIZE - (pos & (YAFFSFS_RW_SIZE -1));
			if(nToRead > nbyte)
				nToRead = nbyte;

			/* Tricky bit...
			 * Need to reverify object in case the device was
			 * unmounted in another thread.
			 */
			obj = yaffsfs_HandleToObject(handle);
			if(!obj)
				nRead = 0;
			else
				nRead = yaffs_file_rd(obj,buf,pos,nToRead);

			if(nRead > 0){
				totalRead += nRead;
				pos += nRead;
				buf += nRead;
			}

			if(nRead == nToRead)
				nbyte-=nRead;
			else
				nbyte = 0; /* no more to read */


			if(nbyte > 0){
				yaffsfs_Unlock();
				yaffsfs_Lock();
			}

		}

		yaffsfs_PutHandle(handle);

		if(!isPread) {
			if(totalRead >= 0)
				fd->position = startPos + totalRead;
			else
				yaffsfs_SetError(-EINVAL);
		}

	}

	yaffsfs_Unlock();
	//printf("yaffs read done\n");
	return (totalRead >= 0) ? totalRead : -1;

}

int yaffs_read(int handle, void *buf, unsigned int nbyte)
{
	return yaffsfs_do_read(handle, buf, nbyte, 0, 0);
}

int yaffs_pread(int handle, void *buf, unsigned int nbyte, unsigned int offset)
{
	return yaffsfs_do_read(handle, buf, nbyte, 1, offset);
}


int yaffsfs_do_write(int handle, const void *vbuf, unsigned int nbyte, int isPwrite, int offset, unsigned int t_id)
{
	yaffsfs_FileDes *fd = NULL;
	struct yaffs_obj *obj = NULL;
	int pos = 0;
	int startPos = 0;
	int endPos;
	int nWritten = 0;
	int totalWritten = 0;
	// For checking we are writing the data chunk on the disk -- write_trhrough = 1
	int write_trhrough = 0;
	int nToWrite = 0;
	const u8 *buf = (const u8 *)vbuf;

	if(!vbuf){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	yaffsfs_Lock();
	fd = yaffsfs_HandleToFileDes(handle);
	obj = yaffsfs_HandleToObject(handle);

	if(!fd || !obj){
		/* bad handle */
		yaffsfs_SetError(-EBADF);
		totalWritten = -1;
	} else if(!fd->writing){
		yaffsfs_SetError(-EINVAL);
		totalWritten=-1;
	} else if(obj->my_dev->read_only){
		yaffsfs_SetError(-EROFS);
		totalWritten=-1;
	} else {
		if(fd->append)
			startPos = yaffs_get_obj_length(obj);
		else if(isPwrite)
			startPos = offset;
		else
		{
			startPos = fd->position;
//printf("HERE\n");
		}
//printf("POS =   %d\n", startPos);		
		yaffsfs_GetHandle(handle);
		pos = startPos;
		endPos = pos + nbyte;

		if(pos < 0 || pos > YAFFS_MAX_FILE_SIZE ||
			nbyte > YAFFS_MAX_FILE_SIZE ||
			endPos < 0 || endPos > YAFFS_MAX_FILE_SIZE){
			totalWritten = -1;
			nbyte = 0;
		}

		while(nbyte > 0) {

			nToWrite = YAFFSFS_RW_SIZE - (pos & (YAFFSFS_RW_SIZE -1));
			if(nToWrite > nbyte)
				nToWrite = nbyte;

			/* Tricky bit...
			 * Need to reverify object in case the device was
			 * remounted or unmounted in another thread.
			 */
			obj = yaffsfs_HandleToObject(handle);
			if(!obj || obj->my_dev->read_only)
				nWritten = 0;
			else
				nWritten = yaffs_wr_file(obj,buf,pos,nToWrite,
							write_trhrough, t_id);
			
			if(nWritten > 0){
				totalWritten += nWritten;
				pos += nWritten;
				buf += nWritten;
			}

			if(nWritten == nToWrite)
				nbyte -= nToWrite;
			else
				nbyte = 0;

			if(nWritten < 1 && totalWritten < 1){
				yaffsfs_SetError(-ENOSPC);
				totalWritten = -1;
			}

			if(nbyte > 0){
				yaffsfs_Unlock();
				yaffsfs_Lock();
			}
		}

		yaffsfs_PutHandle(handle);

		if(!isPwrite){
			if(totalWritten > 0)
				fd->position = startPos + totalWritten;
			else
				yaffsfs_SetError(-EINVAL);
		}
	}

	yaffsfs_Unlock();

	return (totalWritten >= 0) ? totalWritten : -1;
}

int yaffs_write(int fd, const void *buf, unsigned int nbyte, unsigned int t_id)
{
	return yaffsfs_do_write(fd, buf, nbyte, 0, 0, t_id);
}

int yaffs_pwrite(int fd, const void *buf, unsigned int nbyte, unsigned int offset, unsigned int t_id)
{
	return yaffsfs_do_write(fd, buf, nbyte, 1, offset, t_id);
}


int yaffs_truncate(const YCHAR *path,off_t new_size)
{
	struct yaffs_obj *obj = NULL;
	struct yaffs_obj *dir = NULL;
	int result = YAFFS_FAIL;
	int notDir = 0;
	int loop = 0;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1,&dir,&notDir,&loop);
	obj = yaffsfs_FollowLink(obj,0,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else if(obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else if(obj->variant_type != YAFFS_OBJECT_TYPE_FILE)
		yaffsfs_SetError(-EISDIR);
	else if(obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else if(new_size < 0 || new_size > YAFFS_MAX_FILE_SIZE)
		yaffsfs_SetError(-EINVAL);
	else
		result = yaffs_resize_file(obj,new_size);

	yaffsfs_Unlock();

	return (result) ? 0 : -1;
}

int yaffs_ftruncate(int handle, off_t new_size)
{
	yaffsfs_FileDes *fd = NULL;
	struct yaffs_obj *obj = NULL;
	int result = 0;

	yaffsfs_Lock();
	fd = yaffsfs_HandleToFileDes(handle);
	obj = yaffsfs_HandleToObject(handle);

	if(!fd || !obj)
		/* bad handle */
		yaffsfs_SetError(-EBADF);
	else if(!fd->writing)
		yaffsfs_SetError(-EINVAL);
	else if(obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else if( new_size < 0 || new_size > YAFFS_MAX_FILE_SIZE)
		yaffsfs_SetError(-EINVAL);
	else
		/* resize the file */
		result = yaffs_resize_file(obj,new_size);	// TODO Replace get_transaction_id() by t_id
	yaffsfs_Unlock();

	return (result) ? 0 : -1;

}

off_t yaffs_lseek(int handle, off_t offset, int whence)
{
	yaffsfs_FileDes *fd = NULL;
	struct yaffs_obj *obj = NULL;
	int pos = -1;
	int fSize = -1;

	yaffsfs_Lock();
	fd = yaffsfs_HandleToFileDes(handle);
	obj = yaffsfs_HandleToObject(handle);

	if(!fd || !obj)
		yaffsfs_SetError(-EBADF);
	else if(offset > YAFFS_MAX_FILE_SIZE)
		yaffsfs_SetError(-EINVAL);
	else {
		if(whence == SEEK_SET){
			if(offset >= 0)
				pos = offset;
		} else if(whence == SEEK_CUR) {
			if( (fd->position + offset) >= 0)
				pos = (fd->position + offset);
		} else if(whence == SEEK_END) {
			fSize = yaffs_get_obj_length(obj);
			if(fSize >= 0 && (fSize + offset) >= 0)
				pos = fSize + offset;
		}

		if(pos >= 0 && pos <= YAFFS_MAX_FILE_SIZE)
			fd->position = pos;
		else{
			yaffsfs_SetError(-EINVAL);
			pos = -1;
		}
	}

	yaffsfs_Unlock();

	return pos;
}


int yaffsfs_DoUnlink(const YCHAR *path, int isDirectory)
{
	struct yaffs_obj *dir = NULL;
	struct yaffs_obj *obj = NULL;
	YCHAR *name;
	int result = YAFFS_FAIL;
	int notDir = 0;
	int loop = 0;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,0,NULL,NULL,NULL);
	dir = yaffsfs_FindDirectory(NULL,path,&name,0,&notDir,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir)
		yaffsfs_SetError(-ENOENT);
	else if(strncmp(name,_Y("."),2) == 0)
		yaffsfs_SetError(-EINVAL);
	else if(!obj)
		yaffsfs_SetError(-ENOENT);
	else if(obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else if(!isDirectory && obj->variant_type == YAFFS_OBJECT_TYPE_DIRECTORY)
		yaffsfs_SetError(-EISDIR);
	else if(isDirectory && obj->variant_type != YAFFS_OBJECT_TYPE_DIRECTORY)
		yaffsfs_SetError(-ENOTDIR);
	else if(isDirectory && obj == obj->my_dev->root_dir)
		yaffsfs_SetError(-EBUSY); /* Can't rmdir a root */
	else {
		result = yaffs_unlinker(dir,name);

		if(result == YAFFS_FAIL && isDirectory)
			yaffsfs_SetError(-ENOTEMPTY);
	}

	yaffsfs_Unlock();

	return (result == YAFFS_FAIL) ? -1 : 0;
}


int yaffs_unlink(const YCHAR *path)
{
	return yaffsfs_DoUnlink(path,0);
}

int yaffs_rename(const YCHAR *oldPath, const YCHAR *newPath)
{
	struct yaffs_obj *olddir = NULL;
	struct yaffs_obj *newdir = NULL;
	struct yaffs_obj *obj = NULL;
	struct yaffs_obj *newobj = NULL;
	YCHAR *oldname;
	YCHAR *newname;
	int result= YAFFS_FAIL;
	int rename_allowed = 1;
	int notOldDir = 0;
	int notNewDir = 0;
	int oldLoop = 0;
	int newLoop = 0;

	YCHAR *alt_newpath=NULL;

	if(!oldPath || !newPath){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(oldPath) < 0 ||
		yaffsfs_CheckPath(newPath) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	if(yaffsfs_alt_dir_path(newPath, &alt_newpath) < 0){
		yaffsfs_SetError(-ENOMEM);
		return -1;
	}
	if(alt_newpath)
		newPath = alt_newpath;

	yaffsfs_Lock();


	olddir = yaffsfs_FindDirectory(NULL,oldPath,&oldname,0,&notOldDir,&oldLoop);
	newdir = yaffsfs_FindDirectory(NULL,newPath,&newname,0,&notNewDir,&newLoop);
	obj = yaffsfs_FindObject(NULL,oldPath,0,0,NULL,NULL,NULL);
	newobj = yaffsfs_FindObject(NULL,newPath,0,0,NULL,NULL,NULL);

	/* If the object being renamed is a directory and the
	 * path ended with a "/" then the olddir == obj.
	 * We pass through NULL for the old name to tell the lower layers
	 * to use olddir as the object.
	 */

	if(olddir == obj)
		oldname = NULL;

	if((!olddir && notOldDir) || (!newdir && notNewDir)) {
		yaffsfs_SetError(-ENOTDIR);
		rename_allowed = 0;
	} else if(oldLoop || newLoop) {
		yaffsfs_SetError(-ELOOP);
		rename_allowed = 0;
	} else if (olddir && oldname && strncmp(oldname, _Y("."),2) == 0){
		yaffsfs_SetError(-EINVAL);
		rename_allowed = 0;
	}else if(!olddir || !newdir || !obj) {
		yaffsfs_SetError(-ENOENT);
		rename_allowed = 0;
	} else if(obj->my_dev->read_only){
		yaffsfs_SetError(-EROFS);
		rename_allowed = 0;
	} else if(yaffs_is_non_empty_dir(newobj)){
		yaffsfs_SetError(-ENOTEMPTY);
		rename_allowed = 0;
	} else if(olddir->my_dev != newdir->my_dev) {
		/* Rename must be on same device */
		yaffsfs_SetError(-EXDEV);
		rename_allowed = 0;
	} else if(obj && obj->variant_type == YAFFS_OBJECT_TYPE_DIRECTORY) {
		/*
		 * It is a directory, check that it is not being renamed to
		 * being its own decendent.
		 * Do this by tracing from the new directory back to the root,
		 * checking for obj
		 */

		struct yaffs_obj *xx = newdir;

		while( rename_allowed && xx){
			if(xx == obj)
				rename_allowed = 0;
			xx = xx->parent;
		}
		if(!rename_allowed)
			yaffsfs_SetError(-EINVAL);
	}

	if(rename_allowed)
		result = yaffs_rename_obj(olddir,oldname,newdir,newname);

	yaffsfs_Unlock();

	if(alt_newpath)
		kfree(alt_newpath);

	return (result == YAFFS_FAIL) ? -1 : 0;
}


static int yaffsfs_DoStat(struct yaffs_obj *obj,struct yaffs_stat *buf)
{
	int retVal = -1;

	obj = yaffs_get_equivalent_obj(obj);

	if(obj && buf){
	    	buf->st_dev = (int)obj->my_dev->os_context;
	    	buf->st_ino = obj->obj_id;
	    	buf->st_mode = obj->yst_mode & ~S_IFMT; /* clear out file type bits */

	    	if(obj->variant_type == YAFFS_OBJECT_TYPE_DIRECTORY)
			buf->st_mode |= S_IFDIR;
		else if(obj->variant_type == YAFFS_OBJECT_TYPE_SYMLINK)
			buf->st_mode |= S_IFLNK;
		else if(obj->variant_type == YAFFS_OBJECT_TYPE_FILE)
			buf->st_mode |= S_IFREG;

	    	buf->st_nlink = yaffs_get_obj_link_count(obj);
	    	buf->st_uid = 0;
	    	buf->st_gid = 0;;
	    	buf->st_rdev = obj->yst_rdev;
	    	buf->st_size = yaffs_get_obj_length(obj);
	    	buf->st_blksize = obj->my_dev->data_bytes_per_chunk;
	    	buf->st_blocks = (buf->st_size + buf->st_blksize -1)/buf->st_blksize;
#if CONFIG_YAFFS_WINCE
		buf->yst_wince_atime[0] = obj->win_atime[0];
		buf->yst_wince_atime[1] = obj->win_atime[1];
		buf->yst_wince_ctime[0] = obj->win_ctime[0];
		buf->yst_wince_ctime[1] = obj->win_ctime[1];
		buf->yst_wince_mtime[0] = obj->win_mtime[0];
		buf->yst_wince_mtime[1] = obj->win_mtime[1];
#else
    		buf->yst_atime = obj->yst_atime;
	    	buf->yst_ctime = obj->yst_ctime;
	    	buf->yst_mtime = obj->yst_mtime;
#endif
		retVal = 0;
	}
	return retVal;
}

static int yaffsfs_DoStatOrLStat(const YCHAR *path, struct yaffs_stat *buf,int doLStat)
{
        //printf("in yaffsfs_DoStatOrLStat\n");
	struct yaffs_obj *obj=NULL;
	struct yaffs_obj *dir=NULL;
	int retVal = -1;
	int notDir = 0;
	int loop = 0;

	if(!path || !buf){
		printf("Error :: yaffsfs_DoStatOrLStat either path or buffer is null\n");
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		printf("Error :: yaffsfs_DoStatOrLStat yaffsfs_checkpath returns -1\n");
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1,&dir,&notDir,&loop);
        if (obj == NULL)
	{
		printf ("Error :: yaffsfs_DoStatOrLStat obj is NULL\n");
	}

	if(!doLStat && obj)
		obj = yaffsfs_FollowLink(obj,0,&loop);

	if(!dir && notDir)
		{
			printf ("Error :: yaffsfs_DoStatOrLStat directory not found\n");
			yaffsfs_SetError(-ENOTDIR);
		}
	else if(loop)
		{
			printf ("Error :: yaffsfs_DoStatOrLStat loop found in the path\n");
			yaffsfs_SetError(-ELOOP);
		}
	else if(!dir || !obj)
		{
			printf ("Error :: yaffsfs_DoStatOrLStat directory or object null\n");
			yaffsfs_SetError(-ENOENT);
		}
	else
		retVal = yaffsfs_DoStat(obj,buf);

	yaffsfs_Unlock();

	return retVal;

}

int yaffs_stat(const YCHAR *path, struct yaffs_stat *buf)
{
	//printf("yaffs_stat\n");
	return yaffsfs_DoStatOrLStat(path,buf,0);
}

int yaffs_lstat(const YCHAR *path, struct yaffs_stat *buf)
{
	return yaffsfs_DoStatOrLStat(path,buf,1);
}

int yaffs_fstat(int fd, struct yaffs_stat *buf)
{
	struct yaffs_obj *obj;

	int retVal = -1;

	if(!buf){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(obj)
		retVal = yaffsfs_DoStat(obj,buf);
	else
		/* bad handle */
		yaffsfs_SetError(-EBADF);

	yaffsfs_Unlock();

	return retVal;
}

static int yaffsfs_DoUtime(struct yaffs_obj *obj,const struct yaffs_utimbuf *buf)
{
	int retVal = -1;
	int result;

	struct yaffs_utimbuf local;

	obj = yaffs_get_equivalent_obj(obj);

	if(obj && obj->my_dev->read_only) {
		yaffsfs_SetError(-EROFS);
		return -1;
	}


	if(!buf){
		local.actime = Y_CURRENT_TIME;
		local.modtime = local.actime;
		buf = &local;
	}

	if(obj){
		obj->yst_atime = buf->actime;
		obj->yst_mtime = buf->modtime;
		obj->dirty = 1;
		result = yaffs_flush_file(obj,0,0);
		retVal = result == YAFFS_OK ? 0 : -1;
	}

	return retVal;
}

int yaffs_utime(const YCHAR *path, const struct yaffs_utimbuf *buf)
{
	struct yaffs_obj *obj=NULL;
	struct yaffs_obj *dir=NULL;
	int retVal = -1;
	int notDir = 0;
	int loop = 0;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1,&dir,&notDir,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else
		retVal = yaffsfs_DoUtime(obj,buf);

	yaffsfs_Unlock();

	return retVal;

}
int yaffs_futime(int fd, const struct yaffs_utimbuf *buf)
{
	struct yaffs_obj *obj;

	int retVal = -1;

	yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(obj)
		retVal = yaffsfs_DoUtime(obj,buf);
	else
		/* bad handle */
		yaffsfs_SetError(-EBADF);

	yaffsfs_Unlock();

	return retVal;
}


#ifndef CONFIG_YAFFS_WINCE
/* xattrib functions */


static int yaffs_do_setxattr(const YCHAR *path, const char *name,
			const void *data, int size, int flags, int follow)
{
	struct yaffs_obj *obj;
	struct yaffs_obj *dir;
	int notDir = 0;
	int loop = 0;

	int retVal = -1;

	if(!path || !name || !data){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1,&dir,&notDir,&loop);

	if(follow)
		obj = yaffsfs_FollowLink(obj,0,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else {
		retVal = yaffs_set_xattrib(obj,name,data,size,flags);
		if(retVal< 0){
			yaffsfs_SetError(retVal);
			retVal = -1;
		}
	}

	yaffsfs_Unlock();

	return retVal;

}

int yaffs_setxattr(const YCHAR *path, const char *name, const void *data, int size, int flags)
{
	return yaffs_do_setxattr(path, name, data, size, flags, 1);
}

int yaffs_lsetxattr(const YCHAR *path, const char *name, const void *data, int size, int flags)
{
	return yaffs_do_setxattr(path, name, data, size, flags, 0);
}



int yaffs_fsetxattr(int fd, const char *name, const void *data, int size, int flags)
{
	struct yaffs_obj *obj;

	int retVal = -1;

	if(!name || !data){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(!obj)
		yaffsfs_SetError(-EBADF);
	else {
		retVal = yaffs_set_xattrib(obj,name,data,size,flags);
		if(retVal< 0){
			yaffsfs_SetError(retVal);
			retVal = -1;
		}
	}

	yaffsfs_Unlock();

	return retVal;
}

static int yaffs_do_getxattr(const YCHAR *path, const char *name, void *data, int size, int follow)
{
	struct yaffs_obj *obj;
	struct yaffs_obj *dir;
	int retVal = -1;
	int notDir = 0;
	int loop = 0;

	if(!path || !name || !data ){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1,&dir,&notDir,&loop);

	if(follow)
		obj = yaffsfs_FollowLink(obj,0,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else {
		retVal = yaffs_get_xattrib(obj,name,data,size);
		if(retVal< 0){
			yaffsfs_SetError(retVal);
			retVal = -1;
		}
	}
	yaffsfs_Unlock();

	return retVal;

}

int yaffs_getxattr(const YCHAR *path, const char *name, void *data, int size)
{
	return yaffs_do_getxattr( path, name, data, size, 1);
}
int yaffs_lgetxattr(const YCHAR *path, const char *name, void *data, int size)
{
	return yaffs_do_getxattr( path, name, data, size, 0);
}



int yaffs_fgetxattr(int fd, const char *name, void *data, int size)
{
	struct yaffs_obj *obj;

	int retVal = -1;

	if(!name || !data ){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(obj) {
		retVal = yaffs_get_xattrib(obj,name,data,size);
		if(retVal< 0){
			yaffsfs_SetError(retVal);
			retVal = -1;
		}
	} else
		/* bad handle */
		yaffsfs_SetError(-EBADF);

	yaffsfs_Unlock();

	return retVal;
}

static int yaffs_do_listxattr(const YCHAR *path, char *data, int size, int follow)
{
	struct yaffs_obj *obj=NULL;
	struct yaffs_obj *dir=NULL;
	int retVal = -1;
	int notDir = 0;
	int loop = 0;

	if(!path || !data ){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1,&dir,&notDir,&loop);

	if(follow)
		obj = yaffsfs_FollowLink(obj,0,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else {
		retVal = yaffs_list_xattrib(obj, data,size);
		if(retVal< 0){
			yaffsfs_SetError(retVal);
			retVal = -1;
		}
	}

	yaffsfs_Unlock();

	return retVal;

}

int yaffs_listxattr(const YCHAR *path, char *data, int size)
{
	return yaffs_do_listxattr(path, data, size, 1);
}

int yaffs_llistxattr(const YCHAR *path, char *data, int size)
{
	return yaffs_do_listxattr(path, data, size, 0);
}

int yaffs_flistxattr(int fd, char *data, int size)
{
	struct yaffs_obj *obj;

	int retVal = -1;

	if(!data ){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(obj) {
		retVal = yaffs_list_xattrib(obj,data,size);
		if(retVal< 0){
			yaffsfs_SetError(retVal);
			retVal = -1;
		}
	} else
		/* bad handle */
		yaffsfs_SetError(-EBADF);

	yaffsfs_Unlock();

	return retVal;
}

static int yaffs_do_removexattr(const YCHAR *path, const char *name, int follow)
{
	struct yaffs_obj *obj=NULL;
	struct yaffs_obj *dir=NULL;
	int notDir = 0;
	int loop = 0;
	int retVal = -1;

	if(!path || !name){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1, &dir,&notDir,&loop);

	if(follow)
		obj = yaffsfs_FollowLink(obj,0,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else {
		retVal = yaffs_remove_xattrib(obj,name);
		if(retVal< 0){
			yaffsfs_SetError(retVal);
			retVal = -1;
		}
	}

	yaffsfs_Unlock();

	return retVal;

}

int yaffs_removexattr(const YCHAR *path, const char *name)
{
	return yaffs_do_removexattr(path, name, 1);
}

int yaffs_lremovexattr(const YCHAR *path, const char *name)
{
	return yaffs_do_removexattr(path, name, 0);
}

int yaffs_fremovexattr(int fd, const char *name)
{
	struct yaffs_obj *obj;

	int retVal = -1;

	if(!name){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(obj){
		retVal = yaffs_remove_xattrib(obj,name);
		if(retVal< 0){
			yaffsfs_SetError(retVal);
			retVal = -1;
		}
	}else
		/* bad handle */
		yaffsfs_SetError(-EBADF);

	yaffsfs_Unlock();

	return retVal;
}
#endif

#ifdef CONFIG_YAFFS_WINCE
int yaffs_get_wince_times(int fd, unsigned *wctime, unsigned *watime, unsigned *wmtime)
{
	struct yaffs_obj *obj;

	int retVal = -1;

	yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(obj){

		if(wctime){
			wctime[0] = obj->win_ctime[0];
			wctime[1] = obj->win_ctime[1];
		}
		if(watime){
			watime[0] = obj->win_atime[0];
			watime[1] = obj->win_atime[1];
		}
		if(wmtime){
			wmtime[0] = obj->win_mtime[0];
			wmtime[1] = obj->win_mtime[1];
		}


		retVal = 0;
	} else
		/*  bad handle */
		yaffsfs_SetError(-EBADF);

	yaffsfs_Unlock();

	return retVal;
}


int yaffs_set_wince_times(int fd,
						  const unsigned *wctime,
						  const unsigned *watime,
                                                  const unsigned *wmtime)
{
        struct yaffs_obj *obj;
        int result;
        int retVal = -1;

        yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(obj){

		if(wctime){
			obj->win_ctime[0] = wctime[0];
			obj->win_ctime[1] = wctime[1];
		}
		if(watime){
                        obj->win_atime[0] = watime[0];
                        obj->win_atime[1] = watime[1];
                }
                if(wmtime){
                        obj->win_mtime[0] = wmtime[0];
                        obj->win_mtime[1] = wmtime[1];
                }

                obj->dirty = 1;
                result = yaffs_flush_file(obj,0,0);
                retVal = 0;
        } else
		/* bad handle */
		yaffsfs_SetError(-EBADF);

	yaffsfs_Unlock();

	return retVal;
}

#endif


static int yaffsfs_DoChMod(struct yaffs_obj *obj,mode_t mode)
{
	int result = -1;

	if(obj)
		obj = yaffs_get_equivalent_obj(obj);

	if(obj) {
		obj->yst_mode = mode;
		obj->dirty = 1;
		result = yaffs_flush_file(obj,0,0);
	}

	return result == YAFFS_OK ? 0 : -1;
}


int yaffs_access(const YCHAR *path, int amode)
{
	struct yaffs_obj *obj=NULL;
	struct yaffs_obj *dir=NULL;
	int notDir = 0;
	int loop = 0;
	int retval = -1;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	if(amode & ~(R_OK | W_OK | X_OK)){
		yaffsfs_SetError(-EINVAL);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1, &dir,&notDir,&loop);
	obj = yaffsfs_FollowLink(obj,0,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else if((amode & W_OK) && obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else{
		int access_ok = 1;

		if((amode & R_OK) && !(obj->yst_mode & S_IREAD))
			access_ok = 0;
		if((amode & W_OK) && !(obj->yst_mode & S_IWRITE))
			access_ok = 0;
		if((amode & X_OK) && !(obj->yst_mode & S_IEXEC))
			access_ok = 0;

		if(!access_ok)
			yaffsfs_SetError(-EACCES);
		else
			retval = 0;
	}

	yaffsfs_Unlock();

	return retval;

}


int yaffs_chmod(const YCHAR *path, mode_t mode)
{
	struct yaffs_obj *obj=NULL;
	struct yaffs_obj *dir=NULL;
	int retVal = -1;
	int notDir = 0;
	int loop = 0;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	if(mode & ~(0777)){
		yaffsfs_SetError(-EINVAL);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1, &dir, &notDir,&loop);
	obj = yaffsfs_FollowLink(obj,0,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else if(obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else
		retVal = yaffsfs_DoChMod(obj,mode);

	yaffsfs_Unlock();

	return retVal;

}


int yaffs_fchmod(int fd, mode_t mode)
{
	struct yaffs_obj *obj;
	int retVal = -1;

	if(mode & ~(0777)){
		yaffsfs_SetError(-EINVAL);
		return -1;
	}

	yaffsfs_Lock();
	obj = yaffsfs_HandleToObject(fd);

	if(!obj)
		yaffsfs_SetError(-EBADF);
	else if(obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else
		retVal = yaffsfs_DoChMod(obj,mode);

	yaffsfs_Unlock();

	return retVal;
}

int yaffs_mkdir(const YCHAR *path, mode_t mode)
{
	struct yaffs_obj *parent = NULL;
	struct yaffs_obj *dir = NULL;
	YCHAR *name;
	YCHAR *alt_path = NULL;
	int retVal= -1;
	int notDir = 0;
	int loop = 0;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	if(yaffsfs_alt_dir_path(path, &alt_path) < 0){
		yaffsfs_SetError(-ENOMEM);
		return -1;
	}
	if(alt_path)
		path = alt_path;

	yaffsfs_Lock();
	parent = yaffsfs_FindDirectory(NULL,path,&name,0,&notDir,&loop);
	if(!parent && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!parent)
		yaffsfs_SetError(-ENOENT);
	else if(strnlen(name,5) == 0){
		/* Trying to make the root itself */
		yaffsfs_SetError(-EEXIST);
	} else if(parent->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else {
		dir = yaffs_create_dir(parent,name,mode,0,0);
		if(dir)
			retVal = 0;
		else if (yaffs_find_by_name(parent,name))
			yaffsfs_SetError(-EEXIST); /* the name already exists */
		else
			yaffsfs_SetError(-ENOSPC); /* just assume no space */
	}

	yaffsfs_Unlock();

	if(alt_path)
		kfree(alt_path);

	return retVal;
}

int yaffs_rmdir(const YCHAR *path)
{
	int result;
	YCHAR *alt_path;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	if(yaffsfs_alt_dir_path(path, &alt_path) < 0){
		yaffsfs_SetError(-ENOMEM);
		return -1;
	}
	if(alt_path)
		path = alt_path;
	result =  yaffsfs_DoUnlink(path,1);
	if(alt_path)
		kfree(alt_path);
	return result;
}


void * yaffs_getdev(const YCHAR *path)
{
	struct yaffs_dev *dev=NULL;
	YCHAR *dummy;
	dev = yaffsfs_FindDevice(path,&dummy);
	return (void *)dev;
}

int yaffs_mount2(const YCHAR *path,int read_only)
{
	log_init();
	result_log_init();
	char *output;	
	int retVal=-1;
	int result=YAFFS_FAIL;
	struct yaffs_dev *dev=NULL;
	set_transaction_id(1);
	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}


	yaffs_trace(YAFFS_TRACE_MOUNT,"yaffs: Mounting %s",path);


	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	yaffsfs_InitHandles();

	dev = yaffsfs_FindMountPoint(path);
		//printf("yflash2_GetNumberOfBlocks= %d\n", yflash2_GetNumberOfBlocks());
	//printf("dev->param.end_block= %d\n", dev->param.end_block);
	
	if(dev){
		if(!dev->is_mounted){
			dev->read_only = read_only ? 1 : 0;
			result = yaffs_guts_initialise(dev);

				
			if(result == YAFFS_FAIL)
				yaffsfs_SetError(-ENOMEM);
			retVal = result ? 0 : -1;

		}
		else
			yaffsfs_SetError(-EBUSY);
	} else
		yaffsfs_SetError(-ENODEV);

	yaffsfs_Unlock();
// The scanning is done, now we are trying to sync the bitmap file with the on disk bitmap 
if (rollback_type == YAFFS_COW)
        {
        	if (transaction_support) 
		{
	        if (sync_transaction_enable)
			{
				bool r_sync = yaffs_sync_transaction_bitmap (path);
				if (!r_sync)
					{
					output = (char *)(malloc(50));
					sprintf(output, "BUG : yaffs_mount2 : sync unsuccessful");
					fprintf(stderr, "BUG : yaffs_mount2 : sync unsuccessful\n");				
        				log_output (output);
					free (output);
					}
				else 
				     {
					output = (char *)(malloc(50));
					sprintf(output, "Sync Done Successfully from yaffs_mount2");
        				log_output (output);
					free (output);
				     }
			}	
		}
			print_committed_transactions();
	}
        else if (rollback_type == EXT4_JOURNAL)
	{
	       bool journalInit = initialize_journal (path);
	       if (!(journalInit))
	       {
					output = (char *)(malloc(50));
					sprintf(output, "BUG : yaffs_mount2 : journal init unsuccessful");
					fprintf(stderr, "BUG : yaffs_mount2 : journal init unsuccessful\n");				
        				log_output (output);
					free (output);
	       }
	}

	return retVal;

}

int yaffs_mount(const YCHAR *path)
{
	return yaffs_mount2(path,0);
}

int yaffs_sync(const YCHAR *path)
{
        int retVal=-1;
        struct yaffs_dev *dev=NULL;
        YCHAR *dummy;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

        yaffsfs_Lock();
        dev = yaffsfs_FindDevice(path,&dummy);
        if(dev){
                if(!dev->is_mounted)
			yaffsfs_SetError(-EINVAL);
		else if(dev->read_only)
			yaffsfs_SetError(-EROFS);
		else {

                        yaffs_flush_whole_cache(dev);
                        yaffs_checkpoint_save(dev);
                        retVal = 0;

                }
        }else
                yaffsfs_SetError(-ENODEV);

        yaffsfs_Unlock();
        return retVal;
}


static int yaffsfs_IsDevBusy(struct yaffs_dev * dev)
{
	int i;
	struct yaffs_obj *obj;

	for(i = 0; i < YAFFSFS_N_HANDLES; i++){
		obj = yaffsfs_HandleToObject(i);
		if(obj && obj->my_dev == dev)
		return 1;
	}
	return 0;
}


int yaffs_remount(const YCHAR *path, int force, int read_only)
{
        int retVal=-1;
	struct yaffs_dev *dev=NULL;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();
	dev = yaffsfs_FindMountPoint(path);
	if(dev){
		if(dev->is_mounted){
			yaffs_flush_whole_cache(dev);

			if(force || ! yaffsfs_IsDevBusy(dev)){
				if(read_only)
					yaffs_checkpoint_save(dev);
				dev->read_only =  read_only ? 1 : 0;
				retVal = 0;
			} else
				yaffsfs_SetError(-EBUSY);

		} else
			yaffsfs_SetError(-EINVAL);

	}
	else
		yaffsfs_SetError(-ENODEV);

	yaffsfs_Unlock();
	return retVal;

}

int yaffs_unmount2(const YCHAR *path, int force)
{
        int retVal=-1;
	struct yaffs_dev *dev=NULL;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();
	dev = yaffsfs_FindMountPoint(path);
	if(dev){
		if(dev->is_mounted){
			int inUse;
			yaffs_flush_whole_cache(dev);
			yaffs_checkpoint_save(dev);
			inUse = yaffsfs_IsDevBusy(dev);
			if(!inUse || force){
				if(inUse)
					yaffsfs_BreakDeviceHandles(dev);
				yaffs_deinitialise(dev);

				retVal = 0;
			} else
				yaffsfs_SetError(-EBUSY);

		} else
			yaffsfs_SetError(-EINVAL);

	} else
		yaffsfs_SetError(-ENODEV);

//	yaffs_wr_transaction_bitmap ();
	yaffsfs_Unlock();
	return retVal;

}

int yaffs_unmount(const YCHAR *path)
{
	return yaffs_unmount2(path,0);
}

loff_t yaffs_freespace(const YCHAR *path)
{
	loff_t retVal=-1;
	struct yaffs_dev *dev=NULL;
	YCHAR *dummy;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();
	dev = yaffsfs_FindDevice(path,&dummy);
	if(dev  && dev->is_mounted){
		retVal = yaffs_get_n_free_chunks(dev);
		retVal *= dev->data_bytes_per_chunk;

	} else
		yaffsfs_SetError(-EINVAL);

	yaffsfs_Unlock();
	return retVal;
}

loff_t yaffs_totalspace(const YCHAR *path)
{
	loff_t retVal=-1;
	struct yaffs_dev *dev=NULL;
	YCHAR *dummy;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();
	dev = yaffsfs_FindDevice(path,&dummy);
	if(dev  && dev->is_mounted){
		retVal = (dev->param.end_block - dev->param.start_block + 1) -
			dev->param.n_reserved_blocks;
		retVal *= dev->param.chunks_per_block;
		retVal *= dev->data_bytes_per_chunk;

	} else
		yaffsfs_SetError(-EINVAL);

	yaffsfs_Unlock();
	return retVal;
}

int yaffs_inodecount(const YCHAR *path)
{
	loff_t retVal= -1;
	struct yaffs_dev *dev=NULL;
	YCHAR *dummy;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();
	dev = yaffsfs_FindDevice(path,&dummy);
	if(dev  && dev->is_mounted) {
	   int n_obj = dev->n_obj;
	   if(n_obj > dev->n_hardlinks)
		retVal = n_obj - dev->n_hardlinks;
	}

	if(retVal < 0)
		yaffsfs_SetError(-EINVAL);

	yaffsfs_Unlock();
	return retVal;
}


void yaffs_add_device(struct yaffs_dev *dev)
{
	dev->is_mounted = 0;
	dev->param.remove_obj_fn = yaffsfs_RemoveObjectCallback;

	if(!dev->dev_list.next)
		INIT_LIST_HEAD(&dev->dev_list);

	list_add(&dev->dev_list,&yaffsfs_deviceList);
//	printf("Yaffs Add Device %s\n", dev->param.name_);
}

void yaffs_remove_device(struct yaffs_dev *dev)
{
	list_del_init(&dev->dev_list);
}




/* Directory search stuff. */

/*
 * Directory search context
 *
 * NB this is an opaque structure.
 */


typedef struct
{
	u32 magic;
	yaffs_dirent de;		/* directory entry being used by this dsc */
	YCHAR name[NAME_MAX+1];		/* name of directory being searched */
        struct yaffs_obj *dirObj;           /* ptr to directory being searched */
        struct yaffs_obj *nextReturn;       /* obj to be returned by next readddir */
        int offset;
        struct list_head others;
} yaffsfs_DirectorySearchContext;



static struct list_head search_contexts;


static void yaffsfs_SetDirRewound(yaffsfs_DirectorySearchContext *dsc)
{
	if(dsc &&
	   dsc->dirObj &&
	   dsc->dirObj->variant_type == YAFFS_OBJECT_TYPE_DIRECTORY){

           dsc->offset = 0;

           if( list_empty(&dsc->dirObj->variant.dir_variant.children))
                dsc->nextReturn = NULL;
           else
                dsc->nextReturn = list_entry(dsc->dirObj->variant.dir_variant.children.next,
                                                struct yaffs_obj,siblings);
        } else {
		/* Hey someone isn't playing nice! */
	}
}

static void yaffsfs_DirAdvance(yaffsfs_DirectorySearchContext *dsc)
{
	if(dsc &&
	   dsc->dirObj &&
           dsc->dirObj->variant_type == YAFFS_OBJECT_TYPE_DIRECTORY){

           if( dsc->nextReturn == NULL ||
               list_empty(&dsc->dirObj->variant.dir_variant.children))
                dsc->nextReturn = NULL;
           else {
                   struct list_head *next = dsc->nextReturn->siblings.next;

                   if( next == &dsc->dirObj->variant.dir_variant.children)
                        dsc->nextReturn = NULL; /* end of list */
                   else
                        dsc->nextReturn = list_entry(next,struct yaffs_obj,siblings);
           }
        } else {
                /* Hey someone isn't playing nice! */
	}
}

static void yaffsfs_RemoveObjectCallback(struct yaffs_obj *obj)
{

        struct list_head *i;
        yaffsfs_DirectorySearchContext *dsc;

        /* if search contexts not initilised then skip */
        if(!search_contexts.next)
                return;

        /* Iterate through the directory search contexts.
         * If any are the one being removed, then advance the dsc to
         * the next one to prevent a hanging ptr.
         */
         list_for_each(i, &search_contexts) {
                if (i) {
                        dsc = list_entry(i, yaffsfs_DirectorySearchContext,others);
                        if(dsc->nextReturn == obj)
                                yaffsfs_DirAdvance(dsc);
                }
	}

}

yaffs_DIR *yaffs_opendir(const YCHAR *dirname)
{
	yaffs_DIR *dir = NULL;
 	struct yaffs_obj *obj = NULL;
	yaffsfs_DirectorySearchContext *dsc = NULL;
	int notDir = 0;
	int loop = 0;

	if(!dirname){
		yaffsfs_SetError(-EFAULT);
		return NULL;
	}

	if(yaffsfs_CheckPath(dirname) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return NULL;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,dirname,0,1,NULL,&notDir,&loop);

	if(!obj && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!obj)
		yaffsfs_SetError(-ENOENT);
	else if(obj->variant_type != YAFFS_OBJECT_TYPE_DIRECTORY)
		yaffsfs_SetError(-ENOTDIR);
	else {

		dsc = kmalloc(sizeof(yaffsfs_DirectorySearchContext), 0);
		dir = (yaffs_DIR *)dsc;

		if(dsc){
			memset(dsc,0,sizeof(yaffsfs_DirectorySearchContext));
                        dsc->magic = YAFFS_MAGIC;
                        dsc->dirObj = obj;
                        strncpy(dsc->name,dirname,NAME_MAX);
                        INIT_LIST_HEAD(&dsc->others);

                        if(!search_contexts.next)
                                INIT_LIST_HEAD(&search_contexts);

                        list_add(&dsc->others,&search_contexts);
                        yaffsfs_SetDirRewound(dsc);
		}

        }

	yaffsfs_Unlock();

	return dir;
}

struct yaffs_dirent *yaffs_readdir(yaffs_DIR *dirp)
{
	yaffsfs_DirectorySearchContext *dsc = (yaffsfs_DirectorySearchContext *)dirp;
	struct yaffs_dirent *retVal = NULL;

	yaffsfs_Lock();

	if(dsc && dsc->magic == YAFFS_MAGIC){
		yaffsfs_SetError(0);
		if(dsc->nextReturn){
			dsc->de.d_ino = yaffs_get_equivalent_obj(dsc->nextReturn)->obj_id;
			dsc->de.d_dont_use = (unsigned)dsc->nextReturn;
			dsc->de.d_off = dsc->offset++;
			yaffs_get_obj_name(dsc->nextReturn,dsc->de.d_name,NAME_MAX);
			if(strnlen(dsc->de.d_name,NAME_MAX+1) == 0)
			{
				/* this should not happen! */
				strcpy(dsc->de.d_name,_Y("zz"));
			}
			dsc->de.d_reclen = sizeof(struct yaffs_dirent);
			retVal = &dsc->de;
			yaffsfs_DirAdvance(dsc);
		} else
			retVal = NULL;
	} else
		yaffsfs_SetError(-EBADF);

	yaffsfs_Unlock();

	return retVal;

}


void yaffs_rewinddir(yaffs_DIR *dirp)
{
	yaffsfs_DirectorySearchContext *dsc = (yaffsfs_DirectorySearchContext *)dirp;

	yaffsfs_Lock();

	yaffsfs_SetDirRewound(dsc);

	yaffsfs_Unlock();
}


int yaffs_closedir(yaffs_DIR *dirp)
{
	yaffsfs_DirectorySearchContext *dsc = (yaffsfs_DirectorySearchContext *)dirp;

	if(!dsc){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

        yaffsfs_Lock();
        dsc->magic = 0;
        list_del(&dsc->others); /* unhook from list */
        kfree(dsc);
        yaffsfs_Unlock();
        return 0;
}

/* End of directory stuff */


int yaffs_symlink(const YCHAR *oldpath, const YCHAR *newpath)
{
	struct yaffs_obj *parent = NULL;
	struct yaffs_obj *obj;
	YCHAR *name;
	int retVal= -1;
	int mode = 0; /* ignore for now */
	int notDir = 0;
	int loop = 0;

	if(!oldpath || !newpath){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(newpath) < 0 ||
		yaffsfs_CheckPath(oldpath) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();
	parent = yaffsfs_FindDirectory(NULL,newpath,&name,0,&notDir,&loop);
	if(!parent && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if( !parent || strnlen(name,5) < 1)
		yaffsfs_SetError(-ENOENT);
	else if(parent->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else if(parent){
		obj = yaffs_create_symlink(parent,name,mode,0,0,oldpath);
		if(obj)
			retVal = 0;
		else if (yaffsfs_FindObject(NULL,newpath,0,0, NULL,NULL,NULL))
			yaffsfs_SetError(-EEXIST);
		else
			yaffsfs_SetError(-ENOSPC);
	}

	yaffsfs_Unlock();

	return retVal;

}

int yaffs_readlink(const YCHAR *path, YCHAR *buf, int bufsiz)
{
	struct yaffs_obj *obj = NULL;
	struct yaffs_obj *dir = NULL;
	int retVal= -1;
	int notDir = 0;
	int loop = 0;

	if(!path || !buf){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,path,0,1, &dir,&notDir,&loop);

	if(!dir && notDir)
		yaffsfs_SetError(-ENOTDIR);
	else if(loop)
		yaffsfs_SetError(-ELOOP);
	else if(!dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else if(obj->variant_type != YAFFS_OBJECT_TYPE_SYMLINK)
		yaffsfs_SetError(-EINVAL);
	else {
		YCHAR *alias = obj->variant.symlink_variant.alias;
		memset(buf,0,bufsiz);
		strncpy(buf,alias,bufsiz - 1);
		retVal = 0;
	}
	yaffsfs_Unlock();
	return retVal;
}

int yaffs_link(const YCHAR *oldpath, const YCHAR *linkpath)
{
	/* Creates a link called newpath to existing oldpath */
	struct yaffs_obj *obj = NULL;
	struct yaffs_obj *lnk = NULL;
	struct yaffs_obj *obj_dir = NULL;
	struct yaffs_obj *lnk_dir = NULL;
	int retVal = -1;
	int notDirObj = 0;
	int notDirLnk = 0;
	int objLoop = 0;
	int lnkLoop = 0;
	YCHAR *newname;

	if(!oldpath || !linkpath){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(linkpath) < 0 ||
		yaffsfs_CheckPath(oldpath) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	yaffsfs_Lock();

	obj = yaffsfs_FindObject(NULL,oldpath,0,1,&obj_dir,&notDirObj,&objLoop);
	lnk = yaffsfs_FindObject(NULL,linkpath,0,0,NULL,NULL,NULL);
	lnk_dir = yaffsfs_FindDirectory(NULL,linkpath,&newname,0,&notDirLnk,&lnkLoop);

	if((!obj_dir && notDirObj) || (!lnk_dir && notDirLnk))
		yaffsfs_SetError(-ENOTDIR);
	else if(objLoop || lnkLoop)
		yaffsfs_SetError(-ELOOP);
	else if(!obj_dir || !lnk_dir || !obj)
		yaffsfs_SetError(-ENOENT);
	else if(obj->my_dev->read_only)
		yaffsfs_SetError(-EROFS);
	else if(lnk)
		yaffsfs_SetError(-EEXIST);
	else if(lnk_dir->my_dev != obj->my_dev)
		yaffsfs_SetError(-EXDEV);
	else {
		retVal = yaffsfs_CheckNameLength(newname);

		if(retVal == 0) {
			lnk = yaffs_link_obj(lnk_dir,newname,obj);
			if(lnk)
				retVal = 0;
			else{
				yaffsfs_SetError(-ENOSPC);
				retVal = -1;
			}
		}
	}
	yaffsfs_Unlock();

	return retVal;
}

int yaffs_mknod(const YCHAR *pathname, mode_t mode, dev_t dev)
{
	pathname=pathname;
	mode=mode;
	dev=dev;

	yaffsfs_SetError(-EINVAL);
	return -1;
}


/*
 * D E B U G   F U N C T I O N S
 */

/*
 * yaffs_n_handles()
 * Returns number of handles attached to the object
 */
int yaffs_n_handles(const YCHAR *path)
{
	struct yaffs_obj *obj;

	if(!path){
		yaffsfs_SetError(-EFAULT);
		return -1;
	}

	if(yaffsfs_CheckPath(path) < 0){
		yaffsfs_SetError(-ENAMETOOLONG);
		return -1;
	}

	obj = yaffsfs_FindObject(NULL,path,0,1,NULL,NULL,NULL);

	if(obj)
		return yaffsfs_CountHandles(obj);
	else
		return -1;
}

int yaffs_get_error(void)
{
	return yaffsfs_GetLastError();
}

int yaffs_set_error(int error)
{
	yaffsfs_SetError(error);
	return 0;
}

int yaffs_dump_dev(const YCHAR *path)
{
#if 1
	path=path;
#else
	YCHAR *rest;

	struct yaffs_obj *obj = yaffsfs_FindRoot(path,&rest);

	if(obj){
		struct yaffs_dev *dev = obj->my_dev;

		printf("\n"
			   "n_page_writes.......... %d\n"
			   "n_page_reads........... %d\n"
			   "n_erasures....... %d\n"
			   "n_gc_copies............ %d\n"
			   "garbageCollections... %d\n"
			   "passiveGarbageColl'ns %d\n"
			   "\n",
				dev->n_page_writes,
				dev->n_page_reads,
				dev->n_erasures,
				dev->n_gc_copies,
				dev->garbageCollections,
				dev->passiveGarbageCollections
		);

	}

#endif
	return 0;
}


/*
 * ------------------------- Functions Used For Translation of Codes -------------------------------- 
*/

int tr_file_open_with_mode (const char *path, int flags, int mode)
{
 
  struct yaffs_obj *dir = NULL;
  int notDir, loop;
  char *name;
  dir = yaffsfs_FindDirectory(NULL,path,&name,0,&notDir,&loop);
  if (dir == NULL)
  {
	printf ("In tr_file_open_with_mode  yaffsfs_FindDirectory dir is NULL on path %s\n", path);
  }
 else
 { 
	;//printf ("In tr_file_open_with_mode  yaffsfs_FindDirectory dir is not NULL on path %s\n", path);
 }
  fflush (stdout);
  int handle = yaffs_open (path, flags, mode);

  return handle;
}


int tr_file_open_without_mode (const char *path, int flags)
{
 int fd = tr_file_open_with_mode (path, flags, S_IREAD | S_IWRITE);
 //printf("tr_file_open_without_mode returns %d\n", fd);
 return fd;
}

int tr_object_id_parent_from_path (char *path)
{
 //printf("tr_object_id_parent_from_path\n");
 int loop, notDir;
 char *name; 
 struct yaffs_obj *obj_parent = NULL;
 obj_parent = yaffsfs_FindDirectory(NULL,path,&name,0,&notDir,&loop);
 if (obj_parent != NULL)
  {
     return obj_parent->obj_id;
  }
  else
  {
     printf ("Error : tr_object_id_parent_from_path : yaffsfs_FindDirectory returns NULL on path %s\n", path);
     return -1;	
  }
}

struct yaffs_obj * tr_object_from_path (char *path)
{
  char *name;
  int notDir, loop;
  struct yaffs_obj *obj_parent = yaffsfs_FindDirectory(NULL,path,&name,0,&notDir,&loop);
  if (!obj_parent) printf("Error:tr_object_from_path: Parent Obj Id Not Found\n");
  struct yaffs_obj *obj = yaffs_find_by_name(obj_parent, name);
  if (!obj) printf("Error:tr_object_from_path: Obj Id Not Found\n");
  return obj;
}

int tr_object_id_from_path (char *path)
{
  struct yaffs_obj *obj = tr_object_from_path (path);
  if (obj == NULL) 
	{
		printf("tr_object_id_from_path :: Cannot find the object with path %s\n", path);
		return -1;
	}
 else
	{
		return obj->obj_id;
	}	
}


/* 
 * This is the translation for the file create function. It returns object id for the file created. The file is created within the memory itself.
 */
int tr_ext_creat_yaffs_creat (const char *pathname, mode_t mode)
{
 int loop, notDir;
 char *name; 
 //printf("tr_ext_creat_yaffs_creat called pathname = %s\n", pathname);
 struct yaffs_obj *obj_parent = yaffsfs_FindDirectory(NULL,pathname,&name,0,&notDir,&loop);
 if (obj_parent == NULL)
 {
 	printf ("tr_ext_creat_yaffs_creat called  Parent Object NULL  \n");
        return -1;
 }
 else
 {	
	char str[200];
	sprintf(str, "parent directory (/yaffs2) found with object id %d\n", obj_parent->obj_id);
	log_output(str);
 }
 struct yaffs_obj *obj_file = yaffs_create_file_default (obj_parent, name, mode, 0, 0, false);
 int obj_id = -1;
 if (obj_file != NULL)
 {
        obj_id = (obj_file)->obj_id;
	char str[200];
	sprintf(str, "tr_ext_creat_yaffs_creat Object ID %d\n", obj_id);
	log_output(str);
 }
else
 {
	printf("tr_ext_creat_yaffs_creat obj_file is NULL\n");
 }
 return obj_id;
}

/*
 * This is the function that writes the file created to the disk -- as a consequence of the delayed creation mechanism already in place
 */
int tr_sync_file_to_disk (char *pathname)
{
// printf("tr_sync_file_to_disk called path %s \n", pathname);
 struct yaffs_obj* obj = yaffs_sync_file_to_disk (pathname);
 if (obj == NULL)
 {
   printf("tr_sync_file_to_disk :: yaffs_obj NULL for the file with path %s\n", pathname); 
   return -1;	
 }
 else
 {
  printf("tr_sync_file_to_disk returns obj id %d\n", obj->obj_id);
  return (obj->obj_id);
 // return 0;
 }
}


// We are currently writing in the buffer cache of the file as the write-through flag is 0 by default
// In order to test the crashless update of a file we will have to chnage to write-through

int tr_file_write (int handle, const void *buf, int count, unsigned int t_id)
{
 
       int writeBytesFile;
       char *output; 
       unsigned int len;
       struct yaffs_obj *obj = yaffsfs_HandleToObject(handle);
       //printf("fd->pos = %d\t handle %d", (yaffsfs_HandleToFileDes(handle))->position, handle);
       if (obj != NULL)
       {
       		unsigned int obj_id = obj->obj_id;
                struct file_meta_transaction_node *node = get_meta_transaction_node (t_id, obj_id);
  		if (node == NULL)
                {
                  len = obj->variant.file_variant.file_size;
                  node = create_file_meta_transaction_node(t_id, obj_id, len);
		  add_to_meta_transaction_list (node);
                }
		
		
		writeBytesFile = yaffs_write(handle, buf, count, t_id);
		if (writeBytesFile != count)
			{
					output = (char *)(malloc(50));
					sprintf(output, "BUG :: File Write Failed");
					printf("BUG :: File Write Failed\n");
        				log_output (output);
					free (output);
					return -1;
			}
		
		return writeBytesFile;
			                 
	}
      return -1;
}

int tr_yaffs_stat (const char *path, struct stat *buf)
{

  struct yaffs_stat y_buf;
  int ret_val = yaffs_stat(path, &(y_buf)) ;
  //printf("tr_yaffs_lstat, path = %s, fd = %d\n", path, y_buf.st_ino);
  if(ret_val == -1)
  {
    printf ("tr_yaffs_lstat :: returns -1 on path %s\n", path );
    return ret_val;
  }
  if ((buf != NULL) && (ret_val == 0))
  {
// TODO check whether translation is valid !!
	buf->st_dev = y_buf.st_dev;
	buf->st_ino = y_buf.st_ino;
	buf->st_mode = y_buf.st_mode;
	buf->st_nlink = y_buf.st_nlink;
	buf->st_uid = y_buf.st_uid;
	buf->st_gid = y_buf.st_gid;
	buf->st_rdev = y_buf.st_rdev;
	buf->st_size = y_buf.st_size;
	buf->st_blksize = y_buf.st_blksize;
	buf->st_blocks = y_buf.st_blocks;
	buf->st_atime = y_buf.yst_atime;
	buf->st_mtime = y_buf.yst_mtime;
	buf->st_ctime = y_buf.yst_ctime;	
  }
  return ret_val;
}

/*
 * This is the function that translates the file read system call from ext file-system to yaffs file-system
 */
int tr_file_read (int handle, char *buf, int bytes)
{
  //int bytes_read = yaffs_read (handle, (void *)buf, bytes);
  int bytes_read = yaffsfs_do_read (handle, buf, bytes, 0, 0); 
  if (bytes_read == -1)
  {
	printf("Error : tr_file_read : yaffs_read returns -1\n");
  }
  //printf ("In tr_file_read :: Data Read  %s\n", buf);
  return bytes_read;
}

int tr_yaffs_close (int handle, unsigned int t_id)
{
 struct yaffs_obj *obj = yaffsfs_HandleToObject(handle); 
 char result_string[2000];
 
 if (obj == NULL) 
 {
	printf("Obj== NULL\n"); 
	return -1;
 }
	sprintf(result_string, "4:%d:%d:TypeId:Object Id:Size of File:", obj->obj_id, obj->variant.file_variant.file_size); 
 	result_log (result_string);

 if (rollback_type == YAFFS_COW)
 {
         
	
	/* To do delete all the data blocks that have a previous image on the disk - otherwise there may be trouble in the garbage collection procedure, previous copies of the data may get written on to the disk  */

	 struct file_meta_transaction_node *node = get_meta_transaction_node(t_id, obj->obj_id);
	 struct chunk_map_node *chunk_node_map_ptr;
	 struct list_head *node_list_ptr;
	if (node != NULL)
        {
	 list_for_each(node_list_ptr, node->chunk_map_list)
         {
            chunk_node_map_ptr = list_entry(node_list_ptr, struct chunk_map_node, chunk_map_list_entry);
            int prev_chunk = chunk_node_map_ptr->physical_chunk_before;
  	    if (prev_chunk > 0)
		{
			yaffs_chunk_del(obj->my_dev, prev_chunk, 1, __LINE__);            
		}
	}
 	}

	int ret_close = yaffs_close (handle);
	 if (ret_close == -1)
	 {
		printf("BUG In tr_yaffs_close() ::  yaffs_close returns -1\n");
		return -1;
	 } 
	 return ret_close ;
	 }
 else if (rollback_type == EXT4_JOURNAL)
 {
	yaffsfs_Lock ();
	yaffs_flush_file(obj,1,0);	
	yaffsfs_PutHandle(handle);
	struct yaffs_obj *journal_obj = get_journal_obj ();
	if (journal_obj == NULL)
	{
		printf("BUG In tr_yaffs_close() :: Journal Object Not Found, journal handle = %d\n", journal_fd);
		yaffsfs_Unlock ();
		return -1;
	}
	yaffs_flush_file(journal_obj,1,0);
	yaffsfs_Unlock ();
	printf("Close Done On Commit\n");
	return 1;
 }
 return -1;

}

int tr_yaffs_close_abort (int handle, unsigned int t_id)
{
 struct yaffs_obj *obj = yaffsfs_HandleToObject(handle);
 //printf("HANDLE = %d\t obj id =%d", handle, obj->obj_id);
 if (obj != NULL)
 {
    rollback(t_id, obj);
 }
 else
 {
        printf ("BUG In tr_yaffs_close_abort :: handle=%d, Object NULL\n", handle);
	return -1;
 }
 if (rollback_type == YAFFS_COW)
 {
 	int ret_val = yaffs_abort_close(handle); 
	return ret_val ;
 }
 else if (rollback_type == EXT4_JOURNAL)
 {
	yaffs_abort_close(handle);
        yaffsfs_Lock ();
	struct yaffs_obj *journal_obj = get_journal_obj ();
	if (journal_obj == NULL)
	{
		printf("BUG In tr_yaffs_close_abort() :: Journal Object Not Found\n");
		yaffsfs_Unlock ();
		return -1;
	}
	yaffs_flush_file_abort(journal_obj, 1, 0); // We are not closing the journal at any time during the experiment
	yaffsfs_Unlock ();
	return 1;
 }
 return -1;
}


void set_transaction_id (unsigned int t_id)
{
  ctransaction_id = t_id;
}

unsigned int get_transaction_id (void)
{
 return ctransaction_id ;
}

int tr_lseek(int handle, int offset, int whence)
{
  yaffs_lseek(handle,  offset, whence);
  yaffsfs_FileDes *fd = yaffsfs_HandleToFileDes(handle);  
  return fd->position;


  /*// struct yaffs_obj *in;
   if (fd != NULL)
   {
   //in = yaffsfs_HandleToObject(handle);
   if (1)
   {
    switch (whence)
	{
		case (SEEK_SET):
		fd->position = offset;
		//in->variant.file_variant.file_size = offset;
		break;
		
	}		
    }
   }*/ 
}
int result_log_init (void)
{
 return 1;
 char currDir[200];
 getcwd(currDir, 200);
 char *ch = strstr ((const char *)currDir, "direct");
 if (ch == NULL) 
  {
     printf("tr_yaffs_close :: Comparison Failed\n");
  }
 else
 {
 strcpy (ch, "TxFS_Mam/Traces/testbed/Results/result.txt");
 resultFileFd = fopen (currDir, "w"); 
 if (resultFileFd == NULL)
     {
       printf ("BUG: Open for result file failed \n");
     }
 else
     {
	 struct timeval tv;
	 gettimeofday (&(tv), NULL);
	 fprintf (resultFileFd, "100:start:%ld:%ld:\n", tv.tv_sec, tv.tv_usec);
	// fclose (resultFileFd);
     }
 }
 return 1;
}

int log_init (void)
{
 return 1;
 char currDir[200];
 getcwd(currDir, 200);
 char *ch = strstr ((const char *)currDir, "direct");
 if (ch == NULL) 
  {
     printf("tr_yaffs_close :: Comparison Failed\n");
  }
 else
 {
 strcpy (ch, "TxFS_Mam/Traces/testbed/Results/log.txt");
 logFileFd = fopen (currDir, "w"); 
 if (logFileFd == NULL)
     {
       printf ("BUG: Open for result file failed \n");
     }
 else
     {
	 struct timeval tv;
	 gettimeofday (&(tv), NULL);
	 fprintf (logFileFd, "100:start:%ld:%ld:\n", tv.tv_sec, tv.tv_usec);
     }
 }
 return 1;
}


int result_log (char *str)
{
	return 1;
	 struct timeval tv;
	 gettimeofday (&(tv), NULL);
	 fprintf (resultFileFd, "%s:%ld:%ld:\n", str, tv.tv_sec, tv.tv_usec);
	 fflush(resultFileFd);
	 return 1;
}

void log_output (char *str)
{
	return 1;
 	 struct timeval tv;
	 gettimeofday (&(tv), NULL);	 
	 //fprintf (stdout, "%s:%ld:%ld:\n", str, tv.tv_sec, tv.tv_usec);
         //fflush (stdout);
	 fprintf (logFileFd, "%s:%ld:%ld:\n", str, tv.tv_sec, tv.tv_usec);
         fflush (logFileFd);
	 return 1;
}

void log_const (const char *str_const)
{
 char *str = (char *)malloc(strlen(str_const));
 sprintf(str, str_const);
 log_output(str);
 free (str);
 return;
}

int result_log_const (const char *str_const)
{
 char *str = (char *)malloc(strlen(str_const)+1);
 sprintf(str, str_const);
 result_log(str);
 free (str);
 return 1;
}

struct yaffs_obj *get_journal_obj (void)
{
  return (yaffsfs_HandleToObject(journal_fd));
}

int get_journal_offset (void)
{
  yaffsfs_FileDes *fd = yaffsfs_HandleToFileDes(journal_fd); 
  if (fd == NULL)
	{
		printf ("BUG :: get_journal_offset() :: Journal Not Open\n");
		return -1;
	}
  return  (fd->position);
}
struct yaffs_obj *get_obj_from_handle (int handle)
{
 return (yaffsfs_HandleToObject(handle));
}
