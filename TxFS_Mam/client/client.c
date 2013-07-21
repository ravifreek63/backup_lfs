#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <dlfcn.h>
#include "client_config.h"
// Version 1 Client.c
int sockfd;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}
/*
Code to setup the socket connection.
*/
void setup_connection(int portno, char *hostname)
{
 //printf("Connecting on port no %d\n", portno);
 struct sockaddr_in serv_addr;/*serv_addr will contain the address of the server to which we want to connect*/
/*structure hostent defines a host computer on the internet.server is a pointer to such a structure*/
/* Description of data base entry for a single host.  */
 struct hostent *server;

/*
	 Opening a socket to connect the server to the client.
         sockfd is the file descriptor pointing to the socket.
*/
 sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) 
	{
        	error("ERROR opening socket\n");
		return;
	}


    server = gethostbyname(hostname);

    if (server == NULL)
      {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
      }
  /*setting of field in serv_addr*/
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,(char *)&serv_addr.sin_addr.s_addr,server->h_length);
    serv_addr.sin_port = htons(portno);

    /*connect function is called by the client to establish a connection with the server*/
   if (connect(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
	{
        	printf("ERROR connecting on port number %d\n", portno);
		// printf("Enter alternate port no\n");
		//     scanf("%d", &(portno));
                //     serv_addr.sin_port = htons(portno);/*htons converts the port no. to network byte order*/

	}
    //printf("Connection established to host successfully\n");
}
/*
Code to end the socket connection
*/
void disconnect()
{
 //printf("\nlipika %d",getpid());
 //fflush(stdout);
 close(sockfd);
 //printf("Close Done On The Client Side\n");
 fflush(stdout);
}


int call_execute(char *argcalls[])
{
    //printf("in call_execute\n");
    int n,i,no_of_arg,syscall_ret;
    
    char buffer[MAX_BUFFER_SIZE];

    //printf("\nclient %s",argcalls[0]);
    fflush(stdout);
    //static ssize_t (*real_write1)(int,const void *,size_t) = NULL;
    //real_write1 = (size_t(*)(int,const void *,size_t))dlsym(RTLD_NEXT,"write");
    /*Initialize number of messages to server depending upon the system call and its corresponidng number of arguments*/
    if(strcmp(argcalls[0],"txn_beg")==0)/*Its a txn_beg call*/
	no_of_arg = 2;
    
    if(strcmp(argcalls[0],"open")==0)/*Its a open call*/
       no_of_arg = 3;

    if(strcmp(argcalls[0],"txn_commit")==0)/*Its a commit call*/
       no_of_arg = 1;

    if(strcmp(argcalls[0],"txn_abort")==0)/*transaction is aborting*/
       no_of_arg = 1;
 
    if(strcmp(argcalls[0],"quit")==0)/*Its a end/quit call that is used for disconnecting the */
	no_of_arg = 1;
    
    if(strcmp(argcalls[0],"write") == 0)
    {
	no_of_arg = 4;
       // printf("client %s, %s, %s, %s\n",argcalls[0],argcalls[1],argcalls[2],argcalls[3]);
     }

    if(strcmp(argcalls[0],"lseek") == 0)
        no_of_arg = 4;
     
    if(strcmp(argcalls[0],"read")==0)
    {
	no_of_arg = 4;
   	//printf("\nclient %s, %s, %s, %s",argcalls[0],argcalls[1],argcalls[2],argcalls[3]);
    }
    
    if(strcmp(argcalls[0],"rename")==0)
        no_of_arg = 3; 

    if(strcmp(argcalls[0],"creat")==0)
	no_of_arg = 3;

    if(strcmp(argcalls[0],"unlink")==0)
	no_of_arg = 2;

    if(strcmp(argcalls[0],"stat")==0)
	no_of_arg = 3;

    if(strcmp(argcalls[0],"bck_beg")==0)
        no_of_arg = 3;
   
    if(strcmp(argcalls[0],"inode_stat")==0)
        no_of_arg = 2;
   
    /*send message to server*/
    i =0;
  
    do{                 
	    //printf("writing to the socket\n");
	     n = send(sockfd,argcalls[i],strlen(argcalls[i]),0);
	     //printf("writing to the socket  = %s\n", argcalls[i]);
            if (n < 0) 
		{
	         error("ERROR writing to socket");
		/*
		   Transaction Abort Should Take Place Here.
		*/
		}

	     /*receive message from server.This message is just to seperate the messages received from client*/
	    bzero(buffer, MAX_BUFFER_SIZE);
	   //printf("waiting for read from the socket\n");
   	    n = recv(sockfd,buffer,MAX_BUFFER_SIZE-1,0);
	   // printf("read from socket %s\n", buffer);
	/*
	   The buffer is not being read from, then why are we actually receiving data on the buffer itself ??
	*/
	     if (n < 0)
		{
	         printf("ERROR reading from socket");
		/*
		   Transaction Abort Should Take Place Here.
		*/
		}
            //printf("\n %s %s",argcalls[i], buffer);
            fflush(stdout);

            i++;
    }while(i<no_of_arg);
 
   if(strcmp(argcalls[0],"read")==0)/*receives the data read from file as well as bytes read so receives two messages*/
   {
     bzero(buffer,MAX_BUFFER_SIZE);
     n = recv(sockfd,buffer,MAX_BUFFER_SIZE-1,0);
     if (n < 0) 
         error("ERROR reading from socket");
     //printf("\nbuffer %s\n",buffer);
     //fflush(stdout);
     //memset(argcalls[2],0,20);

     strncpy(argcalls[2],buffer,n);
     //printf("n=%d\t argcalls[2]=%s",n, argcalls[2]);
     n = send(sockfd,"ack",strlen("ack"),0);/*sending this to server simply to seperate the two messages received from server*/
     if (n < 0)
                 error("ERROR writing to socket");
   }

    /*receive message from server. It is usually the return value from system calls*/
    bzero(buffer,256);
    n = recv(sockfd,buffer,255,0);
    //printf("buffer contents after ack %s\n", buffer);
    if (n < 0) 
         printf("ERROR reading from socket");

    /*return the contents of buffer to caller*/
    if(strcmp(argcalls[0],"bck_beg")==0)
     {
	printf("\n %s",buffer);
	return;
   }
    else
  {
   // printf("\nbuffer %s %s",argcalls[0],buffer);
    syscall_ret = atoi(buffer);
   //printf("returning from call_execute=%d\n", syscall_ret);
   return syscall_ret;
  }
}
