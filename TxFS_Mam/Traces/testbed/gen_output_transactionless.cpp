#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<cstdlib>
#include<fstream>
#include<string.h>
#include<string>
#define MAX_LINE_SIZE 50


using namespace std;

int main()
{
 ssize_t read, size; size_t len;
 FILE *traceFile = fopen ("trace1.txt", "r");
 if (traceFile == NULL)
   {
   	printf("TraceFile could not be opened  :: BUG\n");
	return -1;
   }
 FILE *read_data = fopen ("read_data.txt", "r");
  if (read_data == NULL)
   {
   	printf("Read Data could not be opened :: BUG\n ");
	return -1;
   }
 FILE *outfile = fopen ("correct_output_transactionless.txt", "w");
  if (outfile== NULL)
   {
   	printf("Output File could not be opened  :: BUG\n");
	return -1;
   }
 char *line = (char *)malloc (sizeof(char) * (MAX_LINE_SIZE));
 char *buf; 
 string str_buf;
 int size_left;
 int buffer_size; 

 // getting the size of the read_data file 
 fseek (read_data, 0, SEEK_END);

 size_left = (int)ftell (read_data);

 fseek (read_data, 0, SEEK_SET);   

 while ((read = getline(&line, &len, traceFile)) != -1) 
 {
    if ((line != NULL) && (int(line[0]) >= 48 && int(line[0]) <= 57))
	{
	       continue;
	}
    else
    {
      if (!(strncmp(line,"txn_beg()", 8)))
      {
	str_buf="";
	printf("txn_beg\n");
      }
      else if (!(strncmp(line, "write", 5)))
      {
	printf("write\n");
        buffer_size = 200;
	size_left = ((size_left > buffer_size) ? (size_left-buffer_size) : 0);
        if (size_left == 0) break;
        buf = (char *)malloc((buffer_size+1) * sizeof(char));
        memset (buf, 0, buffer_size+1);
        size = fread ((void *)buf, 1, buffer_size, read_data);	
	if (size != buffer_size)
	{
		cout << " Size Read Lesser Than The Buffer Size :: BUG" << endl;
        }
        str_buf += string(buf);
	free (buf);
      }
      else if ((!(strncmp(line, "txn_commit()", 12))) || (!(strncmp(line, "txn_abort()", 11))))
      {
	printf("txn_commit\n");
	fwrite (str_buf.c_str(), 1, str_buf.length(), outfile);
      }
    }
 }
  free(line);
  fclose (traceFile);
  fclose (read_data);
  fclose (outfile);
 
 return 0;
}
