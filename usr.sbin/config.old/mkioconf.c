/*	$OpenBSD: mkioconf.c,v 1.2 1997/01/12 07:43:35 downsj Exp $	*/
/*	$NetBSD: mkioconf.c,v 1.38 1996/06/10 02:32:25 thorpej Exp $	*/

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	may be used to endorse or promote products derived from this software
 *	without specific prior written permission.
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
 */

#ifndef lint
/*static char sccsid[] = "from: @(#)mkioconf.c	5.18 (Berkeley) 5/10/91";*/
static char rcsid[] = "$NetBSD: mkioconf.c,v 1.38 1996/06/10 02:32:25 thorpej Exp $";
#endif /* not lint */

#include <stdio.h>
#include "y.tab.h"
#include "config.h"

/*
 * build the ioconf.c file
 */
char	*qu();
char	*intv();

static char *fakepdevs[] = {
	"ether",		/* XXX */
	"log",			/* XXX */
	NULL,
};

pseudo_init(fp)
	FILE *fp;
{
	struct device *dp;
	char **s;
	int ok, num;
	
	fprintf(fp, "\n/* pseudo-devices */\n");
	for (dp = dtab; dp; dp = dp->d_next) {
		if (dp->d_type != PSEUDO_DEVICE)
			continue;
		for (ok = 1, s = fakepdevs; *s && ok; s++)
			if (strcmp(dp->d_name, *s) == 0)
				ok = 0;
		if (ok == 0)
			continue;
		fprintf(fp, "extern void %sattach __P((int));\n",
			dp->d_name);
	}

	fprintf(fp, "\nstruct pdevinit pdevinit[] = {\n");
	for (dp = dtab; dp; dp = dp->d_next) {
		if (dp->d_type != PSEUDO_DEVICE)
			continue;
		for (ok = 1, s = fakepdevs; *s && ok; s++)
			if (strcmp(dp->d_name, *s) == 0)
				ok = 0;
		if (ok == 0)
			continue;
		num = dp->d_slave != UNKNOWN ? dp->d_slave : 0;
		fprintf(fp, "\t{ %sattach, %d },\n",
			dp->d_name, num);
	}
	fprintf(fp, "\t{ 0, 0 }\n};\n");
}

#if MACHINE_HP300
hp300_ioconf()
{
  register struct device *dp, *mp, *np;
  register int hpib, slave;
  FILE *fp;
  extern char *wnum();
  
  fp = fopen(path("ioconf.c"), "w");
  if (fp == 0) {
    perror(path("ioconf.c"));
    exit(1);
  }
  fprintf(fp, "#include \"sys/param.h\"\n");
  fprintf(fp, "#include \"sys/device.h\"\n");
  fprintf(fp, "#include \"sys/buf.h\"\n");
  fprintf(fp, "#include \"sys/map.h\"\n");
  fprintf(fp, "\n");
  fprintf(fp, "#include \"hp300/dev/device.h\"\n\n");
  fprintf(fp, "\n");
  fprintf(fp, "#define C (caddr_t)\n");
  fprintf(fp, "#define D (struct driver *)\n\n");
  /*
   * First print the hpib controller initialization structures
   */
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    mp = dp->d_conn;
    if (dp->d_unit == QUES || mp == 0)
      continue;
    fprintf(fp, "extern struct driver %sdriver;\n", dp->d_name);
  }
  fprintf(fp, "\nstruct hp_ctlr hp_cinit[] = {\n");
  fprintf(fp, "/*\tdriver,\t\tunit,\talive,\taddr,\tflags */\n");
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    mp = dp->d_conn;
    if (dp->d_unit == QUES ||
	dp->d_type != MASTER && dp->d_type != CONTROLLER)
      continue;
    if (mp != TO_NEXUS) {
      printf("%s%s must be attached to an sc (nexus)\n",
	     dp->d_name, wnum(dp->d_unit));
      continue;
    }
    if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
      printf("can't specify drive/slave for %s%s\n",
	     dp->d_name, wnum(dp->d_unit));
      continue;
    }
    fprintf(fp,
	    "\t{ &%sdriver,\t%d,\t0,\tC 0x%x,\t0x%x },\n",
	    dp->d_name, dp->d_unit, dp->d_addr, dp->d_flags);
  }
  fprintf(fp, "\t0\n};\n");
  /* devices */
  fprintf(fp, "\nstruct hp_device hp_dinit[] = {\n");
  fprintf(fp,
	  "/*driver,\tcdriver,\tunit,\tctlr,\tslave,\taddr,\tdk,\tflags*/\n");
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    mp = dp->d_conn;
    if (mp == 0 || dp->d_type != DEVICE || hpbadslave(mp, dp))
      continue;
    if (mp == TO_NEXUS) {
      if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
	printf("can't specify drive/slave for %s%s\n",
	       dp->d_name, wnum(dp->d_unit));
	continue;
      }
      slave = QUES;
      hpib = QUES;
    } else {
      if (dp->d_addr != 0) {
	printf("can't specify sc for device %s%s\n",
	       dp->d_name, wnum(dp->d_unit));
	continue;
      }
      if (mp->d_type == CONTROLLER) {
	if (dp->d_drive == UNKNOWN) {
	  printf("must specify drive for %s%s\n",
		 dp->d_name, wnum(dp->d_unit));
	  continue;
	}
	slave = dp->d_drive;
      } else {
	if (dp->d_slave == UNKNOWN) {
	  printf("must specify slave for %s%s\n",
		 dp->d_name, wnum(dp->d_unit));
	  continue;
	}
	slave = dp->d_slave;
      }
      hpib = mp->d_unit;
    }
    fprintf(fp, "{ &%sdriver,\t", dp->d_name);
    if (mp == TO_NEXUS)
      fprintf(fp, "D 0x0,\t");
    else
      fprintf(fp, "&%sdriver,", mp->d_name);
    fprintf(fp, "\t%d,\t%d,\t%d,\tC 0x%x,\t%d,\t0x%x },\n",
	    dp->d_unit, hpib, slave,
	    dp->d_addr, dp->d_dk, dp->d_flags);
  }
  fprintf(fp, "0\n};\n");
  pseudo_init(fp);
  (void) fclose(fp);
}

#define ishpibdev(n) (eq(n,"rd") || eq(n,"ct") || eq(n,"mt") || eq(n,"ppi"))
#define isscsidev(n) (eq(n,"sd") || eq(n,"st") || eq(n,"ac"))

hpbadslave(mp, dp)
     register struct device *dp, *mp;
{
  extern char *wnum();
  
  if (mp == TO_NEXUS && ishpibdev(dp->d_name) ||
      mp != TO_NEXUS && eq(mp->d_name, "hpib") &&
      !ishpibdev(dp->d_name)) {
    printf("%s%s must be attached to an hpib\n",
	   dp->d_name, wnum(dp->d_unit));
    return (1);
  }
  if (mp == TO_NEXUS && isscsidev(dp->d_name) ||
      mp != TO_NEXUS && eq(mp->d_name, "scsi") &&
      !isscsidev(dp->d_name)) {
    printf("%s%s must be attached to a scsi\n",
	   dp->d_name, wnum(dp->d_unit));
    return (1);
  }
  return (0);
}

char *
  wnum(num)
{
  
  if (num == QUES || num == UNKNOWN)
    return ("?");
  (void) sprintf(errbuf, "%d", num);
  return (errbuf);
}
#endif
