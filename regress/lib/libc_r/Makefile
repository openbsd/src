# $OpenBSD: Makefile,v 1.5 2001/11/10 02:44:48 marc Exp $

SUBDIR= cancel close cwd execve fork group netdb pcap poll \
	preemption pthread_cond_timedwait pthread_create \
	pthread_join pthread_mutex readdir select setjmp signal \
	sigsuspend sigwait sleep socket stdarg stdio switch system

# Not available or disabled: fcntl, pause, preemption_float and pw

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
