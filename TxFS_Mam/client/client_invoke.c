/*Syscall wrapper library to override original syscalls*/
# define _GNU_SOURCE
# include <dlfcn.h>
# include <sys/types.h>
# include <stdio.h>
#include <pthread.h>
#include <sys/stat.h>
#define READ_SIGNATURE int fd, void *buf, size_t nbyte;
#define WRITE_SIGNATURE int fd, void *buf, size_t nbyte;


extern int call_execute(char *argcalls[]);
extern void disconnect();

static int (*real_open)(const char *, int);
static int (*real_open64)(const char *, int);
static int(*real_close)(int);
static ssize_t (*real_read)(int fd, void *buf, size_t nbyte);
static ssize_t (*real_write)(int fd, void *buf, size_t nbyte);
static void (*real_exit)(int status);
static int (*real_rename)(const char *old, const char *new);
static int (*real_creat)(const char *pathname,mode_t mode);
static int (*real_unlink)(const char *pathname);
static int (*real_stat)(const char *pathname, struct stat *buf);

void _init(void);

void _init(void)
{
        real_open = dlsym(RTLD_NEXT,"open");
	/*
	 RLTD_NEXT will find the next occurrence of a function in the search order after the current library. 
	 This allows one to provide a wrapper around a function in another shared library.
	*/
        real_open64 = dlsym(RTLD_NEXT, "open64");
        real_close = dlsym(RTLD_NEXT, "close");
	real_read = dlsym(RTLD_NEXT, "read");
	real_write = dlsym(RTLD_NEXT, "write");
	real_exit = dlsym(RTLD_NEXT,"exit");
	real_rename = dlsym(RTLD_NEXT,"rename");
	real_creat = dlsym(RTLD_NEXT,"creat");
	real_unlink = dlsym(RTLD_NEXT,"unlink");
	real_stat = dlsym(RTLD_NEXT,"lstat");
}

int open(const char *pathname, int flags)
{      
	//printf ("I am here in open\n");
	int fd; 
        char str1[12];        
        snprintf(str1,sizeof(str1),"%d",flags);
	fflush(stdout);
	char *newargv1[] = {"open",pathname,str1,"sesh"};
	fd = call_execute(newargv1);        
        return fd;                               
}

int open64(const char *pathname, int flags)
{
        int fd;
        printf("Open64\n");
        printf("%s", pathname);
        fd = real_open64(pathname, flags);
        printf("am back");
        return fd;
        /*check if for transactional file system)*/
        /*check if can be locked*/
        /*call original syscall*/
        /*If successful assign permanent lock*/
}


/*int close(int fd)
{
  printf("CLOSE\n");
  return 0;
}*/

/*ssize_t read(READ_SIGNATURE)
{
 printf("READ");
}*/

ssize_t write(int fd, void *buf, size_t nbyte)
{
	char str_fd[12],str_nbyte[12];
	int ret_value;
        
        snprintf(str_fd,sizeof(str_fd),"%d",fd);
	//printf("in Write %s\n",str_fd);
 	snprintf(str_nbyte,sizeof(str_nbyte),"%d",nbyte);
        char *newargv1[] = {"write",str_fd,buf,str_nbyte,"sesh"};
        ret_value = call_execute(newargv1);
        
        return ret_value;

}

off_t lseek(int fd, off_t offset, int whence)
{
	char str_fd[12], str_offset[20], str_whence[20];
	off_t ret_value;

        snprintf(str_fd,sizeof(str_fd),"%d",fd);
        snprintf(str_offset,sizeof(str_offset),"%ld",offset);
	snprintf(str_whence,sizeof(str_whence),"%d",whence);
        char *newargv1[] = {"lseek",str_fd,str_offset,str_whence,"sesh"};

	//printf("offset %s whence %s",str_offset,str_whence);
        ret_value = call_execute(newargv1);

	return ret_value;
}

ssize_t read(int fd, void *buf, size_t nbyte)
{
	//printf("Invoking call read from client_invoke\n");
        char str_fd[12],str_nbyte[12];
        long ret_value;
    
	//printf("\nfd = %d",fd);
        snprintf(str_fd,sizeof(str_fd),"%d",fd);
        snprintf(str_nbyte,sizeof(str_nbyte),"%d",nbyte);
        char *newargv1[] = {"read",str_fd,buf,str_nbyte,"sesh"};
        ret_value = call_execute(newargv1);
	//printf("returning from read\n");
        return ret_value;
}

int rename(const char *old,const char *new)
{

	int ret_value;
	char *newargv1[] = {"rename",old,new,"sesh"};
        ret_value = call_execute(newargv1);

        return ret_value;
}

int creat(const char *pathname,mode_t mode)
{
	//printf("i am here in creat \n");
	int ret_value;
	char str_mode[12];	
	snprintf(str_mode,sizeof(str_mode),"%d",mode);
	char *newargv1[] = {"creat",pathname,str_mode,"sesh"};
	ret_value = call_execute(newargv1);
	return ret_value;
}

int lstat(const char *path, struct stat *buff)
{
        //printf("I am here in stat\n");	
	fflush(stdout);
	int ret_value;
 	char *newargv1[] = {"stat",path,"sesh"};/*Not sending buff.Avoiding seding a structure obver a socket. It wrong but avoiding the trouble now. To send a structure over a socket, each memeber element of the structure can be send seperately*/
	ret_value = call_execute(newargv1);
	return ret_value;
}

int unlink(const char *pathname)
{
	int ret_value;
	char *newargv1[] = {"unlink",pathname,"sesh"};
	ret_value = call_execute(newargv1);
	return ret_value;
}



