// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <vector>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// yfs_client::yfs_client()
// {
//     ec = new extent_client();

// }

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

// A helper function to split a string
std::vector<std::string>
yfs_client::split(const std::string &str, char delim) {
    std::vector<std::string> resVec;

    if ("" == str) {
        return resVec;
    }
    
    std::string strs = str;

    size_t pos = strs.find(delim);
    size_t size = strs.size();

    while (pos != std::string::npos) {
        std::string x = strs.substr(0,pos);
        resVec.push_back(x);
        strs = strs.substr(pos+1,size);
        pos = strs.find(delim);
    }

    return resVec;
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

bool
yfs_client::issym(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYM) {
        printf("issym: %lld is a symlink\n", inum);
        return true;
    } 
    printf("issym: %lld is not a symlink\n", inum);
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
    // get the content of inode ino
    std::string buf;
    if (ec->get(ino, buf) != extent_protocol::OK) {
        printf("error getting content of ino: return not OK\n");
        r = IOERR;
        return r;
    }
    // modify its content according to the size
    buf.resize(size, '\0');
    if (ec->put(ino, buf) != extent_protocol::OK) {
        printf("error putting content of ino: return not OK\n");
        r = IOERR;
    }

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

    /*
     * The format of the directory is like :
     * "[filename]/[inum]/[filename]/[inum]/"
     */
    
    // check if parent is a directory
    if (!isdir(parent)) {
        printf("error creating, parent is not a dir\n");
        r = IOERR;
        return r;
    }

    // check if file already exists
    bool found = false;
    inum ino = 0;
    lookup(parent, name, found, ino);
    if (found) {
        printf("error creating, already exists\n");
        r = EXIST;
        return r;
    }

    // create the file
    if (ec->create(extent_protocol::T_FILE, ino_out) != extent_protocol::OK) {
        printf("error creating file, return not OK\n");
        r = IOERR;
        return r;
    }

    // modify the parent imformation
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        printf("error reading parent, return not OK\n");
        r = IOERR;
        return r;
    } 
    buf.append(name);
    buf.append("/");
    buf.append(filename(ino_out));
    buf.append("/");
    if (ec->put(parent, buf) != extent_protocol::OK) {
        printf("error modifying parent, return not OK\n");
        r = IOERR;
    }

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
    
    // check if parent is a directory
    if (!isdir(parent)) {
        printf("error creating dir, parent is not a dir\n");
        r = IOERR;
        return r;
    }

    // check if directory already exists
    bool found = false;
    inum ino = 0;
    lookup(parent, name, found, ino);
    if (found) {
        printf("error creating dir, already exists\n");
        r = EXIST;
        return r;
    }

    // create the directory
    if (ec->create(extent_protocol::T_DIR, ino_out) != extent_protocol::OK) {
        printf("error creating dir, return not OK\n");
        r = IOERR;
        return r;
    }

    // modify the parent imformation
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        printf("error reading parent, return not OK\n");
        r = IOERR;
        return r;
    } 
    buf.append(name);
    buf.append("/");
    buf.append(filename(ino_out));
    buf.append("/");
    if (ec->put(parent, buf) != extent_protocol::OK) {
        printf("error modifying parent, return not OK\n");
        r = IOERR;
    }

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

    // check if it's a directory
    if (!isdir(parent)) {
        printf("error looking up, parent is not a dir\n");
        r = IOERR;
        return r;
    }
    
    // read the directory
    found = false;
    std::list<dirent> list;
    if ((r = readdir(parent, list)) != OK) {
        return r;
    }

    // look up the file in the directory
    std::list<dirent>::iterator it;
    for (it = list.begin(); it != list.end(); ++it) {
        dirent tmp = *it;
        if (tmp.name == name) {
            found = true;
            ino_out = tmp.inum;
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

    /*
     * The format of the directory is like :
     * "[filename]/[inum]/[filename]/[inum]/"
     */
    
    // check if it's a directory
    if (!isdir(dir)) {
        printf("error reading dir, file is not a dir\n");
        r = IOERR;
        return r;
    }

    // read the directory
    std::string buf;
    if (ec->get(dir, buf) != extent_protocol::OK) {
        printf("error reading dir, return not OK\n");
        r = IOERR;
        return r;
    } 

    // if the directory is empty
    if (buf == "") {
        return r;
    }
    
    // make the list with the string buf
    std::vector<std::string> entries = split(buf, '/');
    unsigned int size = entries.size();
    for (unsigned int i = 0; i < size; i += 2) {
        dirent tmp;
        tmp.name = entries[i];
        tmp.inum = n2i(entries[i+1]);
        list.push_back(tmp);
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

    // read from ec
    std::string buf;
    if (ec->get(ino, buf) != extent_protocol::OK) {
        printf("error getting content of ino: return not OK\n");
        r = IOERR;
        return r;
    }

    // check if size smaller than offset
    if ((int)buf.size() <= off) {
        data = "";
        return r;
    }

    data = buf.substr(off, size);
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
    
    // get the content of the file
    std::string buf;
    if (ec->get(ino, buf) != extent_protocol::OK) {
        printf("error getting content of ino: return not OK\n");
        r = IOERR;
        return r;
    }

    // if the file size is smaller than offset
    if (buf.size() < off + size) {
        buf.resize(off + size, '\0');
    }
    buf.replace(off, size, data, size);
    
    // write back
    if (ec->put(ino, buf) != extent_protocol::OK) {
        printf("error writing: return not OK\n");
        r = IOERR;
        return r;
    }

    // set bytes_written
    bytes_written = (int)buf.size() < off ? (off + size - buf.size()) : size;

    return r;
}

int
yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    
    // check if parent is a directory
    if (!isdir(parent)) {
        printf("error unlinking, parent is not a dir\n");
        r = IOERR;
        return r;
    }

    // check if file not exists
    bool found = false;
    inum ino = 0;
    lookup(parent, name, found, ino);
    if (!found) {
        printf("error unlinking, not exists\n");
        r = NOENT;
        return r;
    }

    // remove the file
    if (ec->remove(ino) != extent_protocol::OK) {
        printf("error unlinking, return not OK\n");
        r = IOERR;
        return r;
    }

    // update the parent directory content
    std::list<dirent> list;
    std::string buf;
    if ((r = readdir(parent, list)) != OK) {
        return r;
    }
    std::list<dirent>::iterator it;
    for (it = list.begin(); it != list.end(); it++) {
        if (it->name != name) {
            buf.append(it->name);
            buf.append("/");
            buf.append(filename(it->inum));
            buf.append("/");
        }
    }
    if (ec->put(parent, buf) != extent_protocol::OK) {
        printf("error modifying parent, return not OK\n");
        r = IOERR;
    }

    return r;
}

int
yfs_client::readlink(inum ino, std::string &data)
{
    printf("readlink here\n");
    int r = OK;
    // check if it's a symlink
    if (!issym(ino)){
        printf("error reading symlink, file is not a symlink\n");
        r = IOERR;
        return r;
    }

    // read from ec
    if (ec->get(ino, data) != extent_protocol::OK) {
        printf("error getting content of ino: return not OK\n");
        r = IOERR;
        return r;
    }

    return r;
}

int 
yfs_client::symlink(const char *link, inum parent, const char *name, 
        inum &ino_out)
{
    int r = OK;
    // printf("symlink here\n");
    /*
     * The format of the directory is like :
     * "[filename]/[inum]/[filename]/[inum]/"
     */
    
    // check if parent is a directory
    if (!isdir(parent)) {
        printf("error creating, parent is not a dir\n");
        r = IOERR;
        return r;
    }

    // check if file already exists
    bool found = false;
    inum ino = 0;
    lookup(parent, name, found, ino);
    if (found) {
        printf("error creating, already exists\n");
        r = EXIST;
        return r;
    }

    // create the symlink
    if (ec->create(extent_protocol::T_SYM, ino_out) != extent_protocol::OK) {
        printf("error creating symlink, return not OK\n");
        r = IOERR;
        return r;
    }
    // printf("create success!\n");

    // write the symlink
    if (ec->put(ino_out, std::string(link)) != extent_protocol::OK) {
        printf("error writing symlink, return not OK\n");
        r = IOERR;
        return r;
    }
    // printf("write success!\n");
    // modify the parent imformation
    std::string buf;
    if (ec->get(parent, buf) != extent_protocol::OK) {
        printf("error reading parent, return not OK\n");
        r = IOERR;
        return r;
    } 
    buf.append(name);
    buf.append("/");
    buf.append(filename(ino_out));
    buf.append("/");
    if (ec->put(parent, buf) != extent_protocol::OK) {
        printf("error modifying parent, return not OK\n");
        r = IOERR;
    }
    // printf("symlink here again\n");

    return r;
}
