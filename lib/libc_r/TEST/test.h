
#ifndef _h_test_
#define _h_test_

#include <signal.h>

extern int _thread_sys_write __P((int, const char*, size_t));

static __dead void __panic __P((const char *, int, const char*))
	__attribute__((noreturn));

static void __panic(const char *filenm, int lineno, const char*panicstr) {
	extern __dead void _thread_sys__exit __P((int)) 
		__attribute__((noreturn));
	extern size_t strlen __P((const char*));
	extern int sprintf __P((char *, const char *, ...));
	char buf[1024];

	_thread_sys_write(2, panicstr, strlen(panicstr));
	sprintf(buf, "%s:%d\n", filenm, lineno);
	_thread_sys_write(2, buf, strlen(buf));
	kill(0, SIGINFO);
	_thread_sys__exit(1);
	while(1);
}

#include <stdarg.h>
#include <string.h>

static __dead void __die __P((int err, const char *, int, const char *, ...));
static void __die(int err, const char *file, int line, const char *fmt, ...)

{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	strlcat(buf, ": ", sizeof buf);
	strlcat(buf, strerror(err), sizeof buf);
	_thread_sys_write(2, buf, strlen(buf));
	__panic(file, line, "\ndied at: ");
	__die(0,0,0,0);
}

#define DIE(e, m, args...)	__die(e, __FILE__, __LINE__, m , ## args)

#define PANIC()  __panic(__FILE__, __LINE__, "\npanic at ")

#define OK (0)
#define NOTOK (-1)

#endif _h_test_
