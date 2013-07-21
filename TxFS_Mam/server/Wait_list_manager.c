#include "my_header.h"
extern void error(const char *msg);
extern int on_txn_abort(int txnid);

struct wait_node *get_wait_node(long f_inode)
{
   struct wait_node *new_wait_node;
   
   new_wait_node = (struct wait_node *)malloc(sizeof(struct wait_node));

   new_wait_node->file_inode = f_inode;
   new_wait_node->in_wait = 1;
   
   if(pthread_cond_init(&new_wait_node->f_lock,NULL) != 0)
	error("Error initializing condition variable");
   new_wait_node->txnid_abort = -1;
   new_wait_node->woke_up = 0;
   pthread_mutex_init(&new_wait_node->wake_mutex, NULL);
   if(pthread_cond_init(&new_wait_node->wake_cond,NULL) != 0)
	error("Error initializing condition variable");
   new_wait_node->next = NULL;

   return new_wait_node;
}


struct wait_node *search_wait_list(long file_inode)
{
 struct wait_node *loc_in_wait;

 loc_in_wait = NULL;
 
 if(head_wait_list == NULL)
 {
	return(NULL);
 }
 else
 {
	loc_in_wait = head_wait_list;
	while(loc_in_wait != NULL)
	{
		if (loc_in_wait->file_inode == file_inode)
			return(loc_in_wait);
		else
			loc_in_wait = loc_in_wait->next;
	}
	return(NULL);
 }
}


