#ifdef __GNUC__
#include "../unixish.h"
#else
#include "unixish.h"
#endif

/* VOS does not support SA_SIGINFO, so undefine the macro.  This
   is a work-around for posix-1302.  */
#undef SA_SIGINFO

/* The following declaration is an avoidance for posix-950. */
extern int ioctl (int fd, int request, ...);

/* Specify a prototype for truncate() since we are supplying one. */
extern int truncate (const char *path, off_t len);

/* Specify a prototype for socketpair() since we supplying one. */
extern int socketpair (int family, int type, int protocol, int fd[2]);
