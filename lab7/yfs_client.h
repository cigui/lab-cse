#ifndef yfs_client_h
#define yfs_client_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


#define CA_FILE "./cert/ca.pem:"
#define USERFILE	"./etc/passwd"
#define GROUPFILE	"./etc/group"
#define LOGFILE "logfileForLab5.log"

class yfs_client {
  extent_client *ec;
  lock_client *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST,
  			NOPEM, ERRPEM, EINVA, ECTIM, ENUSE };

  typedef int status;


  struct filestat {
  	unsigned long long size;
	unsigned long mode;
	unsigned short uid;
	unsigned short gid;
  };

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
	unsigned long mode;
	unsigned short uid;
	unsigned short gid;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
	unsigned long mode;
	unsigned short uid;
	unsigned short gid;
  };

  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
  static std::vector<std::string> split(const std::string &, char);
  bool isdir_nl(inum);
  bool issym_nl(inum);
  int lookup_nl(inum, const char *, bool &, inum &);
  int readdir_nl(inum, std::list<dirent> &);
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
  yfs_client();
  yfs_client(std::string, std::string, const char*);

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
