#include "stdlib.h"
#include "stdio.h"

int main()
{
 FILE *fd_in;
 FILE *fd_out;
 fd_in = fopen("file0","r");
 fd_out = fopen ("file0.txt", "w");
 int ch;
 ch = fgetc (fd_in) ;
 while (ch != EOF)
 {
	fputc (ch, fd_out);
        ch = fgetc (fd_in) ;
//        if ((48 <= ch ) && (ch <= 57))
	printf("%c", (char) ch);
//        printf("%d",  ch);
 }
 fputc (ch, fd_out);
 fclose (fd_in);
 fclose (fd_out); 
 return 0;
}
