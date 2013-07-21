#include "my_header.h"

/*Building the wait for graph*/
struct wait_graph_node{
	int txn_id;
	long inode;/*txn_id is waiting for inode*/
	int visited;/*0=white,1=grey,2=black*/
	struct wait_graph_edge *edge;
	struct wait_graph_node *next;
	};
struct wait_graph_edge{
	struct wait_graph_node *edge_to;
	struct wait_graph_edge *next_edge;
	};

struct wait_graph_node *head_wait_graph = NULL;/* will hold all transactions that are waiting*/

int in_deadlock[50][2], in_deadlock_index=0;

void construct_wait_for_graph(void)
{
  int flag =1;
  struct txn_node *ptr_txn;
  struct file_lock *ptr_file;
  struct wait_graph_node *new_wait_node, *ptr_wait, *foll_wait;
  struct wait_graph_edge *ptr_edge,*new_edge;
  ptr_txn = head_txn_list;
  ptr_wait = head_wait_graph;
  
 //pthread_mutex_lock(&txn_mutex);
  /*building a list of waiting transactions*/
  while(ptr_txn!= NULL)
  {
    if (ptr_txn->cur_state == WAITING)
     {
	/*allocate new node*/
	new_wait_node = (struct wait_graph_node *)malloc(sizeof(struct wait_graph_node));
	new_wait_node->txn_id = ptr_txn->txn_id;
	new_wait_node->inode = ptr_txn->waiting_for_inode;
	new_wait_node->visited = 0;
	new_wait_node->edge = NULL;
	new_wait_node->next = NULL;

	/*insert into list*/
 	if(ptr_wait == NULL)
	{
		head_wait_graph = new_wait_node;
		ptr_wait = head_wait_graph;
	}
	else
	{
		ptr_wait->next = new_wait_node;
		ptr_wait = ptr_wait->next;
	}
      }
     ptr_txn = ptr_txn->next;
   }
//pthread_mutex_unlock(&txn_mutex);
 ptr_wait = head_wait_graph;
 while(ptr_wait!=NULL)
 {
	//printf("\nWait list %d inode %ld",ptr_wait->txn_id,ptr_wait->inode);
	//fflush(stdout);
	ptr_wait = ptr_wait->next;
 }
 fflush(stdout);
  /*adding wait_for edges. An edge is added only is the transaction is waiting for another which is also waiting*/
 ptr_wait = head_wait_graph;
 pthread_mutex_lock(&file_mutex);
 while(ptr_wait != NULL)
 {
	
	/*which transaction holds lock to ptr_wait->inode*/
	ptr_file = head_lock_list;
	while(ptr_file->file_inode !=ptr_wait->inode)
	{
		ptr_file = ptr_file->next;
	}

	/*is the parent txn in the waiting list*/
	foll_wait = ptr_wait->next;
	while(foll_wait !=NULL)
	{
		flag = 1;
		
		if(foll_wait->txn_id == ptr_file->parent_txn)
		{	
				/*have to add an edge from ptr_wait to foll_wait*/
				ptr_edge = ptr_wait->edge;
				/*get a new edge*/
				new_edge =  (struct wait_graph_edge *)malloc(sizeof(struct wait_graph_edge));
				new_edge->edge_to = foll_wait;
				new_edge->next_edge = NULL;
				if(ptr_edge == NULL)
					ptr_wait->edge = new_edge;
				else{
					while(ptr_edge->next_edge != NULL)
						ptr_edge = ptr_edge->next_edge;
					ptr_edge->next_edge = new_edge;
					}
				if(ptr_file->file_mode != 0)/*locked in exclusive mode then the inode cannot be locked by anyone else*/
					foll_wait == NULL;/* as the file cannot be locked by any other so we can come out of the loop*/
				else{
					if(ptr_file->next_shared_rec == NULL)
					{
						foll_wait = NULL;/* as the file cannot be locked by any other so we can come out of the loop*/
					}
					else
					{
						ptr_file = ptr_file->next_shared_rec;
						flag = 0;
						foll_wait = ptr_wait->next;
					}
					
				}			
		}		
		
		if((foll_wait == NULL)|| flag == 0)
			continue;
		else
			foll_wait = foll_wait->next;
		
	}
	foll_wait = head_wait_graph;
	while(foll_wait != ptr_wait)
	{
		flag =1;
		
		if(foll_wait->txn_id == ptr_file->parent_txn)
		{	
				/*have to add an edge from ptr_wait to foll_wait*/
				ptr_edge = ptr_wait->edge;
				/*get a new edge*/
				new_edge =  (struct wait_graph_edge *)malloc(sizeof(struct wait_graph_edge));
				new_edge->edge_to = foll_wait;
				new_edge->next_edge = NULL;
				if(ptr_edge == NULL)
					ptr_wait->edge = new_edge;
				else{
					while(ptr_edge->next_edge != NULL)
						ptr_edge = ptr_edge->next_edge;
					ptr_edge->next_edge = new_edge;
					}
				if(ptr_file->file_mode != 0)/*locked in exclusive mode then the inode cannot be locked by anyone else*/
					foll_wait == ptr_wait;/* as the file cannot be locked by any other so we can come out of the loop*/
				else{
								
					
					if(ptr_file->next_shared_rec == NULL)
					{
						foll_wait = ptr_wait;/* as the file cannot be locked by any other so we can come out of the loop*/
					}
					else
					{
						ptr_file = ptr_file->next_shared_rec;
						flag = 0;
						foll_wait = head_wait_graph;
					}
					
				}			
		}		
		
		if((foll_wait == ptr_wait)|| flag == 0)
			continue;
		else
			foll_wait = foll_wait->next;

		fflush(stdout);
		
	}		
 	
  ptr_wait = ptr_wait->next;		
}
pthread_mutex_unlock(&file_mutex);
}	

