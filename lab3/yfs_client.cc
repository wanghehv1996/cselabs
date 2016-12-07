// yfs client.  implements FS operations using extent and lock server

#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>

yfs_client::yfs_client(std::string extent_dst)
{
    ec = new extent_client(extent_dst);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
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

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a file\n", inum);
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
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

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
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
     printf("setattr: setattr %d with size %d\n", ino, size);
     extent_protocol::attr a;
     extent_protocol::status status = ec->getattr(ino, a);
     if (status != extent_protocol::OK) {
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
     printf("create: create %s in %d\n", name, parent);
     bool found = false;
     inum old_ino;
     std::string buf;

     extent_protocol::status status = lookup(parent, name, found, old_ino);
     if(status != extent_protocol::OK){
       return status;
     }
     if(found == true){
       //ino_out = old_ino;
       return EXIST;
     }

     ec->create(extent_protocol::T_FILE, ino_out);
     ec->get(parent, buf);
     buf=buf +std::string(name) +"/"+ filename(ino_out)+"/";
     printf("parent = %d,buf = %s\n",parent,buf.c_str());
     ec->put(parent, buf);
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
     printf("mkdir: mkdir %s in %d\n", name, parent);
     bool found = false;
     inum old_ino;
     std::string buf;

     extent_protocol::status status = lookup(parent, name, found, old_ino);
     if(status != extent_protocol::OK){
       return status;
     }

     if(found == true){
       //ino_out = old_ino;
       return EXIST;
       //return IOERR;
     }

     ec->create(extent_protocol::T_DIR, ino_out);
     ec->get(parent, buf);
     buf=buf+std::string(name) +"/"+ filename(ino_out)+"/";
     ec->put(parent, buf);
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
     printf("lookup: lookup %s in %d\n", name, parent);
     std::string readBuf;
     found = false;

     extent_protocol::attr attr;

     ec->getattr(parent, attr);
     if (attr.type != extent_protocol::T_DIR)
     {
         // maybe wrong -> typeerr
         return EXIST;
     }//check the type

     extent_protocol::status status = ec->get(parent, readBuf);
     if (status != extent_protocol::OK) {
         return status;
     }

     std::list<dirent> list;
     readdir(parent,list);

     for(std::list<dirent>::iterator i = list.begin();i != list.end(); i++){
       if(strcmp((i->name).c_str(),name)==0){
         found = true;
         ino_out = i->inum;
         return r;
       }
     }

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
     if (!isdir(dir)){
         return EXIST;
     }//check type

     std::string readBuf;
     std::string fileStr = "";
     std::string inoStr = "";
     list.clear();
     printf("readdir: read inum %d\n", dir);

     extent_protocol::status status = ec->get(dir, readBuf);
     if (status != extent_protocol::OK) {
         return status;
     }

     //printf("readbuf = %s\n",readBuf.c_str());

     int head=0, tail = 0;
     tail = readBuf.find('/',head);
     //printf("head=%d tail=%d\n",head,tail);
     while(tail!=std::string::npos){
       fileStr = readBuf.substr(head,tail-head);
       //printf("filename = %s\n",fileStr.c_str());
       head = tail+1;
       if(head>=readBuf.size()){
          break;
       }
       tail = readBuf.find('/',head);
       if(tail<0){
          break;
       }
       inoStr = readBuf.substr(head,tail-head);
       //printf("inum = %d\n",n2i(inoStr));
       dirent newEnt;
       newEnt.name = fileStr;
       newEnt.inum = n2i(inoStr);
       list.push_back(newEnt);
       //printf("find name = %s;inum = %d\n",fileStr.c_str(),n2i(inoStr));
       head = tail+1;
       if(head>=readBuf.size())
          break;
       tail = readBuf.find('/',head);
     }
    printf("get list size = %d\n",list.size());
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
     std::string readBuf;
     extent_protocol::status status = ec->get(ino, readBuf);
     if (status != extent_protocol::OK) {
         return status;
     }
      printf("read: ino  =%d, size = %d, off = %d\n",ino,size,off);
      printf("read: data = %s\n",readBuf.c_str());
     if(off > readBuf.size()){
         data = "\0";
         return r;
     }

     if(readBuf.size() < size + off)
         data = readBuf.substr(off, readBuf.size()-off);
     else
         data = readBuf.substr(off, size);
     //
    //  size_t actSize = off+size > data.size() ? data.size()-off : size;
    //  data = data.substr(off, actSize);

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

    printf("write: ino  =%d, size = %d, off = %d, data = %s\n",ino,size,off,data);

     std::string oldData;
     extent_protocol::status status = ec->get(ino, oldData);
     if (status != extent_protocol::OK) {
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
        return r;
}


int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;
    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
     
     if (!isdir(parent)){
         return EXIST;
     }//check type

     std::string readBuf;
     std::string writeBuf;
     std::string fileStr = "";
     std::string inoStr = "";
     printf("unlink: unlink name %s\n", name);

     extent_protocol::status status = ec->get(parent, readBuf);
     if (status != extent_protocol::OK) {
         return status;
     }

     //printf("readbuf = %s\n",readBuf.c_str());

     int head=0, tail = 0;
     bool found = false;
     tail = readBuf.find('/',head);
     //printf("head=%d tail=%d\n",head,tail);
     while(tail!=std::string::npos){
       fileStr = readBuf.substr(head,tail-head);
       printf("filename = %s\n",fileStr.c_str());
       head = tail+1;
       if(head>=readBuf.size()){
          break;
       }
       tail = readBuf.find('/',head);
       if(tail<0){
          break;
       }
       inoStr = readBuf.substr(head,tail-head);
       printf("inum = %d\n",n2i(inoStr));
       if(strcmp(name,fileStr.c_str())==0){
         ec->remove(n2i(inoStr));
         found = true;
       }else{
         writeBuf=writeBuf+fileStr+"/"+inoStr+"/";
       }
       //printf("find name = %s;inum = %d\n",fileStr.c_str(),n2i(inoStr));
       head = tail+1;
       if(head>=readBuf.size())
          break;
       tail = readBuf.find('/',head);
     }
     if (found == false)
{
    return ENOENT;
}
     ec->put(parent,writeBuf);
    return r;
}



int yfs_client::symlink(inum parent, const char* name, const char* link, inum &ino_out)
{
    int r = OK;
     printf("create: create %s in %d\n", name, parent);
     bool found = false;
     inum old_ino;
     std::string buf;

     extent_protocol::status status = lookup(parent, name, found, old_ino);
     if(status != extent_protocol::OK){
       return status;
     }
     if(found == true){
       //ino_out = old_ino;
       return EXIST;
     }

     ec->create(extent_protocol::T_LINK, ino_out);
     ec->get(parent, buf);
     buf=buf +std::string(name) +"/"+ filename(ino_out)+"/";
     printf("parent = %d,buf = %s\n",parent,buf.c_str());
     ec->put(parent, buf);
     ec->put(ino_out, std::string(link));
    return r;
}

int yfs_client::readlink(inum ino, std::string &result)
{
    int r = OK;
    if(ec->get(ino, result) != extent_protocol::OK) {
        r = IOERR;
    }
    return r;
}

