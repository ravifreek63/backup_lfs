#include <stdio.h>
#include <fstream>
#include <iostream>
using namespace std;
int main()
{
  float val, abort_ratio, ratio=0, sum=0;
  int num_val=0;  
  int count=0; 
  fstream infile ("finalResults.txt", fstream :: in);
  fstream outfile ("overheadRatio.txt", fstream :: out);
  while (infile.good())
  {
	count++;
  	infile >> abort_ratio;
	infile >> val;
	sum += val;
	if (val >0)
	{
	 	 num_val++;
	}
	if (count == 10)	
        {
		count = 0;	
		if (num_val != 0)
		ratio = (sum/num_val);
		else
		ratio = 0;
		outfile << ratio << endl;
		sum=0;
		num_val=0;
	}
   }
	infile.close();
	outfile.close();
	return 0;
	
}
