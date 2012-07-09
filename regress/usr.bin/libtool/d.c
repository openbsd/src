/* $OpenBSD: d.c,v 1.1 2012/07/09 21:37:33 espie Exp $ */
/* crazy shit won't compile without the right defines */
extern void f
#if defined A
(
#endif
#if defined B
);
#endif
