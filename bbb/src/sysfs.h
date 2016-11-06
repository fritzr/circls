/*
 * sysfs.h
 * Fritz Reese (C) 2016
 *
 * Abstract wrapper for SysFS pin interfaces.
 *
 */
#ifndef _SYSFS_H_
#define _SYSFS_H_

#include <string>
#include <fstream>

using std::string;
using std::ofstream;

#define SYSFS_PATH "/sys/class/"

namespace bbb
{
  // Abstract wrapper around a SYSFS entry.
class SysFS
{
  protected:
    int number;        /* number of the submodule */
    string category;   /* sub-path of the module in the sysfs tree */
    string dir;        /* set to the sysfs parent path, e.g. /sys/class/gpio/ */
    string modname;    /* set to the pin name with number, e.g. gpio4/ */

  public:
    /* Exports the submodule pin with the given category and name.  */
    SysFS(const string &category, int number);

    virtual ~SysFS();

    /* Set a property, by writing to a file entry in the sysfs tree.
     * Return false iff the set is unsuccessful.  */
    virtual bool setProperty(const string &propName, int value);
    virtual bool setProperty(const string &propName, const string &value);

    /* Set a proprty, by reading from a file entry in the sysfs tree.  */
    virtual string getProperty(const string &propName);

    virtual int getNumber () { return number; }

  protected:
    /* Write data to a sysfs entry under the current 'path'.  */
    bool write(const string &sub_path, int value);
    bool write(const string &sub_path, const string &value);

    /* Read data from a sysfs entry under the current 'path'.  */
    string read(const string &sub_path);

  private:
    /* Export the pin, exposing its sysfs entry.  */
    void exportPin(void);

    /* Un-export the pin, safely closing the sysfs entry.  */
    void unexportPin(void);
}; // class SysFS

}; // namespace bbb

#endif // _SYSFS_H_
