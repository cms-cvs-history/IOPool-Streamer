#include <fstream>
#include <iostream>
#include "IOPool/Streamer/interface/StreamerFileIO.h"
using namespace std;

int main(int argc, char* argv[]){

   if (argc < 2)
   {
      cerr << "No command line argument given, expected path/filename.\n";
      return 0;
   }

   std::string filename(argv[1]);
   ifstream file(filename.c_str(), ios::in | ios::binary | ios::ate);
   if (!file.is_open()) {
     cerr << "File " << filename << " could not be opened.\n";
     return 1;
   }

   ifstream::pos_type size = file.tellg();
   file.seekg (0, ios::beg);

   char *ptr = new char[1024*1024];
   uint32_t a = 1, b = 0;
   
   ifstream::pos_type rsize = 0;
   while(rsize<size) {
     file.read(ptr, 1024*1024);
     rsize+=1024*1024;
     //cout << file.tellg() << " " << rsize << " " << size << " - " << file.gcount() << endl;
     if(file.gcount()) 
       OutputFile::adler32(ptr,file.gcount(),a,b);
     else 
       break;
   }

   uint32 adler = (b << 16) | a;
   cout << hex << adler << " " << filename << endl;

   file.close();
   delete[] ptr;
   return 0;
}
