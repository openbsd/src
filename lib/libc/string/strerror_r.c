/* $OpenBSD: strerror_r.c,v 1.2 2004/05/03 05:07:34 espie Exp $ */
/* Public Domain <marc@snafu.org> */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: strerror_r.c,v 1.2 2004/05/03 05:07:34 espie Exp $";
#endif /* LIBC_SCCS and not lint */

#ifdef NLS
#define catclose	_catclose
#define catgets		_catgets
#define catopen		_catopen
#include <nl_types.h>
#endif

#define sys_errlist	_sys_errlist
#define sys_nerr	_sys_nerr

#include <errno.h>
#include <limits.h>
#include <string.h>

static size_t
__digits10(unsigned int num)
{
	size_t i = 0;

	do {
		num /= 10;
		i++;
	} while (num != 0);

	return i;
}

static void
__itoa(int num, char *buffer, size_t start, size_t end)
{
	size_t pos;
	unsigned int a;
	int neg;

	if (num < 0) {
		a = -num;
		neg = 1;
	}
	else {
		a = num;
		neg = 0;
	}

	pos = start + __digits10(a);
	if (neg)
	    pos++;

	if (pos < end)
		buffer[pos] = '\0';
	else {
		if (end)
			buffer[--end] = '\0'; /* XXX */
	}
	pos--;
	do {
		
		if (pos < end)
			buffer[pos] = (a % 10) + '0';
		pos--;
		a /= 10;
	} while (a != 0);
	if (neg)
		if (pos < end)
			buffer[pos] = '-';
}


#define	UPREFIX	"Unknown error: "

int
strerror_r(int errnum, char *strerrbuf, size_t buflen)
{
	int save_errno;
	int ret_errno;
	size_t len;
#ifdef NLS
	nl_catd catd;
#endif

	save_errno = errno;
	ret_errno = 0;

#ifdef NLS
	catd = catopen("libc", 0);
#endif

	if (errnum >= 0 && errnum < sys_nerr) {
#ifdef NLS
		len = strlcpy(strerrbuf, catgets(catd, 1, errnum,
		    (char *)sys_errlist[errnum]), buflen);
#else
		len = strlcpy(strerrbuf, sys_errlist[errnum], buflen);
#endif
		if (len >= buflen)
			ret_errno = ERANGE;
	} else {
#ifdef NLS
		len = strlcpy(strerrbuf, catgets(catd, 1, 0xffff, UPREFIX), 
		    buflen);
#else
		len = strlcpy(strerrbuf, UPREFIX, buflen);
#endif
		__itoa(errnum, strerrbuf, len, buflen);
		ret_errno = EINVAL;
	}

#ifdef NLS
	catclose(catd);
#endif

	errno = ret_errno ? ret_errno : save_errno;
	return (ret_errno);
}
