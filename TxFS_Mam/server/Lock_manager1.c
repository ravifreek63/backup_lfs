#include "my_header.h"
 
//struct file_lock *loc;/*loc points to node of same inode if file already locked(shared or exclusive) else at node after which new node is to be inserted*/
extern void error(const char *msg);
extern int mutually_serializable(int txnid, int files_read_bit, char *file_path,long f_inode,int f_flags);
/*if no conflict return 1 and processing continues else return 0*/
extern void delayed_update(struct file_lock *file_update,int txnid);
extern void delayed_write(struct file_lock *file_to_write);
extern int tr_yaffs_close(int file_descriptor, unsigned int txnid);

int file_open_flags(char *argument[])
{
  if ((strcmp(argument[2],"0")==0))/*O_RDONLY*/
   return(0);
 if ((strcmp(argument[2],"1")==0))/*O_WRONLY*/
   return(1);
 if ((strcmp(argument[2],"2")==0))/*O_RDWR*/
   return(2);
 if ((strcmp(argument[2],"1025")==0))/*O_WRONLY|O_APPEND*/
    return(3);
 if ((strcmp(argument[2],"1026")==0))/*O_RDWR|O_APPEND*/
   return(4);
}

struct file_lock *get_file_node(long f_inode,long f_length,int f_flags,int txnid)
{
 struct file_lock *new_file_node;
 
 new_file_node = (struct file_lock *)malloc(sizeof(struct file_lock));

 /*Initializing*/
 new_file_node->file_inode = f_inode;
  if(f_flags==0)/*0=shared,1=exclusive*/
	new_file_node->file_mode=0;
  else
	 new_file_node->file_mode = 1;
   new_file_node->fd= -1;/*file descriptor of the file for this particular transaction, -1 = uninitialised*/
   new_file_node->file_offset = -1;/*-1=uninitialised. for this particular file descriptor*/
   new_file_node->file_size = f_length;
   new_file_node->flags = f_flags;/*0-O_RDONLY,1-O_WRONLY,2-O_RDWR,3-O_APPEND*/
   new_file_node->parent_txn = txnid;
   new_file_node->next_txn = NULL;/*points to next locked file of same tranxaction*/
   new_file_node->prev_shared_rec = NULL;/*points tp previous record of same file locked in compatible mode*/
   new_file_node->next_shared_rec = NULL;/*points to next record of same file locked in compatible mode*/
   //new_file_node->txn_wait = NULL;/*points to tranxaction waiting on this lock*/
   new_file_node->next = NULL;
   new_file_node->prev = NULL;
   new_file_node->writes_at = NULL;
 
 return new_file_node; 
}

void free_file_node(struct file_lock *file_node_free)
{
	/*nullifying pointers*/
	file_node_free->next_txn = NULL;
	file_node_free->prev_shared_rec = NULL;
	file_node_free->next_shared_rec = NULL;
	file_node_free->next = NULL;
	file_node_free->prev = NULL;
	free(file_node_free);
}
/*Cases: 1. Empty list, retrun 0 and loc is NULL. 2.Searched inode is 1st in list, return 1, loc is head. 3. Searched inode is in middle, return 1, loc is first node with same inode. 4. Searched inode is last, return 1 and loc is the last node. 5. Searched node not present but lesser then first in list return 0, loc is head.6.Searched node not present and is greater then last return 0 and loc as last node 7. Searched node not present and in middle, return 0 and loc is the node just lesser then serached inode*/

int search_file_list(long f_inode,struct file_lock **add_loc)
{
 struct file_lock *ptr, *foll;
 
 if(head_lock_list == NULL){
//	printf("\n head NULL");
	*add_loc = NULL;
	return(0);
 }

 foll = head_lock_list;
 ptr = foll->next;

while(ptr!=NULL)
{ 
  if(ptr->file_inode >= f_inode)
	break;

  foll = ptr;
  ptr = ptr->next;
}

if(ptr!=NULL)
{
 if(ptr->file_inode == f_inode){
	*add_loc = ptr;
	return (1);
 }
 else{
   if(foll->file_inode == f_inode){
	*add_loc = foll;
	return(1);
	}
   else{	
	*add_loc = foll;
	return(0);
 }
 }
}
else{
/*special case:one node in list which may be lesser,equal or greater the f_inode, or last node is lesser the f_inode*/
    if(foll->file_inode == f_inode){
	*add_loc = foll;
	return(1);
	}
    else{
	*add_loc = foll;
  	return (0);
	}
}
}
  

