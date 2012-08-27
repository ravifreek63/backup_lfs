#include "my_header.h"

extern void error(const char *msg);
extern int tr_ext_creat_yaffs_creat (const char *pathname, mode_t mode);
extern int tr_sync_file_to_disk (char *path);

long extract_record_no(void)
{
   long LRN;
   fseek(event_log,0,SEEK_END);
   LRN = ftell(event_log);
   return(LRN);
}

long get_prev_log_LRN(int ntid)
{
  struct txn_node *ptr;
 
  pthread_mutex_lock(&txn_mutex);/*LOCK TXN_MUTEX*/
  ptr = head_txn_list;
  while(ptr->txn_id != ntid)
	ptr=ptr->next;
  pthread_mutex_unlock(&txn_mutex);/*UNLOCK TXN_MUTEX*/
  return (ptr->last_LRN);
}

void update_txn(int ntid,long last_LRN)
{
 struct txn_node *ptr;

  pthread_mutex_lock(&txn_mutex);/*LOCK TXN_MUTEX*/
  ptr = head_txn_list;
  while(ptr->txn_id != ntid)
        ptr=ptr->next;
  ptr->last_LRN = last_LRN;
  pthread_mutex_unlock(&txn_mutex);/*UNLOCK TXN_MUTEX*/

  return;
}

long log_txn_beg(int ntid)
{
   long log_LRN;
   struct log_record t;
 
   pthread_mutex_lock(&log_mutex);

   /*extract record number in log for the new log record.Its is the last present record+1.1st record number is 0*/
   log_LRN = extract_record_no();
   
   /*initialize a new record to insert into log*/
   t.LRN = log_LRN;
   t.txn_id = ntid;
   t.prev_LRN = -1;
   t.next_LRN = -1;
   t.record_type = TXN_BEG;
   t.fd = -1;
   t.offset = -1;
   t.write_length = strlen("NA\n");
   t.data = malloc((sizeof(char)*t.write_length)+1);/*+1 for the terminating \n*/
   strcpy(t.data,"NA\n");
   t.write_length1 = strlen("NA\n");
   t.data1 = malloc((sizeof(char)*t.write_length1)+1);
   strcpy(t.data1,"NA\n");


   /*append a new record of type tranx_begin at the end of log */
   fseek(event_log, 0,SEEK_END);
   
    fwrite(&t.LRN,sizeof(long),1,event_log);
    fwrite(&t.txn_id,sizeof(int),1,event_log);
    fwrite(&t.prev_LRN,sizeof(long),1,event_log);
    fwrite(&t.next_LRN,sizeof(long),1,event_log);
    fwrite(&t.record_type,sizeof(int),1,event_log);
    fwrite(&t.fd,sizeof(int),1,event_log);
    fwrite(&t.offset,sizeof(long),1,event_log);
    fwrite(&t.write_length,sizeof(long),1,event_log);
    fwrite(t.data,t.write_length,1,event_log);
    fwrite(&t.write_length1,sizeof(long),1,event_log);
    fwrite(t.data1,t.write_length1,1,event_log);

     
    free(t.data);/*freeing allocated memory else willhave memory leak*/     
    free(t.data1);      
    pthread_mutex_unlock(&log_mutex);
    fflush(event_log);
     
    
    /*return record no. of log entry*/
   return(log_LRN);
 }

