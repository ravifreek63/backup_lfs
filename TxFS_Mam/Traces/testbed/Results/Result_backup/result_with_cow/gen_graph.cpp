#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<cstdlib>
#include<fstream>
#include<string.h>
#include<string>
#include <math.h>
#define MAX_LINE_SIZE 500
#define MAX_FILENAME_SIZE 500
#define MAX_SIZE_STR 200
#define MAX_TOKEN_SIZE 100
using namespace std;

void get_token (char *line, int num_token, char *token, char delim)
{
  int len = strlen (line);
  int count = 0;
  char ch;
  int pos=0;
  token[pos] = '\0';
  while (count < len)
  {
	ch = line[count];
	if (num_token == 1)
	{
		if (ch != delim)
		{
			token [pos] = ch;
		}
		else 
			token[pos] = '\0';
		pos++;
	}
        if (ch == delim)
	{
		num_token--;
		if (num_token == 0)
		break;
	}
	count++;
  } 
token[pos] = '\0';
}

int main()
{
 double initial_time, curr_time, out_time;
 FILE *resultFile = fopen ("sizeFile.txt", "r");
 fstream outFile;
 ssize_t read, size;
 size_t len;
 char subtoken[MAX_TOKEN_SIZE];
 const char delim = '\t';
 int typeId;
 int objectHeaderCount=0;
 int count;
 int blockWrite=0;
 bool flag;
 int state;
 int file_size;
 float step_size=0.01;
 float ratio;
 bool bmap_init = false;
 char *line = (char *)malloc (sizeof(char) * (MAX_LINE_SIZE));
 memset (line, 0, MAX_LINE_SIZE);
 read = getline(&line, &len, resultFile);
 memset (line, 0, MAX_LINE_SIZE);
 if (read == -1 ) return -1;
 int curr_size, curr_obj_id =-1, prev_obj_id =-1;
 bool is_init=false;
 bool g_init=false;
 char filename[MAX_FILENAME_SIZE];
 memset(filename , 0, MAX_FILENAME_SIZE);
 int step=0;
  while ((read = getline(&line, &len, resultFile)) != -1) 
  {
    line[strlen(line) -1] = '\0';
    for (count=1; count<=4; count++)
    {
	 get_token (line, count, subtoken, delim);
	 //printf ("%s\n", subtoken);	
         if (count==1)
	 {
		curr_size = atoi(subtoken);
	 }
	else if (count == 2)
	{
		curr_obj_id = atoi (subtoken);
		if (prev_obj_id != curr_obj_id)
		{
			is_init = true;
			prev_obj_id = curr_obj_id;	
		}
	}
	else if (count == 3)
	{
		curr_time = (double)atol(subtoken);
		
	}
	else if (count==4)
	{
		
		curr_time = curr_time * pow(10,6) + (double)atol(subtoken);
		if (is_init)
		{
		  initial_time = curr_time;
		  is_init = false;
		  sprintf (filename, "graph%d.txt", curr_obj_id);
		 

		  if (g_init)
		{			 
			(outFile.close());			
		}
		else
		{
			g_init = true;
		}
			 outFile.open (filename, fstream::out);	
			 step=0; 
		}
                out_time = curr_time - initial_time; 
		step++;               
		outFile <<  curr_size << "\t" << out_time << endl;				
	}
    }
  }
  return 0;
}
