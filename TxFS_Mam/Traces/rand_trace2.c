# include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
# define NUM_FILE_OP 8

int deg_of_con;/*required degree of concurrency. Taken as input*/
int max_no_op;/*maximum number of operations per transaction*/
int max_no_txn;/*total transactions in the entire run*/
FILE *listoffiles;

int txnid;
pthread_mutex_t txnid_mutex;

/*record structure for a trace*/
struct trace_rec{
		int txnid;
		int syscall_length;
		char *syscall;
	};
/*table of filesystem operations*/
const char *file_op[] = {
		"read",
		"write",
		"append",
		"lseek",
		"rename",
		"creat",
		"unlink",
		"stat"
		};
void write_to_file(int txnid,char *syscall,FILE *fd_local)
{
  struct trace_rec trace;
  	trace.txnid = txnid;
  	trace.syscall_length = strlen(syscall)+1;//txn_beg
  	trace.syscall = malloc(sizeof(char)*trace.syscall_length);
	memset(trace.syscall,0,trace.syscall_length*sizeof(char));
  	strcpy(trace.syscall,syscall);
  	//printf("\n %d %s",txnid,trace.syscall);

	//fseek(fd_local, 0,SEEK_END);  	
	fwrite(&trace.txnid,sizeof(int),1,fd_local);
  	fwrite(&trace.syscall_length,sizeof(int),1,fd_local);
  	fwrite(trace.syscall,trace.syscall_length,1,fd_local);
	free(trace.syscall);
	fflush(fd_local);
 
}

