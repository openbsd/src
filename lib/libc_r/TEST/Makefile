# $OpenBSD: Makefile,v 1.21 2000/10/04 05:50:58 d Exp $
# Copyright (c) 1993 Chris Provenzano, proven@athena.mit.edu

LIBC_R?=	/usr/lib/libc_r.a
LIBPTHREAD?=	/usr/lib/libpthread.a

.if ${MACHINE_ARCH} == "sparc"
PTHREAD=	-lpthread
DPADD +=	${LIBPTHREAD}
.else
PTHREAD=	-pthread
DPADD +=	${LIBC_R}
.endif

CFLAGS +=	-Wall # -Werror
DEBUG =		-ggdb
CFLAGS +=	-DSRCDIR='"${.CURDIR}"'
LDLIBS +=	${PTHREAD} -lm  ${LDSTATIC}
MKDEP =		-p ${PTHREAD}

# This list used to include test_select, but that test doesn't terminate.
TESTS = test_create test_pthread_join test_switch test_sleep test_readdir \
	test_fork test_execve test_preemption test_setjmp \
	test_sock_1 test_sock_2 test_stdio_1 test_pthread_mutex \
	test_pthread_cond_timedwait test_netdb test_cwd \
	test_sock_2a test_cancel test_sigsuspend test_sigwait \
	test_select test_poll test_signal test_stdarg test_close \
	test_group

SRCS =		${TESTS:=.c}
CLEANFILES +=	${TESTS} 
SKIP_TESTS +=	test_sock_2a

all : tests

MAXTIME=	5

tests : ${TESTS}
	@faillist= ; \
	for i in ${.ALLSRC} ; do \
	  case " ${SKIP_TESTS} " in \
	  *" $$i "*) \
		  : skip ;; \
	  *) \
		  echo ; echo "*** $$i ***"; \
		  if (ulimit -t ${MAXTIME}; ${.OBJDIR}/$$i); then \
			  echo "-- $$i passed"; \
		  else \
			  echo "-- $$i FAILED (exit code $$?)" >&2; \
			  faillist="$$faillist $$i"; \
		  fi;; \
	  esac; \
	done; \
	if test -n "$$faillist"; then \
		echo; echo "*** tests that failed:$$faillist" >&2; exit 1; \
	else \
		echo; echo "All tests passed OK."; exit 0; \
	fi

.include <bsd.prog.mk>
.include <bsd.dep.mk>
