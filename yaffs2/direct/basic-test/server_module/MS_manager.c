#include "my_header.h"

extern int on_txn_abort(int);

int mutually_serializable(int txnid, int files_read_bit, char *file_path,long f_inode,int f_flags)/*if no conflict return 1 and processing continues else return 0*/
{
   struct txn_node *ptr_txn_node;
   int before_after_bit;
   int abort_txnid;
   int local_wait =0;
   int i;
  /*remember I am holding lock on file_mutex*/ 

  /*get to the concerned transaction in the list of "live transactions*/
   pthread_mutex_lock(&txn_mutex);/*locking as pointers may be changed by outher transactions while this is traversing it*/
   ptr_txn_node = head_txn_list;
   while(ptr_txn_node->txn_id != txnid)
        ptr_txn_node = ptr_txn_node->next;
   pthread_mutex_unlock(&txn_mutex);
   before_after_bit = ptr_txn_node->before_after;

  /*if the txnid has not opened any file as yet*/
  if((ptr_txn_node->file_rec_no) == NULL)
	return 1;/*update before_after bit once the file is locked and opened*/
  else/*txnid has already opened files and hence we need to check for conflicts*/
  { 
      if(files_read_bit == before_after_bit)/*continues to be serializable*/
      {
	/*if it was paused before then nullify pause_times*/
	for(i=0;i<=index_pause_array;i++)
	{
		if(pause_array[i].txnid == txnid)
		{
			pause_array[i].txnid = -1;
			pause_array[i].pause_times = 0;
		}
	}
	return 1;/*continue to lock the file*/
      }
      else/*there is conflict*/
	{
		//tbu_conflict = tbu_conflict+1;/*total user transaction not MS with tb*/
		if((files_read_bit ==0)&&(before_after_bit == 1))/*pause it,hoping that the backup transaction will read it.Pause for a certain number of times then abort as it is probbaly part of a deadlock scenario*/
		{
			
			printf("\n%d will now pause",txnid);
			pthread_mutex_unlock(&file_mutex);


			/*update pause array*/
			pthread_mutex_lock(&pause_mutex);
			for(i=0;i<=index_pause_array;i++)
			{
				if(pause_array[i].txnid == txnid)
				      break;
			}
			if(pause_array[i].txnid == txnid)
			{
				if(pause_array[i].pause_times>5)
				{
					/*abort*/
				   pthread_mutex_unlock(&pause_mutex);
				   printf("\n%d had to be aborted on account of conflict with Backup after a number of pause",txnid);
				   abrt_after_pause = abrt_after_pause+1;
				   abort_txnid = on_txn_abort(txnid);	
					/*abort transaction*/			
					return -1;
				}
				else
				    pause_array[i].pause_times++;
					
			}
			else{
				index_pause_array++;
				pause_array[index_pause_array].txnid = txnid;
				pause_array[index_pause_array].pause_times = 1;
				no_on_wait = no_on_wait +1;/*number of transactions that has to wait fresh*/
			}
			pthread_mutex_unlock(&pause_mutex);	
		
			
			sleep(2);/*sleep the thread for 1 seconds and try locking it again*/
			ptr_txn_node->cur_state = RUNNING;
			total_wait_time = total_wait_time+2;
			return 0;
		}	
		
		else/*file_read_bit == 1 && before_after_bit == 0*/
		{
			no_of_abrt = no_of_abrt +1;/*number of transactions that needs to be aborted*/
			pthread_mutex_unlock(&file_mutex);
			printf("\n%d had to be aborted on account of conflict with Backup",txnid);
			abort_txnid = on_txn_abort(txnid);	
			/*abort transaction*/			
			return -1;
		}
	}
   }
}
