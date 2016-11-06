#include "sysfs.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <errno.h>

using namespace std;

namespace bbb
{

SysFS::SysFS(const string &category, int num)
  : number(num)
{
  stringstream s;

  /* PATH e.g. /sys/class/gpio/ */
  s << SYSFS_PATH << category << "/";
  dir = s.str ();

  /* SUBPATH e.g. /sys/class/gpio/gpio4/ */
  stringstream m;
  m << category << number << "/";
  modname = m.str ();

  exportPin ();
}

SysFS::~SysFS()
{
  unexportPin ();
}

void SysFS::
exportPin (void)
{
  write ("export", number);
}

void SysFS::
unexportPin (void)
{
  write ("unexport", number);
}

bool SysFS::
setProperty (const string &propName, int value)
{
  return write(modname + propName, value);
}

bool SysFS::
setProperty (const string &propName, const string &value)
{
  return write(modname + propName, value);
}

string SysFS::
getProperty (const string &propName)
{
  return read(modname + propName);
}

bool SysFS::
write (const string &sub_path, int value)
{
  stringstream s;
  s << value;
  return write (sub_path, s.str ());
}

bool SysFS::
write (const string &sub_path, const string &value)
{
  ofstream stream;
  string fname = dir + sub_path;
  stream.open (fname.c_str ());
  if (!stream.is_open ())
  {
    cerr << "SysFS: failed to open file '" << fname << "' for writing: "
      << strerror (errno) << endl;
    return false;
  }

  stream << value;
  bool ret = !stream.bad ();
  stream.close ();

  return ret;
}

string SysFS::
read (const string &sub_path)
{
  ifstream stream;
  string fname = dir + sub_path;
  stream.open (fname.c_str ());
  if (!stream.is_open ())
  {
    cerr << "SysFS: failed to open file '" << fname << "' for reading: "
      << strerror (errno) << endl;
    return string();
  }

  string out;
  getline (stream, out);
  stream.close ();

  return out;
}

};