void sim_txn(FILE *fd)
{
  int this_txn,i,k,num_of_op,j,file_op,open_file_index,total_files,pathlength,file_rec,opened,system_call_length;
  int iterations,filed;
  long open_files[50];
  char *filename,*filename1,*system_call,*buffer;
  struct trace_rec trace;
  struct stat buff,buff1;
  FILE *fd_local;

  fd_local = fd;/*local copy file descriptor*/

  iterations = max_no_txn/deg_of_con;

  for(i=0;i<iterations;i++)
  {

	/*initialization for a new transaction*/
	open_file_index = -1;/*open file index indicates the number of opened files of that transaction*/
	opened = 0;

  	/*assign txn_id*/
  	txnid = txnid+1;
  	this_txn = txnid;
 
	write_to_file(this_txn,"txn_beg()",fd_local);
	
	
	num_of_op = rand()%max_no_op+1;//number of operation chosen between 1 and max_no_op
	printf("\n no of op %d",num_of_op);
	
	for(j=0;j<num_of_op;j++)
	{
		/*choose an operation randomly from the table of operations*/
		
		file_op	= rand()%(NUM_FILE_OP);//0 to NUM_FILE_OP-1 as that is the array index
		//printf("\n txnid %d file_op %d",this_txn,file_op);
		switch(file_op){
					case 0: /*read*/
						fseek(listoffiles,-sizeof(int),SEEK_END);
						fread(&total_files,sizeof(int),1,listoffiles);
						
						while(1)
						{
						  /*select a record in listoffiles randomly and check to see if it is a regular file else select again*/
						 file_rec = rand()%total_files+1;/*random number from 1 to total_files*/
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 //printf("\nrand_rec %d rec %d",file_rec,pathlength);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename = malloc(sizeof(char)*pathlength);
						 fread(filename,pathlength,1,listoffiles);
						 if(stat(filename,&buff) == -1)
							printf("Error reading inode");
						 else
						 	if((S_ISREG(buff.st_mode)!=0)&&(filed = open(filename,O_RDONLY))!=-1)/*it is a regular file and can be
						 opened*/{
								printf("\n %s %d filed =%d",filename,strlen(filename),filed);
								close(filed);
								/*don't select an already selected file*/
								for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;
									}
							  	}
								if(opened != 1)/*is a file and has not been opened then continue with operation*/
									break;//come out of the while loop
							}
							
						opened = 0;
						 				 
						}				
	
						
						if((open_file_index == -1) || (opened == 0))//no files have been open as yet or the file has not been opened
						{
							open_file_index = open_file_index+1;
							open_files[open_file_index] = (long)buff.st_ino;
							system_call_length = strlen("open(")+strlen(filename)+strlen(",O_RDONLY)")+1;
							system_call = malloc(sizeof(char)*system_call_length);
							memset(system_call,0,system_call_length);
							strcat(system_call,"open(");
							strcat(system_call,filename);
							strcat(system_call,",O_RDONLY)");
							write_to_file(this_txn,system_call,fd_local);
							free(system_call);
						}
						opened = 0;
						
						system_call_length = (strlen("read(")+strlen(filename)+strlen(",20)")+1);
						system_call =malloc(sizeof(char)*system_call_length);
						memset(system_call,0,system_call_length);
						strcat(system_call,"read(");
						strcat(system_call,filename);
						strcat(system_call,",20)");
						write_to_file(this_txn,system_call,fd_local);
						free(system_call);	
						free(filename);
													
						break;
					case 1:// printf("\nwrite");
						
						fseek(listoffiles,-sizeof(int),SEEK_END);
						fread(&total_files,sizeof(int),1,listoffiles);
						
						while(1)
						{
						  /*select a record in listoffiles randomly and check to see if it is a regular file else select again*/
						 file_rec = rand()%total_files+1;/*random number from 1 to total_files*/
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 //printf("\nrand_rec %d rec %d",file_rec,pathlength);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename = malloc(sizeof(char)*pathlength);
						 fread(filename,pathlength,1,listoffiles);
						 if(stat(filename,&buff) == -1)
							printf("Error reading inode");
						 else
						 	if((S_ISREG(buff.st_mode)!=0)&&(filed = open(filename,O_RDWR))!=-1)/*it is a regular file and can be
						 opened*/{
								printf("\n %s %d filed =%d",filename,strlen(filename),filed);
								close(filed);
								for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;
									}
							  	}
								if(opened != 1)/*is a file and has not been opened then continue with operation*/
									break;//come out of the while loop
							}
							
						opened = 0;
						}				
						
						if((open_file_index == -1)||(opened == 0))//no files have been open as yet
						{
							open_file_index = open_file_index+1;
							open_files[open_file_index] = (long)buff.st_ino;
							system_call_length = (strlen("open(")+strlen(filename)+strlen(",O_RDWR)")+1);
							system_call = malloc(sizeof(char)*system_call_length);
							memset(system_call,0,system_call_length);
							strcat(system_call,"open(");
							strcat(system_call,filename);
							strcat(system_call,",O_RDWR)");
							write_to_file(this_txn,system_call,fd_local);
							free(system_call);
						}
	
						opened = 0;
						system_call_length = strlen("write(")+strlen(filename)+strlen(",20)")+1;
						system_call =malloc(sizeof(char)*system_call_length);
						memset(system_call,0,system_call_length);
						strcat(system_call,"write(");
						strcat(system_call,filename);
						strcat(system_call,",20)");
						write_to_file(this_txn,system_call,fd_local);
						free(filename);
						free(system_call);
						break;
					case 2: //printf("\nappend");
						fseek(listoffiles,-sizeof(int),SEEK_END);
						fread(&total_files,sizeof(int),1,listoffiles);
						
						while(1)
						{
						  /*select a record in listoffiles randomly and check to see if it is a regular file else select again*/
						 file_rec = rand()%total_files+1;/*random number from 1 to total_files*/
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 //printf("\nrand_rec %d rec %d",file_rec,pathlength);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename = malloc(sizeof(char)*pathlength);
						 fread(filename,pathlength,1,listoffiles);
						 if(stat(filename,&buff) == -1)
							printf("Error reading inode");
						 else
							if((S_ISREG(buff.st_mode)!=0)&&(filed = open(filename,O_WRONLY|O_APPEND))!=-1)/*it is a regular file and can be
						 opened*/{
								printf("\n %s %d filed =%d",filename,strlen(filename),filed);
								close(filed);
								for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;
									}
							  	}
								if(opened != 1)/*is a file and has not been opened then continue with operation*/
									break;//come out of the while loop
							}
							
						opened = 0;				 
						}				
						
						/*for(k=0;k<=open_file_index;k++)
						{
							if((long)buff.st_ino == open_files[k])
							{
								opened = 1;
								break;
							}
						}*/
						if((open_file_index == -1)||(opened ==0))//no files have been open as yet or this file is not opened
						{
							open_file_index = open_file_index+1;
							open_files[open_file_index] = (long)buff.st_ino;
							system_call_length = (strlen("open(")+strlen(filename)+strlen(",O_WRONLY|O_APPEND)")+1);
							system_call = malloc(sizeof(char)*system_call_length);
							memset(system_call,0,system_call_length);
							strcat(system_call,"open(");
							strcat(system_call,filename);
							strcat(system_call,",O_WRONLY|O_APPEND)");
							write_to_file(this_txn,system_call,fd_local);
							free(system_call);
						}
						
						opened = 0;
						system_call_length = (strlen("write(")+strlen(filename)+strlen(",20)")+1);
						system_call =malloc(sizeof(char)*system_call_length);
						memset(system_call,0,system_call_length);
						strcat(system_call,"write(");
						strcat(system_call,filename);
						strcat(system_call,",20)");
						write_to_file(this_txn,system_call,fd_local);
						free(system_call);
						free(filename);
						break;
					case 3: //printf("\nlseek");
						fseek(listoffiles,-sizeof(int),SEEK_END);
						fread(&total_files,sizeof(int),1,listoffiles);
						
						while(1)
						{
						  /*select a record in listoffiles randomly and check to see if it is a regular file else select again*/
						 file_rec = rand()%total_files+1;/*random number from 1 to total_files*/
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 //printf("\nrand_rec %d rec %d",file_rec,pathlength);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename = malloc(sizeof(char)*pathlength);
						 fread(filename,pathlength,1,listoffiles);
						 if(stat(filename,&buff) == -1)
							printf("Error reading inode");
						 else
							if((S_ISREG(buff.st_mode)!=0)&&(filed = open(filename,O_RDWR))!=-1)/*it is a regular file and can be
						 opened*/{
								printf("\n %s %d filed =%d",filename,strlen(filename),filed);
								close(filed);
								for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;
									}
							  	}
								if(opened != 1)/*is a file and has not been opened then continue with operation*/
									break;//come out of the while loop
							}
							
						opened = 0;				 
						}				
						
						
						if((open_file_index == -1)||(opened ==0))//no files have been open as yet
						{
							open_file_index = open_file_index+1;
							open_files[open_file_index] = (long)buff.st_ino;
							system_call_length = strlen("open(")+strlen(filename)+strlen(",O_RDONLY)")+1;
							system_call = malloc(sizeof(char)* system_call_length);
							memset(system_call,0,system_call_length);
							strcat(system_call,"open(");
							strcat(system_call,filename);
							strcat(system_call,",O_RDONLY)");
							write_to_file(this_txn,system_call,fd_local);
							free(system_call);
						}
						
						opened = 0;
						system_call_length = strlen("lseek(")+strlen(filename)+strlen(",20)")+1;
						system_call =malloc(sizeof(char)*system_call_length);
						memset(system_call,0,system_call_length);
						strcat(system_call,"lseek(");
						strcat(system_call,filename);
						strcat(system_call,",20)");
						write_to_file(this_txn,system_call,fd_local);
						free(system_call);
						free(filename);
						break;
					case 4:printf("\n rename");
						fseek(listoffiles,-sizeof(int),SEEK_END);
						fread(&total_files,sizeof(int),1,listoffiles);
						

						/*selecting old for system call rename*/
						while(1)/*select unopened files*/
						{
						 file_rec = rand()%total_files+1;/*random number from 1 to total_files*/
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 //printf("\nrand_rec %d rec %d",file_rec,pathlength);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename1 = malloc(sizeof(char)*pathlength);
						 fread(filename1,pathlength,1,listoffiles);

						 /*check if file has already been opened*/
						  if(stat(filename1,&buff) == -1)
							printf("Error reading inode");
						 else{
						 	for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;//come out of the for loop
									}
							  	}
							if(opened != 1)/*has not been opened then continue with operation*/
									break;//come out of the while loop
							}
							
						opened = 0;
						}
						
						/*insert inode of filename1 in open_files*/
						open_file_index = open_file_index+1;
						open_files[open_file_index] = (long)buff.st_ino;

						/*selecting new for system call rename*/
						while(1)
						{
						 file_rec = rand()%total_files+1;/*random number from 1 to total_files*/
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename = malloc(sizeof(char)*pathlength);
						 fread(filename,pathlength,1,listoffiles);
						 filename = dirname(filename);
							
						  /*check if file has already been opened*/
						  if(stat(filename,&buff) == -1)
							printf("Error reading inode");
						 else{
						 	for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;//come out of the for loop
									}
							  	}
							if(opened != 1)/*has not been opened then continue with operation*/
									break;//come out of the while loop
							}
							
						opened = 0;
						}

						/*insert inode of filename1 in open_files*/
						open_file_index = open_file_index+1;
						open_files[open_file_index] = (long)buff.st_ino;

  						 strcat(filename,"/new");
						
						printf("\n %s %s",filename1,filename);
						fflush(stdout);


						system_call_length = strlen("rename(")+strlen(filename1)+strlen(",")+strlen(filename)+strlen(")")+1;
						system_call =malloc(sizeof(char)*system_call_length);
						memset(system_call,0,system_call_length);
						strcat(system_call,"rename(");
						strcat(system_call,filename1);
						strcat(system_call,",");
						strcat(system_call,filename);
						strcat(system_call,")");
						write_to_file(this_txn,system_call,fd_local);
						free(filename);
						free(filename1);
						free(system_call);
						break;
					case 5:printf("\n creat");
						/*finding out the total files in the file system being backep up*/
						fseek(listoffiles,-sizeof(int),SEEK_END);
						fread(&total_files,sizeof(int),1,listoffiles);
						

						/*selecting pathname for system call creat*/
						while(1)
						{
						 file_rec = rand()%total_files+1;/*random number from 1 to total_files*/
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename = malloc(sizeof(char)*pathlength);
						 fread(filename,pathlength,1,listoffiles);
						 filename = dirname(filename);
			 /*the above line will do even if a directory or file is chosen. We will create a file new under the parent of the chosen file. Which will be done by the following line*/
						  /*check if file has already been opened*/
						  if(stat(filename,&buff) == -1)
							printf("Error reading inode");
						 else{
						 	for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;//come out of the for loop
									}
							  	}
							if(opened != 1)/*has not been opened then continue with operation*/
									break;//come out of the while loop
							}
							
						opened = 0;
						}

						/*insert inode of filename1 in open_files*/
						open_file_index = open_file_index+1;
						open_files[open_file_index] = (long)buff.st_ino;
						
  						 strcat(filename,"/new");
						
						printf("\n %s",filename);
						fflush(stdout);


						system_call_length = strlen("creat(")+strlen(filename)+strlen(",")+strlen("S_IRUSR")+strlen(")")+1;
						system_call =malloc(sizeof(char)*system_call_length);
						memset(system_call,0,system_call_length);
						strcat(system_call,"creat(");
						strcat(system_call,filename);
						strcat(system_call,",");
						strcat(system_call,"S_IRUSR");
						strcat(system_call,")");
						write_to_file(this_txn,system_call,fd_local);
						free(filename);
						free(system_call);
						break;			
					case 6: printf("unlink");
						fseek(listoffiles,-sizeof(int),SEEK_END);
						fread(&total_files,sizeof(int),1,listoffiles);
						
						while(1)
						{
						  /*select a record in listoffiles randomly and check to see if it is a regular file else select again*/
						 file_rec = rand()%total_files+1;/*random number from 1 to total_files*/
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 //printf("\nrand_rec %d rec %d",file_rec,pathlength);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename = malloc(sizeof(char)*pathlength);
						 filename1 = malloc(sizeof(char)*pathlength);
						 fread(filename,pathlength,1,listoffiles);
						 strcpy(filename1,filename);
						 filename1 = dirname(filename1);

						/*continue only if both parent and child are not yet opened*/
						 if(stat(filename,&buff) == -1)
							printf("Error reading inode");
						 else
						 	if((S_ISREG(buff.st_mode)!=0)&&(filed = open(filename,O_RDONLY))!=-1)/*it is a regular file and can be
						 unlinked*/{
								printf("\n %s %d filed =%d",filename,strlen(filename),filed);
								close(filed);
								/*check if file has already been opened*/
						  
						 		for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;
									}
							  	}
								if(opened != 1)/*file to be deleted has not been opened then continue with operation*/
								{
									/*check if filename1 has been opened*/
									  if(stat(filename1,&buff1) == -1)
										printf("Error reading inode");
						 			  else{
						 				for(k=0;k<=open_file_index;k++)
										{
											if((long)buff1.st_ino == open_files[k])
											{
												opened = 1;//to indicate the file has already been opened
												break;//come out of the for loop
											}
							  			}
										}
								}
								if(opened != 1)/*has not been opened then continue with operation*/
									break;//come out of the while loop
							     }
														
						opened = 0;
						}	

						/*insert inode of filename1 in open_files*/
						open_file_index = open_file_index+1;
						open_files[open_file_index] = (long)buff.st_ino;
						/*insert inode of filename1 in open_files*/
						open_file_index = open_file_index+1;
						open_files[open_file_index] = (long)buff1.st_ino;
							
						system_call_length = (strlen("unlink(")+strlen(filename)+strlen(")")+1);
						system_call =malloc(sizeof(char)*system_call_length);
						memset(system_call,0,system_call_length);
						strcat(system_call,"unlink(");
						strcat(system_call,filename);
						strcat(system_call,")");
						write_to_file(this_txn,system_call,fd_local);
						free(system_call);	
						free(filename);
						break;
					case 7: printf("stat");
						fseek(listoffiles,-sizeof(int),SEEK_END);
						fread(&total_files,sizeof(int),1,listoffiles);
						
						while(1)
						{
						  file_rec = rand()%total_files+1;
						 fseek(listoffiles,0,SEEK_SET);
						 for(k=1;k<file_rec;k++)
						 {
							fseek(listoffiles,sizeof(int),SEEK_CUR);
							fread(&pathlength,sizeof(int),1,listoffiles);
							fseek(listoffiles,pathlength,SEEK_CUR);
						 }				
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 //printf("\nrand_rec %d rec %d",file_rec,pathlength);
						 fread(&pathlength,sizeof(int),1,listoffiles);
						 filename = malloc(sizeof(char)*pathlength);
						 fread(filename,pathlength,1,listoffiles);
		
						 /*check if file has already been opened*/
						 if(stat(filename,&buff) == -1)
							printf("Error reading inode");
						 else{
						 	for(k=0;k<=open_file_index;k++)
								{
									if((long)buff.st_ino == open_files[k])
									{
										opened = 1;//to indicate the file has already been opened
										break;
									}
							  	}
							if(opened != 1)/*is a file and has not been opened then continue with operation*/
									break;//come out of the while loop
							}
							
						opened = 0;
						}
	
						/*insert inode of filename in open_files*/
						open_file_index = open_file_index+1;
						open_files[open_file_index] = (long)buff.st_ino;
	
						system_call_length = (strlen("stat(")+strlen(filename)+strlen(",")+strlen("&buff)")+1);
						system_call =malloc(sizeof(char)*system_call_length);
						memset(system_call,0,system_call_length);
						strcat(system_call,"stat(");
						strcat(system_call,filename);
						strcat(system_call,",");
						strcat(system_call,"&buff)");
						write_to_file(this_txn,system_call,fd_local);
						free(system_call);	
					free(filename);													

				}
		}  	

	write_to_file(this_txn,"txn_commit()",fd_local);
	 
   }
} 
  
