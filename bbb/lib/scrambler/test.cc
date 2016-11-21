#include "scrambler.h"

#include <cstring> // memset

#include <iostream>
#include <sstream>

using namespace std;
using namespace scrambler;

static string
dump_bits (const void *data, size_t len)
{
  const unsigned char *bytes = reinterpret_cast<const unsigned char *> (data);
  ostringstream out;
  while (len)
  {
    int byte = *bytes++;
    for (unsigned int i = 0; i < 8; i++)
    {
      out << ((byte & 0x80) >> 7);
      byte <<= 1;
    }
    out << ' ';
    --len;
  }
  return out.str ();
}

int main(int argc, char *argv[])
{
  config conf1(8);
  config conf2(12);
  config conf3(20);
  config conf4(32);
  Scrambler *scram0 = Scrambler::create ();
  Scrambler *scram1 = Scrambler::create (&conf1);
  Scrambler *scram2 = Scrambler::create (&conf2);
  Scrambler *scram3 = Scrambler::create (&conf3);
  Scrambler *scram4 = Scrambler::create (&conf4);

  scram2->rseed ();
  scram3->rseed ();
  scram4->rseed ();

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
    }

    cout << " original: " << endl
         << dump_bits (text.c_str (), text.size ()) << endl;

    memset (output, 0, len);
    scram0->scramble (text.c_str (), output, len-1);
    cout << "[ 5] " << "0x" << hex << scram0->get_seed ()
      << " scrambled: " << endl << dump_bits (output, text.size ()) << endl;

    memset (output, 0, len);
    scram1->scramble (text.c_str (), output, len-1);
    cout << "[ 8] " << "0x" << hex << scram1->get_seed ()
      << " scrambled: " << endl << dump_bits (output, text.size ()) << endl;

    memset (output, 0, len);
    scram2->scramble (text.c_str (), output, len-1);
    cout << "[12] " << "0x" << hex << scram2->get_seed ()
      << " scrambled: " << endl << dump_bits (output, text.size ()) << endl;

    memset (output, 0, len);
    scram3->scramble (text.c_str (), output, len-1);
    cout << "[20] " << "0x" << hex << scram3->get_seed ()
      << " scrambled: " << endl << dump_bits (output, text.size ()) << endl;

    memset (output, 0, len);
    scram4->scramble (text.c_str (), output, len-1);
    cout << "[32] " << "0x" << hex << scram4->get_seed ()
      << " scrambled: " << endl << dump_bits (output, text.size ()) << endl;

    getline (cin, text);
  }

  delete scram0;
  delete scram1;
  delete scram2;
  delete scram3;
  delete scram4;

  return 0;
}
