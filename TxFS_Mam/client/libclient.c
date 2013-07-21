#include<stdio.h>
#include <pthread.h>
#include <stdlib.h>
//#include "client_header.h"

#define PORTNO 3001
extern void setup_connection(int portno,char *hostname);
extern void disconnect();
extern int call_execute(char *argcalls[]);

int result_log (char *str, FILE *fptr)
{
 fprintf (fptr, "%s\n", str);
 fflush (fptr);
 return 1;
}

int result_log_const (const char *str_const, FILE *fptr)
{
 char *str = (char *)malloc(strlen(str_const)+1);
 sprintf(str, str_const);
 result_log (str, fptr);
 free (str);
 return 1;
}

void log_client (char *str, FILE* fptr)
{
	result_log (str, fptr);	
        return ;
  int ret_val = fprintf (stdout,"%s\n", str);
  fflush (stdout);   
  if (ret_val < 0)
  {
    printf ("function :: log :: error in file write  \n");
    fflush (stdout); 	
    return;
  }
}


int txn_beg(int t_id, int portNo)
{
    int port;
    char portno[12];
    char txn_id[10];
    sprintf(txn_id,"%d", t_id); 
    port = portNo;
    snprintf(portno,sizeof(portno),"%d",port);
    char *host_name = "localhost";
    char *newargv1[] = {"txn_beg", txn_id,"sesh"};
    setup_connection(port, host_name);
    call_execute(newargv1);
    return 0;
}


void txn_commit()
{
    char *newargv1[] = {"txn_commit","sesh"};
    call_execute(newargv1);    
    char *newargv2[] = {"quit","sesh"};
    call_execute(newargv2);
    disconnect();
    fflush(stdout);
}

void txn_abort()
{
    char *newargv1[] = {"txn_abort","sesh"};
    call_execute(newargv1);    

    char *newargv2[] = {"quit","sesh"};
    call_execute(newargv2);
    disconnect();
    fflush(stdout);
}   
     
void bck_beg(char *bck_src_path,char *bck_dest_path)
{
     int port;
     port = 3000;
     char *host_name = "localhost";
     char *newargv1[] = {"bck_beg",bck_src_path,bck_dest_path,"sesh"};
     setup_connection(port, host_name);
     call_execute(newargv1);
     char *newargv2[] = {"quit","sesh"};
     call_execute(newargv2);
     disconnect();
}

void exit_txn()
{
     
    char *newargv2[] = {"quit","sesh"};
    call_execute(newargv2);
    disconnect();
    //printf("\ncalling real exit");
    //real_exit(status);
   }
