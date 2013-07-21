# include<stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>


int numChildren=2;
int num_runs=1;

void main(int argc, char *argv[])
{
 int childCount=0, count, numArgs=5;
 printf("Parent Pid  = %d\n", getpid());
 int portno;
 int child_id0,child_id1,child_id2,child_id3,child_id4,child_id5,child_id6,status, childPid[numChildren];
 char port[10];
 memset(port, 0, 10);
 char *run;
 if (argc>1) {portno = atoi(argv[1]);sprintf(port, "%d", portno);} 
 int c_run=0;
 char *newargv[numChildren][numArgs];
 char *newenviron[2] = {"LD_PRELOAD=/btp/yaffs2-dir/TxFS_Mam/client/clients.so", NULL};

 char *filePath;// = (char *)malloc(200 * sizeof(char));
 for (; c_run<num_runs; c_run++) 
 {
  run = (char *)malloc(5 * sizeof(char));
  filePath = (char *)malloc(200 * sizeof(char));
  memset(filePath, 0, 200);


 //sprintf (filePath, "/btp/yaffs2-dir/TxFS_Mam/Traces/trace_file_rand1/ravi/file0.txt");
 //char *newargv1[] = {"./transaction","/btp/yaffs2-dir/TxFS_Mam/Traces/trace_file_rand1/ravi/workflows/crashful_recovery/test1.txt", port, NULL};
  //newargv[childCount][5] = {"./transaction",filePath, port, run, NULL}; 
//  newargv = malloc (numChildren*sizeof(char *));
  while (childCount < numChildren)
  {
		   memset(run, 0, 5);
 		   sprintf (run, "%d", childCount);
					
		count=0;
		sprintf (filePath, "/btp/yaffs2-dir/TxFS_Mam/Traces/trace_file_rand1/ravi/file%d.txt", childCount);
			//newargv[childCount][0] = {"./transaction",filePath, port, run, NULL}; 
			while(count < numArgs)
			{			
				newargv[childCount][count] = malloc(50);
				count++;
			}
			sprintf(newargv[childCount][0], "./transaction");
			sprintf(newargv[childCount][1], "%s", filePath);
                        sprintf(newargv[childCount][2], "%s", port);
			sprintf(newargv[childCount][3], "%s", run);
			newargv[childCount][4] = NULL;
				
		printf("%s\n%s\n%s\n%s\n", newargv[childCount][0], newargv[childCount][1], newargv[childCount][2],newargv[childCount][3]);
 	        
		childCount++;
 		
  } 
 //char *newargv1[] = {"./transaction","/btp/yaffs2-dir/TxFS_Mam/Traces/trace_file_rand1/ravi/file0.txt",port,NULL};
//  char *newargv1[] = {"./transaction","/btp/yaffs2-dir/TxFS_Mam/Traces/trace_file_rand1/ravi/abort_read.txt",NULL};

 /*char *newargv2[] = {"./transaction","/home/lipika/TxFS/Traces/trace_subtree_nosharing/file1",NULL};
 char *newargv3[] = {"./transaction","/home/lipika/TxFS/Traces/trace_subtree_nosharing/file2",NULL};
 char *newargv4[] = {"./transaction","/home/lipika/TxFS/Traces/trace_subtree_nosharing/file3",NULL};
 char *newargv5[] = {"./transaction","/home/lipika/TxFS/Traces/trace_subtree_nosharing/file4",NULL};
 char *newargv6[] = {"./transaction","/home/lipika/TxFS/Traces/trace_file_subtree40/file5",NULL};
 char *newargv0[] = {"./backup",NULL};*/
 
 childCount=0;
 //newenviron = {"LD_PRELOAD=/btp/yaffs2-dir/TxFS_Mam/client/clients.so", NULL};
 while (childCount < numChildren)
 {
         childPid[childCount] = fork();
	 if (childPid[childCount]  == 0)
	 {	/*In Child, starting up the process*/
		execve(newargv[childCount][0],newargv[childCount],newenviron);
		return;
	 }	
	 else 
	 {
		childCount++;
 		continue;
	}
 }
 childCount = 0;
 while (childCount < numChildren)
 {
    waitpid(childPid[childCount], &status, 0);
    childCount++;
 }
 free (filePath);
 }
//if ((child_id0=fork())==0)
  // execve(newargv0[0],newargv0,newenviron); 
 
  
 //if ((child_id4 = fork())==0)
   // execve(newargv4[0],newargv4,newenviron);


 ///if ((child_id5 = fork())==0)
    //execve(newargv5[0],newargv5,newenviron);

//if ((child_id3=fork())==0)
  //execve(newargv3[0],newargv3,newenviron);

 //if((child_id2 =fork())==0)
   //  execve(newargv2[0],newargv2,newenviron);

 //if((child_id6 =fork())==0)
   //  execve(newargv6[0],newargv6,newenviron);

 
  //(void)wait(&status);
  //(void)wait(&status);
  //(void)wait(&status);
   //printf("\n 1simulator %d",status);
  //(void)wait(&status);
   //printf("\n 2simulator %d",status);
  //(void)wait(&status);
  //(void)wait(&status);
/*status in wait() is a poiter to an integer location where wait function will return the status of the child process. If the wait function call returns becasue a process has exited, the return value is equal to the pid of the exiting process*/
 //close_log_c();
}

