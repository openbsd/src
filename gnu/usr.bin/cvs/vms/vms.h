/* Determined from CC RTL function prototypes in online documentation */

#define mode_t unsigned int

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

int unlink(char *path);
int link(char *from, char *to);

#define stat(a, b) wrapped_stat(a, b)

#undef POSIX