long log_txn_commit(int ntid)
{
   long log_LRN,prev_log_LRN;
   struct log_record t,s;

   pthread_mutex_lock(&log_mutex);

   /*printf("\n In log_txn_end %d",ntid);*/

   /*extract record number in log for the new log record.Its is the last present record+1.1st record number is 0*/
   log_LRN = extract_record_no();
   //printf("\n log_LRN %ld",log_LRN);
   /*extracting previous record of same transaction*/
   prev_log_LRN = get_prev_log_LRN(ntid);
   //printf("\n prev %ld",prev_log_LRN);

  /*updating that prev log record*/
   fseek(event_log,prev_log_LRN,SEEK_SET);/*The offset now points to the beggining of that record*/
   fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long),SEEK_CUR);/*the file pointer now points to next_LRN*/
   
   if(fwrite(&log_LRN,sizeof(long),1,event_log)==0)
   {
	error("error on fwrite");
	return(-1);
   }
  
   /*updating link list node for this transaction for last_LRN*/
   update_txn(ntid,log_LRN);

   /*initialize a new record to insert into log*/
   t.LRN = log_LRN;
   t.txn_id = ntid;
   t.prev_LRN = prev_log_LRN;
   t.next_LRN = -1;
   t.record_type = TXN_COMMIT;
   t.fd = -1;
   t.offset = -1;
   t.write_length = strlen("NA\n");
   t.data = malloc(t.write_length+1);
   strcpy(t.data,"NA\n");
   t.write_length1 = strlen("NA\n");
   t.data1 = malloc((sizeof(char)*t.write_length1)+1);
   strcpy(t.data1,"NA\n");




   /*append a new record of type tranx_begin at the end of log */
   fseek(event_log, 0,SEEK_END);
   
   fwrite(&t.LRN,sizeof(long),1,event_log);
    fwrite(&t.txn_id,sizeof(int),1,event_log);
    fwrite(&t.prev_LRN,sizeof(long),1,event_log);
    fwrite(&t.next_LRN,sizeof(long),1,event_log);
    fwrite(&t.record_type,sizeof(int),1,event_log);
    fwrite(&t.fd,sizeof(int),1,event_log);
    fwrite(&t.offset,sizeof(long),1,event_log);
    fwrite(&t.write_length,sizeof(long),1,event_log);
    fwrite(t.data,t.write_length,1,event_log);
    fwrite(&t.write_length1,sizeof(long),1,event_log);
    fwrite(t.data1,t.write_length1,1,event_log);

          
    free(t.data);
    free(t.data1);
    pthread_mutex_unlock(&log_mutex);
    fflush(event_log);

    /*return record no. of log entry*/
   return(log_LRN);
 }

long log_txn_abort(int ntid)
{
   long log_LRN,prev_log_LRN;
   struct log_record t,s;

   pthread_mutex_lock(&log_mutex);

   /*printf("\n In log_txn_end %d",ntid);*/

   /*extract record number in log for the new log record.Its is the last present record+1.1st record number is 0*/
   log_LRN = extract_record_no();
   //printf("\n log_LRN %ld",log_LRN);
   /*extracting previous record of same transaction*/
   prev_log_LRN = get_prev_log_LRN(ntid);
   //printf("\n prev %ld",prev_log_LRN);

  /*updating that prev log record*/
   fseek(event_log,prev_log_LRN,SEEK_SET);/*The offset now points to the beggining of that record*/
   fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long),SEEK_CUR);/*the file pointer now points to next_LRN*/
   
   if(fwrite(&log_LRN,sizeof(long),1,event_log)==0){
	error("error on fwrite");
	return(-1);
   }
  
   /*updating link list node for this transaction for last_LRN*/
   update_txn(ntid,log_LRN);

   /*initialize a new record to insert into log*/
   t.LRN = log_LRN;
   t.txn_id = ntid;
   t.prev_LRN = prev_log_LRN;
   t.next_LRN = -1;
   t.record_type = TXN_ABORT;
   t.fd = -1;
   t.offset = -1;
   t.write_length = strlen("NA\n");
   t.data = malloc(t.write_length+1);
   strcpy(t.data,"NA\n");
   t.write_length1 = strlen("NA\n");
   t.data1 = malloc((sizeof(char)*t.write_length1)+1);
   strcpy(t.data1,"NA\n");



   /*append a new record of type tranx_begin at the end of log */
   fseek(event_log, 0,SEEK_END);
   
   fwrite(&t.LRN,sizeof(long),1,event_log);
    fwrite(&t.txn_id,sizeof(int),1,event_log);
    fwrite(&t.prev_LRN,sizeof(long),1,event_log);
    fwrite(&t.next_LRN,sizeof(long),1,event_log);
    fwrite(&t.record_type,sizeof(int),1,event_log);
    fwrite(&t.fd,sizeof(int),1,event_log);
    fwrite(&t.offset,sizeof(long),1,event_log);
    fwrite(&t.write_length,sizeof(long),1,event_log);
    fwrite(t.data,t.write_length,1,event_log);
    fwrite(&t.write_length1,sizeof(long),1,event_log);
    fwrite(t.data1,t.write_length1,1,event_log);

    free(t.data);
    free(t.data1);
          
    pthread_mutex_unlock(&log_mutex);
    fflush(event_log);

    /*return record no. of log entry*/
   return(log_LRN);
 }
