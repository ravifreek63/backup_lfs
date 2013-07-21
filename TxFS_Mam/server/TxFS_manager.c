#ifndef TxFS_MANAGER_
#define TxFS_MANAGER_
#include "my_header.h"
//
extern struct txn_node *get_txn_node(int new_txn_id,long log_rec_no);
extern int assign_new_txnid(void);
extern void new_txn_insert(int new_txn_id,int log_rec_no);
extern void txn_delete(int txnid);
extern long log_txn_beg(int ntid);
int on_txn_abort(int txnid);
extern void error(const char *msg);
extern int tr_file_open_without_mode (const char *path, int flags);
extern int search_file_list(long f_inode,struct file_lock **add_loc);
extern long log_txn_end(int ntid, long prev_log_LRN);
extern long log_txn_commit(int ntid);
extern long log_txn_abort(int ntid);
extern long get_prev_log_LRN(int ntid);
extern void release_locks_on_abort(int txnid);
extern void rename_log(char *incall[],struct file_lock *inserted_pos_old, struct file_lock *inserted_pos_new, int txnid);
extern int write_log(char *incall[],int txnid);
extern void unlink_log(char *incall[],struct file_lock *inserted_pos_child, struct file_lock *inserted_pos_parent, int txnid);
extern ssize_t do_read(int fd,char *read_buf,ssize_t nbytes);
extern int write_possible(int fd);
extern int read_possible(int fd);
extern void release_locks_on_success(int txnid);
extern off_t update_on_lseek(int fd, off_t offset,int whence);
extern void creat_log(char *incall[],struct file_lock *inserted_pos,int txnid);
extern int online_bck(char *argv[]);
extern int file_open_flags(char *argument[]);
extern int tr_object_id_parent_from_path (char *path);
extern int tr_object_id_from_path (char *path);
extern int tr_ext_creat_yaffs_creat (const char *pathname, mode_t mode);
extern tr_yaffs_stat (const char *path, struct stat *buf);
extern int tr_file_write (int handle, const void *buf, int count, unsigned int t_id);
extern int tr_lseek (int, int, int);
#define MAX_PATH_LEN 100


/* Method from where the interfacing should actually start */

int on_txn_beg(int txnid)
{
 int new_txn_id;
 long log_rec_no;

 /*assign new transaction id*/
 new_txn_id = txnid;// assign_new_txnid();
 
 /*make log entry*/
 // Do away with the logging part -- TODO 
 log_rec_no = log_txn_beg(new_txn_id);
 
 /*make entry into "live" transactions list*/
 new_txn_insert(new_txn_id,log_rec_no);

 /*returning the txn_id*/
 return(new_txn_id);
}

