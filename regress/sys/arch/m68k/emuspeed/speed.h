/*	$OpenBSD: speed.h,v 1.3 2002/02/16 21:27:32 millert Exp $	*/
/*	$NetBSD: speed.h,v 1.2 1998/01/09 08:03:57 perry Exp $	*/

#include <sys/cdefs.h>

void illegal(int);
void mul32smem(int);
void mul32sreg(int);

void mul64sreg(int);
void mul64ureg(int);
void mul64smem(int);
void mul64umem(int);

void div64umem(int);
void div64smem(int);
void div64ureg(int);
void div64sreg(int);
