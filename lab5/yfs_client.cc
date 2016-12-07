// yfs client.  implements FS operations using extent and lock server

#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  lc->acquire(1);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
  lc->release(1);
}


yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  extent_protocol::attr a;
  lc->acquire(inum);

  if (ec->getattr(inum, a) != extent_protocol::OK) {
      printf("error getting attr\n");
      lc->release(inum);
      return false;
  }

  if (a.type == extent_protocol::T_FILE) {
      printf("isfile: %lld is a file\n", inum);
      lc->release(inum);
      return true;
  }

  printf("isfile: %lld is not a file\n", inum);
  lc->release(inum);
  return false;
}

/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 *
 * */

bool
yfs_client::isdir(inum inum)
{

      // Oops! is this still correct when you implement symlink?
      extent_protocol::attr a;
      lc->acquire(inum);

      if (ec->getattr(inum, a) != extent_protocol::OK) {
          printf("error getting attr\n");
          lc->release(inum);
          return false;
      }

      if (a.type == extent_protocol::T_DIR) {
          printf("isdir: %lld is a dir\n", inum);
          lc->release(inum);
          return true;
      }
      printf("isdir: %lld is not a dir\n", inum);
      lc->release(inum);
      return false;
}


bool
yfs_client::lockless_isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;
  lc->acquire(inum);
  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
      r = IOERR;
      goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
  lc->release(inum);
  return r;
}


int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;
  lc->acquire(inum);

  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
      r = IOERR;
      goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

release:
  lc->release(inum);
  return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)


int
yfs_client::setattr(inum ino, size_t size)
{
  int r = OK;
  /*
   * your lab2 code goes here.
   * note: get the content of inode ino, and modify its content
   * according to the size (<, =, or >) content length.
   */
   lc->acquire(ino);

   printf("setattr: setattr %d with size %d\n", ino, size);
   extent_protocol::attr a;
   extent_protocol::status status = ec->getattr(ino, a);
   if (status != extent_protocol::OK) {
     lc->release(ino);
       return status;
   }

   std::string readBuf;
   ec->get(ino,readBuf);
   std::string retBuf;
   if(a.size > size){
     retBuf = readBuf.substr(0,size);
   }else{
     std::string addBuf(size-a.size,'\0');// what should i add to the tail???
     retBuf = readBuf+addBuf;
   }
   ec->put(ino, retBuf);
   lc->release(ino);

  return r;
}


int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    lc->acquire(parent);
    printf("create: create %s in %d\n", name, parent);
    bool found = false;
    inum old_ino;
    std::string buf;
    extent_protocol::status status = lockless_lookup(parent, name, found, ino_out);
    if(found == true){
      //ino_out = old_ino;
      lc->release(parent);
      return EXIST;
      //return IOERR;
    }

    ec->create(extent_protocol::T_FILE, ino_out);
    ec->get(parent,buf);
    if (buf.size() > 0)
      buf = buf+ "/";
    buf = buf+ std::string(name) + '/' + filename(ino_out);

    ec->put(parent, buf);
    lc->release(parent);
    return r;

}///begin modify


int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    lc->acquire(parent);
    printf("mkdir: mkdir %s in %d\n", name, parent);
    bool found = false;
    inum old_ino;
    std::string buf;

    extent_protocol::status status = lockless_lookup(parent, name, found, old_ino);
    if(found == true){
      //ino_out = old_ino;
      lc->release(parent);
      return EXIST;
      //return IOERR;
    }

    ec->create(extent_protocol::T_DIR, ino_out);
    ec->get(parent,buf);
    if (buf.size() > 0)
      buf=buf+"/";
    buf=buf+std::string(name) + '/' + filename(ino_out);
    ec->put(parent, buf);
    lc->release(parent);
    return r;

}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    lc->acquire(parent);
    int ret = lockless_lookup(parent, name, found, ino_out);
    lc->release(parent);
    return ret;
}


