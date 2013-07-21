#include <iostream>
#include <fstream>

using namespace std;

int main ()
{
 int x, sum=0;
 fstream infile ("f.txt" , fstream :: in);
 while (infile.good())
 {
 	 infile >> x;
	 sum = sum + x;
 }
 cout << sum;
 return 0;
}
