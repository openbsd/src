/*	$OpenBSD: libsa.h,v 1.5 2013/10/16 16:59:35 miod Exp $	*/

/*
 * libsa prototypes 
 */

#include <machine/prom.h>

extern int boothowto;

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
