#include <stdlib.h>
#include<stdio.h>
#include<string.h>

struct list_rec{
		int seqno;
		int path_length;
		char *file_path;
	};
FILE *listoffiles;
int main()
{
        struct list_rec t;
        
        listoffiles = fopen("/home/lipika/TxFS/Traces/files.txt","rb");
        if(!listoffiles)
        {
                printf("Unable to open file");
                return 1;
        }



        while(fread(&t.seqno,sizeof(int),1,listoffiles)!=0)
        {
		printf("\n %d  ",t.seqno);/*Log Record Number*/
		if(fread(&t.path_length,sizeof(int),1,listoffiles)!=0)
		{
		        printf(" %d",t.path_length);
			t.file_path = malloc(sizeof(char)*t.path_length);
			//memset(t.file_path,0,*sizeof(char));
                	if(fread(t.file_path,t.path_length,1,listoffiles)!=0)
                	printf(" %s",t.file_path);
			free(t.file_path);
		}
		fflush(stdout);
        }
}

