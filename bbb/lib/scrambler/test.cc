#include "scrambler.h"

#include <cstring> // memset

#include <iostream>

using namespace std;
using namespace scrambler;

int main(int argc, char *argv[])
{
  Scrambler *scram = Scrambler::create ();

  string text;
  unsigned char *output = NULL;
  size_t len = 0;

  getline (cin, text);
  while (!cin.bad () && !cin.eof () && !text.empty ())
  {
    if (len < text.size ())
    {
      if (output)
        delete[] output;
      len = text.size () + 1;
      output = new unsigned char[len];
      memset (output, 0, len);
    }

    scram->scramble (text.c_str (), output, len-1);
    cout << "scrambled: " << endl << "  " << output << endl;

    getline (cin, text);
  }

  return 0;
}
