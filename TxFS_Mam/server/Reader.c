/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include "my_header.h"

//#include "server_config.h"
#define PORTNO 3001
//#include "yportenv.h"   
#define MAX_BUFFER_SIZE 10250
#define SIZE_STR 12
//
extern int yaffs_mount(const char *path);
extern int yaffs_unmount(const char *path);
extern int yaffs_start_up(void);
extern int on_txn_beg(int);
extern int on_open(char *incall[],int txnid);
extern int on_txn_commit(int);
extern int on_txn_abort(int);
extern int on_rename(char *incall[],int txnid);
extern int on_write(char *incall[],int txnid);
extern int on_creat(char *incall[], int txnid);
extern int on_unlink(char *incall[], int txnid);
extern int on_stat(char *incall[], int txnid);
extern int on_lseek(char *incall[], int txnid);
extern ssize_t on_read(int fd,char *read_buf,ssize_t nbytes,int txnid);
extern int on_bck_beg(char *incall[]);
extern int tr_ext_creat_yaffs_creat (const char *, int);
extern int tr_sync_file_to_disk (const char *);
extern int on_inode_stat (char *incall[], int txnid);
extern void set_transaction_id (unsigned int);
extern bool yaffs_sync_transaction_bitmap (const char *);
extern void log_const (const char*);
extern int result_log_const (const char *);
extern int result_log (char *);
extern int write_possible(int fd);
extern void log_output (char *str);
extern int getTransactionId (void);
/*structure to store thread client/transaction association*/
struct thread_node{
                        int txn;
                        pthread_t tid;
};

/*Function called when any system call fails*/
void error(const char *msg)
{
    perror(msg);
    //exit(1);I don't want the thread to exit on error
}

void root_dir_init(void)
{
 int fd = tr_ext_creat_yaffs_creat ("/yaffs2", S_IRUSR);
// printf ("creation of yaffs2 directory returns object id %d\n", fd);
 int status = tr_sync_file_to_disk ("/yaffs2");
// printf ("syncing yaffs2 directory to disk returns status = object id %d\n", status);
 
}

/*Function to initialize global data*/
void global_init(void)
{
     event_log = fopen("/btp/yaffs2-dir/TxFS_Mam/server/log.txt","r+b");
     if(event_log == NULL)
 	error("Unable to open log.dat");
// Here we call the functions to initialise the YAFFS file-system
   yaffs_start_up();
   int ret = yaffs_mount ("/yaffs2");
//   printf("mount Returns %d\n", ret);
//   int ret_unmount = yaffs_unmount ("/yaffs2");
//   printf("Unmount Returns %d\n", ret_unmount);
//   ret = yaffs_mount ("/yaffs2");
//   printf("yaffs_mount returns %d\n", ret);
 //  yaffs_sync_transaction_bitmap("/yaffs2");
     
     head_lock_list = NULL;
     head_txn_list = NULL;
     head_wait_list = NULL;

     txn_id = 0;/*txn_id 0 is always reserved for the backup transaction*/
     BACKUP = INACTIVE;

     pthread_mutex_init(&file_mutex, NULL);
     pthread_mutex_init(&txn_mutex,NULL);
     pthread_mutex_init(&log_mutex,NULL);
     pthread_mutex_init(&txnid_mutex,NULL);
     pthread_mutex_init(&pause_mutex,NULL);

     u_conflict = 0;
     tb_conflict = 0;
     tbu_conflict = 0;
     no_of_abrt = 0;
     no_on_wait = 0;
     avg_wait_time = 0;
     total_wait_time = 0;
     abrt_after_pause = 0;
     abrt_on_dead = 0;

     index_pause_array = 0;

     srand(time(0));
     //root_dir_init();
    
}

