
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

void doing_something(char*s) { 
    printf("doing_something: %s\n",s);
}

char *safe_strerror (int l) {
    return strerror(l);
}

char* xmalloc(long l) {
    return malloc(l);
} 

void fprintf_unfiltered (FILE *fp, const char *str, ...) {
    va_list ap;
    if (fp==0) fp=stderr;
    va_start(ap, str);
    vfprintf(fp,str,ap);
    va_end(ap);
}
void error(const char *str, ...) {
    va_list ap;
    va_start(ap, str);
    vprintf(str,ap);
    va_end(ap);
    exit(1);
}

int fputc_unfiltered (int c, FILE *fp) {
    if (fp==0) fp=stderr;
    return fputc(c,fp);
}

char * savestring (ptr, size)
         const char *ptr;
	      int size;
{
      register char *p = (char *) xmalloc (size + 1);
        memcpy (p, ptr, size);
	  p[size] = 0;
	    return p;
}
char * strsave (ptr)
         const char *ptr;
{
      return savestring (ptr, strlen (ptr));
}



void fputs_unfiltered(char* str, FILE* fp) {
    if (fp==0) fp=stderr;
fputs(str,fp);
} 

/* these don't need to do anything */
void* setlist=0;
void* showlist=0;
void perror_with_name() { }
void add_set_cmd() { }
void add_set_enum_cmd() { }
void add_show_from_set() { }

