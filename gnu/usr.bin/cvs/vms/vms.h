/* Determined from CC RTL function prototypes in online documentation */

#if !defined(__VMS_VER)
#define mode_t unsigned int
#elif __VMS_VER < 70000000
#define mode_t unsigned int
#endif

#define fork(x) vfork(x)

#include <sys/types.h>
#include <unixio.h>
#include <unixlib.h>
#include <stdlib.h>
#include <processes.h>
#include <socket.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

extern int fnmatch(char *pattern, char *string, int options);

/* With the define of HAVE_NDIR_H in config.h, lib/system.h and
   diff/system.h should include ndir.h for us.  But I'm too lazy to
   track down and make _sure_ all bases are covered, so I'm leaving in
   this include for now.  */
#include "ndir.h"

#include "pwd.h"
#include "pipe.h"

#if !defined(__VMS_VER)
int vms_unlink(char *path);
#elif __VMS_VER < 70000000
int vms_unlink(char *path);
#else
int vms_unlink(char const*path);
#endif
int link(char *from, char *to);

#define stat(a, b) wrapped_stat(a, b)
#define lstat stat

#undef POSIX