int on_open(char *incall[],int txnid)
{
 struct stat buff;
 long f_inode,f_length;
 int search_ret,f_flags;
 struct file_lock *loc=NULL, *inserted_pos;
 int fd,abrt_txnid;/*file descriptor of opened file*/

 /*if (access(incall[1],F_OK)== -1)/*File does not exist
 {
        printf("\n %s does not exist",incall[1]);
	abrt_txnid = on_txn_abort(txnid);
        return(-1);
  }
 */
 //if(stat(incall[1],&buff)!=0)//transfer inode to buff
//	error("Could not get file status1");
  
// f_inode = (long)buff.st_ino;//from inode extract only inode_no.
 //f_length = (long)buff.st_size;//from inode extract size of file
 char path[255];
 strcpy(path, incall[1]);
 f_inode = tr_object_id_from_path (path);

 /*file-mode is shared or exclusive. File flag is RDONLY,WRONLY,RDWR,APPEND*/
 f_flags = file_open_flags(incall);

 pthread_mutex_lock(&file_mutex);

 /*Search if file is already locked or not*/
 search_ret= search_file_list(f_inode,&loc);/*0=not found,1=found*/

 /*Inserting new node into lock list i.e locking the file if possible else place calling transaction in waiting mode*/
 inserted_pos = insert_file_list(f_inode,incall[1],f_flags,txnid,search_ret,loc);

 /*Backup related code.the transaction had to be aborted while serializing with backup transaction*/
 if(inserted_pos == NULL)
	{
	//	pthread_mutex_unlock(&file_mutex);
		return -1;/*(indicating an abort)*/
	}
 else{
 	;//printf("\nOPEN %d %s %s %d\n",txnid,incall[1], incall[2], f_flags);
 
 	/* open the file using real open*/
 	if ((strcmp(incall[2],"0")==0))
	{
// We call the YAFFS file open functions from here  {}
		//fd = open(incall[1],O_RDONLY);
		fd = tr_file_open_without_mode (incall[1], O_RDONLY);

		if(fd==-1)
		{
			pthread_mutex_unlock(&file_mutex);
			abrt_txnid = on_txn_abort(txnid);
			printf("%d has to be aborted as open returned -1",txnid);
			return -1;
		}
		else
			printf("\n R%d(%s)",txnid,incall[1]);
	}
 	if ((strcmp(incall[2],"1")==0))
	{
		//fd = open(incall[1],O_WRONLY);
		fd = tr_file_open_without_mode (incall[1], O_WRONLY);
		if(fd==-1)
		{
			pthread_mutex_unlock(&file_mutex);
			abrt_txnid = on_txn_abort(txnid);
			printf("%d has to be aborted as open returned -1",txnid);
			return -1;
		}
		else
			;//printf("\n W%d(%s)",txnid,incall[1]);
	}
	
 	if ((strcmp(incall[2],"2")==0))
	{
 		fd = tr_file_open_without_mode (incall[1], O_RDWR);
// 		fd = open(incall[1],O_RDWR);
		//printf ("fd of the file opened = %d\n", fd);
		//printf("on_open :: Opening File in RDWR mode\n");
		if(fd==-1)
		{
			pthread_mutex_unlock(&file_mutex);
			abrt_txnid = on_txn_abort(txnid);
			printf("%d has to be aborted as open returned -1",txnid);
			return -1;
		}
		else
			;//printf("\n W%d(%s)",txnid,incall[1]);
	}
	
 	if ((strcmp(incall[2],"1025")==0)||(strcmp(incall[2],"1026")==0))
	{
                //printf("open on file called in append mode for writing\n"); 
       		fd = tr_file_open_without_mode(incall[1],O_WRONLY|O_APPEND);
 		//printf ("fd of the file opened = %d\n", fd);
		if(fd==-1)
		{
			pthread_mutex_unlock(&file_mutex);
			abrt_txnid = on_txn_abort(txnid);
			printf("%d has to be aborted as open returned -1",txnid);
			return -1;
		}
		else
			;//printf("\n W%d(%s)",txnid,incall[1]);
	}


 	/*insert fd into file list*/
 	inserted_pos->fd = fd;
	inserted_pos->file_offset = 0;

	pthread_mutex_unlock(&file_mutex);

	/*If open returns -1 i.e. error due to many reasons as dipicted in the manual, the transaction will be aborted and -1 shall be returned in fd*/
	

 	return (fd);
}

}

int on_stat(char *incall[],int txnid)
{
 struct stat buff;
 long f_inode;
 int search_ret,f_flags;
 struct file_lock *loc=NULL, *inserted_pos;
 int abrt_txnid;/*file descriptor of opened file*/

 if (access(incall[1],F_OK)== -1)/*File does not exist*/
 {
        printf("\n %s does not exist",incall[1]);
	abrt_txnid = on_txn_abort(txnid);
        return(-1);
  }
 
 if(stat(incall[1],&buff)!=0)//transfer inode to buff
	error("Could not get file status1");
  
 f_inode = (long)buff.st_ino;//from inode extract only inode_no.
 
 pthread_mutex_lock(&file_mutex);

 /*Search if file is already locked or not*/
 search_ret= search_file_list(f_inode,&loc);/*0=not found,1=found*/

 /*Inserting new node into lock list i.e loking the file if possible else place calling transaction in waiting mode*/
 inserted_pos = insert_file_list(f_inode,incall[1],0,txnid,search_ret,loc);

 /*BAckup realted code.the transaction had to be aborted while serializing with backup transaction*/
 if(inserted_pos == NULL)
	return -1;/*(indicating an abort)*/
	
 else{
	if(stat(incall[1],&buff)!=0)//transfer inode to buff
	{	
		pthread_mutex_unlock(&file_mutex);
		return (1);
	}
	else
 	 	printf("\nstat%d (%s)",txnid,incall[1],f_flags);
 
 }	
 	/*insert fd into file list*/
 	inserted_pos->fd = 0;
	inserted_pos->file_offset = 0;

	pthread_mutex_unlock(&file_mutex);

	/*If open returns -1 i.e. error due to many reasons as dipicted in the manual, the transaction will be aborted and -1 shall be returned in fd*/
	

 	return (0);
}

