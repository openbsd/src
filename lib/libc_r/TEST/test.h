
#ifndef _h_test_
#define _h_test_

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>

int	_thread_sys_write __P((int, const char*, size_t));
__dead void _thread_sys__exit __P((int)) __attribute__((noreturn));

static __dead void __vpanic __P((const char *, const char *, const char *, 
	int, const char *, va_list)) __attribute__((noreturn));
static __dead void __panic __P((const char *, const char *, const char *,
	int, const char *, ...)) __attribute__((noreturn));

#if defined(__OpenBSD__) || defined(__FreeBSD__)
#include <pthread.h>
#include <pthread_np.h>
void	_thread_dump_info __P((void));
#define SET_NAME(x)	pthread_set_name_np(pthread_self(), x)
#define DUMP_INFO()	_thread_dump_info()
#else
#define SET_NAME(x)	/* nada */
#define DUMP_INFO()	/* nada */
#endif

static void
__vpanic(type, errstr, filenm, lineno, fmt, ap)
	const char *type; 
	const char *errstr;
	const char *filenm;
	int lineno; 
	const char *fmt; 
	va_list ap;
{
	char buf[1024];

	/* "<type> at <filenm>:<lineno>: <fmt ap...>:<errstr>" */
	snprintf(buf, sizeof buf, "%s at %s:%d\n", type, filenm, lineno);
	_thread_sys_write(2, buf, strlen(buf));
	vsnprintf(buf, sizeof buf, fmt, ap);
	if (errstr != NULL) {
		strlcat(buf, ": ", sizeof buf);
		strlcat(buf, errstr, sizeof buf);
	}
	strlcat(buf, "\n", sizeof buf);
	_thread_sys_write(2, buf, strlen(buf));

	DUMP_INFO();
	_thread_sys__exit(1);

	_thread_sys_write(2, "[locking]\n", 10);
	while(1);
}

static void
__panic(type, errstr, filenm, lineno, fmt)
	const char *type;
	const char *errstr;
	const char *filenm;
	int lineno; 
	const char *fmt;
{
	va_list ap;

	va_start(ap, fmt);
	__vpanic(type, errstr, filenm, lineno, fmt, ap);
	va_end(ap);
}

#define DIE(e, m, args...) \
	__panic("died", strerror(e), __FILE__, __LINE__, m , ## args)

#define PANIC(m, args...)  \
	__panic("panic", NULL, __FILE__, __LINE__, m, ## args)

#define ASSERT(x) do { \
	if (!(x)) \
		__panic("assert failed", NULL, __FILE__, __LINE__, "%s", #x); \
} while(0)

#define ASSERTe(x,rhs) do { \
	int _x; \
	_x = (x); \
	if (!(_x rhs)) { \
	    if (_x == -1) \
		__panic("assert failed", strerror(_x), __FILE__, __LINE__,  \
		    "%s %s", #x, #rhs); \
	    else \
		__panic("assert failed", NULL, __FILE__, __LINE__, \
		    "%s [=%d] %s", #x, _x, #rhs); \
	} \
} while(0)

#define _CHECK(x, rhs, efn) do { \
	int _x; \
	_x = (int)(x); \
	if (!(_x rhs)) \
		__panic("check failed", efn, __FILE__, __LINE__, \
		    "failed check %s (=%d) %s ", #x, _x, #rhs); \
} while(0)

#define CHECKr(x) _CHECK(x, == 0, strerror(_x))
#define CHECKe(x) _CHECK(x, != -1, strerror(errno))
#define CHECKn(x) _CHECK(x, != 0, strerror(errno))
#define CHECKhn(x) _CHECK(x, != 0, hstrerror(h_errno))

#define SUCCEED 	exit(0)

#define OK		(0)
#define NOTOK		(-1)

#endif _h_test_