void delete_file_list(struct file_lock *ptr_file_node)
{
	 if(head_lock_list == ptr_file_node)/*to be released file is the first file*/
        {
                head_lock_list = ptr_file_node->next;
		if(ptr_file_node->next != NULL)/*i.e ptr_file_node is not the last and only file in the list*/
                	(ptr_file_node->next)->prev = head_lock_list;
                free_file_node(ptr_file_node);
	}
        else{
                if(ptr_file_node->next == NULL)/*to be released file is the last file*/
                {
                        (ptr_file_node->prev)->next = NULL;
                        free_file_node(ptr_file_node);
                }
                else{
                        (ptr_file_node->prev)->next = ptr_file_node->next;
                        (ptr_file_node->next)->prev = ptr_file_node->prev;
                        free_file_node(ptr_file_node);
                     }
        }

}

/*Insert into linked list of files locked by same transaction*/
void insert_same_txn_list(int txnid,int read_bit,struct file_lock *new_file_node)
{
 struct txn_node *ptr_txn_node;
 struct file_lock *ptr_file_node;

 pthread_mutex_lock(&txn_mutex);/*locking as pointers may be changed by outher transactions while this is traversing it*/

  ptr_txn_node = head_txn_list;
  while(ptr_txn_node->txn_id != txnid)
  	ptr_txn_node = ptr_txn_node->next;

 pthread_mutex_unlock(&txn_mutex);

  if(ptr_txn_node->file_rec_no == NULL)/*this is the first file to be locked*/
  {
  	ptr_txn_node->file_rec_no = new_file_node;
	ptr_txn_node->before_after = read_bit;
  }
 else{
 	ptr_file_node = ptr_txn_node->file_rec_no;
        while(ptr_file_node->next_txn!=NULL)
        	ptr_file_node = ptr_file_node->next_txn;
        ptr_file_node->next_txn = new_file_node;
 }

}
/*Insert into lock list if allowed to lock else block the calling transaction and put it in wait list*/ 
struct file_lock *insert_file_list(long f_inode,char *file_path,int f_flags,int txnid,int search_ret,struct file_lock *loc)
{
  //printf("In insert file_list\n");

  struct file_lock *new_file_node,*ptr,*file_ptr;
  struct txn_node *ptr_txn_node,*cur_txn_node;
  struct wait_node *loc_in_wait;
  struct stat buff;
  long f_length = 0;
  int read_bit,ret_MS=-2, ret_on_wake;
  loc_in_wait = NULL;
   /*
    * TODO  Implementation to get the length of the file
    */

    
  
  /* if(stat(file_path,&buff)!=0)//transfer inode to buff
        error("Could not get file status");
   f_length = (long)buff.st_size;//from inode extract size of file
   if(buff.st_mode & S_ISVTX)Sticky
         read_bit = 1;If sticky then 1 else 0
   else
        read_bit = 0;*/
   //printf("\n %s read_bit %d",file_path,read_bit);
   /*allocating new node*/
  new_file_node = get_file_node(f_inode,f_length,f_flags,txnid);
  //if(new_file_node == NULL)  printf("LockManager1.c :: insert_file_list :: file node NULL\n");
  //else printf("new_file_node not NULL\n");
  /*update pointer to present transaction.need to lock as pointers may change while traversing the list*/
  pthread_mutex_lock(&txn_mutex);
 
 // if (head_txn_list == NULL)
//	printf("Error : LockManager1.c : insert_file_list : head_txn_list = NULL\n");
 // else  printf("head_txn_list not NULL\n");
  cur_txn_node = head_txn_list;
  while(cur_txn_node->txn_id != txnid)
	cur_txn_node = cur_txn_node->next;
  pthread_mutex_unlock(&txn_mutex);
    
