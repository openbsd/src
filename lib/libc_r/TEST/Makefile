# Generated automatically from Makefile.in by configure.
# === GNUmakefile ============================================================
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu
#
#	Description: This file is for creating the test programs for libpthread.a
#
#	1.00 93/08/03 proven
#		-Initial cut for pthreads.
#

LIBC_R?=	/usr/lib/libc_r.a

CPPFLAGS +=	-pthread
LDFLAGS +=	-pthread
CFLAGS +=	-ggdb -Wall # -Werror
CFLAGS +=	-DSRCDIR='"${.CURDIR}"'
LDLIBS +=	-lm
DPADD +=	${LIBC_R}
MKDEP =		-p

# This list used to include test_select, but that test doesn't terminate.
TESTS = test_create test_pthread_join test_switch test_sleep test_readdir \
	test_fork test_execve test_preemption \
	test_sock_1 test_sock_2 test_stdio_1 test_pthread_mutex \
	test_pthread_cond_timedwait test_netdb test_pw test_cwd \
	test_sock_2a test_cancel

SRCS =		${TESTS:=.c}
CLEANFILES +=	${TESTS} 
SKIP_TESTS +=	test_sock_2a

# This list used to include p_bench_semaphore, but the semaphore support isn't
# defined for all targets (or used for any).

BENCHMARKS = p_bench_read p_bench_mutex p_bench_yield \
	p_bench_getpid p_bench_pthread_create

SRCS +=		${BENCHMARKS:=.c}
CLEANFILES +=	${BENCHMARKS} 


all : tests # benchmarks

tests : ${TESTS}
	@faillist= ; ulimit -t 5; \
	for i in ${.ALLSRC} ; do \
	  case " ${SKIP_TESTS} " in \
	  *" $$i "*) \
		  : skip ;; \
	  *) \
		  echo ; echo "*** $$i ***"; \
		  if ${.OBJDIR}/$$i; then \
			  echo "-- $$i passed"; \
		  else \
			  echo "-- $$i FAILED (exit code $$?)"; \
			  faillist="$$faillist $$i"; \
		  fi;; \
	  esac; \
	done; \
	if test -n "$$faillist"; then \
		echo; echo "*** tests that failed:$$faillist"; exit 1; \
	else \
		echo; echo "All tests passed OK."; exit 0; \
	fi

benchmarks: ${BENCHMARKS}

.include <bsd.prog.mk>
.include <bsd.dep.mk>
