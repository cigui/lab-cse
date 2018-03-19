#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {
  extent_client *ec;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
  static std::vector<std::string> split(const std::string &, char);
  /* Members for Log-based Version Control */
  bool recovering;
  int currentV;
  void log_setattr(inum, unsigned long);
  void log_create(inum, const char *, mode_t);
  void log_write(inum, size_t, off_t, const char *);
  void log_unlink(inum, const char *);
  void log_mkdir(inum, const char *, mode_t);
  void log_symlink(const char *, inum, const char *);
  void recoverTo(unsigned int);

 public:
  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, filestat, unsigned long);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum, const char *);
  int mkdir(inum , const char *, mode_t, inum &);

  int verify(const char* cert_file, unsigned short*);
  
  /** you may need to add symbolic link related methods here.*/
  bool issym(inum);

  // The function getsym is nearly the same as getfile, 
  // so just use getfile instead.
  // int getsym(inum, fileinfo &);

  int readlink(inum, std::string &);
  int symlink(const char *, inum, const char *, inum &);

  /* Functions for Log-based Version Control */
  void commit();
  void undo();
  void redo();


};

#endif 
