#ifndef _LINUX_STRING_H_
#define _LINUX_STRING_H_

#include <sys/types.h>	/* for size_t */

#ifndef NULL
#define NULL ((void *) 0)
#endif

extern inline char * _dl_strcpy(char * dest,const char *src);
extern inline char * _dl_strncpy(char * dest,const char *src,size_t count);
extern inline char * _dl_strcat(char * dest,const char * src);
extern inline char * _dl_strncat(char * dest,const char * src,size_t count);
extern inline int _dl_strcmp(const char * cs,const char * ct);
extern inline int _dl_strncmp(const char * cs,const char * ct,size_t count);
extern inline char * _dl_strchr(const char * s,char c);
extern inline char * _dl_strrchr(const char * s,char c);
extern inline size_t _dl_strspn(const char * cs, const char * ct);
extern inline size_t _dl_strcspn(const char * cs, const char * ct);
extern inline char * _dl_strpbrk(const char * cs,const char * ct);
extern inline char * _dl_strstr(const char * cs,const char * ct);
extern inline size_t _dl_strlen(const char * s);
extern inline char * _dl_strtok(char * s,const char * ct);
extern inline void * _dl_memcpy(void * to, const void * from, size_t n);
extern inline void * _dl_memmove(void * dest,const void * src, size_t n);
extern inline int _dl_memcmp(const void * cs,const void * ct,size_t count);
extern inline void * _dl_memchr(const void * cs,char c,size_t count);
extern inline void * _dl_memset(void * s,char c,size_t count);

/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *		Copyright (C) 1991, 1992 Linus Torvalds
 */
 
extern inline char * _dl_strcpy(char * dest,const char *src)
{
__asm__("cld\n"
	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	: /* no output */
	:"S" (src),"D" (dest):"si","di","ax","memory");
return dest;
}

extern inline char * _dl_strncpy(char * dest,const char *src,size_t count)
{
__asm__("cld\n"
	"1:\tdecl %2\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"rep\n\t"
	"stosb\n"
	"2:"
	: /* no output */
	:"S" (src),"D" (dest),"c" (count):"si","di","ax","cx","memory");
return dest;
}

extern inline char * _dl_strcat(char * dest,const char * src)
{
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"decl %1\n"
	"1:\tlodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	: /* no output */
	:"S" (src),"D" (dest),"a" (0),"c" (0xffffffff):"si","di","ax","cx");
return dest;
}

extern inline char * _dl_strncat(char * dest,const char * src,size_t count)
{
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"decl %1\n\t"
	"movl %4,%3\n"
	"1:\tdecl %3\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n"
	"2:\txorl %2,%2\n\t"
	"stosb"
	: /* no output */
	:"S" (src),"D" (dest),"a" (0),"c" (0xffffffff),"g" (count)
	:"si","di","ax","cx","memory");
return dest;
}

extern inline int _dl_strcmp(const char * cs,const char * ct)
{
register int __res;
__asm__("cld\n"
	"5:\tlodsb\n\t"
	"scasb\n\t"
	"jne 6f\n\t"
	"testb %%al,%%al\n\t"
	"jne 5b\n\t"
	"xorl %%eax,%%eax\n\t"
	"jmp 7f\n"
	"6:\tmovl $1,%%eax\n\t"
	"jb 7f\n\t"
	"negl %%eax\n"
	"7:"
	:"=a" (__res):"D" (cs),"S" (ct):"si","di");
return __res;
}

extern inline int _dl_strncmp(const char * cs,const char * ct,size_t count)
{
register int __res;
__asm__("cld\n"
	"1:\tdecl %3\n\t"
	"js 2f\n\t"
	"lodsb\n\t"
	"scasb\n\t"
	"jne 3f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n"
	"2:\txorl %%eax,%%eax\n\t"
	"jmp 4f\n"
	"3:\tmovl $1,%%eax\n\t"
	"jb 4f\n\t"
	"negl %%eax\n"
	"4:"
	:"=a" (__res):"D" (cs),"S" (ct),"c" (count):"si","di","cx");
return __res;
}

extern inline char * _dl_strchr(const char * s,char c)
{
register char * __res;
__asm__("cld\n\t"
	"movb %%al,%%ah\n"
	"1:\tlodsb\n\t"
	"cmpb %%ah,%%al\n\t"
	"je 2f\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"movl $1,%1\n"
	"2:\tmovl %1,%0\n\t"
	"decl %0"
	:"=a" (__res):"S" (s),"0" (c):"si");
return __res;
}

extern inline char * _dl_strrchr(const char * s,char c)
{
register char * __res;
__asm__("cld\n\t"
	"movb %%al,%%ah\n"
	"1:\tlodsb\n\t"
	"cmpb %%ah,%%al\n\t"
	"jne 2f\n\t"
	"movl %%esi,%0\n\t"
	"decl %0\n"
	"2:\ttestb %%al,%%al\n\t"
	"jne 1b"
	:"=d" (__res):"0" (0),"S" (s),"a" (c):"ax","si");
return __res;
}

extern inline size_t _dl_strspn(const char * cs, const char * ct)
{
register char * __res;
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 1b\n"
	"2:\tdecl %0"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res-cs;
}

extern inline size_t _dl_strcspn(const char * cs, const char * ct)
{
register char * __res;
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n"
	"2:\tdecl %0"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res-cs;
}

