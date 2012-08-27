#include "my_header.h"
//

struct txn_node *get_txn_node(int new_txn_id,long log_rec_no)
{
	struct txn_node *new_txn_node;
	new_txn_node = (struct txn_node *)malloc(sizeof(struct txn_node));
	
	/*Initializing*/
	new_txn_node->txn_id = new_txn_id;
        new_txn_node->cur_state = RUNNING;
        new_txn_node->first_LRN = log_rec_no;/*rec no. of the first log entry for this transaction*/
        new_txn_node->last_LRN = log_rec_no;/*latest rec no of log entry for this transaction*/
        new_txn_node->file_rec_no = NULL;/*pointer of first file it owns.We can get all files after reaching this*/
        new_txn_node->next = NULL;
                        /*THE NEXT FOUR FIELDS ARE USED BY ONLY WAITING TRANXACTIONS*/
        //new_txn_node->tid_next_wait = NULL;/*next tid wating for a file this tid is waiting*/
        new_txn_node->wait_mode = -1;/*0=waiting for a shared lock,1=exclusive lock,-1=not waiting*/
	new_txn_node->waiting_for_inode = 0;
        new_txn_node->blocked_tid = 0;/*If waiting,this holds the thread_id of waiting thread*/
        //new_txn_node->blocked_file = NULL;/*hold info of the file for which the tid got blocked while trying to lock*/

	//Initializing before_after bit to 0. When backup is not taken all transactions are serialized before it and hence 0*/
	new_txn_node->before_after = 0;

	/*return the new node*/
	return new_txn_node;
}






int assign_new_txnid(void)
{ 
  pthread_mutex_lock(&txnid_mutex);
  txn_id = txn_id+1;
  pthread_mutex_unlock(&txnid_mutex);
  return(txn_id);
}

// Inserts the transaction with the new transaction id in the form of a prority queue.
void new_txn_insert(int new_txn_id,int log_rec_no)
{
 struct txn_node *new_txn_node,*ptr_txn_node,*foll_txn_node;

 pthread_mutex_lock(&txn_mutex);
 
 if(head_txn_list == NULL)/*list empty*/
	head_txn_list = get_txn_node(new_txn_id,log_rec_no);
 else{
	new_txn_node = get_txn_node(new_txn_id,log_rec_no);
        ptr_txn_node = head_txn_list;
       
// Checking if the transaction id is already present, else insert the transaction to the last.
	while(ptr_txn_node!=NULL){
		if(ptr_txn_node->txn_id > new_txn_id)
			break;
		foll_txn_node = ptr_txn_node;
		ptr_txn_node = ptr_txn_node->next;
	}
	
	if(ptr_txn_node == head_txn_list){
		head_txn_list=new_txn_node;
		new_txn_node->next = ptr_txn_node;
	}
	else{	
		foll_txn_node->next = new_txn_node;
		new_txn_node->next = ptr_txn_node;
	}
 }
 pthread_mutex_unlock(&txn_mutex);
}

void txn_delete(int txnid)
{

 struct txn_node *ptr_txn_node,*foll_txn_node;
 
 pthread_mutex_lock(&txn_mutex);

 ptr_txn_node = head_txn_list;

 while(ptr_txn_node->txn_id != txnid)
 {
	foll_txn_node = ptr_txn_node;
	ptr_txn_node = ptr_txn_node->next;
 }

 if(ptr_txn_node == head_txn_list)
	head_txn_list = ptr_txn_node->next;
 else
	foll_txn_node->next = ptr_txn_node->next;
 
 free(ptr_txn_node);

 pthread_mutex_unlock(&txn_mutex);
}	
