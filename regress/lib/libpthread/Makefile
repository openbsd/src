# $OpenBSD: Makefile,v 1.3 2001/08/23 04:11:03 fgsch Exp $

SUBDIR= cancel close cwd execve fork group netdb poll \
	preemption pthread_cond_timedwait pthread_create \
	pthread_join pthread_mutex readdir select setjmp signal \
	sigsuspend sigwait sleep socket stdarg stdio switch

# Not available or disabled: fcntl, pause, preemption_float and pw

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
