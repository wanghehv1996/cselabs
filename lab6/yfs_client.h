#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
#include "lock_client.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>


class yfs_client {
  extent_client *ec;
  lock_client *lc;
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
  bool recoveryflag;
  int nowv;
  static std::string filename(inum);
  static inum n2i(std::string);
  static std::string i2n(inum inum);

  void log_setattr(inum ino,int size);
  void log_write(inum ino, size_t size, off_t off, const char *data);
  void log_create(inum parent, const char *name, mode_t mode);
  void log_unlink(inum parent,const char* name);
  void log_symlink(const char*link,inum parent,const char*name);
  void log_mkdir(inum parent, const char *name, mode_t mode);
  void recovery(unsigned int pos);

 public:
  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
  int mkdir(inum , const char *, mode_t , inum &);

	int symlink(inum parent, const std::string name, const std::string link, inum &ino);
  int readlink(inum ino, std::string &link);

  int lockless_getdir(inum, dirinfo &);
  int lockless_lookup(inum, const char *, bool &, inum &);
  bool lockless_isdir(inum);
  int lockless_getfile(inum, fileinfo &);

  void commit();
  void preversion();
  void nextversion();


};

#endif
