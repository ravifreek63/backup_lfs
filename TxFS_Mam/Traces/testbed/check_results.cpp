// This is the code for differentiating results
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILE_SIZE 50
FILE *read_fd;
FILE *write_fd;

char *read_file;// = "read_data.txt";
char *write_file;// = "write_data.txt";
int c_iter;
int count;
int max_num_iter=9;
using namespace std;

int main(int argc, char *argv[])
{
  char ch_write, ch_read;
  if (argc > 1)
	  max_num_iter = atoi (argv[1]);
  read_file = (char *)(malloc(MAX_FILE_SIZE));
  write_file = (char *)(malloc(MAX_FILE_SIZE));
  for (c_iter=0; c_iter<max_num_iter; c_iter++)
  {
	memset(read_file, 0, MAX_FILE_SIZE);
	memset(write_file, 0, MAX_FILE_SIZE);
	count=0;
	sprintf (read_file, "correct_output%d.txt", c_iter);
	sprintf (write_file,"write_data%d.txt", c_iter);	
	read_fd = fopen (read_file, "r");
	if (read_fd == NULL) 
		{
			cout << "Read Data File Open Failed : BUG" << endl;
			return -1;
		}
	write_fd = fopen (write_file, "r");
	if (write_fd == NULL)
		{
			cout << "Write Data File Open Failed : BUG" << endl;
			fclose (read_fd);
			return -1;	
		}
	 ch_write = fgetc(write_fd);
	 ch_read  = fgetc(read_fd);
	count++;
	while ((ch_read != EOF) && (ch_write != EOF))
	{
		count++;
		if (ch_write != ch_read)
			{
				cout << "Position=" << count << "Iteration Number=" << c_iter << endl;
				cout << "Files Do Not Match" << endl;
				break;
			}
		ch_write = fgetc(write_fd);
		ch_read  = fgetc(read_fd);
	}
	if (ch_write == ch_read)
	{
		cout << "Files Match" << endl;
		
	}
	fclose (read_fd);
	fclose (write_fd);
      }
	free(read_file);
	free(write_file);
 	return 0;
}