void destroy_wait_for_graph(void)
{
  struct wait_graph_node *ptr_wait, *foll_wait;
  struct wait_graph_edge *ptr_edge,*foll_edge;
  ptr_wait = head_wait_graph;

  while(ptr_wait != NULL)
  {
	foll_wait = ptr_wait->next;
	ptr_edge = ptr_wait->edge;
	free(ptr_wait);
	while(ptr_edge != NULL)
	{
		foll_edge = ptr_edge->next_edge;
		free(ptr_edge);
		ptr_edge = foll_edge;
	}
	ptr_wait = foll_wait;
  }
 
 head_wait_graph = NULL;
}

void cycle(struct wait_graph_node *ptr_cycle)
{
   struct wait_graph_edge *edge;
   struct wait_graph_node *in_cycle[50];
  
   int i = 0;
   
   in_cycle[i] = ptr_cycle;
   ptr_cycle = NULL;
   while(ptr_cycle != in_cycle[0])
   {
	edge = in_cycle[i]->edge;
	while(1)
	{
		if((edge->edge_to)->visited == 1)/*grey*/
		{
			printf(" and the cycle contains %d",in_cycle[i]->txn_id);
			in_deadlock[in_deadlock_index][0] = in_cycle[i]->txn_id;
			in_deadlock[in_deadlock_index][1] = in_cycle[i]->inode;
			in_deadlock_index++;
			fflush(stdout);
			i++;
			in_cycle[i] = edge->edge_to;
			ptr_cycle = edge->edge_to;
			break;
		}
		edge = edge->next_edge;
		if(edge == NULL)
			break;
	}
    }
   	
}		

void dfs_visit(struct wait_graph_node *ptr_wait)
{
  struct wait_graph_edge *ptr_edge;
  //printf("\nInside DFS visit");
  //fflush(stdout);
  ptr_wait->visited = 1;/*paint it grey*/
  ptr_edge = ptr_wait->edge;
  while(ptr_edge != NULL)
  {
	if((ptr_edge->edge_to)->visited == 1)/*hey its grey, I have visited it before and I just traveled a back edge*/
	{
		printf("\nA cycle has been detected ");
		fflush(stdout);		
		cycle(ptr_edge->edge_to);
	}
	if ((ptr_edge->edge_to)->visited == 0)
	{
		dfs_visit(ptr_edge->edge_to);
	}
	ptr_edge = ptr_edge->next_edge;
  }
 ptr_wait->visited = 2;
  
}


void detect_cycles(void)
{
struct wait_graph_node *ptr_wait;
//printf("\ndetect_cycle");
//fflush(stdout);
ptr_wait = head_wait_graph;
while(ptr_wait!= NULL)
{
	if(ptr_wait->visited == 0)/*not visited*/
		dfs_visit(ptr_wait);
	ptr_wait = ptr_wait->next;
}

}

void deadlock_detection(void)
{
    struct wait_graph_node *ptr_wait;
    struct wait_graph_edge *ptr_edge;
    struct wait_node *loc_in_wait_list;
    int i=0;
	//sleep(5);
    while(i<30) 
    {
	
	fflush(stdout);
        sleep(2);
	head_wait_graph = NULL;
	in_deadlock_index = 0;    	
	pthread_mutex_lock(&txn_mutex);
		printf("\nDeadlock detection");
		//print_lists();
    		construct_wait_for_graph();
    	pthread_mutex_unlock(&txn_mutex);

	
	ptr_wait = head_wait_graph;
	
    
    	detect_cycles();

	destroy_wait_for_graph();

	if (in_deadlock_index != 0)/*there was a cycle*/
	{
		printf("\nAborting %d to resolve deadlock",in_deadlock[0][0]);
		abrt_on_dead++;
		pthread_mutex_lock(&file_mutex);
			loc_in_wait_list = search_wait_list(in_deadlock[0][1]);
			if(loc_in_wait_list == NULL)
				printf("\nError while searching in waitlist");
			loc_in_wait_list->txnid_abort= in_deadlock[0][0];/*insert transaction id of transaction to be aborted. Resetting will be 				done by the thread that will be aborted in the function insert_wait_list and update_wait_list*/
			loc_in_wait_list->woke_up = 0;
			
			pthread_cond_broadcast(&loc_in_wait_list->f_lock);
			
		pthread_mutex_unlock(&file_mutex);
		
				
	}
	i++;
     }	
}
    