void handler(void *newsockfd)
{
  result_log_const("5:New Abort Rate:");
 int tcount = 1;
 struct sockaddr_in cliAddr;
 char buffer[MAX_BUFFER_SIZE];/*server reads charecter from the socket connection into this buffer*/
 int client_sockfd; /*keep a local copy of the client's socket descriptor*/
 int addr_len; /*used to store length of sockaddr_in*/
 //Pointer to strings where each string receives system call and arguments to the systems calls from client
 char *incall[10];
 int i=0,n, no_of_arg=0,txnid=0,commited_txn,fd;
 char str[SIZE_STR];
  char result_string[200];
 client_sockfd = *((int*)newsockfd);/*store client socket descriptor*/
  
 addr_len = sizeof(cliAddr);
 
 /*get clients name and store in cliAddr*/
 getpeername(client_sockfd, (struct sockaddr*)&cliAddr, &addr_len);

 /*now read lines from client*/
 do{
        //printf(" Handling System Call %s\n", incall[0]);
 	if(no_of_arg !=0)
 	{
		if(strcmp(incall[0],"quit") !=0)
		{		
 			/*free space allocated of the messages*/
 			i = 0;
			while(i < (no_of_arg)){
 	 	      		free(incall[i]);        
       				i++;
       			}
	 	}
	}
 bzero(buffer,MAX_BUFFER_SIZE);
 n = read(client_sockfd,buffer,MAX_BUFFER_SIZE-1);
 if (n < 0) 
	printf("1. ERROR reading from socket");
 incall[0] = malloc(1+strlen(buffer)); 
 strcpy(incall[0],buffer);
 char *log_string = (char *)(malloc (200));
 //memset (log_string, 0, 200);
 //sprintf(log_string, " Handling System Call %s", incall[0]);
 //log_output (log_string);
// Initializing the number of arguments in the function call depending on the system call
 if(strcmp(incall[0],"txn_beg")==0)
{	
	 no_of_arg = 1;
	 tcount++;	
	// int transaction_id = atoi(incall[1]);
	 //set_transaction_id(tcount);

}
 if(strcmp(incall[0],"open")==0)
	 no_of_arg = 3;
 if(strcmp(incall[0],"txn_commit")==0)
 {
	      memset(result_string, 0, 200);
       sprintf(result_string, "10:2:%d:txn_commit:Id=%d:",txnid, txnid);	
       result_log(result_string);
	no_of_arg = 1;
 }
 if(strcmp(incall[0],"txn_abort")==0)
 {
	
	memset(result_string, 0, 200);
       sprintf(result_string, "10:3:%d:txn_abort:Id=%d:",txnid, txnid);	
       result_log(result_string);
   	no_of_arg = 1;
 }
 if(strcmp(incall[0],"quit")==0)
         no_of_arg = 1;
 if(strcmp(incall[0],"write")==0)
         no_of_arg = 4;
 if(strcmp(incall[0],"lseek")==0)
	no_of_arg = 4;
 if(strcmp(incall[0],"read")==0)	
        	no_of_arg = 4;	
 if(strcmp(incall[0],"rename")==0)
	no_of_arg = 3;
 if(strcmp(incall[0],"creat")==0)
	no_of_arg = 3;
 if(strcmp(incall[0],"unlink")==0)
	no_of_arg = 2;
 if(strcmp(incall[0],"stat")==0)
	no_of_arg = 2;
 if(strcmp(incall[0],"bck_beg")==0)
  	no_of_arg = 3;
  if(strcmp(incall[0],"inode_stat")==0)
	no_of_arg = 2;
 
// Reply to the client  
 n = write(client_sockfd,"Received",8);/*last argument is the size of message*/

 if (n < 0)
        error("1. ERROR writing to socket");

 /*receiving rest of the messages in the incall buffer*/
 i = 1;
 while(i < (no_of_arg))
      {
        bzero(buffer,MAX_BUFFER_SIZE);	
     	n = read(client_sockfd,buffer,MAX_BUFFER_SIZE-1);

     	if (n < 0) 
		error("2. ERROR reading from socket");
     	incall[i] = (char *) malloc(1+strlen(buffer));
	memset(incall[i], 0, 1+strlen(buffer));
        strcpy(incall[i],buffer);
       
	n = write(client_sockfd,"Received",8);/*last argument is the size of message*/
        if (n < 0)
        	error("2. ERROR writing to socket");
        
       i++;
       }
     
 /*Call functions according to the system call to be serviced*/
 if(strcmp(incall[0],"txn_beg")==0)
 {
       strcpy (str, incall[1]);
       txnid = atoi (incall[1]);  
//     txnid = getTransactionId(); Request to the file-system to provide a specific transaction id     
       memset(result_string, 0, 200);
       sprintf(result_string, "10:1:%d:txn_begin:Id=%d:",txnid, txnid);	
       result_log(result_string);
   //txnid = atoi(incall[1]);
   /*message to client*/
   //snprintf(str,sizeof(str),"%d",txnid);

   set_transaction_id(txnid);
   on_txn_beg(txnid);
  // printf("Txn Beg  transaction id = %d\n", txnid);

   n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
   if (n < 0)
       	error("ERROR writing to socket txn_beg:");
 }

 if(strcmp(incall[0],"open")==0)
 {
   fd = on_open(incall,txnid);  
  // printf("on_open, fd =%d returned\n", fd);
   /*message to client*/
   snprintf(str,sizeof(str),"%d",fd);
   n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
   if (n < 0) 
	error("ERROR writing to socket open:");
 /* if (write_possible(fd))
  {
	printf("File With Handle %d\t can be written\n",fd );
  }*/
  	
 }

 if (strcmp(incall[0],"txn_commit")==0)
 {
       memset(result_string, 0, 200);
       sprintf(result_string, "10:2:%d:txn_commit:Id=%d:",txnid, txnid);	
       result_log(result_string);
  commited_txn = on_txn_commit(txnid);
// printf("\n Txn commit = %d %d",commited_txn,client_sockfd);
  /* message to client*/
  snprintf(str,sizeof(str),"%d",txnid);
  n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
  if (n < 0)
        error("ERROR writing to socket txn_commit: ");
 }

 if (strcmp(incall[0],"txn_abort")==0)
 {
     result_log_const ("8:3:txn_abort()");
  int aborted_txn = on_txn_abort(txnid);
 //printf("\n Txn abort = %d",aborted_txn);
  /* message to client*/
  snprintf(str,sizeof(str),"%d",txnid);
  n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
  if (n < 0)
        error("ERROR writing to socket txn_abort:");
 }

 if (strcmp(incall[0],"quit")==0)
 {
  /* message to client*/
 // printf("\n Quit %d",txnid);
  fflush(stdout);
  snprintf(str,sizeof(str),"%d",txnid);
  n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
  if (n < 0)
        error("ERROR writing to socket, quit:");
 }

 if (strcmp(incall[0],"write")==0)
 {
   
    ssize_t bytes_written = on_write(incall,txnid); 
    if(bytes_written == 0)
    {
	printf("Bug, In Write, on_write() returns 0, Original System Call %s %s  %s\n", incall[0], incall[1],  incall[3]);
    }
  //memset(str, 0, SIZE_STR);
  //sprintf(str,"%d",bytes_written);
  //log_output (str);
  n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
  if (n < 0)
        error("ERROR writing to socket, write:");
 }

if(strcmp(incall[0],"rename")==0)
{
  int status = on_rename(incall,txnid);
  //int status =1;
  
  /*message to client*/
  snprintf(str,sizeof(str),"%d",status);
  n = write(client_sockfd,str,sizeof(str));
  if(n<0)
      error("Error writing to socket, rename:");
}

if(strcmp(incall[0],"creat")==0)
{
 int status = on_creat(incall,txnid);
 //printf("handler on_creat status = %d\n",status);
  /*message to client*/
  snprintf(str,sizeof(str),"%d",status);
  n = write(client_sockfd,str,sizeof(str));
  if(n<0)
      error("Error writing to socket, creat:");
  else
     ;// printf("creat done successfully on file %s with fd = %d", incall[1], status);
}

if(strcmp(incall[0],"unlink")==0)
{
 int status = on_unlink(incall,txnid);
 //int status = 1;
 //printf("%s,%s",incall[0],incall[1]);

  /*message to client*/
  snprintf(str,sizeof(str),"%d",status);
  n = write(client_sockfd,str,sizeof(str));
  if(n<0)
      error("Error writing to socket, creat:");
}

if(strcmp(incall[0],"stat")==0)
{
 int status = on_stat(incall,txnid);
 //int status = 1;
 //printf("%s,%s",incall[0],incall[1]);

  /*message to client*/
  snprintf(str,sizeof(str),"%d",status);
  n = write(client_sockfd,str,sizeof(str));
  if(n<0)
      error("Error writing to socket, creat:");
}

if(strcmp(incall[0],"inode_stat")==0)
{
  //printf("In Reader.c inode_stat called \n");
  int status = on_inode_stat(incall,txnid);
//  printf("In Reader.c inode_stat returns inode number = %d \n", status);
  /*message to client - status contains the inode number*/
  snprintf(str,sizeof(str),"%d",status);
  n = write(client_sockfd, str, sizeof(str));
  if(n<0)
      error("Error writing to socket, creat:");
}

if (strcmp(incall[0],"lseek")==0)
 {
 // printf("IN LSEEK\n");
   off_t offset = on_lseek(incall,txnid);
    /* message to client*/
  snprintf(str,sizeof(str),"%d",offset);
  n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
  if (n < 0)
        error("ERROR writing to socket, lseek:");
 }

 if (strcmp(incall[0],"read")==0)
 {
  //printf("In read\n");
  char *read_buf;
  int fd;
  long nbytes;
  fd = atoi(incall[1]);
  nbytes = atoi(incall[3]);
  read_buf = malloc((nbytes+1)*sizeof(char));
  memset(read_buf,0,nbytes*sizeof(char));
  //printf("Calling on_read\n");
  ssize_t bytes_read = on_read(fd,read_buf,nbytes,txnid);
  //printf("In Readder.c :: Data Read = %s\n", read_buf);
      /* message to client*/
 if(bytes_read == -1)/*read not allowed*/
  {
    /*Data Read*/
     memset(read_buf,0,nbytes*sizeof(char));//files may contain different types of data and read() at times does not work hence...
    n = write(client_sockfd,read_buf,strlen(read_buf));/*last argument is the size of message*/
    if (n < 0)
        error("ERROR writing to socket, read:");
 }
 else{
    /*Data Read*/
    //memset(read_buf,0,nbytes*sizeof(char));//files may contain different types of data and read() at times does not work hence...
   // printf("Data Read = %s\n", read_buf);
    n = write(client_sockfd,read_buf,bytes_read);/*last argument is the size of message*/
    if (n < 0)
        error("ERROR writing to socket, read2:");
  }


  /*ack received from client simply to seperate the two messages going from server*/
   bzero(buffer,256);
   n = read(client_sockfd,buffer,255);
   if (n < 0)
	   error("ERROR reading from socket, read:");
  
  /*Bytes read...return from syscall*/
  snprintf(str,sizeof(str),"%ld",bytes_read);
  
  n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
  if (n < 0)
        error("ERROR writing to socket, read3:");
  free(read_buf);
 }

 if (strcmp(incall[0],"bck_beg")==0)
 {
   //printf("\n Backup root %s",incall[1]);
   fflush(stdout);
   int bck_status = on_bck_beg(incall);
    /* message to client*/
  strcpy(str,"Backup done");
  n = write(client_sockfd,str,sizeof(str));/*last argument is the size of message*/
  if (n < 0)
        error("ERROR writing to socket,bck_beg:");
 }
 

 }while(strcmp(incall[0],"quit")!=0);    

if(strcmp(incall[0],"quit")==0)
{
   //printf ("Closing Socket Connection On The Server's Side\n");
   close(client_sockfd);
}


/*free space of "Quit"*/
//free(incall[0]);
//free(newsockfd);
//log_const("thread close done");
}



