/**
 * unistd.h: Emulation of some standard UNIX APIs
 **/

#include <process.h>
#include <io.h>

/* Map the Windows equivalents to the POSIX function names */
#define open(path,flags,mode) _open(path,flags,mode)
#define close(fd) _close(fd)
#define read(fd,buf,n) _read(fd,buf,n)
#define write(fd,buf,n) _write(fd,buf,n)
#define lseek(fd,offset,whence) _lseek(fd,offset,whence)
#define fdopen(fd,mode) _fdopen(fd,mode)
#define unlink(path) _unlink(path)
#define dup(fd) _dup(fd)
#define getpid() _getpid()

/* Implement some useful sys/stat.h macros that Windows doesn't seem to have */
#define S_ISDIR(mode) ((mode) & _S_IFDIR)
#define S_ISREG(mode) ((mode) & _S_IFREG)