long log_txn_end(int ntid, long prev_log_LRN)
{
   long log_LRN;
   struct log_record t,s;

   pthread_mutex_lock(&log_mutex);

   /*extract record number in log for the new log record.Its is the last present record+1.1st record number is 0*/
   log_LRN = extract_record_no();
      
   /*updating that prev log record*/
   fseek(event_log,prev_log_LRN,SEEK_SET);/*pointer points to beg of this record*/
   fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long),SEEK_CUR);/*the file pointer now points to next_LRN*/

   if(fwrite(&log_LRN,sizeof(long),1,event_log)==0){
        error("error on fwrite");
        return(-1);
   }

   /*updating link list node for this transaction for last_LRN*/
   //update_txn(ntid,log_LRN);not required as this the last record of this transaction and its corresponding node has already been deleted



   /*initialize a new record to insert into log*/
   t.LRN = log_LRN;
   t.txn_id = ntid;
   t.prev_LRN = prev_log_LRN;
   t.next_LRN = -1;
   t.record_type = TXN_END;
   t.fd = -1;
   t.offset = -1;
   t.write_length = strlen("NA\n");
   t.data = malloc(t.write_length+1);
   strcpy(t.data,"NA\n");
   t.write_length1 = strlen("NA\n");
   t.data1 = malloc((sizeof(char)*t.write_length1)+1);
   strcpy(t.data1,"NA\n");


   
   /*append a new record of type tranx_begin at the end of log */
   fseek(event_log, 0,SEEK_END);
   fwrite(&t.LRN,sizeof(long),1,event_log);
    fwrite(&t.txn_id,sizeof(int),1,event_log);
    fwrite(&t.prev_LRN,sizeof(long),1,event_log);
    fwrite(&t.next_LRN,sizeof(long),1,event_log);
    fwrite(&t.record_type,sizeof(int),1,event_log);
    fwrite(&t.fd,sizeof(int),1,event_log);
    fwrite(&t.offset,sizeof(long),1,event_log);
    fwrite(&t.write_length,sizeof(long),1,event_log);
    fwrite(t.data,t.write_length,1,event_log);
    fwrite(&t.write_length1,sizeof(long),1,event_log);
    fwrite(t.data1,t.write_length1,1,event_log);
 
    free(t.data);
    free(t.data1);
    
   pthread_mutex_unlock(&log_mutex);
    fflush(event_log);

    /*return record no. of log entry*/
   return(log_LRN);
 }

long log_write(int txnid,int fd, off_t offset, size_t write_length, char *data)
{
   long log_LRN;
   struct log_record t,s;
   long prev_log_LRN;

    /*extracting previous record of same transaction*/
   prev_log_LRN = get_prev_log_LRN(txnid);

   pthread_mutex_lock(&log_mutex);

   /*extract record number in log for the new log record.Its is the last present record+1.1st record number is 0*/
   log_LRN = extract_record_no();
   //printf("\n log_LRN %d, prev_log_LRN %d",log_LRN,prev_log_LRN);
   //fflush(stdout);
   
   /*updating that prev log record*/
   fseek(event_log,prev_log_LRN,SEEK_SET);
   fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long),SEEK_CUR);/*the file pointer now points to next_LRN*/

   if(fwrite(&log_LRN,sizeof(long),1,event_log)==0){
        error("error on fwrite");
        return(-1);
   }

   /*updating link list node for this transaction for last_LRN*/
   update_txn(txnid,log_LRN);

   /*initialize a new record to insert into log*/
   t.LRN = log_LRN;
   t.txn_id = txnid;
   t.prev_LRN = prev_log_LRN;
   t.next_LRN = -1;
   t.record_type = UPDATE;
   t.fd = fd;
   t.offset = offset;
   t.write_length = write_length;
   //printf("%s",data);
   //fflush(stdout);
   t.data = malloc(strlen(data)+1);/*added +1 after valgrind error as strlen does not count the trailing Null byt strcpy copies it*/
   strcpy(t.data,data);
   t.write_length1 = strlen("NA\n");
   t.data1 = malloc((sizeof(char)*t.write_length1)+1);
   strcpy(t.data1,"NA\n");

   
   
   /*append a new record of type tranx_begin at the end of log */
   fseek(event_log, 0,SEEK_END);
   fwrite(&t.LRN,sizeof(long),1,event_log);
    fwrite(&t.txn_id,sizeof(int),1,event_log);
    fwrite(&t.prev_LRN,sizeof(long),1,event_log);
    fwrite(&t.next_LRN,sizeof(long),1,event_log);
    fwrite(&t.record_type,sizeof(int),1,event_log);
    fwrite(&t.fd,sizeof(int),1,event_log);
    fwrite(&t.offset,sizeof(long),1,event_log);
    fwrite(&t.write_length,sizeof(long),1,event_log);
    fwrite(t.data,t.write_length,1,event_log);
    fwrite(&t.write_length1,sizeof(long),1,event_log);
    fwrite(t.data1,t.write_length1,1,event_log);

   free(t.data);
   free(t.data1);
   pthread_mutex_unlock(&log_mutex);
   fflush(event_log);
       /*return record no. of log entry*/
   return(log_LRN);
 }

