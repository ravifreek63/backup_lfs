/*
Provide the trace file as an argument.
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include "client_config.h"

#define MAX_OPEN_FILES 100
#define FILE_BUFFER_SIZE 100
#define MAX_SYSTEM_CALL_LENGTH 100
#define  RESULT_STRING_SIZE 200

extern int call_execute (char *argcalls[]);
extern void set_transaction_id (unsigned int t_id);
extern int sockfd;
int num_transactions=1000000;

extern int result_log_const (const char *str_const);

// returns the inode number for a particular file 
int inode_stat (const char *path)
{
// printf ("In Inode Stat\n");
 char *newargv1[] = {"inode_stat",path, "sesh"};
 int ret_value = call_execute(newargv1);
 return ret_value;
}



struct trace_rec
        {
		int txnid;
		int syscall_length;
		char *syscall;
	};

 struct open_files{
		long inode;
		int fd;
		};

bool open_read_data_file (FILE **fd)
{
  FILE *fd_file = fopen("/btp/yaffs2-dir/TxFS_Mam/Traces/testbed/read_data.txt", "r");
  if (fd_file == NULL) return false;
  *fd = fd_file;
  return true;
}

bool open_write_data_file (FILE **fd, int iter_num)
{
  char *filePath = (char *)(malloc)(sizeof(char) * 100);
  sprintf (filePath, "/btp/yaffs2-dir/TxFS_Mam/Traces/testbed/concurrent/write_data%d.txt",iter_num);
  printf("FilePath %s\n", filePath);
  FILE *fd_file = fopen(filePath, "w");
  free (filePath);
  if (fd_file == NULL) return false;
  *fd = fd_file;
  return true;
}

bool write_to_file (int write_bytes, char *write_buf, FILE *write_data_fd)
{
  int bytesWritten = fwrite (write_buf, 1, write_bytes, write_data_fd);
  if (bytesWritten == write_bytes) 
     return true;
  else
    {
      printf("BUG :: Reading From File Failed : Check Function read_from_file: transaction.c\n");
      return false;
    }
}


bool read_from_file (int read_bytes, char *read_buf, FILE *read_data_fd)
{
  int bytesRead = fread (read_buf, 1, read_bytes, read_data_fd);
  if (bytesRead == read_bytes) 
     return true;
  else
    {
      printf("BUG :: Reading From File Failed : Check Function read_from_file: transaction.c\n");
      return false;
    }
}

int main(int argc, char *argv[])
{

 int iter_num=0;
 if (argc > 2)
 {
	if (argv[3] != NULL)
		iter_num = atoi (argv[3]);
 }
 FILE *read_data_fd;
 FILE *write_data_fd;
 char *result_string = (char *)malloc (RESULT_STRING_SIZE);
 memset(result_string, 0, RESULT_STRING_SIZE);
 if (!open_read_data_file (&read_data_fd))
   {
      printf("File Opened failed for read data file : BUG \n");
      return -1;
   } 
 if (!open_write_data_file (&write_data_fd, iter_num))
   {
      printf("File Opened failed for write data file : BUG \n");
      return -1;
   } 
 int inode_num = -1;
 char f_buffer[FILE_BUFFER_SIZE];
 struct trace_rec t;
 FILE *fd_trace;
 struct timeval tv;
 struct open_files open_fd[MAX_OPEN_FILES]; 
 char *cur_syscall,*call,*arg1,*arg2,*nullstr = NULL;
 int len,i,j,fd,open_fd_index=0,terminate=0,cur_txnid,count_busy_wait=0,l,l1;
 struct stat buff;
 char buf[20],*read_buf, *write_buf;
 size_t nbyte,read_bytes;
 gettimeofday(&tv,NULL);
 srand((tv.tv_sec*1000)+(tv.tv_usec/1000));
 fd_trace = fopen(argv[1],"r+b");
 int to_read;
 if(!fd_trace)
 {
       char str[100];
	printf("Unable to open file %s\n",argv[1]);	
       sprintf(str, "Unable to open file %s\n",argv[1]);	
       printf("%s\n", str);
       return -1;
 }
 memset (f_buffer, 0, FILE_BUFFER_SIZE);
 while (fgets(f_buffer, FILE_BUFFER_SIZE, fd_trace) != NULL)
 {
   	t.txnid = atoi(f_buffer);
   	//memset(result_string, 0, RESULT_STRING_SIZE);
        //sprintf(result_string, "transaction id  = %d\n", t.txnid);
        //result_log(result_string, fptr);
	memset (f_buffer, 0, FILE_BUFFER_SIZE);
	fgets (f_buffer, FILE_BUFFER_SIZE, fd_trace);
	t.syscall_length = strlen(f_buffer);
	if (t.syscall_length>0 && f_buffer[t.syscall_length-1] == '\n')
		t.syscall_length--;
        t.syscall = (char *) malloc(sizeof(char)*(t.syscall_length+1));
	memset(t.syscall, 0, t.syscall_length+1);
        strncpy (t.syscall, f_buffer, t.syscall_length);
   	//memset(result_string, 0, RESULT_STRING_SIZE);
	//sprintf(result_string, "system call length = %d", t.syscall_length);
	//result_log(result_string, fptr);
   	//memset(result_string, 0, RESULT_STRING_SIZE);
	//sprintf(result_string, "system call = %s", t.syscall);        
	//result_log(result_string, fptr);
       
	/*
		Copying the system call to a local variable
	*/
	cur_syscall = (char *)malloc((sizeof(char))*(t.syscall_length+1));
	memset(cur_syscall,0,t.syscall_length+1);
	strcpy(cur_syscall,t.syscall);

	/*Extracting the system call from the trace into the string call*/
	j=0;
	len=0;
	while(cur_syscall[len] !='(')
		len = len+1;
	len = len+1;

	call = (char *) malloc(sizeof(char)*len);
	memset(call,0,len*sizeof(char));
	while(cur_syscall[j]!='(')
	{
		call[j]=cur_syscall[j];
		j=j+1;
	}
		

	/*Calling the actual syscalls corresponding to the call in trace*/
	if(strcmp(call,"txn_beg")==0)
	{
		num_transactions--;
		if(num_transactions<0) 
		break;
		
		memset(result_string, 0, RESULT_STRING_SIZE);
		//sprintf(result_string, "function main :: system call = txn_beg transaction id = %d\n",t.txnid);	
		//log_client (result_string, fptr);
                int portNo;
		if (argv[2] != NULL)
                portNo = atoi(argv[2]);	
		txn_beg(t.txnid, portNo);
		/*resetting open files array for the new transaction*/
		for(i=0;i<MAX_OPEN_FILES;i++)
		{
			open_fd[i].inode=0;
			open_fd[i].fd=0;
		}
		open_fd_index =0;
	}
	
	if(strcmp(call,"txn_commit")==0)
	{
		char str[100];
		//sprintf(str, "function main :: system call = txn_commit :: transaction id = %d\n",t.txnid);	
		//log_client (str, fptr);
		fflush(stdout);
		txn_commit();
	}
	if(strcmp(call,"txn_abort")==0)
	{
		char str[100];
		//sprintf(str, "function main :: system call = txn_abort :: transaction id = %d\n",t.txnid);	
		//log_client (str, fptr);
		fflush(stdout);
		txn_abort();
	}

	if(strcmp(call,"open")==0)
	{
		char str[100];
		//sprintf(str, "function main :: system call = open :: transaction id = %d\n",t.txnid);	
		//log_client (str, fptr);
		/*j as above is the index of ( in the trace string. So arg1 is from j+1 to ","*/
		len=j+1;
		while(cur_syscall[len] !=',')
			len = len+1;
		len = (len-j)+1;
 		arg1 = (char *)malloc(sizeof(char)*len);
		memset(arg1,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=',')
		{
			arg1[i]=cur_syscall[j];
			j=j+1;	i=i+1;
		}
		//printf(" arg1 %s %d",arg1,strlen(arg1));

		/*j as above is the index of , in the trace string. So arg2 is from j+1 to ","*/
		len=j+1;
		while(cur_syscall[len] !=')')
			len = len+1;
		len = (len-j)+1;
 		arg2 = (char *)malloc(sizeof(char)*len);
		memset(arg2,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=')')
		{
			arg2[i]=cur_syscall[j];
			j=j+1;
			i=i+1;
		}
		arg2[i]='\0';
		//printf(" arg2 %s",arg2);
		fflush(stdout);
		if(strcmp(arg2,"O_RDONLY")==0)
		{
			//printf(" arg2 %s",arg2);
			fd = open(arg1,O_RDONLY);
		}
		if(strcmp(arg2,"O_WRONLY")==0)
		{	
			//printf(" arg2 %s",arg2);
			fd = open(arg1,O_WRONLY);
		}
		if(strcmp(arg2,"O_RDWR")==0)
		{
			//printf(" 1arg2 %s",arg2);
			fflush(stdout);
			fd = open(arg1,O_RDWR);
		}
		if(strcmp(arg2,"O_WRONLY|O_APPEND")==0 || strcmp(arg2,"O_APPEND")==0)
		{
			fd = open(arg1,O_WRONLY|O_APPEND);
			//printf(" arg2 %s",arg2);
		}
		
		if(fd !=-1)
		{
			inode_num = inode_stat(arg1);
			if(inode_num == -1)
				{
					printf("From System Call open() :: Error reading inode\n");
				}
		
			open_fd[open_fd_index].inode = (long)(inode_num);
			open_fd[open_fd_index].fd = fd;	
		}
		else
		{
			printf("\nError on open %d\n",t.txnid);
			fflush(stdout);
			exit_txn();
			/*skip records to read next transaction as this transaction has been aborted*/
			terminate = 1;			
		}
		fflush(stdout);
		free(arg1);
		free(arg2);

	
		//sprintf(str, "main: open system call on object %d returns file descriptor = %d",inode_num, fd);

		//log_client (str, fptr);
	}

	if(strcmp(call,"read")==0)
	{
		char str[100];
		//sprintf(str, "function main :: system call = read :: transaction id = %d\n",t.txnid);	
		//log_client (str, fptr);

		fflush(stdout);
		len=j+1;
		while(cur_syscall[len] !=',')
			len = len+1;
		len = (len-j)+1;
 		arg1 = (char *) malloc(sizeof(char)*len);
		memset(arg1,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=',')
		{
			arg1[i]=cur_syscall[j];
			j=j+1;	i=i+1;
		}
		
		inode_num = inode_stat(arg1);
			if(inode_num == -1)
				{
					printf("From System Call write() :: Error reading inode\n");
				}			

		i =0;
		for(i=0;i<=open_fd_index;i++)
			if(open_fd[i].inode == (long)(inode_num))
				break;
		//printf("File Descriptor = %d\n", open_fd[i].fd);
		int fileHandle = open_fd[i].fd;
// Reading the number of bytes to be read 
		len=j+1;
		while(cur_syscall[len] !=')')
			len = len+1;
		len = (len-j)+1;
 		arg2 = (char *) malloc(sizeof(char)*len);
		memset(arg2,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=')')
		{
			arg2[i]=cur_syscall[j];
			j=j+1;
			i=i+1;
		}
		arg2[i]='\0';						
		read_bytes = atoi(arg2);                 		
                while (read_bytes > 0)
			{
				to_read = (read_bytes >=MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE-1:read_bytes;
				read_buf = (char *) malloc((to_read+1)*sizeof(char));				
				read_bytes -= to_read; 			
				memset(read_buf,0,to_read+1);
				strcpy(read_buf,"dummy");
				//printf("File Descriptor = %d\n", open_fd[i].fd);
		 		if(read(fileHandle,read_buf,to_read) == -1)
				{
					printf("\n Error reading from file for %d",t.txnid);
					fflush(stdout);
					exit_txn();
					/*skip records to read next transaction as this transaction has been aborted*/
					terminate = 1;
				}
				else
				{
				 	//printf("\n Read data %s\n",read_buf);
					write_to_file (to_read, read_buf, write_data_fd);
				}
				free(read_buf);
		         }
 
		free(arg1);
		free(arg2);
//		free(read_buf);
		//printf("\n read %d %d",thread_node->txnid,open_fd[i].fd);
	    }
	
	if(strcmp(call,"write")==0)
	{
		int to_write=0;
		int ret_val;
		char str[100];
		//sprintf(str, "function main :: system call = write :: transaction id = %d\n",t.txnid);	
		//log_client (str, fptr);

		len=j+1;
		while(cur_syscall[len] !=',')
			len = len+1;

		len = (len-j)+1;
		arg1 = malloc(sizeof(char)*len);
		memset(arg1,0,len*sizeof(char));

		j=j+1;
		i=0;

		// arg1 is the path
		while(cur_syscall[j]!=',')
		{
			arg1[i]=cur_syscall[j];
			j=j+1;	i=i+1;
		}

			inode_num = inode_stat(arg1);
			if(inode_num == -1)
				{
					printf("From System Call write() :: Error reading inode\n");
				}		
			else 
				{
					;//printf("Inode Number = %d\n", inode_num);
				}	

		i =0;
		for(i=0;i<=open_fd_index;i++)
			if(open_fd[i].inode == (long)(inode_num))
				break;
		int fileHandle = open_fd[i].fd;
	// Reading the number of bytes to write 
		len=j+1;
		while(cur_syscall[len] !=')')
			len = len+1;
		len = (len-j)+1;
 		arg2 = malloc(sizeof(char)*len);
		memset(arg2,0,len*sizeof(char));
		j=j+1;
		i=0;

		while(cur_syscall[j]!=')')
		{

			arg2[i]=cur_syscall[j];
			j=j+1;
			i=i+1;
		}

		arg2[i]='\0';

		int write_bytes = atoi (arg2);
		if (write_bytes == 0)
		{
			printf("System Call Write : Number of bytes to be written = %d\n", write_bytes);
		}
		while (write_bytes > 0)
			{
				to_write = (write_bytes >=MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE-1:write_bytes;
				write_buf = (char *) malloc((to_write+1)*sizeof(char));				
				write_bytes -= to_write; 			
				memset(write_buf,0,to_write+1);
				
				
		 		if(read_from_file (to_write, write_buf, read_data_fd) == 0)
				{
					printf("\n Error reading from file for %d",t.txnid);
					fflush(stdout);
					/*skip records to read next transaction as this transaction has been aborted*/
					terminate = 1;
				}
				else
				{
				 	ret_val = write(fileHandle, write_buf, to_write);
					if(ret_val == 0)
					{
	       					printf("\n Error writting to file for %d\n",t.txnid);
						fflush(stdout);
						exit_txn();
						/*skip records to read next transaction as this transaction has been aborted*/
						terminate = 1;
					}
					else
					{
					
//sprintf(str, "function main :: system call = write :: transaction id = %d :: on finish writes %d Bytes on file %s\n",t.txnid ,ret_val, arg1);	
			//log_client (str, fptr);
					}
				}
				free(write_buf);
		         }
		free(arg1);
		free(arg2);
	}


	if(strcmp(call,"lseek")==0)
	{
		printf("\n%d %s",t.txnid,t.syscall);
		fflush(stdout);
		len=j+1;
		while(cur_syscall[len] !=',')
			len = len+1;
		len = (len-j)+1;
		arg1 = malloc(sizeof(char)*len);
		memset(arg1,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=',')
		{
			arg1[i]=cur_syscall[j];
			j=j+1;	i=i+1;
		}
		inode_num = inode_stat(arg1);
			if(inode_num == -1)
				{
					printf("From System Call write() :: Error reading inode\n");
				}
			

		i =0;
		for(i=0;i<=open_fd_index;i++)
		{
			//printf("Inode Number  %d, Inode Number = %d, File Handle %d\n", inode_num, open_fd[i].inode, open_fd[i].fd);	
			if(open_fd[i].inode == (long)(inode_num))
			{
				//printf("Inode Number  %d, File Handle %d\n", open_fd[i].inode, open_fd[i].fd);	
				break;
			}
		}
			
		int fileHandle = open_fd[i].fd;
		len=j+1;
		while(cur_syscall[len] !=')')
			len = len+1;
		len = (len-j)+1;
 		arg2 = malloc(sizeof(char)*len);
		memset(arg2,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=')')
		{
			arg2[i]=cur_syscall[j];
			j=j+1;
			i=i+1;
		}
		arg2[i]='\0';
		int seek_offset = atoi (arg2);		
		if(lseek(fileHandle, seek_offset,SEEK_SET) == -1)
		{
       			printf("\n Error seeking to file for %d",t.txnid);
			fflush(stdout);
			exit_txn();
			/*skip records to read next transaction as this transaction has been aborted*/
			terminate = 1;
		}
		free(arg1);
		printf("\n lseek Done  %d\n",fileHandle);
	}


	if(strcmp(call,"rename")==0)
	{
		printf("\n %d %s",t.txnid,t.syscall);
		fflush(stdout);
		/*j as above is the index of ( in the trace string. So arg1 is from j+1 to ","*/
		len=j+1;
		while(cur_syscall[len] !=',')
			len = len+1;
		len = (len-j)+1;
		arg1 = malloc(sizeof(char)*len);
		memset(arg1,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=',')
		{
			arg1[i]=cur_syscall[j];
			j=j+1;	i=i+1;
		}
		//printf(" arg1 %s",arg1);
		/*j as above is the index of , in the trace string. So arg2 is from j+1 to ","*/
		len=j+1;
		while(cur_syscall[len] !=')')
			len = len+1;
		len = (len-j)+1;
		arg2 = malloc(sizeof(char)*len);
		memset(arg2,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=')')
		{
			arg2[i]=cur_syscall[j];
			j=j+1;
			i=i+1;
		}
		//printf(" arg2 %s",arg2);
		if(rename(arg1,arg2) == -1)
		{
       			printf("\n Error while doing rename for %d",t.txnid);
			fflush(stdout);
			exit_txn();
			terminate = 1;
		}
		free(arg1);
		free(arg2);
	}

	if(strcmp(call,"creat")==0)
	{

		//fflush(stdout);
		/*j as above is the index of ( in the trace string. So arg1 is from j+1 to ","*/
		len=j+1;
		while(cur_syscall[len] !=',')
			len = len+1;
		len = (len-j)+1;
		arg1 = malloc(sizeof(char)*len);
		memset(arg1,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=',')
		{
			arg1[i]=cur_syscall[j];
			j=j+1;	i=i+1;
		}

		/*j as above is the index of , in the trace string. So arg2 is from j+1 to ","*/
		len=j+1;
		while(cur_syscall[len] !=')')
			len = len+1;
		len = (len-j)+1;
		arg2 = malloc(sizeof(char)*len);
		memset(arg2,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=')')
		{
			arg2[i]=cur_syscall[j];
			j=j+1;
			i=i+1;
		}
		/*t_mode of creat at present is only for S_IRUSR. To increase just increase the if statements*/
		
		int creat_ret =0;
		if(strcmp(arg2,"S_IRUSR")==0)
		  {	
			creat_ret = creat(arg1,S_IRUSR);
			char str[100];
			//sprintf(str, "function = main : create on client side returns %d (Object Id for the file)", creat_ret);
			//log_client (str, fptr);
			
		  }
		else if(strcmp(arg2,"S_IWUSR")==0)
		  {	
			creat_ret = creat(arg1,S_IWUSR);
			char str[100];
			//sprintf(str, "function = main : create on client side returns %d (Object Id for the file)", creat_ret);
			//log_client (str, fptr);
			
		  }
		else if(strcmp(arg2,"S_IRUSR|S_IWUSR")==0)
		  {	
			creat_ret = creat(arg1,S_IRUSR|S_IWUSR);
			char str[100];
			//sprintf(str, "function = main : create on client side returns %d (Object Id for the file)", creat_ret);
			//log_client (str, fptr);
			
		  }
		if(creat_ret == -1)
		{
       			printf("\n Error while doing creat for %d",t.txnid);
			fflush(stdout);
			exit_txn();
			terminate = 1;
		}
		free(arg1);
		free(arg2);
	}
	if(strcmp(call,"unlink")==0)
	{
		printf("\n %d %s",t.txnid,t.syscall);
		fflush(stdout);
		/*j as above is the index of ( in the trace string. So arg1 is from j+1 to ","*/
		len=j+1;
		while(cur_syscall[len] !=')')
			len = len+1;
		len = (len-j)+1;
		arg1 = malloc(sizeof(char)*len);
		memset(arg1,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=')')
		{
			arg1[i]=cur_syscall[j];
			j=j+1;	i=i+1;
		}
		
		/*t_mode of creat at present is only for S_IRUSR. To increase just increase the if statements*/
		//printf(" arg2 %s",arg2);

		if(unlink(arg1) == -1)
		{
       			printf("\n Error while doing unlink for %d",t.txnid);
			fflush(stdout);
			exit_txn();
			terminate = 1;
		}
		free(arg1);
	}
// Stat Is Not Supported //
	if(strcmp(call,"stat")==0)
	{
		printf("\n %d %s",t.txnid,t.syscall);
		fflush(stdout);
		/*j as above is the index of ( in the trace string. So arg1 is from j+1 to ","*/
		len=j+1;
		while(cur_syscall[len] !=',')
			len = len+1;
		len = (len-j)+1;
		arg1 = malloc(sizeof(char)*len);
		memset(arg1,0,len*sizeof(char));
		j=j+1;
		i=0;
		while(cur_syscall[j]!=',')
		{
			arg1[i]=cur_syscall[j];
			j=j+1;	i=i+1;
		}
		
		/*t_mode of creat at present is only for S_IRUSR. To increase just increase the if statements*/
		//printf(" arg2 %s",arg2);
		struct stat sb;
		//if(stat_f(arg1,&sb) == -1)
		{
       	//		printf("\n Error while doing stat for %d",t.txnid);
	//		fflush(stdout);
	//		exit_txn();
	//		terminate = 1;
		}
		free(arg1);
	}
	
	

	
	free(t.syscall);
	free(cur_syscall);
	free(call);
	/*Skipping records when a transaction is aborted*/
	if(terminate == 1)
	{
		cur_txnid = t.txnid;
		while(1)
		{
			/*if operations in this file is over*/
			if(fread(&t.txnid,sizeof(int),1,fd_trace)==0)
				break;
			
			if(t.txnid != cur_txnid)
			{
				terminate = 0;
				break;
			}
 			/*reading one record*/
  			fread(&t.syscall_length,sizeof(int),1,fd_trace);
        		fseek(fd_trace,t.syscall_length,SEEK_CUR);
        	}
		fseek(fd_trace,-sizeof(int),SEEK_CUR);
	
	}
	/*if aborted transaction is the last transaction in this file*/
	if(terminate == 1)
		break;

/*busy wait between operations*/
count_busy_wait =0;
count_busy_wait = rand()%2000;
l=0;
int dummy =0, v;
char c[500];
//printf("Before loop l=%d count_busy_wait%d",l,count_busy_wait);
for(l=1;l<= count_busy_wait;l++)
{
	if(dummy == 0)
	{
		//printf("After loop l=%d count = %d",l,count_busy_wait);
		dummy =1;
	}
	else{
		if(dummy == 1)
		{
			dummy = dummy+dummy/5;
			dummy = 2;
		}
		else{
			if(dummy == 2)
			{
				for(v=0;v<500;v++)
					c[v] = 'c';
				dummy = 0;
			}
		    }
	}
}
				
  //for(l1=1;l1<=count_busy_wait;l1++);

	
}
fclose(read_data_fd);
fclose(write_data_fd);
fflush(stdout);
printf("End on the client side\n");
free (result_string);
return 0;
}


