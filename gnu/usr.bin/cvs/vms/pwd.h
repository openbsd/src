#ifndef _PWD_H
#define _PWD_H

/* Trying to declare uid_t is a mess.  We tried #include <sys/types.h>, which
   only worked on VAX (VMS 6.2, I think), and we tried defining it here
   which only worked on alpha, I think.  In any event, the VMS C library's
   concept of uid_t is fundamentally broken anyway (getuid() returns only
   the group part of the UIC), so we are better off with higher-level
   hooks like get_homedir and SYSTEM_GETCALLER.  */

#if !defined(__VMS_VER)
#define pid_t int
#elif __VMS_VER < 70000000
#define pid_t int
#endif

struct passwd {
   char  *pw_name;
};

struct passwd *getpwuid(/* really uid_t, but see above about declaring it */);
char *getlogin(void); 

#else
#endif /* _PWD_H */