// returns the value of the inode for the file contained in incall[1] 
int on_inode_stat(char *incall[], int txnid)
{
// printf ("in TxFS_manager on_inode_stat called \n");
 int f_inode = -1;
 int search_ret,f_flags;
 struct file_lock *loc=NULL, *inserted_pos;
 int abrt_txnid; /*file descriptor of opened file*/

/* if (access(incall[1],F_OK)== -1)/*File does not exist
 {
        printf("\n %s does not exist",incall[1]);
	abrt_txnid = on_txn_abort(txnid);
        return(-1);
  }
 */

// if(stat(incall[1],&buff)!=0)//transfer inode to buff
//	error("Could not get file status1");
 
 struct stat stat_buf;
 char filepath[MAX_PATH_LEN];
 strcpy (filepath, incall[1]);
 int ret_val = tr_yaffs_stat (filepath, &(stat_buf));
 if (ret_val == -1)
 {
	printf ("Error in function :: on_inode_stat ::  tr_yaffs_stat returns -1 :: for the file path %s\n", incall[1]);
	return f_inode;
 }
 else
 {
      f_inode = stat_buf.st_ino;
      return (f_inode);
 } 
 

// printf("inode of the  file %d ---  before file_mutex\n", f_inode); 
 //f_inode = (long)buff.st_ino;//from inode extract only inode_no.
 
 pthread_mutex_lock(&file_mutex);
// printf("after file_mutex\n", f_inode); 
 /*Search if file is already locked or not*/
 search_ret= search_file_list(f_inode,&loc);/*0=not found,1=found*/

 /*Inserting new node into lock list i.e loking the file if possible else place calling transaction in waiting mode*/
 inserted_pos = insert_file_list(f_inode,incall[1],0,txnid,search_ret,loc);

 /*BAckup realted code.the transaction had to be aborted while serializing with backup transaction*/
/* if(inserted_pos == NULL)
	return -1;/*(indicating an abort)
	
 else{
	if(stat(incall[1],&buff)!=0)//transfer inode to buff
	{	
		pthread_mutex_unlock(&file_mutex);
		return (1);
	}
	else
 	 	printf("\nstat%d (%s)",txnid,incall[1],f_flags);
 
 }	
*/
 	/*insert fd into file list*/
 	inserted_pos->fd = 0;
	inserted_pos->file_offset = 0;

	pthread_mutex_unlock(&file_mutex);

	/*If open returns -1 i.e. error due to many reasons as dipicted in the manual, the transaction will be aborted and -1 shall be returned in fd*/
	

 	return (f_inode);
}

int on_txn_commit(int txnid)
{
  long log_rec_no,prev_LRN;
 /*make log entry*/
 
 log_rec_no = log_txn_commit(txnid);
 
 /*do write in place*/

 /*update lock list,release locks*/
 release_locks_on_success(txnid);
 
 /*go to first file, check if wait list present.If no, check if shrared. accordingly either update pointers or free node by closing files.fd can be obtained from the node itself, if wait list, again check whether shared or exclusive. if any txn needs to be woken up,wake it up*/
 prev_LRN = get_prev_log_LRN(txnid);
 txn_delete(txnid);
 log_rec_no = log_txn_end(txnid,prev_LRN);

 return(txnid);
 
}

