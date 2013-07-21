#include "my_header.h"
extern long log_write(int txnid,int fd, off_t offset, size_t write_length, char *data);
extern long log_rename(int txnid, char *incall[]);
extern long log_creat(int txnid, char *incall[]);
long log_unlink(int txnid, char *incall[]);
extern int tr_file_read (int handle, char *buf, int bytes);

/*Checks if the file on which a write has been issued was opened for write or not*/
int write_possible(int fd)
{
 struct file_lock *ptr_file_node;
 pthread_mutex_lock(&file_mutex);	/*LOCK FILE_MUTEX*/
 ptr_file_node = head_lock_list;
 if (ptr_file_node == NULL)
 {
	printf("BUG :: Head of the Lock List Is NULL\n");
	fflush (stdout);
	pthread_mutex_unlock(&file_mutex);
 	return -1;
 }
 while((ptr_file_node != NULL)  && (ptr_file_node->fd != fd))
	{
		
 		ptr_file_node = ptr_file_node->next;
		if (ptr_file_node == NULL)
		{
			printf ("BUG :: file with fd = %d, not found within the list\n", fd);
			fflush (stdout);
                        pthread_mutex_unlock(&file_mutex);
			return -1;
		}
	}
 pthread_mutex_unlock(&file_mutex);	/*UNLOCK FILE_MUTEX*/
 if (ptr_file_node == NULL) 
 {
	printf ("BUG :: file with fd = %d, not found within the list\n", fd);
 }
 if((ptr_file_node->flags == 0))		/*i.e opened for reading only*/
	return -1;
 else
	return 1;
}

/*Checks if the file on which a read has been issued was opened for read or not*/
int read_possible(int fd)
{
 struct file_lock *ptr_file_node;

 pthread_mutex_lock(&file_mutex);/*LOCK FILE_MUTEX*/
 ptr_file_node = head_lock_list;

 while(ptr_file_node->fd != fd)
        ptr_file_node = ptr_file_node->next;
 pthread_mutex_unlock(&file_mutex);/*UNLOCK FILE_MUTEX*/

 if((ptr_file_node->flags == 1)||(ptr_file_node->flags == 3))/*i.e opened for writing only OR opened for append*/
        return -1;
 else
        return 1;
}

void extract_data_log(int LRN_write,char *rec_data)
{
  size_t rec_write_length;
  fseek(event_log,LRN_write+(sizeof(long)+sizeof(int)+sizeof(long)+sizeof(long)+sizeof(int)+sizeof(int)+sizeof(off_t)),SEEK_SET);
  fread(&rec_write_length,sizeof(size_t),1,event_log);
  fread(rec_data,rec_write_length*sizeof(char),1,event_log);
  //printf("\nextract data from log:  %s",rec_data);
 return;
}

 /*Data to be read may be spread across many write and hence log records and also the original file.Hence a link list of type data_read will collect therequested read portion by portion and concatenate it at the end.The list is kept in a sorted order of the beginning offset*/
 struct data_read{
                        off_t beg;
                        off_t end;
                        char *data;
                        struct data_read *next;
                };
 struct data_read *head = NULL;


 struct data_read *get_new_node(off_t read_from,off_t read_to,int index,char *rec_data,size_t bytes_read)
 {
  struct data_read *new;
  new = (struct data_read *)malloc(sizeof(struct data_read));
  new->data = malloc((bytes_read+1)*sizeof(char));
  memset(new->data,0,(bytes_read+1)*sizeof(char));
  new->beg = read_from;
  new->end = read_to;
  new->next = NULL;
          /*Extract only the portion of data that has been requested*/
  strncpy(new->data,rec_data+index,bytes_read);
  return(new);
 }

void destroy_data_read(void)
{
 struct data_read *ptr;
 //printf("\nwill destroy data read");
 while(head != NULL)
 {
	ptr = head;
	head = head->next;
	free(ptr->data);
	free(ptr);
  }
 
}

 void insert_read_list(struct data_read *new)
{
 struct data_read *ptr,*foll;
   if(head == NULL)
	   head = new;
   else{
           ptr = head;
           while(ptr != NULL)
          {
                if((ptr->beg) > (new->beg))
                          break;
                foll = ptr;
                ptr = ptr->next;
           }
	if(ptr == head)
	{
		new->next = head;
		head = new;
	}
	else{
           new->next = foll->next;
           foll->next = new;
	}	
      }  
}



