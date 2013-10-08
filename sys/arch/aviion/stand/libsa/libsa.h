/*	$OpenBSD: libsa.h,v 1.3 2013/10/08 21:55:22 miod Exp $	*/

/*
 * libsa prototypes 
 */

#include <machine/prom.h>

extern int boothowto;

#define	BOOT_ETHERNET_ZERO	0x0001

void	delay(unsigned int);
void	exec(char *, const char *, int, int, int);
int	parse_args(const char *, char **, int);