int on_txn_abort(int txnid)
{
 long log_rec_no,prev_LRN;
  
 /*make a log entry for preparing abort*/
 log_rec_no = log_txn_abort(txnid);

/*update locklist and do not write to place.Release locks*/
 release_locks_on_abort(txnid);

 prev_LRN = get_prev_log_LRN(txnid);
 txn_delete(txnid);
 log_rec_no = log_txn_end(txnid,prev_LRN);
 return(txnid);
}

ssize_t on_write(char *incall[], int txnid)
{
 int fd, bytes_written;
 size_t nbyte;

 fd = atoi(incall[1]);
 nbyte = atoi(incall[3]);

 if(write_possible(fd) == -1)/*if -1 is returned then write is not possible on the file pointed by fd ..maybe because it was not opened for writing*/
	{
		printf ("Error : on_write: write_possible returns -1 as file with fd = %s is not ready for write\n", incall[1]);
		fflush(stdout);
		return -1;
	}
// Earlier Implementation
 //bytes_written = write_log(incall,txnid);
// In our new design the write_log will have to be done away with as we would write the data to the cache, mark them dirty and then on transaction commit when the file is closed those caches would be flushed back to the flash disk.
 bytes_written = tr_file_write (fd, incall[2], nbyte, txnid);

 if (bytes_written == 0)
 {
	printf ("Error : function write_log : tr_file_write returns 0\n");
 }

 if (bytes_written == -1)
 {
	printf ("Error : function write_log : tr_file_write returns -1\n");
 }
 return(bytes_written);
}

int lock_subtree(char *path, int txnid)
{
 struct stat buff_subtree;
 long inode_subtree;
 int search_ret_subtree;
 struct file_lock *loc_subtree=NULL,*inserted_pos_subtree = NULL;
 char *path_array[] = {path,NULL};
 
 FTSENT *node;
 //printf("\nbefore");
 fflush(stdout);
 FTS *tree = fts_open(path_array, FTS_NOCHDIR,0);

	if(!tree){
		error("fts_open");
		return 1;
		}
	
	while((node = (FTSENT*)fts_read(tree))){
		if(node->fts_level == 0)
			continue;
		if(node->fts_level >0 && node->fts_name[0] == '.')
			fts_set(tree, node, FTS_SKIP);
		else if(node->fts_info && FTS_F){
			/*read lock*/
			if(stat(path,&buff_subtree)!=0)
				error("Could not get file status");
  			inode_subtree =(long)buff_subtree.st_ino;

  			search_ret_subtree = search_file_list(inode_subtree,&loc_subtree);
  			inserted_pos_subtree = insert_file_list(inode_subtree,path,0,txnid,search_ret_subtree,loc_subtree);
 			 /*BAckup realted code.the transaction had to be aborted while serializing with backup transaction*/
 			if(inserted_pos_subtree == NULL)
 			{
				return -1;/*(indicating an abort)*/
 			}
  			else{
				inserted_pos_subtree->fd = 0;
       				inserted_pos_subtree->file_offset = 0;
 			}

			//printf("Got file name %s \n", node->fts_path);
		}
	}
	
	if(fts_close(tree)){
		error("fts_close");
		return 1;
	}

	return 0;

}