/*incall[] was the char array where we received the write syscall.*/
int write_log(char *incall[],int txnid)
{
 struct file_lock *ptr_file_node;
 struct write_records *new_write_record,*ptr_write_record,*ptr_writes_at;
 int fd;
 long offset,log_rec_no;
 size_t write_length;

 
 write_length = atoi(incall[3]);/*length of bytes to be written into file*/
 fd = atoi(incall[1]);/*fd is the file descriptor of the file we are writing to*/

  pthread_mutex_lock(&file_mutex);/*LOCK FILE_MUTEX*/

 ptr_file_node = head_lock_list;
 while(ptr_file_node->fd != fd)
	 ptr_file_node = ptr_file_node->next;/*ptr_file_node points to the file where writing is going on*/
 
 if ((ptr_file_node->flags == 3)||(ptr_file_node->flags == 4))/*i.e file has been opened for appending*/
  {
	if(ptr_file_node->writes_at == NULL)/*i.e no writes has as yet taken place*/
		offset = lseek(fd,0,SEEK_END);/*set offset to end of file before writing/appending*/
	else{/*go to end of writes_at listand get the offset*/
		ptr_writes_at = ptr_file_node->writes_at;
		while(ptr_writes_at->next != NULL)
			ptr_writes_at = ptr_writes_at->next;
		offset = ptr_writes_at->initial_off+ptr_writes_at->write_length;/*we calculate as stored offset may not be the end of file as it may be reset by read or lseek*/
	     }
   }	

 else
	offset = ptr_file_node->file_offset;
 
 /*Get a new write_record node*/
 new_write_record = (struct write_records *)malloc(sizeof(struct write_records));
 
 /*populate the new node*/
 new_write_record->initial_off = offset;
 new_write_record->write_length = write_length;
 new_write_record->LRN = 0;/*record number in log where data will be written will enter later*/
 new_write_record->next = NULL;
 new_write_record->prev = NULL;

 /*update offset in the locked file node*/
 ptr_file_node->file_offset = offset+write_length;
 
 /*update file size in the locked file node*/
 if(ptr_file_node->file_offset > ptr_file_node->file_size)
	ptr_file_node->file_size = ptr_file_node->file_offset;

  

 /*insert new write_record node in the linked list of all writes to the file associated with the file node*/
 if(ptr_file_node->writes_at == NULL)
	ptr_file_node->writes_at = new_write_record;
 else{
       ptr_write_record = ptr_file_node->writes_at;
       while(ptr_write_record->next != NULL)
		ptr_write_record = ptr_write_record->next;
	ptr_write_record->next = new_write_record;
	new_write_record->prev = ptr_write_record;
 }
 pthread_mutex_unlock(&file_mutex);

  /*enter into log*/
 log_rec_no = log_write(txnid,fd,offset,write_length, incall[2]);

 new_write_record->LRN = log_rec_no;/*doing it now and not before is to avoind locking many resources together*/
 return(write_length);
	
}

off_t update_on_lseek(int fd, off_t offset,int whence)
{
 struct file_lock *ptr_file_lock; 
 pthread_mutex_lock(&file_mutex);
 
 ptr_file_lock = head_lock_list;

 while(ptr_file_lock->fd != fd)
	ptr_file_lock = ptr_file_lock->next;

 pthread_mutex_unlock(&file_mutex);/*I can release the lock after I reach the node as no one else can change the node as it is the only transaction owning the lock as its been open in exclusive mode*/
 
 if(whence == 0)/*SEEK_SET*/
	ptr_file_lock->file_offset = offset;
 if(whence == 1)/*SEEK_CUR*/
	ptr_file_lock->file_offset = ptr_file_lock->file_offset+offset;/*remember offset may be a negative value*/
 if(whence == 2)/*SEEK_END*/
	ptr_file_lock->file_offset = offset+ptr_file_lock->file_size;

 return ptr_file_lock->file_offset;
}


/* Current Implementation of reading a file through YAFFS */
ssize_t do_read(int fd, char *read_buf, ssize_t nbytes)
{
  int total_bytes_read = tr_file_read (fd, read_buf, nbytes);
  return total_bytes_read;	
}


