/*	$OpenBSD: boot.c,v 1.2 2000/03/03 00:54:49 todd Exp $ */

/*-
 * Copyright (c) 1995 Theo de Raadt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Theo de Raadt
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>

#define _KERNEL
#include <machine/sic.h>
#include <machine/asi.h>
#include <machine/asm.h>
#include <machine/psl.h>

#include "stand.h"
#include "promboot.h"

/*
 * Boot device is derived from ROM provided information.
 */
#define LOADADDR	0x10000

extern char		version[];
char	defname[32] = "hello";
char	line[80];

#if 0
u_int	bootdev = MAKEBOOTDEV(0, sdmajor, 0, 0, 0);	/* disk boot */
#endif
u_int	bootdev = MAKEBOOTDEV(1, 0, 0, 0, 0);		/* network boot */

char **environ;

char *
getenv (const char *str)
{
  char **c;
  size_t len = strlen (str);

  for (c = environ; *c; c++)
    {
      if ((*c)[len] == '=' && strncmp (*c, str, len) == 0)
	return (*c) + len + 1;
    }
  return NULL;
}

void
disp_hex (unsigned char c)
{
  putchar ("0123456789abcdef"[c >> 4]);
  putchar ("0123456789abcdef"[c & 0x0f]);
  putchar (' ');
}

void
dump (unsigned char *addr, int len)
{
  int i, j;

  for (i = 0; i < len; i+= 16)
    {
      printf ("%x: ", i);
      for (j = 0; j < 16 && (i + j < len); j++)
	disp_hex(addr[i+j]);
      printf ("\n");
    }
}

void
sic_init (void)
{
  unsigned char irc_status;
  unsigned char id;
  unsigned char v;

  set_ipl (0x0f);

  printf ("1");  
  /* Disable the receiver.  */
  irc_status = lduba (ASI_IRXC, 0);
  while (irc_status & SIC_IRC_E)
    {
      stba (ASI_IRXC, 0, irc_status & ~SIC_IRC_E);
      irc_status = lduba (ASI_IRXC, 0);
      printf ("2");  
    }

  /* Enable all interruptions.  */
  sta (ASI_IPR, 0, 0);
  
  printf ("3");  

  /* Set device id.  */
  id = lda (ASI_BID, 0) & 0x0f;
  stba (ASI_DIR, 0, id);

  printf ("4");  
  /* Ack int.  */
  lda (ASI_ACK_IPV, 0);

  printf ("5");  
  /* Enable the receiver.  */
  stba (ASI_IRXC, 0, irc_status | SIC_IRC_E);

  printf ("6");  
  set_ipl (0);
  *(unsigned long *)0x17030000 = id | 0x40; 

  printf ("7");  
  /* Enable interruptions from the system board.  */
  v = *(volatile char *)0x17031000; 
  putchar ('8');
  printf ("9\n");  
}

void
sendint (int boardid, int level)
{
  printf ("sendint level %d: ", level);
  if (lduba (ASI_ITXC, 0) & SIC_ITXC_E)
    {
      printf ("busy\n");
      return; 
    }
  else
    printf ("not busy...\n");
  sta (ASI_IXR, 0, ((level & 0xff) << 8) | SIC_IXR_DIR | (boardid & 0x0f));
  stba (ASI_ITXC, 0, SIC_ITXC_E);
}

void
set_ipr (int level)
{
}

extern int debug;
extern int in_trap_handler;

void
main (int argc, char *argv[], char *envp[])
{
  char *cp, *file;
  int	io;
  int ask = 0;

  environ = envp;
  printf(">> OpenBSD netboot [%s]\n", version);
  /*	printf("model MVME%x\n", cputyp); */

  printf ("Intr: ");
  sic_init ();
  printf ("Enabled\n");

  disp_zs0_b ();
  udelay (10000);
#if 0
  printf ("Before sendint...\n");
  sendint (1, 150);
  printf ("Again...\n");
  sendint (1, 150);
#endif
#if 0
  printf ("Set IPR to 160 and send 160\n");
  sta (ASI_IPR, 0, 0xa0);
  sendint (1, 161);
  sendint (1, 160);
  printf ("Set IPR to 0\n");
  sta (ASI_IPR, 0, 0);
#endif
#if 0
  printf ("Set psl to 15, sendint and reset it\n");
  setpil15();
  sendint (1, 155);
  sendint (1,160);
  sendint (1,160);
  setpil0();

  disp_date ();
  udelay (1000000);
  printf ("Exit\n");
  _exit (0);
#endif

  debug = 1;
  io = open ("hello", 0);
  if (io == -1)
    printf ("open:(%d) %s\n", errno, strerror (errno));
#if 0
  asm ("ta 127");
  _exit (0);
#endif

  prom_get_boot_info();
  file = defname;
  
  cp = prom_bootfile;
  if (cp && *cp)
    file = cp;
  
  for (;;) {
    if (ask) {
      printf("boot: ");
      gets(line);
      if (line[0]) {
	prom_get_boot_info();
      }
    }
    exec_sun(file, (char *)LOADADDR, prom_boothow);
    printf("boot: %s\n", strerror(errno));
    ask = 1;
  }
}
