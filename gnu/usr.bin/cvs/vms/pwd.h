#ifndef _PWD_H
#define _PWD_H

#define uid_t unsigned int
#define gid_t unsigned int
#define pid_t int

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
