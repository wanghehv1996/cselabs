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
#include <fstream>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h> 

bool readAccess(unsigned long long mode,unsigned short uid,unsigned short gid, unsigned short uuid){
    if(uuid==uid)
    if((mode&0400)!=0)
      return true;
    else
      return false;
    
  if( (uuid==0) || (uuid==gid))
    if((mode&040)!=0)
      return true;
    else 
      return false;

  if((mode&04)!=0)
    return true;
  return false;
}

bool writeAccess(unsigned long long mode,unsigned short uid,unsigned short gid, unsigned short uuid){

  if(uuid==uid)
    if((mode&0200)!=0)
      return true;
    else
      return false;
    
  if( (uuid==0) || (uuid==gid))
    if((mode&020)!=0)
      return true;
    else 
      return false;

  if((mode&02)!=0)
    return true;
  return false;
}

yfs_client::yfs_client()
{
  ec = NULL;
  lc = NULL;
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst, const char* cert_file)
{
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);

	unsigned short temp;
	verify(cert_file, &temp);
	uid = temp;

	if(uid == 1003)
		gid = 1004;
	else if(uid == 1004)
		gid = 1005;
	else
		gid = 0;

	lc->acquire(1);
  if (ec->put(1, "") != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
	lc->release(1);
}

int
yfs_client::verify(const char* name, unsigned short *uid)
{
  int ret = OK;
  FILE *fp=fopen(name,"rb");
  if(fp==NULL)  {
    return yfs_client::ERRPEM;
  }

  BIO *b=BIO_new_file(name,"r");
  X509 *usrCert=PEM_read_bio_X509(b,NULL,NULL,NULL);
  BIO_free(b);
  if(usrCert==NULL){
      return yfs_client::ERRPEM;
  }
  X509_NAME *issuer = X509_get_issuer_name(usrCert);//发布者
  X509_NAME *subject = X509_get_subject_name(usrCert);

  BIO *cab=BIO_new_file(CA_FILE,"r");
  X509 *caCert=PEM_read_bio_X509(cab,NULL,NULL,NULL);
  BIO_free(cab);
  if(caCert==NULL){
      return yfs_client::ERRPEM;
  }

  X509_NAME *caissuer = X509_get_issuer_name(caCert);
  if(X509_NAME_cmp(caissuer,issuer)!=0)
    return yfs_client::yfs_client::EINVA;

  ASN1_TIME *timena = X509_get_notAfter(usrCert);
  time_t tt = time(NULL);

  if(X509_cmp_time (timena, &tt)<0)
    return yfs_client::ECTIM;
    
  char* commonname;
  X509_NAME_get_text_by_NID( subject,  NID_commonName, commonname, 128);

  if(strcmp(commonname,"root")==0)
    *uid = 0;
  else if(strcmp(commonname,"user1")==0)
    *uid = 1003;
  else if(strcmp(commonname,"user2")==0)
    *uid = 1004;
  else if(strcmp(commonname,"user3")==0)
    *uid = 1005;
    

	return ret;
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
	lc->release(inum);
        printf("error getting attr\n");
        return false;
    }
	lc->release(inum);

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
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
	extent_protocol::attr attr_temp;
	lc->acquire(inum);
	ec->getattr(inum, attr_temp);
	lc->release(inum);
    	if(!isfile(inum)&&attr_temp.type==extent_protocol::T_DIR) 
		return true;
	else
		return false;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
	lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
	fin.mode = a.mode;
	fin.uid = a.uid;
	fin.gid = a.gid;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
	lc->release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
	lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;
	din.mode = a.mode;
	din.uid = a.uid;
	din.gid = a.gid;

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

// Only support set size of attr
int
yfs_client::setattr(inum ino, filestat st, unsigned long size)
{  int r = OK;
  /*
   * your lab2 code goes here.
   * note: get the content of inode ino, and modify its content
   * according to the size (<, =, or >) content length.
   */
  extent_protocol::attr a;
	ec->getattr(ino, a);
	
	if((st.uid==0)&&(st.gid==0)){//chmod
		if(uid == 0 || uid ==a.uid){
			ec->setattr(ino, (unsigned long long)st.mode, a.uid, a.gid);
			return r;
		}
		else
			return NOPEM;
	}
	
	if(st.mode==0){//chown
		if(uid == 0){
			ec->setattr(ino, a.mode, st.uid, st.gid);
			return r;
		}
		else
			return NOPEM;
	}

	std::string buf;
	lc->acquire(ino);
	ec->get(ino, buf);

	if(buf.length() > size)
		buf.erase(size);
	else if(buf.length() < size)
		buf.resize(size);
	ec->put(ino, buf);
	
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
	extent_protocol::attr a;
	ec->getattr(parent, a);



	if(!writeAccess(a.mode,a.uid,a.gid,uid))
		return NOPEM;
		

	if (ec->create(extent_protocol::T_FILE, (unsigned long long)mode, uid, gid, ino_out) != extent_protocol::OK) {
        	lc->release(parent);
        	printf("error creating file\n");
        	return IOERR;
    	}

	std::string buf;
	
	ec->get(parent, buf);
	if(buf.length() != 0)
		buf.resize(buf.length() - 2);
	
 	buf = buf + filename(ino_out) + "/" + name + "/0/";
	ec->put(parent, buf);

	lc->release(parent);
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

	extent_protocol::attr a;
	ec->getattr(parent, a);


	if(!writeAccess(a.mode,a.uid,a.gid,uid))
		return NOPEM;

	ec->create(extent_protocol::T_DIR, (unsigned long long)mode, uid, gid, ino_out);
	
	std::string buf;

		ec->get(parent, buf);
	if(buf.length() != 0)
		buf.resize(buf.length() - 2);
	
 	buf = buf + filename(ino_out) + "/" + name + "/0/";
	ec->put(parent, buf);
	ec->put(parent, buf);


	lc->release(parent);

    	return r;

}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;
    lc->acquire(parent);
    r = lookup_nl(parent, name, found, ino_out);
    lc->release(parent);
    return r;
}