extern inline char * _dl_strpbrk(const char * cs,const char * ct)
{
register char * __res;
__asm__("cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"movl %%ecx,%%edx\n"
	"1:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 2f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 1b\n\t"
	"decl %0\n\t"
	"jmp 3f\n"
	"2:\txorl %0,%0\n"
	"3:"
	:"=S" (__res):"a" (0),"c" (0xffffffff),"0" (cs),"g" (ct)
	:"ax","cx","dx","di");
return __res;
}

extern inline char * _dl_strstr(const char * cs,const char * ct)
{
register char * __res;
__asm__("cld\n\t" \
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"	/* NOTE! This also sets Z if searchstring='' */
	"movl %%ecx,%%edx\n"
	"1:\tmovl %4,%%edi\n\t"
	"movl %%esi,%%eax\n\t"
	"movl %%edx,%%ecx\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 2f\n\t"		/* also works for empty string, see above */
	"xchgl %%eax,%%esi\n\t"
	"incl %%esi\n\t"
	"cmpb $0,-1(%%eax)\n\t"
	"jne 1b\n\t"
	"xorl %%eax,%%eax\n\t"
	"2:"
	:"=a" (__res):"0" (0),"c" (0xffffffff),"S" (cs),"g" (ct)
	:"cx","dx","di","si");
return __res;
}

extern inline size_t _dl_strlen(const char * s)
{
register int __res;
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %0\n\t"
	"decl %0"
	:"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff):"di");
return __res;
}

extern char * ___strtok;

extern inline char * _dl_strtok(char * s,const char * ct)
{
register char * __res;
__asm__("testl %1,%1\n\t"
	"jne 1f\n\t"
	"testl %0,%0\n\t"
	"je 8f\n\t"
	"movl %0,%1\n"
	"1:\txorl %0,%0\n\t"
	"movl $-1,%%ecx\n\t"
	"xorl %%eax,%%eax\n\t"
	"cld\n\t"
	"movl %4,%%edi\n\t"
	"repne\n\t"
	"scasb\n\t"
	"notl %%ecx\n\t"
	"decl %%ecx\n\t"
	"je 7f\n\t"			/* empty delimeter-string */
	"movl %%ecx,%%edx\n"
	"2:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 7f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 2b\n\t"
	"decl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"je 7f\n\t"
	"movl %1,%0\n"
	"3:\tlodsb\n\t"
	"testb %%al,%%al\n\t"
	"je 5f\n\t"
	"movl %4,%%edi\n\t"
	"movl %%edx,%%ecx\n\t"
	"repne\n\t"
	"scasb\n\t"
	"jne 3b\n\t"
	"decl %1\n\t"
	"cmpb $0,(%1)\n\t"
	"je 5f\n\t"
	"movb $0,(%1)\n\t"
	"incl %1\n\t"
	"jmp 6f\n"
	"5:\txorl %1,%1\n"
	"6:\tcmpb $0,(%0)\n\t"
	"jne 7f\n\t"
	"xorl %0,%0\n"
	"7:\ttestl %0,%0\n\t"
	"jne 8f\n\t"
	"movl %0,%1\n"
	"8:"
	:"=b" (__res),"=S" (___strtok)
	:"0" (___strtok),"1" (s),"g" (ct)
	:"ax","cx","dx","di","memory");
return __res;
}

extern inline void * _dl_memcpy(void * to, const void * from, size_t n)
{
__asm__("cld\n\t"
	"movl %%edx, %%ecx\n\t"
	"shrl $2,%%ecx\n\t"
	"rep ; movsl\n\t"
	"testb $1,%%dl\n\t"
	"je 1f\n\t"
	"movsb\n"
	"1:\ttestb $2,%%dl\n\t"
	"je 2f\n\t"
	"movsw\n"
	"2:\n"
	: /* no output */
	:"d" (n),"D" ((long) to),"S" ((long) from)
	: "cx","di","si","memory");
return (to);
}

extern inline void * _dl_memmove(void * dest,const void * src, size_t n)
{
if (dest<src)
__asm__("cld\n\t"
	"rep\n\t"
	"movsb"
	: /* no output */
	:"c" (n),"S" (src),"D" (dest)
	:"cx","si","di");
else
__asm__("std\n\t"
	"rep\n\t"
	"movsb\n\t"
	"cld"
	: /* no output */
	:"c" (n),
	 "S" (n-1+(const char *)src),
	 "D" (n-1+(char *)dest)
	:"cx","si","di","memory");
return dest;
}

extern inline int _dl_memcmp(const void * cs,const void * ct,size_t count)
{
register int __res;
__asm__("cld\n\t"
	"repe\n\t"
	"cmpsb\n\t"
	"je 1f\n\t"
	"movl $1,%%eax\n\t"
	"jb 1f\n\t"
	"negl %%eax\n"
	"1:"
	:"=a" (__res):"0" (0),"D" (cs),"S" (ct),"c" (count)
	:"si","di","cx");
return __res;
}

extern inline void * _dl_memchr(const void * cs,char c,size_t count)
{
register void * __res;
if (!count)
	return NULL;
__asm__("cld\n\t"
	"repne\n\t"
	"scasb\n\t"
	"je 1f\n\t"
	"movl $1,%0\n"
	"1:\tdecl %0"
	:"=D" (__res):"a" (c),"D" (cs),"c" (count)
	:"cx");
return __res;
}

extern inline void * _dl_memset(void * s,char c,size_t count)
{
__asm__("cld\n\t"
	"rep\n\t"
	"stosb"
	: /* no output */
	:"a" (c),"D" (s),"c" (count)
	:"cx","di","memory");
return s;
}

#endif