  if(search_ret == 0)/*file is not previously locked*/
  {
		
        /*MS check before inserting the file into lock list i.e before locking the file*/
	if(txnid !=0)
	{
		if(BACKUP == INACTIVE)/*no need to serialize with backup*/
		{
			//printf("\n Backup inactive");
			ret_MS = 1;
		}
		else
			{
			//	printf("Backup Active");
				ret_MS = mutually_serializable(txnid,read_bit,file_path,f_inode,f_flags);
				//printf("ret_MS = %d\n", ret_MS);
			}
	}
	//printf("\n ret_MS %d txnid->before_after %d file read bit %d",ret_MS,cur_txn_node->before_after,read_bit);
	if((ret_MS == 1)||(ret_MS == -2))/*txnid is MS with the backup transaction or it is the backup transaction and hence continue locking it*/
	{
	
		if(loc == NULL)/*empty file list*/
		{
			head_lock_list = new_file_node;
			new_file_node->prev = head_lock_list;
			//printf("File List Empty, Inserting file node\n");
		}
		else{
			if((loc==head_lock_list)&&(loc->file_inode > f_inode))/*f_inode is smaller then the first file in the list*/
			{
				new_file_node->next = loc;
				loc->prev = new_file_node;
				new_file_node->prev = new_file_node;
				head_lock_list = new_file_node;
			}
			else{
	  			new_file_node->next = loc->next;
				if(loc->next != NULL)
					(loc->next)->prev = new_file_node;
    	 			loc->next = new_file_node;
				new_file_node->prev = loc;
			}
		}
         	/*update txn_node list to connect locked files of the transaction*/
		insert_same_txn_list(txnid,read_bit,new_file_node);
	}
	/*else if 0 is returned from the mutually_serializable*/
	else{
	 if(ret_MS == 0)/*was paused*/
	   {
		/*release the space occupied by new_file_node*/
		free_file_node(new_file_node);
		
		/*trying locking the file again "hoping" it now does not conflict with tb*/
	    	new_file_node = resume(f_inode,file_path,f_flags,txnid);
	  }
	 else/*transaction has been aborted*/
	  {
		/*release the space occupied by new_file_node*/
		free_file_node(new_file_node);

	  	new_file_node = NULL;
	}
	}
  }
  else/*file locked previously in either shared or exclusive mode*/
  {
	/*file may be locked by same transaction or different transaction. Same transaction in case of operation example 
          1.  165 rename(/media/New Volume/Lipika/lipika/.thumbnails/normal/6cec9b6ffc6c1e52f14c22675fb63c57.png,/media/New Volume/Lipika/lipika/Lipika/ March/Sep_2009/OCT272009/new)
	2. 195 rename(/media/New Volume/Lipika/lipika/ March2010/Sep_2009/Interesting file system papers/NTFS/cc163388.aspx_files/clear.gif,/media/New Volume/Lipika/lipika/Lipika/ March/Sep_2009/OCT272009/new)*/
	/*if locked by same transaction no need to lock again*/
	if(loc->parent_txn == txnid)/* assuming if file is locked by same transaction then it is done so in only exclusive mode(not necessarily in real traces). HAve controlled this in the trace generation*/
		return loc;
	/*if locked by different transaction do the following*/
	if(loc->file_mode == 1)/*file is locked in exclusive mode*/
	{
		
		/*check if a  wait list is already present*/
		loc_in_wait = search_wait_list(f_inode);
		
		if(loc_in_wait == NULL)/*no wait list*/
		{
			
			if(f_flags == 0)
			{
				cur_txn_node->cur_state = WAITING;
				cur_txn_node->waiting_for_inode = f_inode;
				cur_txn_node->wait_mode = 0;/*i.e shared*/
			}
			else
			{
				cur_txn_node->cur_state = WAITING;
				cur_txn_node->waiting_for_inode = f_inode;
				cur_txn_node->wait_mode = 1;/*i.e exclusive*/
			}
			
			/*free space occupied by new_file_node*/
			free_file_node(new_file_node);
			
			/*block thread and insert into wait list*/		
			ret_on_wake = insert_wait_list(f_inode,txnid);
			
			if(ret_on_wake == 1)
			{
				/*this transaction has been woken up and I proceed with the transaction*/
				cur_txn_node->cur_state = RUNNING;
				cur_txn_node->waiting_for_inode = 0;
				cur_txn_node->wait_mode = -1;
				new_file_node = resume(f_inode,file_path,f_flags,txnid);
			
				/*wake up further sleeping transactions if the present waking transaction has opened in shared mode and if any other transactions are sleeping on this file lock*/
				/*if(f_flags == 0)
					continue_wake_up(f_inode);*/
			}
			else
				new_file_node = NULL; 			
		} 
		else/*waitlist*/
		{
					
			/*Update transaction entry,increase in_wait count and call pthread_cond_wait*/
			if(f_flags == 0)
			{
				cur_txn_node->cur_state = WAITING;
				cur_txn_node->waiting_for_inode = f_inode;
				cur_txn_node->wait_mode = 0;/*i.e shared*/
			}
                        else
			{
				cur_txn_node->cur_state = WAITING;
				cur_txn_node->waiting_for_inode = f_inode;
                                cur_txn_node->wait_mode = 1;/*i.e exclusive*/
			}


			 /*free space occupied by new_file_node*/
                        free_file_node(new_file_node);
                        
			/*block thread and insert into wait list*/
                        ret_on_wake = update_wait_list(loc_in_wait,txnid);
			
			if(ret_on_wake == 1)
			{	
			 	/*this transaction has been woken up and I proceed with the transaction*/
			 	cur_txn_node->cur_state = RUNNING;
                        	cur_txn_node->waiting_for_inode = 0;
                        	cur_txn_node->wait_mode = -1;
                       		new_file_node = resume(f_inode,file_path,f_flags,txnid);

				 /*wake up further sleeping transactions if the present waking transaction has opened in shared mode and if any other transactions are sleeping on this file lock*/  
                        	/*if(f_flags == 0)
					continue_wake_up(f_inode);*/
			}
			else
				new_file_node = NULL;
		}
	}
	else/*file locked in shared mode*/
	{
		/*check if a  wait list is already present*/
                loc_in_wait = search_wait_list(f_inode);

		if(loc_in_wait == NULL)/*no wait list*/
		{
			
			if(f_flags == 0)/* request is also for a shared mode lock*/
			{
	
			 /*MS check before inserting the file into lock list i.e before locking the file*/
			if(txnid != 0)
		        	ret_MS = mutually_serializable(txnid,read_bit,file_path,f_inode,f_flags);
			//printf("\n In shared ret_MS %d",ret_MS);
			if((ret_MS == 1) ||(txnid == 0))
			{		
			 	/*loc points to first occurence of the file locked in shared mode*/
			 	ptr = loc;
			 	while(ptr->next_shared_rec!=NULL)
					ptr = ptr->next_shared_rec;
				/*updating prev and next*/
				new_file_node->next = ptr->next;
				if(ptr->next != NULL)
					(ptr->next)->prev = new_file_node;
				ptr->next = new_file_node;
				new_file_node->prev = ptr;
				/*updating shared list*/
				new_file_node->prev_shared_rec = ptr;
				ptr->next_shared_rec = new_file_node;
				/*update same txn list*/
	  			insert_same_txn_list(txnid,read_bit,new_file_node);
			}
			else{
			         if(ret_MS == 0)/*was paused*/
            				new_file_node = resume(f_inode,file_path,f_flags,txnid);
         			else/*has been aborted*/
           				new_file_node = NULL;
        		}

			} 
			else/*request is for a exclusive lock hence has to wait for lock release*/
			{
			  cur_txn_node->cur_state = WAITING;
			  cur_txn_node->waiting_for_inode = f_inode;
			  cur_txn_node->wait_mode = 1;/*i.e exclusive*/
		   
                         /*free space occupied by new_file_node*/
                        free_file_node(new_file_node);
                                             

                         /*block thread and insert into wait list*/
			 ret_on_wake = insert_wait_list(f_inode,txnid);

			 /*if ret_on_wake = 1 then continue with the execution. If 0 then return NULL as the transaction has been aborted*/		  				
			if(ret_on_wake == 1)
			{	
			 	/*this transaction has been woken up and I proceed with the transaction*/
			 	cur_txn_node->cur_state = RUNNING;
                        	cur_txn_node->waiting_for_inode = 0;
                        	cur_txn_node->wait_mode = -1;
                        	new_file_node = resume(f_inode,file_path,f_flags,txnid);
 
			/*wake up further sleeping transactions if the present waking transaction has opened in shared mode and if any other transactions are sleeping on this file lock*/  
        			/*if(f_flags == 0)
		                	continue_wake_up(f_inode);*/
			}
			else
				new_file_node = NULL;

			}	
		}
		else/*file is locked in shared mode and wait list present*/
		{
			/*if the directory locked in shared mode is owned by tb and the requested lock is also of a shred mode the locking can proceed even if there is a wait list of exclusive lock requests. This is because reading can happen but writing cannot when a directories children are being copied*/
			/*loc holds the location of first file occurence of this file inode*/
			file_ptr = loc;
			while(file_ptr->parent_txn != 0)/*0 is the txnid of backup*/
			{
				file_ptr = file_ptr->next_shared_rec;
				if(file_ptr == NULL)
					break;
			}
			
			if((file_ptr!= NULL)&&(f_flags == 0))/*if file_ptr != NULL that means it is equal to tb.this is the case then shared lock requests are granted*/
			{
				 /*MS check before inserting the file into lock list i.e before locking the file*/
				if(txnid != 0)
		        		ret_MS = mutually_serializable(txnid,read_bit,file_path,f_inode,f_flags);
				//printf("\n In shared ret_MS %d",ret_MS);
				if((ret_MS == 1) ||(txnid == 0))
				{		
			 		/*loc points to first occurence of the file locked in shared mode*/
			 		ptr = loc;
			 		while(ptr->next_shared_rec!=NULL)
						ptr = ptr->next_shared_rec;
					/*updating prev and next*/
					new_file_node->next = ptr->next;
					if(ptr->next != NULL)
						(ptr->next)->prev = new_file_node;
					ptr->next = new_file_node;
					new_file_node->prev = ptr;
					/*updating shared list*/
					new_file_node->prev_shared_rec = ptr;
					ptr->next_shared_rec = new_file_node;
					/*update same txn list*/
	  				insert_same_txn_list(txnid,read_bit,new_file_node);
				}
				else{
			        	 if(ret_MS == 0)/*was paused*/
            					new_file_node = resume(f_inode,file_path,f_flags,txnid);
         				else/*has been aborted*/
           					new_file_node = NULL;
        			}					
			}
			else{
				 /*Update transaction entry,increase in_wait count and call pthread_cond_wait*/
                        	if(f_flags == 0)
				{
					cur_txn_node->cur_state = WAITING;
					cur_txn_node->waiting_for_inode = f_inode;
                        	        cur_txn_node->wait_mode = 0;/*i.e shared*/
				}
                        	else
				{
					cur_txn_node->cur_state = WAITING;
					cur_txn_node->waiting_for_inode = f_inode;
                        	        cur_txn_node->wait_mode = 1;/*i.e exclusive*/
				}
			
				 /*free space occupied by new_file_node*/
                        	free_file_node(new_file_node);
                        
				/*block thread and insert into wait list*/
                        	ret_on_wake = update_wait_list(loc_in_wait,txnid);
			
				if(ret_on_wake == 1)
				{
					 /*this transaction has been woken up and I proceed with the transaction*/
					 cur_txn_node->cur_state = RUNNING;
        	        	        cur_txn_node->waiting_for_inode = 0;
        	        	        cur_txn_node->wait_mode = -1;
	
        	        	        new_file_node = resume(f_inode,file_path,f_flags,txnid);
 			
					/*wake up further sleeping transactions if the present waking transaction has opened in shared mode and if any other transactions are sleeping on this file lock*/  
        	        	        /*if(f_flags == 0)
						continue_wake_up(f_inode);*/
				}
				else
					new_file_node = NULL;

			}
		}

	}
  }
 return new_file_node;
} 


