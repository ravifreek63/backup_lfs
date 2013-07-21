#include "my_header.h"
#include<stdio.h>

int main()
{
        int counter;
        FILE *file_logging;
        struct log_record t;
        
        file_logging = fopen("/home/lipika/TxFS/server/log.txt","rb");
        if(!file_logging)
        {
                printf("Unable to open file");
                return 1;
        }



        while(fread(&t.LRN,sizeof(long),1,file_logging)!=0)
        {
		//fread(&t.LRN,sizeof(long),1,file_logging);
                 printf("\n NEXT RECORD");
		fflush(stdout);
                 printf(" LRN %ld",t.LRN);/*Log Record Number*/
		fflush(stdout);
		fread(&t.txn_id,sizeof(int),1,file_logging);
                 printf(" tid %d",t.txn_id);
		fflush(stdout);
		fread(&t.prev_LRN,sizeof(long),1,file_logging);
                 printf(" prev_LRN %ld",t.prev_LRN);/*for a particular tid.first record is 0*/
		fflush(stdout);
		fread(&t.next_LRN,sizeof(long),1,file_logging);
                 printf(" next_LRN %ld",t.next_LRN);/*for a particular tid.last record is 0*/
		fflush(stdout);
		fread(&t.record_type,sizeof(int),1,file_logging);
                 printf(" record_type %d",t.record_type);
		fflush(stdout);
		fread(&t.fd,sizeof(int),1,file_logging);
		 printf(" fd %d",t.fd);
		fflush(stdout);
		fread(&t.offset,sizeof(long),1,file_logging);
		 printf(" offset %ld",t.offset);
		fflush(stdout);
		fread(&t.write_length,sizeof(long),1,file_logging);
                 printf(" write length %ld",t.write_length);
		fflush(stdout);
		t.data=malloc(sizeof(char)*t.write_length);
		fread(t.data,t.write_length,1,file_logging);
		 printf(" data %s",t.data);
		fread(&t.write_length1,sizeof(long),1,file_logging);
                 printf(" write length1 %ld",t.write_length1);
                fflush(stdout);
                t.data1=malloc(sizeof(char)*t.write_length1);
                fread(t.data1,t.write_length1,1,file_logging);
                 printf(" data %s",t.data1);

		fflush(stdout);
        }
}

