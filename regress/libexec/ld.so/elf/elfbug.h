/* 
 * Public domain - Matthieu Herrb 2002
 * 
 * $OpenBSD: elfbug.h,v 1.1 2002/02/05 21:47:23 matthieu Exp $
 */
extern int (*func)(void);
extern int uninitialized(void);
extern int bar(void);
extern void fooinit(void);