/* Earlier Implementation for file read -- now we read the same file through the yaffs_read  */
ssize_t do_read_old(int fd,char *read_buf,ssize_t nbytes)
{
  struct data_read *new,*ptr_read_list;

  struct file_lock *ptr_file_lock;/*variable used to point to the locked file node in the file list*/
  struct write_records *ptr_writes_at;
  ssize_t total_bytes_read =0,bytes_read=0,bytes_to_read=nbytes;/*total_bytes_read is the total bytes read till present,bytes_read is bytes read at one read and bytes_to_read is the bytes yet to be read*/
  off_t read_from, read_to, x,y;/*offset to be read from read_from to read_to*/
  char *rec_data;/*read_data stores the data or portion of the requested data,rec_data is the data read from a log record*/
  int LRN_write,index;/*index is a position from within data in a log record from where we need to extract byte_read amount of data*/

  pthread_mutex_lock(&file_mutex);

  ptr_file_lock = head_lock_list;

  while(ptr_file_lock->fd != fd)
        ptr_file_lock = ptr_file_lock->next;
  
 pthread_mutex_unlock(&file_mutex);
/*I can release the lock after I reach the node as no one else can change the node as it is the only transaction owning the lock as its been open in exclusive mode*/
 

/* We first check for which mode the file has been opened in */
 if(ptr_file_lock->flags == 0)/*opened in read only mode then read directly from file*/
 {
	total_bytes_read = read(fd, read_buf, nbytes);
	ptr_file_lock->file_offset = ptr_file_lock->file_offset+total_bytes_read;/*Not required in this case but for conssitency*/
	return(total_bytes_read);
 }
 else
 {
	/*move to the end of the writes-at list*/
	ptr_writes_at = ptr_file_lock->writes_at;
	if(ptr_writes_at != NULL)/*no writes have take place the ptr_writes_at will be NULL*/
	{
		while(ptr_writes_at->next != NULL)
			ptr_writes_at = ptr_writes_at->next;
	}
		
	if(ptr_writes_at == NULL){/*nothing has as yet been written by this transaction to the file and hence read directly from file*/
                total_bytes_read = read(fd,read_buf,nbytes);
   		 ptr_file_lock->file_offset = ptr_file_lock->file_offset+total_bytes_read;/*Updating offset*/
                return(total_bytes_read);
        }

	/*The following code will be executed if something was written by this transaction*/
	
	/*Request to read is from offset read_from to read_to*/ 
	read_from = ptr_file_lock->file_offset;
	read_to = (ptr_file_lock->file_offset)+ bytes_to_read;

	while((bytes_to_read != 0)&&(ptr_writes_at !=NULL))/*ptr_write_at may be NULL if nothing is written to file or when we have traversed the entire list*/
	{
		/*present writes_at node has data from offset x to y*/
		x = ptr_writes_at->initial_off;
		y = (ptr_writes_at->initial_off)+(ptr_writes_at->write_length);

		if((read_from >= x) &&(read_to <= y))/*the requested read falls all within this log write*/
		{
			bytes_read = read_to-read_from;
				/*Extract from log*/
			rec_data = malloc((ptr_writes_at->write_length)*sizeof(char));
			extract_data_log(ptr_writes_at->LRN,rec_data);
			//printf("\n DATA1 %s",rec_data);
				
				/*allocate a new node to hold read data*/
			index = read_from-x;
			new = get_new_node(read_from,read_to,index,rec_data,bytes_read);
		        //printf("\nExtracted data1 %s",new->data);		
				/*Put new into list of data read till now*/
			insert_read_list(new);
	
			total_bytes_read = total_bytes_read+bytes_read;
			bytes_to_read = bytes_to_read - bytes_read;
			ptr_writes_at = ptr_writes_at->prev;
		}
		else{
			if((read_from<x) && (read_to<y))
			{
				bytes_read = read_to - x;
					  /*Extract from log*/
        	                rec_data = malloc((ptr_writes_at->write_length)*sizeof(char));
	        	        extract_data_log(ptr_writes_at->LRN,rec_data);
                        	//printf("\n DATA2 %s",rec_data);
                	                /*allocate a new node to hold read data*/
	                        index = 0;
        	                new = get_new_node(x,read_to,index,rec_data,bytes_read);
				//printf("\nExtracted data2 %s",new->data);
				
					 /*Put new into list of data read till now*/
                	        insert_read_list(new);

                        	total_bytes_read = total_bytes_read+bytes_read;
                        	bytes_to_read = bytes_to_read - bytes_read;
                        	ptr_writes_at = ptr_writes_at->prev;
				read_to = x;
			}
			else{
				 if((read_from > x) && (read_to >y))
	                        {
                	                bytes_read = y-read_from;
                        	                  /*Extract from log*/
                                	rec_data = malloc(((ptr_writes_at->write_length)+1)*sizeof(char));/*added +1 after valgrind error*/
					memset(rec_data,0,(ptr_writes_at->write_length+1)*sizeof(char));
                                	extract_data_log(ptr_writes_at->LRN,rec_data);
                                	//printf("\n DATA3 %s",rec_data);
                                        	/*allocate a new node to hold read data*/
                                	index = read_from - x;
                                	new = get_new_node(x,read_to,index,rec_data,bytes_read);
                                	//printf("\nExtracted data3 %s",new->data);

                                        	 /*Put new into list of data read till now*/
                                	insert_read_list(new);

                                	total_bytes_read = total_bytes_read+bytes_read;
                                	bytes_to_read = bytes_to_read - bytes_read;
                                	ptr_writes_at = ptr_writes_at->prev;
                                	read_from = y;
					free(rec_data);
        	                }
				
			}
		}
		
	}	
	if(bytes_to_read != 0)/*if there is any portion of the read left to do from original file*/
	{
	  lseek(fd,read_from,SEEK_SET);
          rec_data = malloc(bytes_to_read*sizeof(char));
	  memset(rec_data,0,bytes_to_read);
          read(fd,rec_data,bytes_to_read);
	   //printf("\n DATA3 %s",rec_data);
                   /*allocate a new node to hold read data*/
           index = 0;
           new = get_new_node(read_from,(read_from+bytes_to_read),index,rec_data,bytes_to_read);
           //printf("\nExtracted data3 %s",new->data);

                    /*Put new into list of data read till now*/
           insert_read_list(new);
	   total_bytes_read = total_bytes_read+bytes_to_read;
	   free(rec_data);
	}

	ptr_read_list = head;
	while(ptr_read_list!=NULL)
	{
		//printf("\n %s",ptr_read_list->data);
		strcat(read_buf,ptr_read_list->data);
		ptr_read_list = ptr_read_list->next;
	}
	//printf("\n offset before read %ld",ptr_file_lock->file_offset);
	if((ptr_file_lock->file_offset+total_bytes_read) >(ptr_file_lock->file_size))//we have tried reading beyond the file length
		ptr_file_lock->file_offset = ptr_file_lock->file_size;
	else
		 ptr_file_lock->file_offset = ptr_file_lock->file_offset+total_bytes_read;/*Updating offset*/
         //printf("\n offset after read %ld and file size %ld",ptr_file_lock->file_offset,ptr_file_lock->file_size);
        destroy_data_read();
	return(total_bytes_read);

   }	
	
}


