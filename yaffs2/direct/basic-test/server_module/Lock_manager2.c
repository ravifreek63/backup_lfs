#include "my_header.h"
extern int search_file_list(long f_inode,struct file_lock **add_loc);
extern void delete_file_list(struct file_lock *ptr_file_node);
extern int tr_yaffs_close_abort (int handle, unsigned int txnid);

struct file_lock *resume(long f_inode,char *file_path,int f_flags,int txnid)
{
   int search_ret;
   struct file_lock *loc=NULL,*inserted_pos;

    pthread_mutex_lock(&file_mutex);
    //printf("\n %d entering resume",txnid);

    /*Search if file is already locked or not*/
    search_ret= search_file_list(f_inode,&loc);/*0=not found,1=found*/

     //printf("  search return %d",search_ret);

    /*Inserting new node into lock list i.e loking the file if possible else place calling transaction in waiting mode*/
    inserted_pos = insert_file_list(f_inode,file_path,f_flags,txnid,search_ret,loc);
    return inserted_pos;
}

void free_write_at(struct file_lock *file_to_write)
{
  struct write_records *ptr_write_at, *foll_write_at;

  ptr_write_at = file_to_write->writes_at;

  while(ptr_write_at != NULL)
  {
	foll_write_at = ptr_write_at;
	ptr_write_at = ptr_write_at->next;
	free(foll_write_at);/*free the space occupied by the node*/
   }
 return;
}	
/*Releasing locks when a transaction txn_id commits or aborts*/
void release_locks_on_abort(int txnid)
{
   struct txn_node *ptr_txn_node;
   struct file_lock *txn_id_file_list,*ptr_file_lock;
   struct wait_node *wait_list_loc;

  // printf("\nRelease locks on abort");
   fflush(stdout);
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
   fflush(event_log);/*FOR DURABILITY BEFORE ABORTING*/

   while(txn_id_file_list != NULL)
   {
	//printf("On Abort file descriptors in the list = %d\n", ptr_file_lock->fd);
	ptr_file_lock = txn_id_file_list;
	
        /*if the file was opened for writing and if there are writes we simply discard them and do not write in place*/
        if ((ptr_file_lock->flags != 0)&&(ptr_file_lock->writes_at != NULL))/*have to release space occupied by the writes_at*/
		free_write_at(ptr_file_lock);
   	/*find out if wait list for the file pointed at by txn_id_file_list is present*/		
   	wait_list_loc = search_wait_list(ptr_file_lock->file_inode);

   	/*point txn_id_file_list to the next file of the commiting/aborting transaction*/
   	txn_id_file_list = txn_id_file_list->next_txn;
	//printf("\ntxn_id_file_list %d",txn_id_file_list->file_inode);
   	if (wait_list_loc == NULL)/*no tranxaction is waiting,hence simply close the file and release the node*/
   	{	
		
		if(ptr_file_lock->fd >=0)/*<0 if its a file for rename and hence not opened*/
			tr_yaffs_close_abort (ptr_file_lock->fd, txnid);//close(ptr_file_lock->fd);
		delete_file_list(ptr_file_lock);
   	}
   	else/*there is a wait list present*/
   	{
		if(ptr_file_lock->file_mode == 0)/*file to be released is locked in shared mode*/
		{
			/*if only transaction holding a shared lock then have to perform the wakeup process else adjust pointers in file list*/
			if((ptr_file_lock->next_shared_rec == NULL) && (ptr_file_lock->prev_shared_rec == NULL))
			{
				if(ptr_file_lock->fd >=0)/*<0 if its a file for rename and hence not opened*/
				 	 tr_yaffs_close_abort (ptr_file_lock->fd, txnid);//close(ptr_file_lock->fd);
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
				if(ptr_file_lock->fd >=0 )/*<0 if its a file for rename and hence not opened*/
					 tr_yaffs_close_abort (ptr_file_lock->fd, txnid);//close(ptr_file_lock->fd);
				delete_file_list(ptr_file_lock);
			}
		}

		else/*file to be released is locked in exclusive mode*/	
		{
			if(ptr_file_lock->fd >= 0)/*<0 if its a file for rename and hence not opened*/
				 tr_yaffs_close_abort (ptr_file_lock->fd, txnid);//close(ptr_file_lock->fd);
			/*delete node from lock list*/
			delete_file_list(ptr_file_lock);
			wake_up(wait_list_loc);
		}
    	}	
   }
   //printf("\nABORT %d",txnid);
   pthread_mutex_unlock(&file_mutex);		
		
}                                                                                                                                 