int on_rename(char *incall[],int txnid)
{
 struct stat buff_old,buff_new,buff_mov;
 long inode_old,inode_new,inode_mov;
 int search_ret_old,search_ret_new,search_ret_mov,flags_old,flags_new,ret,abrt_txnid,ret_lock_subtree;
 struct file_lock *loc_old=NULL,*loc_new=NULL, *loc_mov=NULL, *inserted_pos_old = NULL, *inserted_pos_new = NULL, *inserted_pos_mov = NULL;
 char oldpath[255],newpath[255],*oldpath_parent=NULL,*newpath_parent=NULL; 

 bzero(oldpath,255);
 bzero(newpath,255);

 strcpy(oldpath,incall[1]);
 strcpy(newpath,incall[2]);

 
 if (access(incall[1],F_OK)== -1)/*File to be renamed does not exist*/
 {
        printf("\n %s does not exist",incall[1]);
	abrt_txnid = on_txn_abort(txnid);
        return(-1);
  }
 
 oldpath_parent = dirname(oldpath);
 newpath_parent = dirname(newpath);

 
 if (access(newpath_parent,F_OK)== -1)/*Directory to be moved to does not exist*/
 {
        printf("\n %s does not exist",newpath_parent);
	abrt_txnid = on_txn_abort(txnid);
        return(-1);
  }
 
 if(stat(oldpath_parent,&buff_old)!=0)//transfer inode to buff
	error("Could not get file status");
 inode_old = (long)buff_old.st_ino;//from inode extract only inode_no.

 flags_old =2;

  pthread_mutex_lock(&file_mutex);

 /*Search if file is already locked or not*/
 search_ret_old= search_file_list(inode_old,&loc_old);/*0=not found,1=found*/

 /*Inserting new node into lock list i.e loking the file if possible else place calling transaction in waiting mode*/
 inserted_pos_old = insert_file_list(inode_old,oldpath_parent,flags_old,txnid,search_ret_old,loc_old);

 /*BAckup realted code.the transaction had to be aborted while serializing with backup transaction*/
 if(inserted_pos_old == NULL)
                return -1;/*(indicating an abort)*/
 else{
	if(inserted_pos_old != loc_old)/*already locked by same transaction*/
	{
	       	inserted_pos_old->fd = 0;
        	inserted_pos_old->file_offset = 0;
	}
     }


//I have not unlocked mutex//
 if(strcmp(oldpath_parent,newpath_parent)!=0)
 {
	 if(stat(newpath_parent,&buff_new)!=0)//transfer inode to buff
	        error("Could not get file status");
 	 inode_new = (long)buff_new.st_ino;//from inode extract only inode_no.
 
         flags_new = 2;

	 /*Search if file is already locked or not*/
 	search_ret_new= search_file_list(inode_new,&loc_new);/*0=not found,1=found*/

	/*Inserting new node into lock list i.e loking the file if possible else place calling transaction in waiting mode*/
 	inserted_pos_new = insert_file_list(inode_new,newpath_parent,flags_new,txnid,search_ret_new,loc_new);

	
 	/*BAckup realted code.the transaction had to be aborted while serializing with backup transaction*/
 	if(inserted_pos_new == NULL)
	{
		return -1;/*(indicating an abort)*/
	}
	 else{
		if(inserted_pos_new != loc_new)/*already not locked by same transaction*/
		{
 			inserted_pos_new->fd = 0;
	        	inserted_pos_new->file_offset = 0;
		}
	}
 }

//Lock the file to be moved in write mode as its .. entry is changed

  if(stat(incall[1],&buff_mov)!=0)
	error("Could not get file status");
  inode_mov =(long)buff_mov.st_ino;

  search_ret_mov = search_file_list(inode_mov,&loc_mov);
  inserted_pos_mov = insert_file_list(inode_mov,oldpath,2,txnid,search_ret_mov,loc_mov);
  /*BAckup realted code.the transaction had to be aborted while serializing with backup transaction*/
 if(inserted_pos_mov == NULL)
 {
	return -1;/*(indicating an abort)*/
 }
  else{
	inserted_pos_mov->fd = 0;
        inserted_pos_mov->file_offset = 0;
 }

 /*if oldpath is a directory recursively lock its children in read mode*/
 if(S_ISDIR(buff_mov.st_mode)!=0)/*It is a directory*/
 {
	ret_lock_subtree = lock_subtree(oldpath,txnid);
 	if(ret_lock_subtree == -1)
		return -1;/*aborted while trying to lock tree*/
 }

 printf("\nrename%d(%s,%s)",txnid,incall[1],incall[2]);
 fflush(stdout);
 pthread_mutex_unlock(&file_mutex);
 rename_log(incall,inserted_pos_old,inserted_pos_new,txnid);/*write to log*/   
 return(1);
 }

