# $OpenBSD: Makefile,v 1.22 2004/06/11 19:26:13 marc Exp $

SUBDIR= cancel cancel2 close closefrom cwd execve fork group \
	malloc_duel netdb pcap poll \
	preemption preemption_float pthread_cond_timedwait pthread_create \
	pthread_join pthread_kill pthread_mutex pthread_specific readdir \
	select setjmp signal sigdeliver siginfo signodefer sigsuspend \
	sigwait sleep socket stdarg stdio switch system

# Not available or disabled: fcntl, pause, and pw

install:

.PHONY: testall
testall:
	rm -f /tmp/thread-{shared,static,debug}
	make cleandir && make obj && make depend && \
	REGRESS_LOG=/tmp/thread-shared make
	(export LDFLAGS=-static; make clean && \
	 REGRESS_LOG=/tmp/thread-static make)
	(export DEBUG=-g; make clean && \
	 REGRESS_LOG=/tmp/thread-debug make)
	@echo "\n\nTest Results:"
	@(shared=$$(grep FAIL /tmp/thread-shared) || true; \
	  if [ "$$shared" ]; then \
	     echo "** SHARED lib failures:"; echo "$$shared"; echo; \
	  else \
	     echo "No SHARED lib failures"; \
	  fi)
	@(static=$$(grep FAIL /tmp/thread-static) || true; \
	  if [ "$$static" ]; then \
	     echo "** STATIC lib failures:"; echo "$$static"; echo; \
	  else \
	     echo "No STATIC lib failures"; \
	  fi)
	@(debug=$$(grep FAIL /tmp/thread-debug) || true; \
	  if [ "$$debug" ]; then \
	     echo "** DEBUG lib failures:"; echo "$$debug"; echo; \
	  else \
	     echo "No DEBUG lib failures"; \
	  fi)

.include <bsd.subdir.mk>
