#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<cstdlib>
#include<fstream>

using namespace std;

float alpha = 0.7;
int max_transactions=5000;
int max_num=10;
unsigned int start_transaction_num=2;
int buffer_size=200;
int data_read = 1000000;
int num_commits = 0;
int num_aborts = 0;
int committed_data = 0;

int main()
{
 int count, count_val=0, r_num;
 fstream outfile ("trace1_transactionless.txt", fstream::out);
 unsigned int curr_transaction_num=start_transaction_num;
  outfile << curr_transaction_num << endl;
  outfile << "txn_beg()" << endl;
  outfile << curr_transaction_num << endl;
  outfile << "creat(/yaffs2/file.txt,S_IRUSR|S_IWUSR)" << endl;
  outfile << curr_transaction_num << endl;
  outfile << "open(/yaffs2/file.txt,O_APPEND)" << endl;
  outfile << curr_transaction_num << endl;
  outfile << "write(/yaffs2/file.txt,"<< buffer_size << ")" << endl;
 
 for(count=1; count<max_transactions; count++)
 {
  outfile << curr_transaction_num << endl;
  outfile << "write(/yaffs2/file.txt,"<< buffer_size << ")" << endl; 
  outfile << curr_transaction_num << endl;
  outfile << "write(/yaffs2/file.txt,"<< buffer_size << ")" << endl;  
 }
 //cout << ((float)count_val/(float)max_transactions) << endl;
 // Last Transaction Is For Read 
   outfile << curr_transaction_num << endl;
  outfile << "txn_commit()" << endl;

  curr_transaction_num++; 
  outfile << curr_transaction_num << endl;			// Start of a transaction

  outfile << "txn_beg()" << endl;
  outfile << curr_transaction_num << endl;
  outfile << "open(/yaffs2/file.txt,O_RDWR)" << endl;
  int num_read_loops = data_read/buffer_size;
  for(count=0; count<num_read_loops; count++)
  {
     outfile << curr_transaction_num << endl;
     outfile << "read(/yaffs2/file.txt,"<< buffer_size << ")" << endl; 
  }
  outfile << curr_transaction_num << endl;
  outfile << "txn_commit()" << endl;
  num_commits++;
  outfile.close();

  cout << "Number of Commits "<< num_commits << endl;
  cout << "Number of Aborts "<< num_aborts << endl;
  cout << "Ratio of Aborts "<< (float)num_aborts/(float)(num_commits+num_aborts) << endl;

  return 0;
}
