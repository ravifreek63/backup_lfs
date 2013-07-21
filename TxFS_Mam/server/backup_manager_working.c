#include "my_header.h"

struct stack_type{
	char path[300];
	size_t path_len;
	long int offset; /*-1 in NULL*/
};

struct stack_type stack[20],curr;
int top=0;/*top points to top of stack. Insert should be at top+1 and on deletion top = top-1*/
int backup_file, read_indicate;
struct stat path_buf;
//DIR *fd_dir;
//int fd_file;
struct dirent *s_dirent;

void copying()
{
  struct tala{
                        int rec_no;
                        long file_inode;
			char empty[500];
   };
  FILE *file_locking_write, *file_locking_read;
  struct tala t,f;
  int i,j,count;
  file_locking_write = fopen("/home/lipika/Examples/abc1.bin","rb+");
  file_locking_read = fopen("/home/lipika/Examples/abc.bin","rb+");
  count = rand()%20000;

  for(i=0;i<count;i++)
  {
	if (fread(&f,sizeof(struct tala),1,file_locking_read) ==0)
 		 printf("fread did not work");
	if (fwrite(&f,sizeof(struct tala),1,file_locking_write)== 0)
 		 printf("fwrite did not work");
  }

  fclose(file_locking_write);

  fclose(file_locking_read);
}


int is_read(char *path)
{
 int read_path;
 if(path_buf.st_mode & S_ISVTX)/*Sticky*/
	 read_path = 1;/*If sticky then 1 else 0*/
 else
 	read_path = 0;
 /*If read_indicate and read_path same then concerned file is already read hence return 1 else 0*/

 if(read_indicate == read_path)
	return 1;
 else
	return 0;
}


void push(char *path, long int offset)
{
  top++;
  bzero(stack[top].path,300);
  //stack[top].path = malloc(sizeof(char)*strlen(path));
  stack[top].path_len = strlen(path);
  strcpy(stack[top].path,path);
  stack[top].offset =offset;
} 

void pop()
{
  //struct stack_type t;
  //t = malloc(sizeof(struct stack_type));
  bzero(curr.path,300);
  //t->path = malloc(sizeof(char)*(stack[top].path_len));
  strncpy(curr.path,(stack[top].path),(stack[top].path_len));
  curr.path[(stack[top].path_len)+1] = '\n';
  curr.path_len = stack[top].path_len;
  curr.offset = stack[top].offset;
  //free(stack[top].path);
  top--;
  return ;
}

//Locking the file before reading
struct file_lock *lock_file(char *path)
{
 struct stat buff;
 long f_inode,f_length;
 int search_ret,f_flags;
 struct file_lock *loc=NULL, *inserted_pos;
 
 if(stat(path,&buff)!=0)//transfer inode to buff
        error("Could not get file status");
 f_inode = (long)buff.st_ino;//from inode extract only inode_no.
 //f_length = (long)buff.st_size;//from inode extract size of file

 /*file-mode is shared or exclusive. File flag is RDONLY,WRONLY,RDWR,APPEND*/
 f_flags = 1;/*WRONLY and hence will open in exclusive mode i.e with write lock*/

 pthread_mutex_lock(&file_mutex);

 /*Search if file is already locked or not*/
 search_ret= search_file_list(f_inode,&loc);/*0=not found,1=found*/

 /*Inserting new node into lock list i.e loking the file if possible else place calling transaction in waiting mode*/
 inserted_pos = insert_file_list(f_inode,path,f_flags,0,search_ret,loc);/*txnid is 0 as backup*/

 printf("\nBf(%s)",path);

 pthread_mutex_unlock(&file_mutex);

 return inserted_pos;

}

//Locking the directory before reading
struct file_lock *lock_directory(char *path)
{
 struct stat buff;
 long d_inode,d_length;
 int search_ret,d_flags;
 struct file_lock *loc=NULL, *inserted_pos;
 
 if(stat(path,&buff)!=0)//transfer inode to buff
        error("Could not get file status");
 d_inode = (long)buff.st_ino;//from inode extract only inode_no.
 //f_length = (long)buff.st_size;//from inode extract size of file

 /*file-mode is shared or exclusive. File flag is RDONLY,WRONLY,RDWR,APPEND*/
 d_flags = 1;/*WRONLY and hence will open in exclusive mode i.e with write lock*/