/*Releasing locks when a transaction txn_id commits or aborts*/
void release_locks_on_success(int txnid)
{
   struct txn_node *ptr_txn_node;
   struct file_lock *txn_id_file_list,*ptr_file_lock;
   struct wait_node *wait_list_loc;
   int ret_close;

   /*get the file list locked by the commiting or aborting transaction txn_id*/
   pthread_mutex_lock(&txn_mutex);/*locking as pointers may be changed by outher transactions while this is traversing it*/
   ptr_txn_node = head_txn_list;
   while(ptr_txn_node->txn_id != txnid)
	ptr_txn_node = ptr_txn_node->next;
   pthread_mutex_unlock(&txn_mutex);
   
   /*txn_id_file_list will hold the pointer to the head of the file list held by txn_id*/
   txn_id_file_list = ptr_txn_node->file_rec_no;
   
   /*make the locked file list pointed at from the transactions datastructure NULL*/
   ptr_txn_node->file_rec_no = NULL;	
   
   pthread_mutex_lock(&file_mutex);
   //printf("\n %d entering release locks",txnid);
   fflush(event_log);/*FOR DURABILITY BEFORE WRITING IN PLACE AND COMMITING*/

   while(txn_id_file_list != NULL)
   {
	
	ptr_file_lock = txn_id_file_list;
	
        /*if the file was opened for writing or renaming or create, check if there are writes and perform delayed writes/delayed renames/delayed creates*/
        if ((ptr_file_lock->flags != 0)&&(ptr_file_lock->writes_at != NULL))/*if it is a rename tne writes at record will hold the LRN*/
	{
		
		if((ptr_file_lock->writes_at)->write_length < 0)/*-1,-2,-3 for rename. -4 for creat*/
		
			delayed_update(ptr_file_lock,txnid);
		else
			delayed_write(ptr_file_lock); // We are here assuming that the delayed write will occur as a result of file close
	}
   	/*find out if wait list for the file pointed at by txn_id_file_list is present*/		
   	wait_list_loc = search_wait_list(ptr_file_lock->file_inode);

   	/*point txn_id_file_list to the next file of the commiting/aborting transaction*/
   	txn_id_file_list = txn_id_file_list->next_txn;
	//printf("\ntxn_id_file_list %d",txn_id_file_list->file_inode);
   	if (wait_list_loc == NULL)/*no tranxaction is waiting,hence simply close the file and release the node*/
   	{	
		
		if(ptr_file_lock->fd > 0)/*0 if its a file for rename and hence not opened*/
// close system call has to be replaced with tr_yaffs_close -- the file will be flushed out when the it closed 
		{
		//printf(" Function release_locks_on_success :: Wait List Lock NULL, hence closing the file\n");
		// OLD IMPLEMENTATION
			// close (ptr_file_lock->fd);
		// NEW IMPLEMENTATION
			ret_close = tr_yaffs_close(ptr_file_lock->fd, txnid);
			
			if (ret_close == -1)
			{
				;//printf("Error :: release_locks_on_success :: file close returns -1 \n");
			}
		}
		delete_file_list(ptr_file_lock);
   	}
   	else /*there is a wait list present*/
   	{
		if(ptr_file_lock->file_mode == 0)/*file to be released is locked in shared mode*/
		{
			/*if only transaction holding a shared lock then have to perform the wakeup process else adjust pointers in file list*/
			if((ptr_file_lock->next_shared_rec == NULL) && (ptr_file_lock->prev_shared_rec == NULL))
			{
				if(ptr_file_lock->fd != 0)/*o if its a file for rename and hence not opened*/
  				{
					// OLD IMPLEMENTATION
					// close(ptr_file_lock->fd);
					// NEW IMPLEMENTATION
					ret_close = tr_yaffs_close(ptr_file_lock->fd, txnid);
					if (ret_close == -1)
					{
						printf("Error :: release_locks_on_success :: file close returns -1 \n");
					}
				}
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
				if(ptr_file_lock->fd != 0)/*o if its a file for rename and hence not opened*/
				{
					// OLD IMPLEMENTATION
					// close(ptr_file_lock->fd);
					// NEW IMPLEMENTATION
					ret_close = tr_yaffs_close(ptr_file_lock->fd, txnid);
					if (ret_close == -1)
					{
						;//printf("Error :: release_locks_on_success :: file close returns -1 \n");
					}
				}
				delete_file_list(ptr_file_lock);
			}
		}

		else/*file to be released is locked in exclusive mode*/	
		{
			if(ptr_file_lock->fd != 0)/*o if its a file for rename and hence not opened*/
				{
					// OLD IMPLEMENTATION
					// close(ptr_file_lock->fd);
					// NEW IMPLEMENTATION
					ret_close = tr_yaffs_close(ptr_file_lock->fd, txnid);
					if (ret_close == -1)
					{
						;//printf("Error :: release_locks_on_success :: file close returns -1 \n");
					}
				}
			/*delete node from lock list*/
			delete_file_list(ptr_file_lock);
			wake_up(wait_list_loc);
		}
    	}	
   }
   //printf("\nCOMMIT %d",txnid);
   //printf("\n Status:");
   //print_lists();
   pthread_mutex_unlock(&file_mutex);		
		
}