off_t on_lseek(char *incall[], int txnid)
{
 int fd,whence;
 off_t offset,ret_offset; 
 fd = atoi(incall[1]);
 offset = atoi(incall[2]);
 whence = atoi(incall[3]);
 printf("lseek(%d,%ld,%d)\n",fd,offset,whence);
 fflush (stdout);
 //ret_offset = update_on_lseek(fd,offset,whence);
 printf("lseek(%d,%ld,%d)\n",fd,offset,whence);
 fflush (stdout);
 ret_offset = tr_lseek (fd, offset, whence);
 printf("lseek(%d,%ld,%d)\t returns %d",fd,offset,whence, ret_offset);
// fflush (stdout);
 return ret_offset;
}

int on_creat(char *incall[], int txnid)
{
 struct stat buff;
 long inode_parent;
 int search_ret;
 struct file_lock *loc, *inserted_pos = NULL;
 char newpath[256],*parent=NULL; 
 bzero(newpath,255);
 strcpy(newpath,incall[1]);

 /*printf("arg0 %s\n", incall[0]);
 printf(" arg1 %s\n", incall[1]);
 printf(" arg2 %s\n", incall[2]);
 printf("transaction id %d\n", txnid);
 */
 parent = dirname(newpath);
 //printf ("Parent = %s\n", parent);
// The implementation for stat has to be re-written for YAFFS
/*  if(stat(parent,&buff)!=0)
	error("Could not get file status");
  inode_parent =(long)buff.st_ino;
*/
 inode_parent = (long) (tr_object_id_parent_from_path (newpath));
 //printf("parent's object id = %d, for path %s\n", inode_parent, newpath);

   pthread_mutex_lock(&file_mutex);
  //printf(" pthread_mutex_lock done\n");
// This is a list of locks within which we obtain a lock on the parent file, when creating a child's file.

  search_ret = search_file_list(inode_parent, &loc);
  //printf("search_file_list done with search return = %d\n", search_ret);	
  inserted_pos = insert_file_list(inode_parent,parent,2,txnid,search_ret,loc);
 if(inserted_pos == NULL)
 {
	printf("Error : on_creat :: Inserted Position NULL");
	return -1;/*(indicating an abort)*/
 }
  else{
	inserted_pos->fd = 0;
        inserted_pos->file_offset = 0;
 }

// printf("\ncreat%d(%s,%s)",txnid,incall[1],incall[2]);
 pthread_mutex_unlock(&file_mutex);

 creat_log(incall,inserted_pos,txnid);/*write to log*/
 // TODO checking for the different modes 
 
 int mode = atoi (incall[2]);
 /*
  *  Call to the YAFFS to create an in-memory data structure representing this file should be done, that can be later flushed out to the disk for storage
  *  It should return handle to the file.	
  */
 int inode_number = tr_ext_creat_yaffs_creat (incall[1], mode);
 return inode_number ;
// return 1;
}

ssize_t on_read(int fd,char *read_buf,ssize_t nbytes,int txnid)
{
 long bytes_read;
 /*If file is opened in O_WRONLY and O_WRONLY|O_APPEND mode then read is not allowed. Return -1*/
// if(read_possible(fd) == -1){
//	strcpy(read_buf,"NA");
//	return -1;
 //}
 //else{
	//printf("Calling do_read\n");
	bytes_read = do_read(fd,read_buf,nbytes);
	if(bytes_read == 0)/*if actual read returned zero*/
	{
 		strcpy(read_buf,"Zero\n");
 		bytes_read = strlen(read_buf);
	}
 	//printf("\nhave reached read %s %ld",read_buf,bytes_read);
 	return (bytes_read);
 //}
}


