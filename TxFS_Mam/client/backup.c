#include <stdio.h>
#include <string.h>
int main()
{
  char bck_src_pwd[100],bck_dest_pwd[100];
  bzero(bck_src_pwd,100);
  printf("\nBACKUP");
  strcpy(bck_src_pwd,"/media/New\ Volume/Lipika_FS/March");
  bzero(bck_dest_pwd,100);
  strcpy(bck_dest_pwd,"/media/New\ Volume/BackUp/backupfile.txt");
  bck_beg(bck_src_pwd,bck_dest_pwd);
  printf("\n Backup completed");
}


