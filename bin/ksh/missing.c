/*	$OpenBSD: missing.c,v 1.1.1.1 1996/08/14 06:19:11 downsj Exp $	*/

/*
 * Routines which may be missing on some machines
 */

#include "sh.h"
#include "ksh_stat.h"
#include "ksh_dir.h"


#ifndef HAVE_MEMSET
void *
memset(d, c, n)
	void *d;
	int c;
	size_t n;
{
	unsigned char *p = (unsigned char *) d;

	/* Not amazingly fast.. */
	for (; n > 0; --n)
		*p++ = c;
	return d;
}
#endif /* !HAVE_MEMSET */

#if !defined(HAVE_MEMMOVE) && !defined(HAVE_BCOPY)
void *
memmove(d, s, n)
	void *d;
	const void *s;
	size_t n;
{
	char *dp = (char *) d, *sp = (char *) s;

	if (n <= 0)
		;
	else if (dp < sp)
		do
			*dp++ = *sp++;
		while (--n > 0);
	else if (dp > sp) {
		dp += n;
		sp += n;
		do
			*--dp = *--sp;
		while (--n > 0);
	}
	return d;
}
#endif /* !HAVE_MEMMOVE && !HAVE_BCOPY */


#ifndef HAVE_STRCASECMP
/*
 * Case insensitive string compare routines, same semantics as str[n]cmp()
 * (assumes ASCII..).
 */
static const char ichars[256] = {
		   0,  0x1,  0x2,  0x3,  0x4,  0x5,  0x6,  0x7,
		 0x8,  0x9,  0xa,  0xb,  0xc,  0xd,  0xe,  0xf,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
		0x40,  'a',  'b',  'c',  'd',  'e',  'f',  'g',
		 'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
		 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
		 'x',  'y',  'z', 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
		0x60,  'a',  'b',  'c',  'd',  'e',  'f',  'g',
		 'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
		 'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
		 'x',  'y',  'z', 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
		0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
		0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
		0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
		0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
		0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
		0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
		0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
	};

int
strcasecmp(s1, s2)
	const char *s1;
	const char *s2;
{
	const unsigned char *us1 = (const unsigned char *) s1;
	const unsigned char *us2 = (const unsigned char *) s2;

	while (ichars[*us1] == ichars[*us2++])
		if (!*us1++)
			return 0;

	return ichars[*us1] - ichars[*--us2];
}

int
strncasecmp(s1, s2, n)
	const char *s1;
	const char *s2;
	int n;
{
	const unsigned char *us1 = (const unsigned char *) s1;
	const unsigned char *us2 = (const unsigned char *) s2;

	while (--n >= 0 && ichars[*us1] == ichars[*us2++])
		if (!*us1++)
			return 0;

	return n < 0 ? 0 : ichars[*us1] - ichars[*--us2];
}
#endif /* HAVE_STRCASECMP */


#ifndef HAVE_STRSTR
char *
strstr(s, p)
	const char *s;
	const char *p;
{
	int len;

	if (s && p)
		for (len = strlen(p); *s; s++)
			if (*s == *p && strncmp(s, p, len) == 0)
				return (char *) s;

	return 0;
}
#endif /* HAVE_STRSTR */


#ifndef HAVE_STRERROR
char *
strerror(err)
	int err;
{
	static char	buf[64];
# ifdef HAVE_SYS_ERRLIST
#  ifndef SYS_ERRLIST_DECLARED
	extern int	sys_nerr;
	extern char	*sys_errlist[];
#  endif
	char		*p;

	if (err < 0 || err >= sys_nerr)
		shf_snprintf(p = buf, sizeof(buf), "Unknown system error %d",
			err);
	else
		p = sys_errlist[err];
	return p;
# else /* HAVE_SYS_ERRLIST */
	switch (err) {
	  case EINVAL:
		return "Invalid argument";
	  case EACCES:
		return "Permission denied";
	  case ESRCH:
		return "No such process";
	  case EPERM:
		return "Not owner";
	  case ENOENT:
		return "No such file or directory";
	  case ENOTDIR:
		return "Not a directory";
	  case ENOEXEC:
		return "Exec format error";
	  case ENOMEM:
		return "Not enough memory";
	  case E2BIG:
		return "Argument list too long";
	  default:
		shf_snprintf(buf, sizeof(buf), "Unknown system error %d", err);
		return buf;
	}
# endif /* HAVE_SYS_ERRLIST */
}
#endif /* !HAVE_STRERROR */


#ifdef TIMES_BROKEN
# include "ksh_time.h"
# include "ksh_times.h"
# ifdef HAVE_GETRUSAGE
#  include <sys/resource.h>
# else /* HAVE_GETRUSAGE */
#  include <sys/timeb.h>
# endif /* HAVE_GETRUSAGE */

clock_t
ksh_times(tms)
	struct tms *tms;
{
	static clock_t base_sec;
	clock_t rv;

# ifdef HAVE_GETRUSAGE
	{
		struct timeval tv;
		struct rusage ru;

		getrusage(RUSAGE_SELF, &ru);
		tms->tms_utime = ru.ru_utime.tv_sec * CLK_TCK
			+ ru.ru_utime.tv_usec * CLK_TCK / 1000000;
		tms->tms_stime = ru.ru_stime.tv_sec * CLK_TCK
			+ ru.ru_stime.tv_usec * CLK_TCK / 1000000;

		getrusage(RUSAGE_CHILDREN, &ru);
		tms->tms_cutime = ru.ru_utime.tv_sec * CLK_TCK
			+ ru.ru_utime.tv_usec * CLK_TCK / 1000000;
		tms->tms_cstime = ru.ru_stime.tv_sec * CLK_TCK
			+ ru.ru_stime.tv_usec * CLK_TCK / 1000000;

		gettimeofday(&tv, (struct timezone *) 0);
		if (base_sec == 0)
			base_sec = tv.tv_sec;
		rv = (tv.tv_sec - base_sec) * CLK_TCK;
		rv += tv.tv_usec * CLK_TCK / 1000000;
	}
# else /* HAVE_GETRUSAGE */
	/* Assume times() available, but always returns 0
	 * (also assumes ftime() available)
	 */
	{
		struct timeb tb;

		if (times(tms) == (clock_t) -1)
			return (clock_t) -1;
		ftime(&tb);
		if (base_sec == 0)
			base_sec = tb.time;
		rv = (tb.time - base_sec) * CLK_TCK;
		rv += tb.millitm * CLK_TCK / 1000;
	}
# endif /* HAVE_GETRUSAGE */
	return rv;
}
#endif /* TIMES_BROKEN */

#ifdef OPENDIR_DOES_NONDIR
/* Prevent opendir() from attempting to open non-directories.  Such
 * behavior can cause problems if it attempts to open special devices...
 */
DIR *
ksh_opendir(d)
	const char *d;
{
	struct stat statb;

	if (stat(d, &statb) != 0)
		return (DIR *) 0;
	if (!S_ISDIR(statb.st_mode)) {
		errno = ENOTDIR;
		return (DIR *) 0;
	}
	return opendir(d);
}
#endif /* OPENDIR_DOES_NONDIR */