void update_wait_list_on_wake(struct wait_node *wait_list_loc)
{

  struct wait_node *ptr_wait_node;

  wait_list_loc->in_wait--;
  /*free the node if it is the last to wake up*/
  if(wait_list_loc->in_wait == 0)
	{
	/*move to node just before wait_list_loc*/
		ptr_wait_node = head_wait_list;	
		/*This node is the last node in the wait list or the first node in the list*/
		if(ptr_wait_node == wait_list_loc)
			head_wait_list = head_wait_list->next;/*updating pointer*/
		else
		{
			while(ptr_wait_node->next != wait_list_loc)
				ptr_wait_node = ptr_wait_node->next;

			ptr_wait_node->next = wait_list_loc->next;/*updating pointer*/
		}
		
		pthread_cond_destroy(&wait_list_loc->f_lock);
		pthread_mutex_destroy(&wait_list_loc->wake_mutex);
		pthread_cond_destroy(&wait_list_loc->wake_cond);
		wait_list_loc->next = NULL;
		free(wait_list_loc);
	}
}
/*Begin a wait list for a file*/
int insert_wait_list(long f_inode,int txnid)/*returns 1 if returning from signalled to resume. returns 0 if returning after aborting due to a deadlock*/
{
 struct wait_node *new_wait_node,*ptr,*foll;
  int aborted_txn;
 
 /*get a new node and initialize*/
 new_wait_node = get_wait_node(f_inode);
  
 /*insert into list*/
 ptr = head_wait_list;
 //printf("\nptr=head");
 fflush(stdout);
 while(ptr!=NULL)
 {
   if(ptr->file_inode > f_inode)
        break;
   foll = ptr;
   ptr = ptr->next;
 }

 if(head_wait_list == NULL)
 	head_wait_list = new_wait_node;
 else{
	 if(ptr == head_wait_list)/*f_inode of first node is greater then the present f_inode*/
 	 {
		new_wait_node->next = ptr;
        	head_wait_list = new_wait_node;
 	 }
 	else{
		new_wait_node->next = foll->next;
		foll->next = new_wait_node;
 	}
}
  printf("\n %d is about to be blocked",txnid) ;
  /*Updating number conflict count*/
  if(txnid ==0)/*i.e backup*/
	tb_conflict = tb_conflict+1;
  else
  	u_conflict = u_conflict+1;
 /*Blocking current thread on condition variable till the transaction owning the file releases the file and updates the condition variable*/
 pthread_cond_wait(&new_wait_node->f_lock, &file_mutex);

 /*A thread blocked on a condition variable may be signalled to wake up and resume(i.e by a thread that commits or aborts normally) or signalled to abort as part of deadlock resolving. If it is the first case then new_wait_node->txnid_abort will be -1, the while loop is bypassed and the if statement returns true and the woken up thread resumes execution. In case of the second case, a woken up thread may be the one that is to be aborted or one that should keep on staying blocked. So a thread wakes up to check whether it is itself that should abort by checking the new_wait_node->txind field. If it is not the it waits on the condition var again but before that it increments the number of threads that have woken up so far by deadlock.c and if it was the last thread that woke up it signals it to the thread that should abort to go ahead and abort. the signalling to abort is thru the condition variable wake_cond.The to be aborted threadwaits for all threads to wake up and go back to waiting mode before aborting and resetting the variable txnid_abort*/

 while((new_wait_node->txnid_abort != -1)&&(new_wait_node->txnid_abort != txnid))/*there is a deadlock victim but its not me*/
 {
	new_wait_node->woke_up = new_wait_node->woke_up+1;/*increment the number of threads woken up*/
	
	pthread_mutex_lock(&new_wait_node->wake_mutex);

	if (new_wait_node->woke_up == new_wait_node->in_wait)/*all threads have woken up and the last to wake up is not the one to be aborted*/
		/*signal the thread to be aborted to move ahead as all threads have woken up*/
		pthread_cond_signal(&new_wait_node->wake_cond);
	
	pthread_mutex_unlock(&new_wait_node->wake_mutex);
	 
	pthread_cond_wait(&new_wait_node->f_lock,&file_mutex);/* this thread goes back to sleep.automativally the mutex is unlocked*/
 }

 if(new_wait_node->txnid_abort == -1)/*-1 means woken up to resume. else woken up to abort*/
 {      
        /*decrement count of number of threads blocked.If it is the last the delete the node.Under lock file_mutex*/
        update_wait_list_on_wake(new_wait_node);
	/*file mutex will get locked automatically once the waiting thread is signalled*/
	pthread_mutex_unlock(&file_mutex);
 	printf("\n %d has woken up to resume",txnid);
	return 1;
  }
 else
  {	
	new_wait_node->woke_up = new_wait_node->woke_up+1;/*increment the number of threads woken up*/
	if (new_wait_node->woke_up == new_wait_node->in_wait)/*last thread to wake up then no need to wait on the semaphore*/
	{
		printf("\n %d has woken up to abort",txnid);
		new_wait_node->txnid_abort = -1;/*resetting it back*/
		/*update wait list.If no more threads waiting then free node*/
 		 update_wait_list_on_wake(new_wait_node);
		/*file mutex will get locked automatically once the waiting thread is signalled*/
 		 pthread_mutex_unlock(&file_mutex);
 		 aborted_txn = on_txn_abort(txnid);
		
	}
	else/*this thread waits to abort till all threads waiting on this file goes back to sleep*/
	{
		pthread_mutex_unlock(&file_mutex);/*have to do this else may get blocked in the next pthread_cond_wait*/
		pthread_mutex_lock(&new_wait_node->wake_mutex);
		if (new_wait_node->woke_up < new_wait_node->in_wait)
			pthread_cond_wait(&new_wait_node->wake_cond,&new_wait_node->wake_mutex);
		
		pthread_mutex_unlock(&new_wait_node->wake_mutex);
		
		pthread_mutex_lock(&file_mutex);
		printf("\n %d has woken up to abort",txnid);
		new_wait_node->txnid_abort = -1;/*resetting it back*/
		/*update wait list.If no more threads waiting then free node*/
 		 update_wait_list_on_wake(new_wait_node);
		/*file mutex will get locked automatically once the waiting thread is signalled*/
 		 pthread_mutex_unlock(&file_mutex);
 		 aborted_txn = on_txn_abort(txnid);	
	}
	return 0;
  }		
	
}
           