int main(int argc, char *argv[])
{
     
     //printf("Num of txn=%d\n", NUM_OF_TXN);
     /*sockfd is a file descriptor and stores the value returned by the socket system call,newsockfd by the accept system call*/
     /*portno stores the port number on which the server accepts connections.given as command line argument*/
     int sockfd, *newsockfd, portno;
     if (argc > 1)     portno = atoi(argv[1]);
     socklen_t clilen;/*stores length of address of client*/
     struct sockaddr_in serv_addr, cli_addr;/*sockaddr is a structure containing an internet address and is defined in netinet/in.h*/
     void *res;
     struct thread_node thread_list[100];
     pthread_t deadlock_tid; 
     int i;
     struct timeval tv;
     time_t curtime1=0,curtime2=0,curtime_half=0;
 
     /*record the time at the beginning*/
     gettimeofday(&tv,NULL);
     curtime1 = tv.tv_sec;
     
     /*Initializing global data*/
      global_init();

/*     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }*/
    
     /*socket() system call creates a new socket.argument1:address domain, we use internet domain, argument2:type of socket,we use stream socket, argument3:is protocol, if 0 as in our case OS will choose the appropriate.TCP for stream and UDP for datagram sockets*/
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     
     /*initialise serv_addr to 0*/
     bzero((char *) &serv_addr, sizeof(serv_addr));

     //portno = atoi(argv[1]);
     //portno = PORTNO;
     //printf("PORTNO %d\n", portno);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);/*htons converts the port no. to network byte order*/

     /*binds an address to a socket*. in this case the current host and port number */
      while (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
	{
         	     printf("ERROR on binding\n");
		     printf("Enter alternate port no\n");
		     scanf("%d", &(portno));
                     serv_addr.sin_port = htons(portno);/*htons converts the port no. to network byte order*/
	}

     /*listens on the socket for connection. 5 is the backlog queue..no. of waiting connections*/
     listen(sockfd,5);
 
// We need not run the deadlock detection thread just now 
// DEADLOCK  
  //pthread_create(&deadlock_tid,NULL,(void*)&deadlock_detection,NULL); 
    //for(i=0;i<NUM_OF_TXN;i++)   
    for (;;)
    {
	newsockfd = malloc(sizeof(int));
     	clilen = sizeof(cli_addr);
 	
     	/*causes the process to block until a client connects to the server.thus it wakes up when a connection when a connection from the server has been successfully established*/
     	*newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr,&clilen);
     	if (newsockfd < 0) 
        	  error("ERROR on accept");
      
    	pthread_create(&thread_list[i].tid,NULL,(void*)&handler,(void*) newsockfd);      
    }

    
    for(i=0;i<NUM_OF_TXN;i++)
    { 
     	pthread_join(thread_list[i].tid, &res);
	if(i==(NUM_OF_TXN -2))
	{
		gettimeofday(&tv,NULL);
     		curtime_half = tv.tv_sec;
	}
		
    }
  printf("\n Number of user conflict %d",u_conflict);
  printf("\n Number of tb conflict %d",tb_conflict);
  printf("\n Number of MS conflict %d",no_of_abrt+no_on_wait);
  printf("\n Number of user aborted due to MS conflict %d",no_of_abrt);
  printf("\n Number of user waiting due to MS conflict %d",no_on_wait);
  if(no_on_wait>0)
  	printf("\n Average waiting time %d",total_wait_time/no_on_wait);
  printf("\n Number of transactions that has to be aborted after pausing %d",abrt_after_pause);
  printf("\n Number of transactions that needs to be aborted to resolve deadloack %d",abrt_on_dead);
  

  /*record the time at the end*/
     gettimeofday(&tv,NULL);
     curtime2 = tv.tv_sec;
  
  printf("\n Time taken for complete run %d",curtime2-curtime1);   
  printf("\n Time taken for only application transactions %d\n",curtime_half-curtime1);
    pthread_join(deadlock_tid,&res);
     print_lists();
    close(sockfd);
    printf("main close done\n");
    return 0; 
}