int main()
{

  int thread_index,txn_file_prefix_len;
  FILE *fd;
  char *txn_file,*txn_file_prefix,sufix[12];
  void *res;

  txnid =0;

  srand(time(0));

  printf("\n Enter the total number of transactions");
  scanf(" %d",&max_no_txn);

  printf("\n Enter required degree of concurrency..max at the moment is 20");
  scanf(" %d",&deg_of_con);
  
  printf("\n Enter the maximum number of operations per transaction");
  scanf(" %d",&max_no_op);
  

  /*open file to store traces*/
  listoffiles = fopen("/home/lipika/TxFS/Traces/files.txt","r+b");

  txn_file_prefix_len = strlen("/home/lipika/TxFS/Traces/trace_new_rand5/file");
  txn_file_prefix = malloc(txn_file_prefix_len+1);
  strcpy(txn_file_prefix,"/home/lipika/TxFS/Traces/trace_new_rand5/file");
  
  /*create a pool of threads equla to the specified degree of concurrency*/ 
  for(thread_index = 0;thread_index<deg_of_con;thread_index++)
  {	
	snprintf(sufix,sizeof(sufix),"%d",thread_index);  
	txn_file = malloc(txn_file_prefix_len+strlen(sufix)+1);
        strcat(txn_file,txn_file_prefix);
	strcat(txn_file,sufix);
	printf("\n%s",txn_file);

	fd = fopen(txn_file,"a+b");
	if(fd == NULL)
		printf("\nerror on opening %s",txn_file);

	sim_txn(fd);
	
	free(txn_file);
  }

  free(txn_file_prefix);
}
