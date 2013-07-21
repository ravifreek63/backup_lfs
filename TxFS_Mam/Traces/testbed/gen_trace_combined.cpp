#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<cstdlib>
#include<fstream>

using namespace std;
//
float alpha = 0.0;     // INITIAL ABORT RATE 0.0 	
int max_transactions=1000; // MAX TRANSACTIONS 100
int max_num=10;     
unsigned int start_transaction_num=3;
int write_size_min[]={5000,  10000, 20000 };
int write_size_max[]={10000, 20000, 40000 };
int write_size;
int data_read = 5000;
int num_commits = 0;
int num_aborts = 0;
int committed_data = 0;
int num_iters=5;
int sizeOfDataType=3;


int main(int argc, char *argv[])
{
  int count, count_val, r_num, iter_num, p_Size=0;
  int write_diff;
  fstream traceFileStream;
  char *traceFileName;
  unsigned int curr_transaction_num;
  while (p_Size < sizeOfDataType)
  {
  	iter_num =0;
        write_diff = write_size_max[p_Size]-write_size_min[p_Size];
        traceFileName = (char *)(malloc)(100 * sizeof (char));
	alpha = 0.0;

  curr_transaction_num=start_transaction_num;
  fstream abortRatioFile ("abortRatio.txt", fstream :: out);
  for(;iter_num<num_iters; iter_num++)
  {
          
	  committed_data=0;
	  num_aborts=0;
	  num_commits=0;	
	  sprintf (traceFileName, "trace%d_%d.txt", iter_num, p_Size);
	  traceFileStream.open (traceFileName, fstream::out);	  
	  traceFileStream << curr_transaction_num << endl;
	  traceFileStream << "txn_beg()" << endl;
	  traceFileStream << curr_transaction_num << endl;
	  traceFileStream << "creat(/yaffs2/file"<< iter_num <<".txt,S_IRUSR|S_IWUSR)" << endl;
	  traceFileStream << curr_transaction_num << endl;
	  traceFileStream << "open(/yaffs2/file"<< iter_num <<".txt,O_RDWR)" << endl;
	  traceFileStream << curr_transaction_num << endl;	
	  traceFileStream << "txn_commit()" << endl;
	  num_commits++;
	 
	 for(count=1; count<max_transactions; count++)
	 {
		  curr_transaction_num++; 
		  traceFileStream << curr_transaction_num << endl;			// Start of a transaction
		  traceFileStream << "txn_beg()" << endl;
		  traceFileStream << curr_transaction_num << endl;
		  traceFileStream << "open(/yaffs2/file"<< iter_num <<".txt,O_APPEND)" << endl;
		  traceFileStream << curr_transaction_num << endl;
		  write_size = 	write_size_min[p_Size] + (rand() % write_diff);
		  traceFileStream << "write(/yaffs2/file" << iter_num <<".txt,"<< write_size << ")" << endl; 
		  traceFileStream << curr_transaction_num << endl;
		  r_num = (rand () % max_num); 
		  if (((float)r_num/(float)max_num) < (alpha))
		  {
		   // a transaction abort
		    num_aborts++;
		    traceFileStream << "txn_abort()" << endl;
		  }
		  else
		  {
		    // else a transaction commit 
		    num_commits++;
		    traceFileStream << "txn_commit()" << endl;
                    committed_data += write_size;	
		  }
         }
		 //cout << ((float)count_val/(float)max_transactions) << endl;
		 // Last Transaction Is For Read 
		 
		  curr_transaction_num++; 
		  traceFileStream << curr_transaction_num << endl;			// Start of a transaction
		  traceFileStream << "txn_beg()" << endl;
		  traceFileStream << curr_transaction_num << endl;
		  traceFileStream << "open(/yaffs2/file"<< iter_num <<".txt,O_RDWR)" << endl;
		  int num_read_loops = 1;
		  for(count=0; count<num_read_loops; count++)
		  {
		     traceFileStream << curr_transaction_num << endl;
		     data_read=committed_data ;
		     traceFileStream << "read(/yaffs2/file"<< iter_num <<".txt,"<< data_read << ")" << endl; 
		  }
		  traceFileStream << curr_transaction_num << endl;
		  traceFileStream << "txn_commit()" << endl;
		  num_commits++;
		  traceFileStream.close();
		  //cout << "Number of Commits "<< num_commits << endl;
		  //cout << "Number of Aborts "<< num_aborts << endl;
		  abortRatioFile << "Ratio of Aborts "<< (float)num_aborts/(float)(num_commits+num_aborts) << " abort ratio="<< alpha<< endl;
		  alpha += 1.0/num_iters;
  }
  free (traceFileName);
  p_Size++;	
  }
  return 0;
}
