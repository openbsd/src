/*	$NetBSD: machdep.c,v 1.1.1.1 1995/07/25 23:12:22 chuck Exp $	*/

#include <sys/param.h>
#include <sys/types.h>
#include <sys/exec.h>

#include <sys/reboot.h>

#include "config.h"

/*
 * get_boothowto: boothowto for kernel
 */

static int get_boothowto(cmd_flags)

char *cmd_flags;

{
  int result = 0;

  if (cmd_flags == NULL) return(0);

  while (*cmd_flags) {
    switch (*cmd_flags) {
      case 's': result |= RB_SINGLE; break;
      case 'a': result |= RB_ASKNAME; break;
      default: break;
    }
    cmd_flags++;
  }

  return(result);
}

/*
 * cmd_parse: parse command line
 * expected format: "b[oot] [kernel_name] [-flags]"
 */

char *cmd_parse(cmd_buf, howto)
char *cmd_buf;
int *howto;
{
  char *cmd_kernel, *cmd_flags;
  u_char *cp;
  *howto = 0;

  cp = cmd_buf+1; 				/* skip 'b' */
  while (*cp && *cp != ' ') cp++;		/* skip to end or space */
  while (*cp == ' ') cp++;			/* skip spaces */
  if (*cp == '\0') return(NULL);		/* no args */
  if (*cp == '-') {				/* only flags? */
    *howto = get_boothowto(cp);
    return(NULL);
  }
  cmd_kernel = cp;				/* got kernel name */
  while (*cp && *cp != ' ') cp++;		/* skip to end or space */
  if (*cp == ' ') *cp++ = 0;			/* null terminate kernel */
  while (*cp == ' ') cp++;			/* skip spaces */
  if (*cp == '\0') return(cmd_kernel);		/* no flags */
  if (*cp != '-') return(cmd_kernel);           /* garbage flags */
  cmd_flags = cp;				/* save flags */

  while (*cp && *cp != ' ') cp++;		/* skip to end or space */
  if (*cp == ' ') *cp++ = 0;			/* null terminate flags */

  *howto = get_boothowto(cmd_flags);

  return(cmd_kernel);
}


/*
 * machdep_common_ether: get ethernet address
 */

void machdep_common_ether(ether)
     unsigned char *ether;
{
    caddr_t addr;
    int *ea = (int *) ETHER_ADDR;
    int e = *ea;
    if (( e & 0x2fffff00 ) == 0x2fffff00) 
      panic("ERROR: ethernet address not set!\r\n");
    ether[0] = 0x08;
    ether[1] = 0x00;
    ether[2] = 0x3e;
    e = e >> 8;
    ether[5] = e & 0xff;
    e = e >> 8;
    ether[4] = e & 0xff;
    e = e >> 8;
    ether[3] = e;
}

/*
 * console i/o
 */

/*
 * hardware
 */

struct zs_hw {
  volatile u_char ctl;
  volatile u_char data;
};

struct zs_hw *zs =  (struct zs_hw *)CONS_ZS_ADDR;

/*
 * putchar: put char to console
 */

void putchar(char c) 
{
  if (c == '\n') putchar('\r');
  zs->ctl = 0;
  while ((zs->ctl & 0x04) == 0) {
    zs->ctl = 0;
  }
  zs->ctl = 8;
  zs->ctl = c;
}

/*
 * getchar: get char from console
 */

int
getchar()
{
	int i;
	while (1) {
		zs->ctl = 0;
		if ((zs->ctl & 0x1) != 0) break;
		for (i = 100 ; i > 0 ; i--)
			;
	}
	zs->ctl = 8;
	return(zs->ctl);
}

/*
 * peekchar
 */

peekchar()
{
	zs->ctl = 0;
	return(zs->ctl & 0x1);
}