int on_unlink(char *incall[], int txnid)
{

 /*Lock parent and the to-be-deleted child. Then log the command in the Log. At release lock time perform the actual delete.*/
 struct stat buff_child,buff_parent;
 long inode_parent,inode_child;
 int search_ret_parent,search_ret_child;
 struct file_lock *loc_parent=NULL, *loc_child = NULL,*inserted_pos_parent = NULL, *inserted_pos_child = NULL;
 char newpath[256],*parent=NULL; 

 bzero(newpath,255);

 strcpy(newpath,incall[1]);

 
 parent = dirname(newpath);
 
  if(stat(parent,&buff_parent)!=0)
	error("Could not get file status");
  inode_parent =(long)buff_parent.st_ino;
 if(stat(incall[1],&buff_child)!=0)
	error("Could not get file status");
 inode_child = (long)buff_child.st_ino;

   pthread_mutex_lock(&file_mutex);
 /*lock parent*/
  search_ret_parent = search_file_list(inode_parent,&loc_parent);
  inserted_pos_parent = insert_file_list(inode_parent,parent,2,txnid,search_ret_parent,loc_parent);
  /*BAckup realted code.the transaction had to be aborted while serializing with backup transaction*/
 if(inserted_pos_parent == NULL)
 {
	return -1;/*(indicating an abort)*/
 }
  else{
	inserted_pos_parent->fd = 0;
        inserted_pos_parent->file_offset = 0;
 }
 /*lock child*/
 
 search_ret_child = search_file_list(inode_child,&loc_child);
  inserted_pos_child = insert_file_list(inode_child,incall[1],2,txnid,search_ret_child,loc_child);
  /*BAckup realted code.the transaction had to be aborted while serializing with backup transaction*/
 if(inserted_pos_child == NULL)
 {
	return -1;/*(indicating an abort)*/
 }
  else{
	inserted_pos_child->fd = 0;
        inserted_pos_child->file_offset = 0;
 }

  printf("\nunlink%d(%s)",txnid,incall[1]);

 pthread_mutex_unlock(&file_mutex);

 unlink_log(incall,inserted_pos_child,inserted_pos_parent,txnid);/*write to log*/
 return 1;
}

int on_bck_beg(char *incall[])
{
  int bck_txnid;
  long log_rec_no,prev_LRN;
  BACKUP = ACTIVE;
  bck_txnid = 0;/*id 0 is reserved for the backup transaction*/

  /*make log entry*/
  log_rec_no = log_txn_beg(bck_txnid);

  /*make entry into "live" transactions list*/
  new_txn_insert(bck_txnid,log_rec_no);

  //printf("\nBACKUP %d",BACKUP);

  /*Executing the backup program*/
  int ret_val = online_bck(incall);

  /*removing backup txn from "live" transactions list. and commiting by writing to log*/
  prev_LRN = get_prev_log_LRN(bck_txnid);
  txn_delete(bck_txnid);
  log_rec_no = log_txn_end(bck_txnid,prev_LRN);

  BACKUP = INACTIVE;
  return ret_val;
}
  
void print_lists()
{
 struct txn_node *ptr_txn_node;
 struct file_lock *ptr_file_lock;
 ptr_txn_node = head_txn_list;
 ptr_file_lock = head_lock_list;
 while(ptr_txn_node!=NULL){
	printf("\ntxn_id:  %d cur_status %d",ptr_txn_node->txn_id,ptr_txn_node->cur_state);
	ptr_txn_node = ptr_txn_node->next;
 }
 
while(ptr_file_lock !=NULL){
	printf("\nfile_inode %ld file_fd %d parent txn %d",ptr_file_lock->file_inode,ptr_file_lock->fd,ptr_file_lock->parent_txn);
	if(ptr_file_lock->next_txn == NULL)
		printf(" same txn NULL");
	else
		printf(" same txn %ld",(ptr_file_lock->next_txn)->file_inode);
	if(ptr_file_lock->prev_shared_rec == NULL)
		printf(" prev_shared_rec NULL");
	else
		printf(" prev_shared_rec %d",(ptr_file_lock->prev_shared_rec)->parent_txn);
	if(ptr_file_lock->next_shared_rec == NULL)
		printf(" next_shared_rec NULL");
	else
		printf(" next_shared_rec %d",(ptr_file_lock->next_shared_rec)->parent_txn);
	if(ptr_file_lock->next == NULL)
		printf(" next NULL");
	else
		printf(" next %ld",(ptr_file_lock->next)->file_inode);
	if(ptr_file_lock->prev == NULL)
		printf(" prev NULL");
	else
		printf(" prev %ld",(ptr_file_lock->prev)->file_inode);
	ptr_file_lock = ptr_file_lock->next;
}

}

#endif
