#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<cstdlib>
#include<fstream>
#include<string.h>
#include<string>
#define MAX_LINE_SIZE 50


using namespace std;
int num_iter=1;

int main(int argc, char *argv[])
{
 if (argc>1)
	if (argv[1] != NULL)
		num_iter = atoi (argv[1]);
 ssize_t read, size; size_t len;
 FILE *traceFile;
 FILE *read_data;
 FILE *outfile;
 char *traceFilePath = (char *) (malloc(100));
 char *outputFilePath = (char *) (malloc(100));
 int c_iter;
 char *pos_b, *pos_e, *num_str;
 int length;
 for (c_iter=0; c_iter<num_iter; c_iter++)
 {
         sprintf(traceFilePath, "trace%d.txt", c_iter);
	 traceFile = fopen (traceFilePath, "r");
	 if (traceFile == NULL)
	   {
	   	printf("TraceFile could not be opened  :: BUG\n");
		return -1;
	   }
	  read_data = fopen ("read_data.txt", "r");
	  if (read_data == NULL)
	   {
	   	printf("Read Data could not be opened :: BUG\n ");
		return -1;
	   }
          sprintf(outputFilePath, "correct_output%d.txt", c_iter);
	  outfile = fopen (outputFilePath, "w");
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
		//printf("txn_beg\n");
	      }
	      else if (!(strncmp(line, "write", 5)))
	      {
		pos_b = strstr (line, "txt") + 4;
		if (pos_b == NULL)
		{
			cout << " Error in reading the size of write buffer :: Cannot find txt" << endl;
			return -1;
		}
		pos_e = strstr (line, ")") -1;
		if (pos_e == NULL)
		{
			cout << " Error in reading the size of write buffer :: Cannot find )" << endl;
			return -1;
		}
		length = pos_e-pos_b+1;
		num_str = (char *) malloc(length+1);
		memset (num_str, 0, length+1);
		strncpy (num_str, pos_b, length);
		buffer_size = atoi(num_str);
		//cout << "buffer_size=" << num_str << endl;
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
	      else if (!(strncmp(line, "txn_commit()", 12)))
	      {
		//printf("txn_commit\n");
		fwrite (str_buf.c_str(), 1, str_buf.length(), outfile);
	      }
	    }
	 }
	  free(line);
	  fclose (traceFile);
	  fclose (read_data);
	  fclose (outfile);
       }	 
  free (traceFilePath);
  free (outputFilePath);
  return 0;
}