void delayed_write(struct file_lock *file_to_write)
{
  int fd;
  off_t offset;
  size_t write_length;
  char *data;
  struct write_records *ptr_write_at, *foll_write_at;

  ptr_write_at = file_to_write->writes_at;

  while(ptr_write_at != NULL)
  {
	foll_write_at = ptr_write_at;
	ptr_write_at = ptr_write_at->next;

	/*foll_write_at now points to the node which has the offset,write_length and LRN of the record we shall write in place*/
	/*extract info from log to write in place*/
	fseek(event_log,foll_write_at->LRN,SEEK_SET);
	fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long)+sizeof(long)+sizeof(int),SEEK_CUR);
	fread(&fd,sizeof(int),1,event_log);
	fread(&offset,sizeof(off_t),1,event_log);
	fread(&write_length,sizeof(size_t),1,event_log);
        data = malloc(sizeof(char)*write_length);
        fread(data,write_length,1,event_log);

	/*write in place*/
	if(lseek(fd,offset,SEEK_SET) != offset)
		printf("\nError while lseeking");
	if(write(fd,data,write_length)==0)
		printf("\nError in writing in-place");
	//printf("LRN %ld",foll_write_at->LRN);
	fflush(stdout);
	free(foll_write_at);/*free the space occupied by the node*/
	free(data);/*free the space occupied by data*/
   }
 return;
}	
  
 
long log_rename(int txnid, char *incall[])
{
  long log_LRN;
   struct log_record t,s;
   long prev_log_LRN;

    /*extracting previous record of same transaction*/
   prev_log_LRN = get_prev_log_LRN(txnid);

   pthread_mutex_lock(&log_mutex);

   /*extract record number in log for the new log record.Its is the last present record+1.1st record number is 0*/
   log_LRN = extract_record_no();
   //printf("\n log_LRN %d, prev_log_LRN %d",log_LRN,prev_log_LRN);
   //fflush(stdout);
   
   /*updating that prev log record*/
   fseek(event_log,prev_log_LRN,SEEK_SET);
   fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long),SEEK_CUR);/*the file pointer now points to next_LRN*/

   if(fwrite(&log_LRN,sizeof(long),1,event_log)==0){
        error("error on fwrite");
        return(-1);
   }

   /*updating link list node for this transaction for last_LRN*/
   update_txn(txnid,log_LRN);

   /*initialize a new record to insert into log*/
   t.LRN = log_LRN;
   t.txn_id = txnid;
   t.prev_LRN = prev_log_LRN;
   t.next_LRN = -1;
   t.record_type = RENAME;
   t.fd = -1;
   t.offset = -1;
   t.write_length = strlen(incall[1])+1;
   //printf("%s",data);
   //fflush(stdout);
   t.data = malloc((t.write_length)*sizeof(char));
   strcpy(t.data,incall[1]);
   t.write_length1 = strlen(incall[2])+1;
   t.data1 = malloc(sizeof(char)*(t.write_length1));
   strcpy(t.data1,incall[2]);

   
   
   /*append a new record of type tranx_begin at the end of log */
   fseek(event_log, 0,SEEK_END);
   fwrite(&t.LRN,sizeof(long),1,event_log);
    fwrite(&t.txn_id,sizeof(int),1,event_log);
    fwrite(&t.prev_LRN,sizeof(long),1,event_log);
    fwrite(&t.next_LRN,sizeof(long),1,event_log);
    fwrite(&t.record_type,sizeof(int),1,event_log);
    fwrite(&t.fd,sizeof(int),1,event_log);
    fwrite(&t.offset,sizeof(long),1,event_log);
    fwrite(&t.write_length,sizeof(long),1,event_log);
    fwrite(t.data,t.write_length,1,event_log);
    fwrite(&t.write_length1,sizeof(long),1,event_log);
    fwrite(t.data1,t.write_length1,1,event_log);

   free(t.data);
   free(t.data1);
   pthread_mutex_unlock(&log_mutex);
   fflush(event_log);
       /*return record no. of log entry*/
   return(log_LRN);

}