void rename_log(char *incall[],struct file_lock *inserted_pos_old, struct file_lock *inserted_pos_new, int txnid)
{
   long log_rec_no;
   struct write_records *new_write_record, *ptr_write_record;
   /*make a log entry from where rename in place will be done on txncommit*/
   log_rec_no = log_rename(txnid,incall);

   
   /*Now in the write_at record of the oldpath and newpath(i.e the two directories that are locked) indicate whether it is the oldpath or newpath or both are same and update the log_rec_no*/
   /*No need to lock file_mutex as we already know the positions*/
     
  /*Get a new write_record node*/
  new_write_record = (struct write_records *)malloc(sizeof(struct write_records));
 
  /*populate the new node*/
  new_write_record->initial_off = 0;
  if(inserted_pos_new == NULL)/*oldpath is same as newpath*/
  	new_write_record->write_length = -3;
  else
	new_write_record->write_length = -1;
  new_write_record->LRN = log_rec_no;/*record number in log where data will be written will enter later*/
  new_write_record->next = NULL;
  new_write_record->prev = NULL;

  /*insert new write_record node in the linked list of all writes to the file associated with the file node. This list may not be empty as the parent directory may be locked previously by the same txn. It will happen when we are trying to rename files from same directory or the same file itself. Also if we r creating into same directory or deleting from same directory*/
  if(inserted_pos_old->writes_at != NULL)/*NULL if locking for first time else non NULL*/
  {
  	ptr_write_record = inserted_pos_old->writes_at;
  	while(ptr_write_record->next != NULL)
		ptr_write_record = ptr_write_record->next;
	ptr_write_record->next = new_write_record;
	new_write_record->prev = ptr_write_record;
   }
  else//Locked for first time//
  	inserted_pos_old->writes_at = new_write_record;
 
   if(inserted_pos_new != NULL)
  {
	 /*Get a new write_record node*/
	  new_write_record = (struct write_records *)malloc(sizeof(struct write_records));
 
	  /*populate the new node*/
	  new_write_record->initial_off = 0;
	  new_write_record->write_length = -2;/*newpath*/
  	  new_write_record->LRN = log_rec_no;/*record number in log where data will be written will enter later*/
  	  new_write_record->next = NULL;
 	  new_write_record->prev = NULL;
	  

  	/*insert new write_record node in the linked list of all writes to the file associated with the file node*/
	 if(inserted_pos_new->writes_at != NULL)/*NULL if locking for first time else non NULL*/
  	{
  		ptr_write_record = inserted_pos_new->writes_at;
  		while(ptr_write_record->next != NULL)
			ptr_write_record = ptr_write_record->next;
		ptr_write_record->next = new_write_record;
		new_write_record->prev = ptr_write_record;
   	}
  	else//Locked for first time//
 		 inserted_pos_new->writes_at = new_write_record;
   }
    
}

