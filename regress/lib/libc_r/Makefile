# $OpenBSD: Makefile,v 1.6 2001/11/11 01:16:26 marc Exp $

SUBDIR= cancel close cwd execve fork group netdb poll \
	preemption pthread_cond_timedwait pthread_create \
	pthread_join pthread_mutex readdir select setjmp signal \
	sigsuspend sigwait sleep socket stdarg stdio switch system

# Not available or disabled: fcntl, pause, preemption_float and pw

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