 pthread_mutex_lock(&file_mutex);

 /*Search if file is already locked or not*/
 search_ret= search_file_list(d_inode,&loc);/*0=not found,1=found*/

 /*Inserting new node into lock list i.e loking the file if possible else place calling transaction in waiting mode*/
 inserted_pos = insert_file_list(d_inode,path,d_flags,0,search_ret,loc);/*txnid is 0 as backup*/

 printf("\nBd(%s)",path);

 pthread_mutex_unlock(&file_mutex);

 return inserted_pos;

}

/*Releasing lock of one file or directory as it completes reading and also reading of its children*/
void release_locks_by_backup(int txnid,long f_inode)
{
   struct txn_node *ptr_txn_node;
   struct file_lock *txn_id_file_list,*ptr_file_lock,*foll_file_lock;
   struct wait_node *wait_list_loc;

   /*get the file list locked by the commiting or aborting transaction txn_id*/
   pthread_mutex_lock(&txn_mutex);/*locking as pointers may be changed by outher transactions while this is traversing it*/
   ptr_txn_node = head_txn_list;
   while(ptr_txn_node->txn_id != txnid)
	ptr_txn_node = ptr_txn_node->next;
   pthread_mutex_unlock(&txn_mutex);
   
   /*txn_id_file_list will hold the pointer to the head of the file list held by txn_id*/
   txn_id_file_list = ptr_txn_node->file_rec_no;
   foll_file_lock = NULL;
   
   pthread_mutex_lock(&file_mutex);
   
   while(txn_id_file_list->file_inode != f_inode)
   {
	foll_file_lock = txn_id_file_list;
	txn_id_file_list = txn_id_file_list->next_txn;
    }

   /*txn_id_file_list is pointing to the file to be closed*/

   ptr_file_lock = txn_id_file_list;

   /*Update pointers of the file locked list of the backup transcation*/
   if(foll_file_lock == NULL)/*file to be closed is the first file in the list*/
	ptr_txn_node->file_rec_no = txn_id_file_list->next_txn;
   else
	foll_file_lock->next_txn = txn_id_file_list->next_txn;
    
	
   /*find out if wait list for the file pointed at by txn_id_file_list is present*/		
   wait_list_loc = search_wait_list(ptr_file_lock->file_inode);

   //printf("\ntxn_id_file_list %d",txn_id_file_list->file_inode);
   if (wait_list_loc == NULL)/*no tranxaction is waiting,hence simply close the file and release the node*/
   {	
	delete_file_list(ptr_file_lock);
    }
   else/*there is a wait list present*/
   {
		if(ptr_file_lock->file_mode == 0)/*file to be released is locked in shared mode*/
		{
			/*if only transaction holding a shared lock then have to perform the wakeup process else adjust pointers in file list*/
			if((ptr_file_lock->next_shared_rec == NULL) && (ptr_file_lock->prev_shared_rec == NULL))
			{
				/*delete node from lock list*/
	                        delete_file_list(ptr_file_lock);
                	        wake_up(wait_list_loc);
			}
			else
			{
				if((ptr_file_lock->next_shared_rec == NULL)&&(ptr_file_lock->prev_shared_rec !=NULL))
					(ptr_file_lock->prev_shared_rec)->next_shared_rec = NULL;
				if((ptr_file_lock->prev_shared_rec == NULL)&&(ptr_file_lock->next_shared_rec != NULL))
					(ptr_file_lock->next_shared_rec)->prev_shared_rec = NULL;
				if((ptr_file_lock->next_shared_rec != NULL) && (ptr_file_lock->prev_shared_rec != NULL))	
				{
					(ptr_file_lock->prev_shared_rec)->next_shared_rec = ptr_file_lock->next_shared_rec;
					(ptr_file_lock->next_shared_rec)->prev_shared_rec = ptr_file_lock->prev_shared_rec;
				}
                                
				delete_file_list(ptr_file_lock);
			}
		}

		else/*file to be released is locked in exclusive mode*/	
		{
                       /*delete node from lock list*/
			delete_file_list(ptr_file_lock);
			wake_up(wait_list_loc);
		}
   }	
   
   //printf("\n%d has released lock on %ld and now unlocking file_mutex",txnid,f_inode);
   //printf("\n Status:");
   //print_lists();
   pthread_mutex_unlock(&file_mutex);		
		
}
void copy_inode()
{
 char str[100],str_temp[20];
 
 bzero(str,100);
 bzero(str_temp,20);

  snprintf(str_temp,sizeof(str_temp),"%ld",path_buf.st_ino);
  strcat(str,"\n");
  strcat(str,str_temp);
  strcat(str,"##");/*to seperate from next entry which is the contents of the directory*/
  //printf("\n %s",str); 
  fflush(stdout); 
  write(backup_file,str,strlen(str));
}