void delayed_rename(struct file_lock *file_rename1)
{
  int fd,status,flag = 0;
  char *oldpath,*newpath;
  size_t write_length1,write_length2;
  struct file_lock *file_rename2; 
  struct write_records *ptr_rename1=NULL, *ptr_rename2=NULL;

  ptr_rename1 = file_rename1->writes_at;
  
  	/*we hold a lock on file_mutex from release locks()*/
  	if(ptr_rename1->write_length != -3)/*OLDPATH NOT EQUAL TO NEWPATH. Find the other path*/
  	{
     		file_rename2 = file_rename1->next_txn;/*This may be NULL*/
		//printf("\n %d %d",(file_rename2->writes_at)->LRN,ptr_rename1->LRN);
		fflush(stdout);
     		while(1)
		{
			if(file_rename2 == NULL)//Should not happen but just in case
			{
				printf("\nfile_rename2 = NULL");
				fflush(stdout);
				break;
			}
			if(file_rename2->writes_at != NULL)//Searches the writes_at list of a particular file
			{
				ptr_rename2 = file_rename2->writes_at;
				while(ptr_rename2 != NULL)
				{
					if(ptr_rename2->LRN == ptr_rename1->LRN)
					{
						flag =1;
						break;
					}
					else
						ptr_rename2 = ptr_rename2->next;
				}
			}
			if(flag == 1)
				break;
			else
				file_rename2 = file_rename2->next_txn;
		}
			
     		
  	}

	flag = 0;
  	/*extract info from log to rename in place*/
  	fseek(event_log,ptr_rename1->LRN,SEEK_SET);
  	fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long)+sizeof(long)+sizeof(int)+sizeof(int)+sizeof(off_t),SEEK_CUR);
  	fread(&write_length1,sizeof(size_t),1,event_log);
  	oldpath = malloc(sizeof(char)*(write_length1));
  	fread(oldpath,write_length1,1,event_log);
  	fread(&write_length2,sizeof(size_t),1,event_log);
  	newpath = malloc(sizeof(char)*(write_length2));
  	fread(newpath,write_length2,1,event_log);
  

  	status = rename(oldpath,newpath);
  	if (status == -1)
		error("\n ERROR on rename");
  
  	/*if(file_rename1->writes_at != NULL)
	{
		printf("\n %d",(file_rename1->writes_at)->LRN);
		fflush(stdout);
	}*/
  	if(ptr_rename1->write_length != -3)
  	{
		if (file_rename2->writes_at == ptr_rename2)
		{
  			file_rename2->writes_at = ptr_rename2->next;
			if(file_rename2->writes_at != NULL)
				(file_rename2->writes_at)->prev = NULL;				
		}
		else{
			(ptr_rename2->prev)->next = ptr_rename2->next;
			if(ptr_rename2->next != NULL)
				(ptr_rename2->next)->prev = ptr_rename2->prev;
		}
		
  		free(ptr_rename2);
  	}
  	free(ptr_rename1);
  	free(oldpath);
  	free(newpath);
 return;
}	
  

