/*	$OpenBSD: libsa.h,v 1.4 2013/10/10 21:22:07 miod Exp $	*/

/*
 * libsa prototypes 
 */

#include <machine/prom.h>

extern int boothowto;

#define	BOOT_ETHERNET_ZERO	0x0001

struct boot_info {
	unsigned int bootdev;
	unsigned int bootunit;
	unsigned int bootlun;
	unsigned int bootpart;
};

extern struct boot_info bi;

int	badaddr(void *, int);
void	delay(unsigned int);
void	exec(char *, const char *, uint, uint, uint, uint);
int	parse_args(const char *, char **, int);