void creat_log(char *incall[],struct file_lock *inserted_pos,int txnid)
{
  long log_rec_no;
  struct write_records *new_write_record, *ptr_write_record;
   
  /*make a log entry from where creat in place will be done on txncommit*/
  log_rec_no = log_creat(txnid,incall);

   
   /*Now in the write_at record of the parent of new file and update the log_rec_no*/
   /*No need to lock file_mutex as we already know the positions*/
     
  /*Get a new write_record node*/
  new_write_record = (struct write_records *)malloc(sizeof(struct write_records));
 
  /*populate the new node*/
  new_write_record->initial_off = 0;
  new_write_record->write_length = -4;/* -4 is code for creat*/
  new_write_record->LRN = log_rec_no;/*record number in log where data will be written will enter later*/
  new_write_record->next = NULL;
  new_write_record->prev = NULL;

  /*insert new write_record node in the linked list of all writes to the file associated with the file node. This list may not be empty as the parent directory may be locked previously by the same txn.*/
  if(inserted_pos->writes_at != NULL)/*NULL if locking for first time else non NULL*/
  {
  	ptr_write_record = inserted_pos->writes_at;
  	while(ptr_write_record->next != NULL)
		ptr_write_record = ptr_write_record->next;
	ptr_write_record->next = new_write_record;
	new_write_record->prev = ptr_write_record;
   }
  else//Locked for first time//
  	inserted_pos->writes_at = new_write_record; 
}


void unlink_log(char *incall[],struct file_lock *inserted_pos_child, struct file_lock *inserted_pos_parent, int txnid)
{
 long log_rec_no;
  struct write_records *new_write_record, *ptr_write_record;
   
  /*make a log entry from where creat in place will be done on txncommit*/
  log_rec_no = log_unlink(txnid,incall);

   
   /*Now in the write_at record of the to-be-deletd file's parent update the log_rec_no*/
   /*No need to lock file_mutex as we already know the positions*/
     
  /*Get a new write_record node*/
  new_write_record = (struct write_records *)malloc(sizeof(struct write_records));
 
  /*populate the new node*/
  new_write_record->initial_off = 0;
  new_write_record->write_length = -5;/* -5 is code for unlink*/
  new_write_record->LRN = log_rec_no;/*record number in log where data will be written will enter later*/
  new_write_record->next = NULL;
  new_write_record->prev = NULL;

  /*insert new write_record node in the linked list of all writes to the file associated with the file node. This list may not be empty.*/
  if(inserted_pos_parent->writes_at != NULL)/*NULL if locking for first time else non NULL*/
  {
  	ptr_write_record = inserted_pos_parent->writes_at;
  	while(ptr_write_record->next != NULL)
		ptr_write_record = ptr_write_record->next;
	ptr_write_record->next = new_write_record;
	new_write_record->prev = ptr_write_record;
   }
  else//Locked for first time//
  	inserted_pos_parent->writes_at = new_write_record; 

}