long log_creat(int txnid, char *incall[])
{
  long log_LRN;
  struct log_record t,s;
  long prev_log_LRN;

  /*extracting previous record of same transaction*/
  prev_log_LRN = get_prev_log_LRN(txnid);

  pthread_mutex_lock(&log_mutex);

  /*extract record number in log for the new log record.Its is the last present record+1.1st record number is 0*/
  log_LRN = extract_record_no();
     
  /*updating that prev log record*/
  fseek(event_log,prev_log_LRN,SEEK_SET);
  fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long),SEEK_CUR);/*the file pointer now points to next_LRN*/

  if(fwrite(&log_LRN,sizeof(long),1,event_log)==0){
        error("error on fwrite");
        return(-1);
   }

  /*updating link list node for this transaction for last_LRN*/
  update_txn(txnid,log_LRN);

  /*initialize a new record to insert into log*/
  t.LRN = log_LRN;
  t.txn_id = txnid;
  t.prev_LRN = prev_log_LRN;
  t.next_LRN = -1;
  t.record_type = CREAT;
  t.fd = -1;
  t.offset = -1;
  t.write_length = strlen(incall[1])+1;
  t.data = malloc((t.write_length)*sizeof(char));
  strcpy(t.data,incall[1]);
  t.write_length1 = strlen(incall[2])+1;
  t.data1 = malloc(sizeof(char)*(t.write_length1));
  strcpy(t.data1,incall[2]);

   
   
   /*append a new record of type CREAT at the end of log */
   fseek(event_log, 0,SEEK_END);
   fwrite(&t.LRN,sizeof(long),1,event_log);
   fwrite(&t.txn_id,sizeof(int),1,event_log);
   fwrite(&t.prev_LRN,sizeof(long),1,event_log);
   fwrite(&t.next_LRN,sizeof(long),1,event_log);
   fwrite(&t.record_type,sizeof(int),1,event_log);
   fwrite(&t.fd,sizeof(int),1,event_log);
   fwrite(&t.offset,sizeof(long),1,event_log);
   fwrite(&t.write_length,sizeof(long),1,event_log);
   fwrite(t.data,t.write_length,1,event_log);
   fwrite(&t.write_length1,sizeof(long),1,event_log);
   fwrite(t.data1,t.write_length1,1,event_log);

   free(t.data);
   free(t.data1);
   pthread_mutex_unlock(&log_mutex);
   fflush(event_log);
       /*return record no. of log entry*/
   return(log_LRN);

}

void delayed_creat(struct file_lock *file_creat, int txnid)
{
  int status;
  char *newpath,*mode;
  mode_t new_mode;
  size_t write_length1,write_length2;/*length of the path to new file to be created*/
  struct file_lock *file_rename2; 
  struct write_records *ptr_creat=NULL;
  struct stat path_buf;
  struct txn_node *ptr_txn_node;

  ptr_creat = file_creat->writes_at;
  
  /*we hold a lock on file_mutex from release locks()*/
  /*extract info from log to creat in place*/
  fseek(event_log,ptr_creat->LRN,SEEK_SET);
  fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long)+sizeof(long)+sizeof(int)+sizeof(int)+sizeof(off_t),SEEK_CUR);
  fread(&write_length1,sizeof(size_t),1,event_log);
  newpath = malloc(sizeof(char)*(write_length1));
  fread(newpath,write_length1,1,event_log);
  fread(&write_length2,sizeof(size_t),1,event_log);
  mode = malloc(sizeof(char)*(write_length2));
  fread(mode,write_length2,1,event_log);

  if(strcmp(mode,"256")==0)
	{
	//	status = creat(newpath,S_IRUSR);/*status will be the fd of the new created file if successful else -1*/
	        status = tr_sync_file_to_disk (newpath);	
	        printf ("delayed_creat ::  status returned = %d\n", status);
	}	
  if (status == -1)
	error("\n ERROR on creat");
  else{/*marking the read_bit of the newly created file*/
	/* if(stat(newpath,&path_buf)!=0)
		error("Could not get file status");*/
/*	  pthread_mutex_lock(&txn_mutex);

 	ptr_txn_node = head_txn_list;

 	while(ptr_txn_node->txn_id != txnid)
 		ptr_txn_node = ptr_txn_node->next;

	pthread_mutex_unlock(&txn_mutex);

	if(ptr_txn_node->before_after == 0)/*txnid is before tb thus newpath will be nonsticky
	{
		/*make it non sticky
		 new_mode = path_buf.st_mode& ~S_ISVTX;
       		 chmod(newpath,new_mode);
	}
	else/*txnid is after tb thus mark newpath as sticky for consistency
	{
		/*make it sticky
		new_mode = path_buf.st_mode|S_ISVTX;
        	chmod(newpath,new_mode);
	}*/
		
   }

  
  free(newpath);

 return;
}	

