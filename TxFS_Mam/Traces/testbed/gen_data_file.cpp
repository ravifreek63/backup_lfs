#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<cstdlib>
#include<fstream>

using namespace std;
int number_transactions=6000;
int data_per_transaction=400*5*2;
int ASCII_start=65;
int ASCII_end=90;

int main()
{
 int diff= ASCII_end-ASCII_start;
 int r_num;
 char ch;
 fstream outfile ("read_data.txt", fstream::out);
 int count=0, data_written;
 for(; count<number_transactions; count++)
 {
   data_written=0;
   for(; data_written<data_per_transaction; data_written++)
   {
     r_num = ASCII_start + (rand () % diff);
     ch = r_num;
     outfile << ch;
   }
 }
 outfile.close();
 return 0;
}