void copy_directory(char *path)
{
  int i;
  char str[300];
  bzero(str,300);

  strcat(str,"copied contents of directory");/*did this and not the commented code because the str length becomes huge...ofcourse there is way out..but this will do too*/
  strcat(str,path); 
  write(backup_file,str,strlen(str));
  copying();
}

void copy_file(char *path)
{
  int i,j;
  //void *read_buffer;
  char read_buffer[300];
  ssize_t nbyte;
 bzero(read_buffer,300);

  //read_buffer = malloc(sizeof(char)*nbyte);/Just to keep it simple.This works*/
  
  //if(read(fd_file,read_buffer,nbyte) == 0)
//	printf("\nError on reading file");
  //else
  strcat(read_buffer,"Copying file");
  strcat(read_buffer,path);
  nbyte=strlen(read_buffer);
	write(backup_file,read_buffer,nbyte);
  copying();

  return;
}

void mark_as_read(char *path)
{
  mode_t new_mode;
  /*Mark as readd, If sticky reset it(so 0 means read now) and if non sticky set it(so sticky bit set means read now)*/
  if(path_buf.st_mode & S_ISVTX)/*resetting as it was Sticky*/
  {
         new_mode = path_buf.st_mode& ~S_ISVTX;
        chmod(path,new_mode);
       // printf("\nSTICKY");
        fflush(stdout);
  }
 else{/*setting as it was non sticky*/
        new_mode = path_buf.st_mode|S_ISVTX;
        chmod(path,new_mode);
       // printf("\nNOT STICKY");
        fflush(stdout);
  }
}


