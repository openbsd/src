
#include <sys/param.h>
#include <sys/reboot.h>

#include <machine/mon.h>

#include "stand.h"
#include "promboot.h"

char prom_bootdev[32];
char *prom_bootfile;
int prom_boothow;
int debug = 0;

/*
 * Get useful info from the PROM bootparams struct, i.e.:
 * arg[0] = sd(0,0,0)netbsd
 * arg[1] = -sa
 */

void
prom_get_boot_info()
{
	MachMonBootParam *bpp;
	char	c, *src, *dst;

#ifdef	DEBUG
	printf("prom_get_boot_info\n");
#endif

	bpp = *romp->bootParam;

	/* Get device and file names. */
	src = bpp->argPtr[0];
	dst = prom_bootdev;
	*dst++ = *src++;
	*dst++ = *src++;
	if (*src == '(') {
		while (*src) {
			c = *src++;
			*dst++ = c;
			if (c == ')')
				break;
		}
		*dst = '\0';
	}
	prom_bootfile = src;

	/* Get boothowto flags. */
	src = bpp->argPtr[1];
	if (src && (*src == '-')) {
		while (*src) {
			switch (*src++) {
			case 'a':
				prom_boothow |= RB_ASKNAME;
				break;
			case 's':
				prom_boothow |= RB_SINGLE;
				break;
			case 'd':
				prom_boothow |= RB_KDB;
				debug++;
				break;
			}
		}
	}

	if (debug) {
		printf("Debug level %d - enter c to continue...", debug);
		/* This will print "\nAbort at ...\n" */
		asm("	trap #0");
	}
}
