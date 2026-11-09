// in this file i keep a tiny helper that reads a whole text file into
// a std::string. i mostly use it for loading my shader source code.
//
// i want to remember:
//   - readfile returns true on success and fills outstring
//   - on failure it prints an error with the file name
//   - i should always check the return value when i load shaders

#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <string>
 
using namespace std;
 
bool ReadFile(const char *pFileName, string &outFile)
{
    ifstream f(pFileName);
 
    bool ret = false;
 
    if (f.is_open())
    {
        string line;
        while (getline(f, line))
        {
            outFile.append(line);
            outFile.append("\n");
        }
 
        f.close();
        ret = true;
    }
    else
    {
        fprintf(stderr, "Error in loading file: '%s'\n", pFileName);
    }
    return ret;
}
