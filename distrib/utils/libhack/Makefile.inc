# $OpenBSD: Makefile.inc,v 1.2 2000/03/01 22:10:12 todd Exp $
# $NetBSD: Makefile.inc,v 1.2 1995/10/13 18:10:20 gwr Exp $
# Include this fragment to build libhack.o
# It is .o and not .a to make sure these are the
# objects you get (and not the ones in libc.a)

HACKOBJS= gethost.o getpwent.o getgrent.o getnetgr.o

libhack.o : $(HACKOBJS)
	$(LD) -r -o $@ $(HACKOBJS)

gethost.o : ${HACKSRC}/gethost.c
	$(CC) -c ${HACKSRC}/gethost.c

getpwent.o : ${HACKSRC}/getpwent.c
	$(CC) -c ${HACKSRC}/getpwent.c

getgrent.o : ${HACKSRC}/getgrent.c
	$(CC) -c ${HACKSRC}/getgrent.c

getnetgr.o : ${HACKSRC}/getnetgr.c
	$(CC) -c ${HACKSRC}/getnetgr.c

yplib.o : ${HACKSRC}/yplib.c
	$(CC) -c ${HACKSRC}/yplib.c