int
yfs_client::lookup_nl(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */

	found = false;
	std::list<dirent> dirent_list;
	r = readdir_nl(parent, dirent_list);
	std::list<dirent>::iterator iter = dirent_list.begin();
	while(iter != dirent_list.end()){
		if(iter->name == std::string(name)){
			found = true;
			ino_out = iter->inum;
			break;
		}
		++iter;
	}

	return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
	extent_protocol::attr a;
	ec->getattr(dir, a);

	
	if(!readAccess(a.mode,a.uid,a.gid,uid))
			return NOPEM;
  int r = OK;
  lc->acquire(dir);
  r = readdir_nl(dir, list);
  lc->release(dir);
  return r;

}

int
yfs_client::readdir_nl(inum dir, std::list<dirent> &list)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

	std::string buf;
	ec->get(dir, buf);
	
	int hptr = 0, tptr = 0;
	while(true){
		tptr = buf.find('/', hptr);
		if(tptr == -1) 
			break;
		std::string ino_num_str = buf.substr(hptr, tptr - hptr);
		int ino_num = n2i(ino_num_str);
		if(ino_num == 0) 
			break;

		hptr = tptr + 1;
		tptr = buf.find('/', hptr);
		std::string filename = buf.substr(hptr, tptr - hptr);
		hptr = tptr + 1;

		dirent temp;
		temp.name = filename;
		temp.inum = ino_num;
		list.push_back(temp);
	}

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

	extent_protocol::attr a;
	ec->getattr(ino, a);
	
	if(!readAccess(a.mode,a.uid,a.gid,uid))
	return NOPEM;

	std::string buf;
	lc->acquire(ino);
	ec->get(ino, buf);
	
	if(off > (int)buf.length()) 
		data = "";
	else 
		data = buf.substr(off, size);
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
	extent_protocol::attr a;
	ec->getattr(ino, a);
	
	if(!writeAccess(a.mode,a.uid,a.gid,uid))
		return NOPEM;
	
		std::string buf;
		std::string data_str = std::string(data, size);
		lc->acquire(ino);
		ec->get(ino, buf);

		if(off > (int)buf.length()){
			buf.resize(off);
			buf += data_str;
			if(buf.size() > off+size)
				buf.erase(off+size);
		}
		else if((off+size) >= buf.length()){
			if(off < (int)buf.size())
				buf.erase(off);
			buf += data_str;
			if(buf.size() > off+size)
				buf.erase(off+size);
		}
		else
			buf.replace(off, size, data_str.substr(0,size));
	
		ec->put(ino, buf);

		std::string sdata;
    sdata.assign(data, size);

		lc->release(ino);
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

	std::list<dirent> dlist;
	bool found;
	inum ino;
	lc->acquire(parent);
	r = lookup_nl(parent, name, found, ino);
	
	std::string data;
  ec->get(ino, data);

	if(!found){
		r = NOENT; 
		lc->release(parent);
    return r;
	}
	
	lc->acquire(ino);
	ec->remove(ino);
	lc->release(ino);
	std::list<dirent>::iterator iter;
	readdir_nl(parent, dlist);
	for(iter = dlist.begin(); iter != dlist.end(); ++iter){
		if(iter->name == std::string(name)){
			dlist.erase(iter);
			break;
		}
	}
	std::string buf = "";
	for(iter = dlist.begin(); iter != dlist.end(); ++iter)
		buf = buf + filename(iter->inum) + "/" + iter->name + "/";

	buf += "0/";
	ec->put(parent, buf);
		
	lc->release(parent);
    return r;
}

int
yfs_client::symlink(const char* link, inum parent, const char* name, inum& ino_out){

	int r = OK;
	bool found;
	inum ino_num;
	lc->acquire(parent);
	lookup_nl(parent, name, found, ino_num);
	
	if(found){
		lc->release(parent);
		r = EXIST;
		return r;
	}

	ec->create(extent_protocol::T_SYMLINK, 0777, uid, gid, ino_out);

	lc->acquire(ino_out);
	
	std::string buf;
	
	ec->get(parent, buf);
	if(buf.length() != 0)
		buf.resize(buf.length() - 2);
	
 	buf = buf + filename(ino_out) + "/" + name + "/0/";
	ec->put(parent, buf);
	ec->put(ino_out, std::string(link));

	lc->release(ino_out);
	lc->release(parent);
	return r;
}

int
yfs_client::readlink(inum ino, std::string& link_str){
	int r = OK;
	lc->acquire(ino);
	ec->get(ino, link_str);
	lc->release(ino);
	return r;
}


int yfs_client::getlink(inum inum, symlinkinfo &sin){
	int r = OK;

    printf("getlink %016llx\n", inum);
    extent_protocol::attr a;
	lc->acquire(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    sin.atime = a.atime;
    sin.mtime = a.mtime;
    sin.ctime = a.ctime;
    sin.size = a.size;
    printf("getsim %016llx -> sz %llu\n", inum, sin.size);

release:
	lc->release(inum);
    return r;
}
