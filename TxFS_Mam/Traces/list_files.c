#include <ftw.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct list_rec{
		int seqno;
		int path_length;
		char *file_path;
	};
FILE *listoffiles;
int no_of_files;/*couter to keep track of files visited*/
static int list_the_files(const char *fpath,const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
  int fd;
  struct list_rec t;
  struct stat buff;

  if(stat(fpath,&buff) == -1)
	printf("Error reading inode");

  /*the file will be listed if it is directory or if it is a regular file and can be opened for writing*/
  if(S_ISDIR(buff.st_mode)==0)
  {
	//printf("\nIT IS NOT A DIRECTORY %s",fpath);
	if(S_ISREG(buff.st_mode)!=0)
	{
		//printf("IT IS A REGULAR FILE %s",fpath);
		fd = open(fpath,O_RDWR);
  		if(fd != -1)
  		{
			close(fd);
			no_of_files = no_of_files+1;
  			t.seqno = no_of_files;
  			t.path_length = strlen(fpath)+1;
  			t.file_path = malloc(sizeof(char)*t.path_length);
  			strcpy(t.file_path,fpath);

  			fwrite(&t.seqno,sizeof(int),1,listoffiles);
  			fwrite(&t.path_length,sizeof(int),1,listoffiles);
  			fwrite(t.file_path,sizeof(char)*t.path_length,1,listoffiles);

  			free(t.file_path);
		}
	   	else
			printf("\n is regular but could not open %s",fpath);
	}
	else
		printf("\nNot a regular file %s",fpath);	
  }
  else
  {
        no_of_files = no_of_files+1;
  	t.seqno = no_of_files;
  	t.path_length = strlen(fpath)+1;
  	t.file_path = malloc(sizeof(char)*t.path_length);
  	strcpy(t.file_path,fpath);

  	fwrite(&t.seqno,sizeof(int),1,listoffiles);
  	fwrite(&t.path_length,sizeof(int),1,listoffiles);
  	fwrite(t.file_path,sizeof(char)*t.path_length,1,listoffiles);

  	free(t.file_path);
   }
  return 0;
}

int main(int argc, char *argv[])
{
  listoffiles = fopen("/home/lipika/TxFS/Traces/files.txt","w+b");
  no_of_files = 0;
  ftw(argv[1],list_the_files,20);
  fwrite(&no_of_files,sizeof(int),1,listoffiles);//Writing the total number of records at the end of the file
}