int
yfs_client::lockless_lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
     
    printf("lookup: lookup %s in %d\n", name, parent);
    if(!lockless_isdir(parent)) {
        found = false;
        return r;
    }

    std::string readBuf;
    extent_protocol::status status = ec->get(parent, readBuf);
    if (status != extent_protocol::OK) {
       lc->release(parent);
         return IOERR;
     }
    
    std::string fileStr;
    std::string inoStr;
    bool fileGet = false;
    bool inoGet = false;
    uint32_t length = readBuf.length();
    //find it by pos
    uint32_t pos = 0;
    while ( pos < length) {
        // get filename
        if (!fileGet) {
           if (readBuf[pos] == '/') {
               fileGet = true;
               pos++;
           } else fileStr += readBuf[pos];
        }
        // get inostr
        if (fileGet && !inoGet) {
           if (readBuf[pos] == '/')
               inoGet = true;
           else {
               if (pos == length-1) 
                    inoGet = true;
               inoStr += readBuf[pos];
           }
        }
       // get names and trans
        if (fileGet && inoGet) {
            if (fileStr == std::string(name)){
                found =true;
                ino_out = n2i(inoStr);
                return r;
            }
            fileGet = false;
            inoGet = false;
            fileStr = "";
            inoStr = "";
        }
        pos++;
    }
    r = NOENT;
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
     lc->acquire(dir);
     if (!lockless_isdir(dir)) {
        lc->release(dir);
        return EXIST;
     }

    std::string readBuf;
    extent_protocol::status status = ec->get(dir, readBuf);
    if(status != extent_protocol::OK) {
        r = IOERR;
        lc->release(dir);
        return r;
    }

    std::string fileStr;
    std::string inoStr;
    bool fileGet = false;
    bool inoGet = false;
    uint32_t length = readBuf.length();
    //find it by pos
    uint32_t pos = 0;
    while (pos < length) {
        // get filename
        if (!fileGet) {
            if (readBuf[pos] == '/') {
                fileGet = true;
                pos++;
            } else fileStr += readBuf[pos];
        }
        // get inostr
        if (fileGet && !inoGet) {
            if (readBuf[pos] == '/')
                inoGet = true;
            else {
                if (pos == length-1) 
                    inoGet = true;
                inoStr += readBuf[pos];
            }
        }
        // add to dirent
        if (fileGet && inoGet) {
            dirent entry;
            entry.name = fileStr;
            entry.inum = n2i(inoStr);
            list.push_back(entry);
            fileGet = false;
            inoGet = false;
            fileStr = "";
            inoStr = "";
        }
        pos++;
    }

    lc->release(dir);
    return r;
}




int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
  int r = OK;

  /*
   * your lab2 code goes here.
   * note: read using ec->get().
   */
   lc->acquire(ino);

   std::string readBuf;
   extent_protocol::status status = ec->get(ino, readBuf);
   if (status != extent_protocol::OK) {
     lc->release(ino);
       return status;
   }
    printf("read: ino  =%d, size = %d, off = %d\n",ino,size,off);
    printf("read: data = %s\n",readBuf.c_str());
   if(off > readBuf.size()){
       data = "\0";
       lc->release(ino);
       return r;
   }

   if(readBuf.size() < size + off)
       data = readBuf.substr(off, readBuf.size()-off);
   else
       data = readBuf.substr(off, size);

  lc->release(ino);

  return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
  int r = OK;

  /*
   * your lab2 code goes here.
   * note: write using ec->put().
   * when off > length of original file, fill the holes with '\0'.
   */

   lc->acquire(ino);


  printf("write: ino  =%d, size = %d, off = %d, data = %s\n",ino,size,off,data);

   std::string oldData;
   extent_protocol::status status = ec->get(ino, oldData);
   if (status != extent_protocol::OK) {
     lc->release(ino);
     return status;
  }

  int bufsize = oldData.size();
  printf("filesize = %d\n",bufsize);
  if(oldData.size() < off) {
      oldData.resize(off,'\0');
      oldData.append(data, size);
  } else {
      if(oldData.size() < off + (int)size)
        oldData.resize(off + size);
      oldData.replace(off, size, std::string(data,size));
  }
  bytes_written = size;
  ec->put(ino, oldData);
  lc->release(ino);
      return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    lc->acquire(parent);
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    inum inum = 0;
    bool found = false;
    r = lockless_lookup(parent, name, found, inum);
    if (r == IOERR) {
        lc->release(parent);
        return r;
    }

    if(!found){
        r = NOENT;
        lc->release(parent);
        return r;
    }

    std::string readBuf;
    extent_protocol::status status = ec->get(parent, readBuf);
    if(status != extent_protocol::OK) {
        r = IOERR;
        return r;
    }
    ec->remove(inum);
    size_t namePos = readBuf.find(name);
    //delete the fileinfo
    readBuf.replace(namePos, strlen(name)+filename(inum).size()+2,"");
    size_t lastPos = readBuf.length()-1;
    if (readBuf[lastPos] == '/')//deal the last pos
        readBuf.replace(lastPos, 1, "");
    ec->put(parent, readBuf);
    lc->release(parent);
    return r;
}

int yfs_client::symlink(inum parent, const std::string name, const std::string link, inum &ino_out)
{
    int r = OK;
    lc->acquire(parent);

    printf("create: create %s in %d\n", name.c_str(), parent);

    bool found = false;
    inum old_ino;
    std::string buf;
    extent_protocol::status status = lockless_lookup(parent, name.c_str(), found, ino_out);
    if(found == true){
      //ino_out = old_ino;
      lc->release(parent);
      return EXIST;
    }

    // create a link file
    ec->create(extent_protocol::T_LINK, ino_out);
    ec->get(parent,buf);
    if (buf.size() > 0)
      buf=buf+"/";
    buf=buf+std::string(name) + '/' + filename(ino_out);
    ec->put(parent, buf);

    // write thing to link file
    ec->put(ino_out, link);
    lc->release(parent);
    return r;
}

int yfs_client::readlink(inum ino, std::string &link)
{
  int r = OK;
  lc->acquire(ino);
  if(ec->get(ino, link) != extent_protocol::OK) {
      r = IOERR;
  }
  lc->release(ino);
  return r;
}
