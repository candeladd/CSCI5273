
#include <iostream>
#include <vector>
#include <sstream>
using namespace std;
int main ()
{
  vector<string> request;
  string input = "GET http://www.google.com HTTP/1.0";
	
  stringstream ss(input);

  /* Convert input string to vector */
  string element;
  string line;
  while (getline(ss, line)) 
  {		
	stringstream line_ss(line);
	while (getline(line_ss, element, ' ')) 
    {
	  cout << element<< endl;
	  request.push_back(element);
	}
  }

  return 0;
}
