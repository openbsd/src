
#ifndef _h_test_
#define _h_test_

static __dead void __panic __P((char *filenm, int lineno))
	__attribute__((noreturn));

static void __panic(char *filenm, int lineno) {
	extern int _thread_sys_write __P((int, char*, size_t));
	extern __dead void _thread_sys__exit __P((int)) 
		__attribute__((noreturn));
	extern size_t strlen __P((const char*));
	extern int sprintf __P((char *, const char *, ...));
	char buf[1024];
	char *panicstr = "\npanic at ";

	_thread_sys_write(2, panicstr, sizeof panicstr - 1);
	sprintf(buf, "%s:%d\n", filenm, lineno);
	_thread_sys_write(2, buf, strlen(buf));
	_thread_sys__exit(1);
	__panic(NULL, 0);
}

#define PANIC()  __panic(__FILE__, __LINE__)

#define OK (0)
#define NOTOK (-1)

#endif _h_test_
