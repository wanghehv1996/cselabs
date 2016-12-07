// yfs client.  implements FS operations using extent and lock server

#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  nowv=0;
  recoveryflag=false;
  ec->reset(0);
  lc->acquire(1);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
  lc->release(1);
}


yfs_client::inum
yfs_client::n2i(std::string n){
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum){
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
   log_setattr(ino,size);

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
    //inum old_ino;
    std::string buf;
    //extent_protocol::status status = lockless_lookup(parent, name, found, ino_out);
    if(found == true){
      //ino_out = old_ino;
      lc->release(parent);
      return EXIST;
      //return IOERR;
    }
    log_create(parent,name,mode);

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

    //extent_protocol::status status = lockless_lookup(parent, name, found, old_ino);
    if(found == true){
      //ino_out = old_ino;
      lc->release(parent);
      return EXIST;
      //return IOERR;
    }

    log_mkdir(parent,name,mode);
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
  log_write(ino,size,off,data);

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
    log_unlink(parent,name);
    
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
    
    printf("symlink: symlink %s in %d\n", name.c_str(), parent);

    bool found = false;
    inum old_ino;
    std::string buf;
    extent_protocol::status status = lockless_lookup(parent, name.c_str(), found, ino_out);
    if(found == true){
      //ino_out = old_ino;
      lc->release(parent);
      return EXIST;
    }
    log_symlink(link.c_str(),parent,name.c_str());

    // create a link file
    ec->create(extent_protocol::T_LINK, ino_out);
    ec->get(parent,buf);
    if (buf.size() > 0)
      buf=buf+"/";
    buf=buf+string(name) + '/' + filename(ino_out);
    ec->put(parent, buf);

    // write thing to link file
    ec->put(ino_out, link);
    lc->release(parent);
    return r;
}

int yfs_client::readlink(inum ino, string &link)
{
  int r = OK;
  lc->acquire(ino);
  if(ec->get(ino, link) != extent_protocol::OK) {
      r = IOERR;
  }
  lc->release(ino);
  return r;
}

void yfs_client::log_create(inum parent, const char *name, mode_t mode){
	if(recoveryflag)
		return;
	ofstream ofs;
	ofs.open("log",ios::app);
	if(!ofs){
		printf("openerr\n");
		return;
	}
	unsigned int length;
	length=sizeof(name);
	string logcontent = "create "+filename(parent)+' '+name+' '+filename(mode)+'\n';
	ofs<<logcontent;
	ofs.close();
}


void yfs_client::log_mkdir(inum parent, const char *name, mode_t mode){
	if(recoveryflag)
		return;
	ofstream ofs("log",ios::app);
	if(!ofs){
		printf("openerr\n");
		return;
	}
	unsigned int length;
	string logcontent;
	length=strlen(name);
	logcontent = "mkdir "+filename(parent)+' '+name+' '+filename(mode)+'\n';
	ofs<<logcontent;
	ofs.close();

}

void yfs_client::log_setattr(inum ino,int size){
	if(recoveryflag)
		return;
	ofstream ofs("log",ios::app);
	if(!ofs){
		printf("openerr\n");
		return;
	}
	string logcontent= "setattr "+filename(ino)+' '+filename(size)+"\n";
	ofs<<logcontent;
	ofs.close();
}

void yfs_client::log_write(inum ino, size_t size, off_t off, const char *data){
	if(recoveryflag)
		return;
	ofstream ofs("log",ios::app);
	if(!ofs){
		printf("openerr\n");
		return;
	}
	string logcontent="write "+filename(ino)+' '+filename(size)+' '+filename(off)+' ';
	for(size_t i=0;i<size;i++)
		logcontent+=data[i];
	logcontent+='\n';
	ofs<<logcontent;
	ofs.close();

}

