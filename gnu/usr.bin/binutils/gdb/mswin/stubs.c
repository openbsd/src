

#include "windefs.h"
#include <stdio.h>
#include <stdlib.h>
#include "../defs.h"
// define last to prevent windows from conflicting with bfd's boolean 

#define abort ff



void ff() {
printf("OUUCH!!!\n");
}

int fork () { abort(); return 0; }


void kill() { abort(); }

void wait() { abort(); }
int vfork() { abort(); return 0; }


void getkey() { abort(); }
int getgid() { return 1; }
int getpagesize() { return 4096;}

char **environ;

int getuit() { return 0;}


int ScreenRows() { return 20; }
int ScreenCols() { return 80;}


/*int sys_nerr=1;*/
char *sys_siglist[] = {"OOPS"};


PTR mmalloc(PTR x,size_t y) { return malloc(y) ;}
PTR mrealloc(PTR x,PTR y,size_t z) { return realloc(y,z);}
PTR mcalloc(PTR x,size_t y,size_t z) { return calloc(y,z) ;}
void mfree(PTR x,PTR y) { free(y); }
int mmcheck(PTR x, void (*f) (void)) { return 1;}
int mmtrace(void) { return 1; }
#undef isascii
int isascii (x) { return __isascii (x);}

int getuid() { return 1;}


void ScreenGetCursor() { abort(); }
void ScreenSetCursor() { abort(); }
char *environ_vector[20];



#if 0
void c_error() { abort(); }
void c_parse() { abort(); }
#endif





void chown() {}
void pipe() {}
int sbrk() { return 0;}

#if 0	/* FIXME!  not sure if this is right?? */
void operator_chars() {}
#endif


#if 0
void core_file_matches_executable_p(x,y) { abort();}
void coffstab_build_psymtabs() { abort(); }
char *chill_demangle(const char *a) { return 0;}
void f_error () { abort(); }
void f_parse() { abort(); }

#endif

char *tilde_expand(char *n) {return strdup(n);}

int isnan() { return 0;}

int sleep(int secs) 
{
  unsigned long stop = GetTickCount() + secs * 1000;

  /* on win32s, sleep returns immediately if there's
     nothing else ready to run, so loop. */
  while (GetTickCount() < stop)
    Sleep (100);
  return 0;
}

char* strsignal(int x)
{
return 0;
}