int online_bck(char *argv[])
{
  DIR *fd_dir;
  int fd_file;
  char path[300];
  struct file_lock *inserted_pos;

  //struct stack_type *curr;

  bzero(path,300);
  //printf("\n ONLINE_BCK %s %s",argv[1],argv[2]);

  /*argv[1] is the root of the tree to be backed up and argv[2] is the destination file*/
 strcpy(path,argv[1]);
 backup_file = open(argv[2],O_WRONLY);

 
/*Initialize read_indicate variable*/
 if(stat(path,&path_buf) == -1)
          perror("stat1");

 if(path_buf.st_mode & S_ISVTX)/*If root is Sticky, initialize to opposite i,e 0*/
          read_indicate = 0;
 else
          read_indicate = 1;
 
 push(path,-1);

 while(top != 0)
 {
	bzero(path,300);
	pop();
        strcpy(path,curr.path);
	//printf("\n %s",path);
	//fflush(stdout);
        
	if(stat(path,&path_buf) == -1)
	{
	        perror("stat2");
		printf("\n %s",path);
		continue;//for symbolic links like .so files like libfuncs.so for which the target may not be present.
	}
	
	 if(S_ISDIR(path_buf.st_mode)!=0)/*It is a directory*/
	 {
		//printf("\n%s is a directory",path);
		/*check if read or unread*/
		if(!is_read(path))/*If 0 is returned then the directory is not read*/
		{
		
			/*The function lock_directory will lock the file and return the position in the file list where it is inserted.The thread may be blocked in the middle if locking was not possible*/

			inserted_pos = lock_directory(path);/*Lock directory in exclusive mode*/

			//printf("\n%s has not been read",path);
			copy_inode();/*copy the inode*/
			
			fd_dir = opendir(path);
			if(fd_dir == NULL)
        			perror("opendir");
		
			/*insert fd into file list*/
			//inserted_pos->fd_dir = fd_dir;
	 		inserted_pos->file_offset = 0;

			copy_directory(path);/*copy directory contents*/
			
			mark_as_read(path);/*mark the directory as read*/
			
			//printf("\n downgrading lock on %s",path);
			/*downgrade the lock to shared mode :was done before 
			As on 3/nov/2011 changing it to releasing locks here*/
			//inserted_pos->flags = 0; commented on 3.nov.2011
			release_locks_by_backup(0,(long)path_buf.st_ino);/*txnid =0, and the file inode to release*/

			
			/*fd_dir is still open and hence go to the beginning of the directory*/
			rewinddir(fd_dir);
			
			while((s_dirent = readdir(fd_dir)) != NULL)
			{
        			if('.' == s_dirent->d_name[0])
              				continue;
        			else
					break;
			}
			if(s_dirent == NULL)/*Empty directory, No children*/
			{
				//printf("\nreleasing locks of %s",path);
				/*release locks and close file*/
				//release_locks_by_backup(0,(long)path_buf.st_ino);/*txnid =0, and the file inode to release*/commented on 3.nov.2011
				
				if(closedir(fd_dir)!=0)
					perror("closedir1");
		
			}
			else/*s_dirent hold the first child of current directory*/
			{
				//printf("\nExtracting first child");
				/*Push the current in, update its offset*/
				push(path,telldir(fd_dir));
				/*if(closedir(fd_dir)!=0)
					perror("closedir2");moved it to after push(path,-1) due to valgrind error. Should not access data, returned by readdir() after calling a closedir(). This is because closedir() may free any resources like memory allocated in opendir/readdir*/	
				
				/*push the child in, offset is -1*/								
				strcat(path,"/");
				strcat(path,s_dirent->d_name);
				push(path,-1);
  				if(closedir(fd_dir)!=0)
					perror("closedir2");
				
			}	
      		}
		else/*If 1 is returned then the directory has already been read*/
		{
			//printf("\n %s has been read",path);
			fd_dir = opendir(path);

  			if(fd_dir == NULL)
        			perror("Error opening directory"); 
			/*move to the position till which previously read*/
			seekdir(fd_dir,curr.offset);
			/*Read from that position*/
			while((s_dirent = readdir(fd_dir)) != NULL)
                        {
                                if('.' == s_dirent->d_name[0])
                                        continue;
                                else
                                        break;
                        }
                        if(s_dirent == NULL)/* No more children*/
			{

				
				/*release locks and close directory*/
				//release_locks_by_backup(0,(long)path_buf.st_ino);/*txnid =0, and inode*/commented on 3.nov.2011
				
				if(closedir(fd_dir)!=0)
					perror("closedir1");
								
			}
                        else/*s_dirent hold the next child of current directory*/
                        {
				//printf("\nExtracting next child");
                                /*Push the current in, update its offset*/
                                push(path,telldir(fd_dir));
                               /* if (closedir(fd_dir)!=0)
					perror("closedir4");putting it after push(path,-1)as closedir release memory of s_dirent*/
				
                                /*push the child in, offset is -1*/
                                strcat(path,"/");
                                strcat(path,s_dirent->d_name);
                                push(path,-1);
				
				if (closedir(fd_dir)!=0)
					perror("closedir4");


                                }
		}

	}
	else
	{
	  if(S_ISREG(path_buf.st_mode)!=0)/*It is a regular file*/
	  {
		                
		/*The function lock_file will lock the file and return the position in the file list where it is inserted.The thread may be blocked in the middle if locking was not possible*/

		inserted_pos = lock_file(path);/*Lock file*/

		//printf("\n%s is a regular file",path);
		 copy_inode();/*copy the inode*/
               
		fd_file = open(path,O_RDONLY);
		if(fd_file == -1)
		{
			printf("\n %s",path);
			perror("\nError opening file");
                 }
		/*insert fd into file list*/
		inserted_pos->fd = fd_file;
 		inserted_pos->file_offset = 0;

                //printf(" copy_file %s",path);
		copy_file(path);		

		mark_as_read(path);
		
		/*release locks and close file*/
		release_locks_by_backup(0,(long)path_buf.st_ino);/*txnid =0*/
		if(close(fd_file) != 0)
			perror("Error on close");
		/*delay next read*/
		//sleep(5);
	
	  }	
	 }
  }
}