void yfs_client::log_symlink(const char*link,inum parent,const char*name){
	if(recoveryflag)
		return;
	ofstream ofs("log",ios::app);
	if(!ofs){
		printf("openerr\n");
		return;
	}
	unsigned int length1,length2;
	string logcontent;
	//length1=sizeof(link);
	//length2=strlen(name);
	//logcontent=logcontent + "symlink "+filename(length1)+' ';
	//for(size_t i=0;i<length1;i++)
	//	logcontent+=link[i];
	//logcontent=logcontent +filename(parent)+' '+filename(length2)+' ';
	//for(size_t i=0;i<length2;i++)
	//	logcontent+=name[i];
	//logcontent+='\n';
	
	logcontent="symlink "+string(link)+' '+filename(parent)+' '+name+'\n';
	ofs<<logcontent;
	ofs.close();

}

void yfs_client::log_unlink(inum parent,const char* name){
	if(recoveryflag)
		return;
	ofstream ofs("log",ios::app);
	if(!ofs){
		printf("openerr\n");
		return;
	}
	string logcontent;
	//unsigned int length;
	//length=sizeof(name);
	//logcontent=logcontent + "unlink "+filename(parent)+' '+filename(length)+' ';
	//for(size_t i=0;i<length;i++)
	//	logcontent+=name[i];
	//logcontent+='\n';
	logcontent="unlink "+filename(parent)+' '+name+'\n';
	ofs<<logcontent;
}

void yfs_client::commit(){//just write version
	if(recoveryflag)
		return;
	ofstream ofs("log",ios::app);
	if(!ofs){
		printf("openerr\n");
		return;
	}
	string logcontent="version "+filename(nowv)+'\n';
	ofs<<logcontent;
	nowv++;
}


void yfs_client::recovery(unsigned int pos){
	ifstream ifs;
	ifs.open("log");
	string logcontent;
	recoveryflag=true;
	if(!ifs){
		recoveryflag=false;
		return;
	}
	ec->reset(0);
	unsigned int line=0;
	printf("recovery at pos %d\n",pos);
	while(line!=pos && !ifs.eof()){
		string buf;
		ifs>>buf;
		

		if(buf=="create"){
			inum parent,ino_out;
			size_t size;
			int mode;
			string name;
			//ifs>>parent>>size>>name>>mode;
			ifs>>parent>>name>>mode;
			printf("redo create %d %s %d\n",parent,name.c_str(),ino_out);
			create(parent,name.c_str(),mode,ino_out);
			printf("redo create %d %s %d\n",parent,name.c_str(),ino_out);
			logcontent=logcontent + "create "+filename(parent)+' '+filename(size)+' '+name+' '+filename(mode)+'\n';
		}else	if(buf=="mkdir"){
			inum parent,ino_out;
			size_t size;
			int mode;
			string name;
			//ifs>>parent>>size>>name>>mode;
			ifs>>parent>>name>>mode;
			printf("redo mkdir %d %s\n",parent,name.c_str());
			mkdir(parent,name.c_str(),mode,ino_out);
			printf("redo mkdir %d %s\n",parent,name.c_str());
			logcontent=logcontent +"mkdir "+filename(parent)+' '+filename(size)+' '+name+' '+filename(mode)+'\n';
		}	else if(buf=="setattr"){
			inum inode;
			int size;
			ifs>>inode>>size;
			printf("setattr %d %d\n",inode,size);
			setattr(inode,size);
			printf("setattr %d %d\n",inode,size);
			logcontent=logcontent + "setattr "+filename(inode)+' '+filename(size)+"\n";
		}
		else if(buf=="write"){
			inum inode;
			size_t size;
			off_t off;
			size_t size_out=0;
			ifs>>inode>>size>>off;
			char ch;
			string data;
			ifs.get(ch);
			for(size_t i=0;i<size;i++){
				ifs.get(ch);
				data+=ch;
			}			
			printf("redo write %d %d %d %d\n",inode,size,off,size_out);
			write(inode,size,off,data.c_str(),size_out);
			printf("redo write %d %d %d %d\n",inode,size,off,size_out);
			logcontent=logcontent + "write "+filename(inode)+' '+filename(size)+' '+filename(off)+' ';
			for(size_t i=0;i<size;i++)
				logcontent+=data[i];
			logcontent+='\n';
		}	else if(buf=="unlink"){
			inum parent;
			size_t size;
			string name;
			char ch;
			//ifs>>parent>>size;
			//ifs.get(ch);
			//for(size_t i=0;i<size;i++){
			//	ifs.get(ch);
			//	name+=ch;
			//}
			ifs>>parent>>name;
			printf("redo unlink %d %s\n",parent,name.c_str());
			unlink(parent,name.c_str());
			printf("redo unlink %d %s\n",parent,name.c_str());
			logcontent=logcontent + "unlink "+filename(parent)+' '+filename(size)+' ';
			for(size_t i=0;i<size;i++)
				logcontent+=name[i];
			logcontent+='\n';
		} else if(buf=="symlink"){
			inum parent,ino_out;
			size_t size1,size2;
			string link,name;
			char ch;
			//ifs>>size1;
			//for(size_t i=0;i<size1;i++){
			//	ifs.get(ch);
			//	link+=ch;
			//}
			//ifs.get(ch);
			//ifs>>parent;
			//ifs>>size2;
			//for(size_t i=0;i<size2;i++){
			//	ifs.get(ch);
			//	name+=ch;
			//}
			ifs>>link>>parent>>name;
			printf("redo symlink %s %s %d\n",name.c_str(),link.c_str(),ino_out);
			symlink(parent,name.c_str(),link.c_str(),ino_out);
			printf("redo symlink %s %s %d\n",name.c_str(),link.c_str(),ino_out);
			//logcontent=logcontent + "symlink "+filename(size1)+' ';
			//for(size_t i=0;i<size1;i++)
			//	logcontent+=link[i];
			//logcontent=logcontent +filename(parent)+' '+filename(size2)+' ';
			//for(size_t i=0;i<size2;i++)
			//	logcontent+=name[i];
			//logcontent+='\n';
		}else	if(buf=="version"){
			ifs>>nowv;
			logcontent=logcontent + "version "+filename(nowv)+'\n';
			//ofstream ofs("log2",ios::app);
			//ofs<<logcontent;//for debug
			logcontent="";
		}
		line++;
		printf("recovery l=%d\n",line);
	}
	recoveryflag=false;
	//ofstream ofs("log2",std::ios::app);
	//ofs<<endl<<endl;//for debug
	
}