int update_wait_list(struct wait_node *loc_in_wait,int txnid)
{
	int aborted_txn;
	loc_in_wait->in_wait++;/*increase waiting transaction count*/
	printf("\n update_wait_list About to be blocked %d",txnid);
	/*Updating Count*/
	if(txnid ==0)/*i.e backup*/
		tb_conflict = tb_conflict+1;
  	else
 		u_conflict = u_conflict+1;

	pthread_cond_wait(&loc_in_wait->f_lock,&file_mutex);

	 while((loc_in_wait->txnid_abort != -1)&&(loc_in_wait->txnid_abort != txnid))/*there is a deadlock victim but its not me*/
	 {
		loc_in_wait->woke_up = loc_in_wait->woke_up+1;/*increment the number of threads woken up*/
		pthread_mutex_lock(&loc_in_wait->wake_mutex);

		if (loc_in_wait->woke_up == loc_in_wait->in_wait)/*all threads have woken up and the last to wake up is not the one to be aborted*/
		/*signal the thread to be aborted to move ahead as all threads have woken up*/
		pthread_cond_signal(&loc_in_wait->wake_cond);
	
		pthread_mutex_unlock(&loc_in_wait->wake_mutex);
		pthread_cond_wait(&loc_in_wait->f_lock,&file_mutex);/* this thread goes back to sleep.automativally the mutex is unlocked*/
 	}

	 if(loc_in_wait->txnid_abort == -1)/*-1 means woken up to resume. else woken up to abort*/	
	 {
		/*decrement count of number of threads blocked.If it is the last the delete the node.Under lock file_mutex*/
                update_wait_list_on_wake(loc_in_wait);
		/*file mutex will get locked automatically once the waiting thread is signalled*/
		pthread_mutex_unlock(&file_mutex);
 		printf("\n %d has woken up to resume",txnid);
		return 1;
  	}
 	else
  	{	
		loc_in_wait->woke_up = loc_in_wait->woke_up+1;/*increment the number of threads woken up*/
		if (loc_in_wait->woke_up == loc_in_wait->in_wait)/*last thread to wake up then no need to wait on the semaphore*/
		{

			printf("\n %d has woken up to abort",txnid);
			loc_in_wait->txnid_abort = -1;/*resetting it back*/
			/*update wait list.If no more threads waiting then free node*/
 		 	update_wait_list_on_wake(loc_in_wait);
			/*file mutex will get locked automatically once the waiting thread is signalled*/
 		 	pthread_mutex_unlock(&file_mutex);
			printf("\nUpdatewaitlist aborting %d",txnid);
 		 	aborted_txn = on_txn_abort(txnid);
		}
		else
		{
			
			pthread_mutex_unlock(&file_mutex);/*have to do this else may get blocked in the next pthread_cond_wait*/
			pthread_mutex_lock(&loc_in_wait->wake_mutex);
			if (loc_in_wait->woke_up < loc_in_wait->in_wait)
				pthread_cond_wait(&loc_in_wait->wake_cond,&loc_in_wait->wake_mutex);
		
			pthread_mutex_unlock(&loc_in_wait->wake_mutex);
		
			pthread_mutex_lock(&file_mutex);
			printf("\n %d has woken up to abort",txnid);
			loc_in_wait->txnid_abort = -1;/*resetting it back*/
			/*update wait list.If no more threads waiting then free node*/
 		 	update_wait_list_on_wake(loc_in_wait);
			/*file mutex will get locked automatically once the waiting thread is signalled*/
 		 	pthread_mutex_unlock(&file_mutex);
			printf("\nwaking up %d",txnid);
 		 	aborted_txn = on_txn_abort(txnid);	
		}
	return 0;
	  }		
	
}

void wake_up(struct wait_node *wait_list_loc)
{
	struct wait_node *ptr_wait_node;
	
	
	pthread_cond_broadcast(&wait_list_loc->f_lock);
	
}

/*void continue_wake_up(long f_inode)
{
 struct wait_node *wait_list_loc;
	
 /*find out if wait list for f_inode is present*/
 /*wait_list_loc = search_wait_list(f_inode);

 if (wait_list_loc != NULL)
	wake_up(wait_list_loc);
}*/


