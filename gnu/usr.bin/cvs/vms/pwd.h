#ifndef _PWD_H
#define _PWD_H

#include <sys/types.h>

struct passwd {
   char  *pw_name;
   uid_t  pw_uid;  
   gid_t  pw_gid;
   char  *pw_dir;
   };

struct passwd *getpwuid(uid_t);
struct passwd *getpwnam(char *);
char *getlogin(void); 

#else
#endif /* _PWD_H */
