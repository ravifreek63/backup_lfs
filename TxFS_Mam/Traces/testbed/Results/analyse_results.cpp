#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<cstdlib>
#include<fstream>
#include<string.h>
#include<string>
#define MAX_LINE_SIZE 500
#define MAX_SIZE_STR 200
#define MAX_TOKEN_SIZE 100

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
}

int main()
{
 ssize_t read, size;
 size_t len;
 char subtoken[MAX_TOKEN_SIZE];

 const char delim = ':';
 FILE *resultFile = fopen ("result.txt", "r");
 int typeId;
 int objectHeaderCount=0;
 int blockWrite=0;
 bool flag;
 int state;
 int file_size;
 FILE *analysesFile = fopen ("analyses.txt", "w");
 FILE *finalResults = fopen ("finalResults.txt", "w");
 FILE *sizeFile_fd = fopen ("sizeFile.txt" , "w"); 
 float step_size=0.01;
 float ratio;
 bool bmap_init = false;
 int curr_obj_id=0, obj_id;
 float abort_ratio=0;
 int iter_num=0;
 long time;

 if (analysesFile == NULL)
   {
   	printf("AnalysesFile could not be opened  :: BUG\n");
	return -1;
   }
  if (finalResults == NULL)
   {
   	printf("finalResults.txt could not be opened  :: BUG\n");
	return -1;
   }
 if (resultFile == NULL)
   {
   	printf("ResultFile could not be opened  :: BUG\n");
	return -1;
   }
 if (sizeFile_fd == NULL)
   {
   	printf("sizeFile.txt could not be opened  :: BUG\n");
	return -1;
    }
 char suffix_str[MAX_SIZE_STR];
 char size_info[MAX_SIZE_STR];
 memset (suffix_str, 0, MAX_SIZE_STR);
 memset (size_info, 0, MAX_SIZE_STR);
  char *line = (char *)malloc (sizeof(char) * (MAX_LINE_SIZE));
  memset (line, 0, MAX_LINE_SIZE);
  while ((read = getline(&line, &len, resultFile)) != -1) 
  {
     flag = true;   
     line[strlen(line)-1] =  '\0';
     //printf("%s\n", line);  
     for (state=1; flag==true ; state++) 
     {
                   get_token (line, state, subtoken, delim);		   		  
		   //printf("%s\n", subtoken);
		   if(state == 1)
		   	typeId = atoi (subtoken);
		   switch (typeId)
		   {
			case (1):
			objectHeaderCount++;
			flag=false;			
			break;
		
			case (2):
			flag=false;
			break;

			case (3):
			blockWrite++;
			flag=false;
			break;
				
			case (4):
			if (state == 3)
			{
				file_size = atoi (subtoken);
				flag=false;
			}			
			break;

			case (5):
			
			if (bmap_init)
			{
				 abort_ratio += step_size;
				 fprintf (analysesFile, "iternum:%d\n", iter_num++);				
				 fprintf (analysesFile, "4:%d\n", file_size);
				 fprintf (analysesFile, "1:BlocksWritten:%d\n", blockWrite);
				 if (file_size != 0)
					{
         			 		ratio = (float)blockWrite*2048/file_size;					 		
					}
				else
				ratio=0;
				fprintf (analysesFile, "6:%f\n", ratio);
				fprintf (finalResults, "%f\t%f\n", abort_ratio, ratio);
			}
			else
			{
				bmap_init=true;
			}
				objectHeaderCount=0;
				blockWrite=0;
				file_size=0;
				ratio=0;
				flag=false;				
			break;

			case (6):
			if ((state == 2)||(state == 3)||(state == 7)||(state == 8))
			{
						
				sprintf (suffix_str, "%s\t", subtoken);				
				strcat(size_info, suffix_str); 
				memset (suffix_str, 0, MAX_SIZE_STR);
				if (state == 8)
					{
						fprintf(sizeFile_fd, "%s\n", size_info);
						memset (size_info, 0, MAX_SIZE_STR);
						flag = false;
					}
			}
			break;
			
			case (100):
			flag=false;
			break;
		   }
                  
               }
  memset (line, 0, MAX_LINE_SIZE);
  }
 
 //Calculating the number of object headers written 


				 abort_ratio += step_size;
				 fprintf (analysesFile, "iternum:%d\n", iter_num++);				
				 fprintf (analysesFile, "4:%d\n", file_size);
				 fprintf (analysesFile, "1:BlocksWritten:%d\n", blockWrite);
				 if (file_size != 0)
					{
         			 		ratio = (float)blockWrite*2048/file_size;					 		
					}
				else
					ratio=0.0;
				fprintf (analysesFile, "6:%f\n", ratio);
				fprintf (finalResults, "%f\t%f\n", abort_ratio, ratio);


 fclose(resultFile);
 fclose(analysesFile);
 fclose(finalResults);
 fclose (sizeFile_fd);
 return 0;
}