long log_unlink(int txnid, char *incall[])
{
  long log_LRN;
  struct log_record t,s;
  long prev_log_LRN;

  /*extracting previous record of same transaction*/
  prev_log_LRN = get_prev_log_LRN(txnid);

  pthread_mutex_lock(&log_mutex);

  /*extract record number in log for the new log record.Its is the last present record+1.1st record number is 0*/
  log_LRN = extract_record_no();
     
  /*updating that prev log record*/
  fseek(event_log,prev_log_LRN,SEEK_SET);
  fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long),SEEK_CUR);/*the file pointer now points to next_LRN*/

  if(fwrite(&log_LRN,sizeof(long),1,event_log)==0){
        error("error on fwrite");
        return(-1);
   }

  /*updating link list node for this transaction for last_LRN*/
  update_txn(txnid,log_LRN);

  /*initialize a new record to insert into log*/
  t.LRN = log_LRN;
  t.txn_id = txnid;
  t.prev_LRN = prev_log_LRN;
  t.next_LRN = -1;
  t.record_type = UNLINK;
  t.fd = -1;
  t.offset = -1;
  t.write_length = strlen(incall[1])+1;
  t.data = malloc((t.write_length)*sizeof(char));
  strcpy(t.data,incall[1]);
  t.write_length1 = strlen("NA\n")+1;
  t.data1 = malloc(sizeof(char)*(t.write_length1));
  strcpy(t.data1,"NA\n");

   
   
   /*append a new record of type CREAT at the end of log */
   fseek(event_log, 0,SEEK_END);
   fwrite(&t.LRN,sizeof(long),1,event_log);
   fwrite(&t.txn_id,sizeof(int),1,event_log);
   fwrite(&t.prev_LRN,sizeof(long),1,event_log);
   fwrite(&t.next_LRN,sizeof(long),1,event_log);
   fwrite(&t.record_type,sizeof(int),1,event_log);
   fwrite(&t.fd,sizeof(int),1,event_log);
   fwrite(&t.offset,sizeof(long),1,event_log);
   fwrite(&t.write_length,sizeof(long),1,event_log);
   fwrite(t.data,t.write_length,1,event_log);
   fwrite(&t.write_length1,sizeof(long),1,event_log);
   fwrite(t.data1,t.write_length1,1,event_log);

   free(t.data);
   free(t.data1);
   pthread_mutex_unlock(&log_mutex);
   fflush(event_log);
       /*return record no. of log entry*/
   return(log_LRN);

}


void delayed_unlink(struct file_lock *file_creat)
{
  int status;
  char *newpath;
  size_t write_length1;/*length of the path of the to-be-deleted file */
  struct file_lock *file_rename2; 
  struct write_records *ptr_creat=NULL;
 
  ptr_creat = file_creat->writes_at;
  
  /*we hold a lock on file_mutex from release locks()*/
  /*extract info from log to creat in place*/
  fseek(event_log,ptr_creat->LRN,SEEK_SET);
  fseek(event_log,sizeof(long)+sizeof(int)+sizeof(long)+sizeof(long)+sizeof(int)+sizeof(int)+sizeof(off_t),SEEK_CUR);
  fread(&write_length1,sizeof(size_t),1,event_log);
  newpath = malloc(sizeof(char)*(write_length1));
  fread(newpath,write_length1,1,event_log);
  
  status = unlink(newpath);/*status will be 0 if successful else -1*/
  if (status == -1)
	error("\n ERROR on creat");
  
  free(newpath);

 return;
}

void delayed_update(struct file_lock *file_update,int txnid)
{
 	while(file_update->writes_at != NULL)
	{
		if(((file_update->writes_at)->write_length == -1)||((file_update->writes_at)->write_length == -2)||((file_update->writes_at)->write_length == -3))//Its a rename
			delayed_rename(file_update);

		if((file_update->writes_at)->write_length == -4)//Its a creat
				delayed_creat(file_update,txnid);
		
		if((file_update->writes_at)->write_length == -5)//Its a unlink
				delayed_unlink(file_update);
	
		file_update->writes_at = (file_update->writes_at)->next;
	}
}