void yfs_client::preversion(){//find the versionline and recovery
	recoveryflag=true;
	if(nowv==0)
		return;
	ifstream ifs;
	ifs.open("log");
	if(!ifs){
		printf("open err\n");
		return;
	}
	printf("pv %d\n",nowv);
	unsigned int pos=0;
	unsigned int line=0;
	while(!ifs.eof()){
		string label;
		string buf;
		line++;
		ifs>>label;
		if(label=="setattr"||label=="create"||label=="unlink"||label=="symlink"||label=="mkdir")
			getline(ifs,buf);
		if(label=="version"){
			unsigned int version;
			ifs>>version;
			if(version==nowv-1){
				pos=line;
			}
		}
		if(label=="write"){
			inum inode;
			size_t size;
			off_t off;
			ifs>>inode>>size>>off;
			char ch;
			string data;
			ifs.get(ch);
			for(size_t i=0;i<size;i++){
				ifs.get(ch);
				data+=ch;
			}
		}
	}
	if(pos>0)
		recovery(pos);
}

void yfs_client::nextversion(){//find the versionline and recovery
	recoveryflag=true;
	ifstream ifs;
	ifs.open("log");
	if(!ifs){
		printf("open err\n");
		return;
	}
	unsigned int pos=0;
	unsigned int line=0;
	printf("nvdebug = %d\n",nowv);
	string label;
	while(!ifs.eof()){
		
		string buf;
		line++;
		ifs>>label;
		if(label=="setattr"||label=="create"||label=="unlink"||label=="symlink"||label=="mkdir")
			getline(ifs,buf);
		if(label=="version"){
			unsigned int version;
			ifs>>version;
			if(version==nowv+1){
				pos=line;
			}//the next version
		}
		if(label=="write"){
			inum inode;
			size_t size;
			off_t off;
			size_t size_out;
			ifs>>inode>>size>>off;
			char ch;
			string data;
			ifs.get(ch);
			for(size_t i=0;i<size;i++){
				ifs.get(ch);
				data+=ch;
			}
		}
	}
	if(pos>0)
		recovery(pos);
	
}


