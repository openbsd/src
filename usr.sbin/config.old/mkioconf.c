/*	$NetBSD: mkioconf.c,v 1.37 1995/08/17 17:22:17 thorpej Exp $	*/

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
static char rcsid[] = "$NetBSD: mkioconf.c,v 1.37 1995/08/17 17:22:17 thorpej Exp $";
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

#if MACHINE_VAX
vax_ioconf()
{
  register struct device *dp, *mp, *np;
  register int uba_n, slave;
  FILE *fp;
  
  fp = fopen(path("ioconf.c"), "w");
  if (fp == 0) {
    perror(path("ioconf.c"));
    exit(1);
  }
  fprintf(fp, "#include \"vax/include/pte.h\"\n");
  fprintf(fp, "#include \"sys/param.h\"\n");
  fprintf(fp, "#include \"sys/device.h\"\n");
  fprintf(fp, "#include \"sys/buf.h\"\n");
  fprintf(fp, "#include \"sys/map.h\"\n");
  fprintf(fp, "\n");
  fprintf(fp, "#include \"vax/mba/mbavar.h\"\n");
  fprintf(fp, "#include \"vax/uba/ubavar.h\"\n\n");
  fprintf(fp, "\n");
  fprintf(fp, "#define C (caddr_t)\n\n");
  /*
   * First print the mba initialization structures
   */
  if (seen_mba) {
    for (dp = dtab; dp != 0; dp = dp->d_next) {
      mp = dp->d_conn;
      if (mp == 0 || mp == TO_NEXUS ||
	  !eq(mp->d_name, "mba"))
	continue;
      fprintf(fp, "extern struct mba_driver %sdriver;\n",
	      dp->d_name);
    }
    fprintf(fp, "\nstruct mba_device mbdinit[] = {\n");
    fprintf(fp, "\t/* Device,  Unit, Mba, Drive, Dk */\n");
    for (dp = dtab; dp != 0; dp = dp->d_next) {
      mp = dp->d_conn;
      if (dp->d_unit == QUES || mp == 0 ||
	  mp == TO_NEXUS || !eq(mp->d_name, "mba"))
	continue;
      if (dp->d_addr) {
	printf("can't specify csr address on mba for %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_vec != 0) {
	printf("can't specify vector for %s%d on mba\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_drive == UNKNOWN) {
	printf("drive not specified for %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_slave != UNKNOWN) {
	printf("can't specify slave number for %s%d\n", 
	       dp->d_name, dp->d_unit);
	continue;
      }
      fprintf(fp, "\t{ &%sdriver, %d,   %s,",
	      dp->d_name, dp->d_unit, qu(mp->d_unit));
      fprintf(fp, "  %s,  %d },\n",
	      qu(dp->d_drive), dp->d_dk);
    }
    fprintf(fp, "\t0\n};\n\n");
    /*
     * Print the mbsinit structure
     * Driver Controller Unit Slave
     */
    fprintf(fp, "struct mba_slave mbsinit [] = {\n");
    fprintf(fp, "\t/* Driver,  Ctlr, Unit, Slave */\n");
    for (dp = dtab; dp != 0; dp = dp->d_next) {
      /*
       * All slaves are connected to something which
       * is connected to the massbus.
       */
      if ((mp = dp->d_conn) == 0 || mp == TO_NEXUS)
	continue;
      np = mp->d_conn;
      if (np == 0 || np == TO_NEXUS ||
	  !eq(np->d_name, "mba"))
	continue;
      fprintf(fp, "\t{ &%sdriver, %s",
	      mp->d_name, qu(mp->d_unit));
      fprintf(fp, ",  %2d,	%s },\n",
	      dp->d_unit, qu(dp->d_slave));
    }
    fprintf(fp, "\t0\n};\n\n");
  }
  /*
   * Now generate interrupt vectors for the unibus
   */
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    if (dp->d_vec != 0) {
      struct idlst *ip;
      mp = dp->d_conn;
      if (mp == 0 || mp == TO_NEXUS ||
	  (!eq(mp->d_name, "uba") && !eq(mp->d_name, "bi")))
	continue;
      fprintf(fp,
	      "extern struct uba_driver %sdriver;\n",
	      dp->d_name);
      fprintf(fp, "extern ");
      ip = dp->d_vec;
      for (;;) {
	fprintf(fp, "X%s%d()", ip->id, dp->d_unit);
	ip = ip->id_next;
	if (ip == 0)
	  break;
	fprintf(fp, ", ");
      }
      fprintf(fp, ";\n");
      fprintf(fp, "int\t (*%sint%d[])() = { ", dp->d_name,
	      dp->d_unit);
      ip = dp->d_vec;
      for (;;) {
	fprintf(fp, "X%s%d", ip->id, dp->d_unit);
	ip = ip->id_next;
	if (ip == 0)
	  break;
	fprintf(fp, ", ");
      }
      fprintf(fp, ", 0 } ;\n");
    }
  }
  fprintf(fp, "\nstruct uba_ctlr ubminit[] = {\n");
  fprintf(fp, "/*\t driver,\tctlr,\tubanum,\talive,\tintr,\taddr */\n");
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    mp = dp->d_conn;
    if (dp->d_type != CONTROLLER || mp == TO_NEXUS || mp == 0 ||
	!eq(mp->d_name, "uba"))
      continue;
    if (dp->d_vec == 0) {
      printf("must specify vector for %s%d\n",
	     dp->d_name, dp->d_unit);
      continue;
    }
    if (dp->d_addr == 0) {
      printf("must specify csr address for %s%d\n",
	     dp->d_name, dp->d_unit);
      continue;
    }
    if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
      printf("drives need their own entries; dont ");
      printf("specify drive or slave for %s%d\n",
	     dp->d_name, dp->d_unit);
      continue;
    }
    if (dp->d_flags) {
      printf("controllers (e.g. %s%d) ",
	     dp->d_name, dp->d_unit);
      printf("don't have flags, only devices do\n");
      continue;
    }
    fprintf(fp,
	    "\t{ &%sdriver,\t%d,\t%s,\t0,\t%sint%d, C 0%o },\n",
	    dp->d_name, dp->d_unit, qu(mp->d_unit),
	    dp->d_name, dp->d_unit, dp->d_addr);
  }
  fprintf(fp, "\t0\n};\n");
  /* unibus devices */
  fprintf(fp, "\nstruct uba_device ubdinit[] = {\n");
  fprintf(fp,
	  "\t/* driver,  unit, ctlr,  ubanum, slave,	intr,	addr,	dk, flags*/\n");
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    mp = dp->d_conn;
    if (dp->d_unit == QUES || dp->d_type != DEVICE || mp == 0 ||
	mp == TO_NEXUS || mp->d_type == MASTER ||
	eq(mp->d_name, "mba"))
      continue;
    np = mp->d_conn;
    if (np != 0 && np != TO_NEXUS && eq(np->d_name, "mba"))
      continue;
    np = 0;
    if (eq(mp->d_name, "uba")) {
      if (dp->d_vec == 0) {
	printf("must specify vector for device %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_addr == 0) {
	printf("must specify csr for device %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
	printf("drives/slaves can be specified ");
	printf("only for controllers, ");
	printf("not for device %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      uba_n = mp->d_unit;
      slave = QUES;
    } else {
      if ((np = mp->d_conn) == 0) {
	printf("%s%d isn't connected to anything ",
	       mp->d_name, mp->d_unit);
	printf(", so %s%d is unattached\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      uba_n = np->d_unit;
      if (dp->d_drive == UNKNOWN) {
	printf("must specify ``drive number'' ");
	printf("for %s%d\n", dp->d_name, dp->d_unit);
	continue;
      }
      /* NOTE THAT ON THE UNIBUS ``drive'' IS STORED IN */
      /* ``SLAVE'' AND WE DON'T WANT A SLAVE SPECIFIED */
      if (dp->d_slave != UNKNOWN) {
	printf("slave numbers should be given only ");
	printf("for massbus tapes, not for %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_vec != 0) {
	printf("interrupt vectors should not be ");
	printf("given for drive %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_addr != 0) {
	printf("csr addresses should be given only ");
	printf("on controllers, not on %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      slave = dp->d_drive;
    }
    fprintf(fp, "\t{ &%sdriver,  %2d,   %s,",
	    eq(mp->d_name, "uba") ? dp->d_name : mp->d_name, dp->d_unit,
	    eq(mp->d_name, "uba") ? " -1" : qu(mp->d_unit));
    fprintf(fp, "  %s,	%2d,	%s, C 0%-6o,  %d,  0x%x },\n",
	    qu(uba_n), slave, intv(dp), dp->d_addr, dp->d_dk,
	    dp->d_flags);
  }
  fprintf(fp, "\t0\n};\n");
  pseudo_init(fp);
  (void) fclose(fp);
}
#endif

#if MACHINE_TAHOE
tahoe_ioconf()
{
  register struct device *dp, *mp, *np;
  register int vba_n, slave;
  FILE *fp;
  
  fp = fopen(path("ioconf.c"), "w");
  if (fp == 0) {
    perror(path("ioconf.c"));
    exit(1);
  }
  fprintf(fp, "#include \"sys/param.h\"\n");
  fprintf(fp, "#include \"tahoe/include/pte.h\"\n");
  fprintf(fp, "#include \"sys/device.h\"\n");
  fprintf(fp, "#include \"sys/buf.h\"\n");
  fprintf(fp, "#include \"sys/map.h\"\n");
  fprintf(fp, "\n");
  fprintf(fp, "#include \"tahoe/vba/vbavar.h\"\n");
  fprintf(fp, "\n");
  fprintf(fp, "#define C (caddr_t)\n\n");
  /*
   * Now generate interrupt vectors for the versabus
   */
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    mp = dp->d_conn;
    if (mp == 0 || mp == TO_NEXUS || !eq(mp->d_name, "vba"))
      continue;
    if (dp->d_vec != 0) {
      struct idlst *ip;
      fprintf(fp,
	      "extern struct vba_driver %sdriver;\n",
	      dp->d_name);
      fprintf(fp, "extern ");
      ip = dp->d_vec;
      for (;;) {
	fprintf(fp, "X%s%d()", ip->id, dp->d_unit);
	ip = ip->id_next;
	if (ip == 0)
	  break;
	fprintf(fp, ", ");
      }
      fprintf(fp, ";\n");
      fprintf(fp, "int\t (*%sint%d[])() = { ", dp->d_name,
	      dp->d_unit);
      ip = dp->d_vec;
      for (;;) {
	fprintf(fp, "X%s%d", ip->id, dp->d_unit);
	ip = ip->id_next;
	if (ip == 0)
	  break;
	fprintf(fp, ", ");
      }
      fprintf(fp, ", 0 } ;\n");
    } else if (dp->d_type == DRIVER)  /* devices w/o interrupts */
      fprintf(fp,
	      "extern struct vba_driver %sdriver;\n",
	      dp->d_name);
  }
  fprintf(fp, "\nstruct vba_ctlr vbminit[] = {\n");
  fprintf(fp, "/*\t driver,\tctlr,\tvbanum,\talive,\tintr,\taddr */\n");
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    mp = dp->d_conn;
    if (dp->d_type != CONTROLLER || mp == TO_NEXUS || mp == 0 ||
	!eq(mp->d_name, "vba"))
      continue;
    if (dp->d_vec == 0) {
      printf("must specify vector for %s%d\n",
	     dp->d_name, dp->d_unit);
      continue;
    }
    if (dp->d_addr == 0) {
      printf("must specify csr address for %s%d\n",
	     dp->d_name, dp->d_unit);
      continue;
    }
    if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
      printf("drives need their own entries; dont ");
      printf("specify drive or slave for %s%d\n",
	     dp->d_name, dp->d_unit);
      continue;
    }
    if (dp->d_flags) {
      printf("controllers (e.g. %s%d) ",
	     dp->d_name, dp->d_unit);
      printf("don't have flags, only devices do\n");
      continue;
    }
    fprintf(fp,
	    "\t{ &%sdriver,\t%d,\t%s,\t0,\t%sint%d, C 0x%x },\n",
	    dp->d_name, dp->d_unit, qu(mp->d_unit),
	    dp->d_name, dp->d_unit, dp->d_addr);
  }
  fprintf(fp, "\t0\n};\n");
  /* versabus devices */
  fprintf(fp, "\nstruct vba_device vbdinit[] = {\n");
  fprintf(fp,
	  "\t/* driver,  unit, ctlr,  vbanum, slave,	intr,	addr,	dk, flags*/\n");
  for (dp = dtab; dp != 0; dp = dp->d_next) {
    mp = dp->d_conn;
    if (dp->d_unit == QUES || dp->d_type != DEVICE || mp == 0 ||
	mp == TO_NEXUS || mp->d_type == MASTER ||
	eq(mp->d_name, "mba"))
      continue;
    np = mp->d_conn;
    if (np != 0 && np != TO_NEXUS && eq(np->d_name, "mba"))
      continue;
    np = 0;
    if (eq(mp->d_name, "vba")) {
      if (dp->d_vec == 0)
	printf(
	       "Warning, no interrupt vector specified for device %s%d\n",
	       dp->d_name, dp->d_unit);
      if (dp->d_addr == 0) {
	printf("must specify csr for device %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
	printf("drives/slaves can be specified ");
	printf("only for controllers, ");
	printf("not for device %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      vba_n = mp->d_unit;
      slave = QUES;
    } else {
      if ((np = mp->d_conn) == 0) {
	printf("%s%d isn't connected to anything ",
	       mp->d_name, mp->d_unit);
	printf(", so %s%d is unattached\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      vba_n = np->d_unit;
      if (dp->d_drive == UNKNOWN) {
	printf("must specify ``drive number'' ");
	printf("for %s%d\n", dp->d_name, dp->d_unit);
	continue;
      }
      /* NOTE THAT ON THE UNIBUS ``drive'' IS STORED IN */
      /* ``SLAVE'' AND WE DON'T WANT A SLAVE SPECIFIED */
      if (dp->d_slave != UNKNOWN) {
	printf("slave numbers should be given only ");
	printf("for massbus tapes, not for %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_vec != 0) {
	printf("interrupt vectors should not be ");
	printf("given for drive %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      if (dp->d_addr != 0) {
	printf("csr addresses should be given only ");
	printf("on controllers, not on %s%d\n",
	       dp->d_name, dp->d_unit);
	continue;
      }
      slave = dp->d_drive;
    }
    fprintf(fp, "\t{ &%sdriver,  %2d,   %s,",
	    eq(mp->d_name, "vba") ? dp->d_name : mp->d_name, dp->d_unit,
	    eq(mp->d_name, "vba") ? " -1" : qu(mp->d_unit));
    fprintf(fp, "  %s,	%2d,	%s, C 0x%-6x,  %d,  0x%x },\n",
	    qu(vba_n), slave, intv(dp), dp->d_addr, dp->d_dk,
	    dp->d_flags);
  }
  fprintf(fp, "\t0\n};\n");
  pseudo_init(fp);
  (void) fclose(fp);
}
#endif

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
#define isscsidev(n) (eq(n,"sd") || eq(n,"st"))

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

#if MACHINE_I386
char *shandler();
char *sirq();

i386_ioconf()
{
  register struct device *dp, *mp, *np;
  register int seq = 0;
  FILE *fp;
  
  fp = fopen(path("ioconf.c"), "w");
  if (fp == 0) {
    perror(path("ioconf.c"));
    exit(1);
  }
  fprintf(fp, "/*\n");
  fprintf(fp, " * ioconf.c \n");
  fprintf(fp, " * Generated by config.old program\n");
  fprintf(fp, " */\n\n");
  fprintf(fp, "#include <sys/param.h>\n");
  fprintf(fp, "#include <sys/device.h>\n");
  fprintf(fp, "#include <sys/buf.h>\n");
  fprintf(fp, "#include <machine/pte.h>\n");
  fprintf(fp, "\n");
  fprintf(fp, "#define C (caddr_t)\n");
  fprintf(fp, "#define D &isa_devtab\n");
  fprintf(fp, "\n");
  /*
   * First print the isa initialization structures
   */
  if (seen_isa) {
    
    fprintf(fp, "/*\n");
    fprintf(fp, " * ISA devices\n");
    fprintf(fp, " */\n\n");
    fprintf(fp, "#include <i386/isa/isa_device.h>\n");
    fprintf(fp, "#include <i386/isa/isa.h>\n");
    fprintf(fp, "#include <i386/isa/icu.h>\n\n");
    
    for (dp = dtab; dp != 0; dp = dp->d_next) {
      mp = dp->d_conn;
      if (mp == 0 || mp == TO_NEXUS)
	continue;
      fprintf(fp, "extern struct cfdriver %7scd;\n",
	      dp->d_name);
      if (dp->d_irq == 2) {
	fprintf(stderr, "remapped irq 2 to irq 9, please update your config file\n");
	dp->d_irq = 9;
      }
    }

    fprintf(fp, "\nstruct isa_device isa_devtab[] = {\n");
    fprintf(fp, "\
/*    driver    iobase    irq drq      maddr   msiz unit   flags phys parent */\n");
    for (dp = dtab; dp != 0; dp = dp->d_next) {
      mp = dp->d_conn;
      if (mp == 0 || mp == TO_NEXUS)
	continue;
      dp->d_seq = seq++;
    }
    for (dp = dtab; dp != 0; dp = dp->d_next) {
      mp = dp->d_conn;
      if (mp == 0 || mp == TO_NEXUS)
	continue;
      fprintf(fp, "{ &%7scd,", dp->d_name);
      if (dp->d_port)
        fprintf(fp, " %8s,", dp->d_port);
      else
        fprintf(fp, "   0x%04x,", dp->d_portn);
      fprintf(fp, " %5s, %2d, C 0x%05x, %5d,  %2d, 0x%04x, %3d",
	      sirq(dp->d_irq), dp->d_drq, dp->d_maddr, dp->d_msize,
	      dp->d_unit, dp->d_flags,
	      eq(mp->d_name, "isa") ? 0 :
	      dp->d_drive == UNKNOWN ? dp->d_slave : dp->d_drive);
      if (eq(mp->d_name, "isa"))
        fprintf(fp, ",  NULL");
      else
        fprintf(fp, ", D[%2d]", mp->d_seq);
      fprintf(fp, " },\n");
    }
    fprintf(fp, "0\n};\n");
  }
  pseudo_init(fp);
  (void) fclose(fp);
}

/*
 * XXX - there should be a general function to print devtabs instead of these
 * little pieces of it.
 */

char *
  sirq(int i)
{
  static char buf[20];
  
#if 0
  sprintf(buf, "%d", i);
  return buf;
#else
  switch(i) {
  case -2:
    return "0";
  case -1:
    return "-1";
  default:
    sprintf(buf, "IRQ%d", i);
    return buf;
  }
#endif
}

char *
  shandler(dp)
struct device *dp;
{
  static char buf[32+20];
  
  if(dp->d_irq==-2)
    return ("NULL");
  sprintf(buf, "X%.32s%d", dp->d_name, dp->d_unit);
  return buf;
}
#endif

#if MACHINE_PC532
pc532_ioconf()
{
	struct device *dp, *mp, *np;
	int uba_n, slave;
	FILE *fp;

	fp = fopen(path("ioconf.c"), "w");
	if (fp == 0) {
		perror(path("ioconf.c"));
		exit(1);
	}
	fprintf(fp, "/*\n");
	fprintf(fp, " * ioconf.c \n");
	fprintf(fp, " * Generated by config.old program\n");
	fprintf(fp, " */\n\n");
	fprintf(fp, "#include \"../../dev/device.h\"\n");
	fprintf(fp, "#include <types.h>\n");
	fprintf(fp, "#include <cdefs.h>\n");
	fprintf(fp, "#include <device.h>\n");
	fprintf(fp, "\n");

	for (dp = dtab; dp != 0; dp = dp->d_next) {
	   mp = dp->d_conn;
	   if (dp->d_unit == QUES || mp == 0)
		continue;
	   fprintf(fp, "extern struct pc532_driver %sdriver;\n", mp->d_name);
	}

	fprintf(fp, "\nstruct pc532_device pc532_devtab_bio[] = {\n");
	fprintf(fp, "/* driver	name	unit	drive	flags alive */\n");
	for (dp = dtab; dp != 0; dp = dp->d_next) {
	    mp = dp->d_conn;
	    if (mp == 0 || !eq(dp->d_mask, "bio"))
		continue;
	    fprintf (fp, "{&%sdriver,\t\"%s\",\t%d,\t%d,\t%d,\t0},\n",
	    	mp->d_name, dp->d_name, dp->d_unit, dp->d_drive, dp->d_flags);
	}
	fprintf(fp, "0\n};\n");

	fprintf(fp, "\nstruct pc532_device pc532_devtab_tty[] = {\n");
	fprintf(fp, "/* driver	name	unit	scsiid	flags alive */\n");
	for (dp = dtab; dp != 0; dp = dp->d_next) {
	    mp = dp->d_conn;
	    if (mp == 0 || !eq(dp->d_mask, "tty"))
		continue;
	    fprintf (fp, "{&%sdriver,\t\"%s\",\t%d,\t0,\t0,\t0},\n",
	    	 mp->d_name, dp->d_name, dp->d_unit);
	}
	fprintf(fp, "0\n};\n");

	fprintf(fp, "\nstruct pc532_device pc532_devtab_net[] = {\n");
	fprintf(fp, "/* driver	unit	scsiid	alive */\n");
	fprintf(fp, "0\n};\n");

	pseudo_init(fp);
	(void) fclose(fp);
}
#endif

#if MACHINE_PMAX
pmax_ioconf()
{
	register struct device *dp, *mp;
	FILE *fp;

	fp = fopen(path("ioconf.c"), "w");
	if (fp == 0) {
		perror(path("ioconf.c"));
		exit(1);
	}
	fprintf(fp, "/*\n");
	fprintf(fp, " * ioconf.c \n");
	fprintf(fp, " * Generated by config.old program\n");
	fprintf(fp, " */\n\n");
	fprintf(fp, "#include \"sys/param.h\"\n");
	fprintf(fp, "#include \"sys/types.h\"\n");
	fprintf(fp, "#include \"sys/device.h\"\n");
	fprintf(fp, "#include \"sys/buf.h\"\n");
	fprintf(fp, "\n");
	fprintf(fp, "#include \"pmax/dev/device.h\"\n\n");
	fprintf(fp, "#define C (char *)\n\n");

	/* print controller initialization structures */
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		if (dp->d_type == PSEUDO_DEVICE)
			continue;
		fprintf(fp, "extern struct driver %sdriver;\n", dp->d_name);
	}
	fprintf(fp, "\nstruct pmax_ctlr pmax_cinit[] = {\n");
	fprintf(fp, "/*\tdriver,\t\tunit,\taddr,\t\tflags */\n");
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		if (dp->d_type != CONTROLLER && dp->d_type != MASTER)
			continue;
		if (dp->d_conn != TO_NEXUS) {
			printf("%s%s must be attached to a nexus (internal bus)\n",
				dp->d_name, wnum(dp->d_unit));
			continue;
		}
		if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
			printf("can't specify drive/slave for %s%s\n",
				dp->d_name, wnum(dp->d_unit));
			continue;
		}
		if (dp->d_unit == UNKNOWN || dp->d_unit == QUES)
			dp->d_unit = 0;
		fprintf(fp,
			"\t{ &%sdriver,\t%d,\tC 0x%x,\t0x%x },\n",
			dp->d_name, dp->d_unit, dp->d_addr, dp->d_flags);
	}
	fprintf(fp, "\t0\n};\n");

	/* print devices connected to other controllers */
	fprintf(fp, "\nstruct scsi_device scsi_dinit[] = {\n");
	fprintf(fp,
	   "/*driver,\tcdriver,\tunit,\tctlr,\tdrive,\tslave,\tdk,\tflags*/\n");
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		if (dp->d_type == CONTROLLER || dp->d_type == MASTER ||
		    dp->d_type == PSEUDO_DEVICE)
			continue;
		mp = dp->d_conn;
		if (mp == 0 || 
			(!eq(mp->d_name, "asc") && !eq(mp->d_name, "sii"))) {
			printf("%s%s: devices must be attached to a SCSI controller\n",
				dp->d_name, wnum(dp->d_unit));
			continue;
		}
		if ((unsigned)dp->d_drive > 6) {
			printf("%s%s: SCSI drive must be in the range 0..6\n",
				dp->d_name, wnum(dp->d_unit));
			continue;
		}
		/* may want to allow QUES later */
		if ((unsigned)dp->d_slave > 7) {
			printf("%s%s: SCSI slave (LUN) must be in the range 0..7\n",
				dp->d_name, wnum(dp->d_unit));
			continue;
		}
		fprintf(fp, "{ &%sdriver,\t&%sdriver,", dp->d_name, mp->d_name);
		fprintf(fp, "\t%d,\t%d,\t%d,\t%d,\t%d,\t0x%x },\n",
			dp->d_unit, mp->d_unit, dp->d_drive, dp->d_slave,
			dp->d_dk, dp->d_flags);
	}
	fprintf(fp, "0\n};\n");
	pseudo_init(fp);
	(void) fclose(fp);
}
#endif

#if MACHINE_AMIGA
amiga_ioconf()
{
	register struct device *dp, *mp, *np;
	register int controller, slave;
	FILE *fp;
	extern char *awnum();

	fp = fopen(path("ioconf.c"), "w");
	if (fp == 0) {
		perror(path("ioconf.c"));
		exit(1);
	}
	fprintf(fp, "#include \"sys/param.h\"\n");
	fprintf(fp, "#include \"sys/buf.h\"\n");
	fprintf(fp, "\n");
	fprintf(fp, "#include \"arch/amiga/dev/device.h\"\n\n");
	fprintf(fp, "\n");
	fprintf(fp, "#define C (caddr_t)\n");
	fprintf(fp, "#define D (struct driver *)\n\n");
	/*
	 * First print the controller initialization structures
	 */
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		mp = dp->d_conn;
		if (dp->d_unit == QUES || mp == 0)
			continue;
		fprintf(fp, "extern struct driver %sdriver;\n", dp->d_name);
	}
	fprintf(fp, "\nstruct amiga_ctlr amiga_cinit[] = {\n");
	fprintf(fp, "/*\tdriver,\t\tunit,\talive,\taddr,\tflags */\n");
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		mp = dp->d_conn;
		if (dp->d_unit == QUES ||
			dp->d_type != MASTER && dp->d_type != CONTROLLER)
			continue;
		if (mp != TO_NEXUS) {
			printf("%s%s must be attached to an controller\n",
				dp->d_name, awnum(dp->d_unit));
			continue;
		}
		if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
			printf("can't specify drive/slave for %s%s\n",
				dp->d_name, awnum(dp->d_unit));
			continue;
		}
		fprintf(fp,
			"\t{ &%sdriver,\t%d,\t0,\tC 0x%x,\t0x%x },\n",
			dp->d_name, dp->d_unit, dp->d_addr, dp->d_flags);
	}
	fprintf(fp, "\t0\n};\n");
/* devices */
	fprintf(fp, "\nstruct amiga_device amiga_dinit[] = {\n");
	fprintf(fp,
	   "/*driver,\tcdriver,\tunit,\tctlr,\tslave,\taddr,\tdk,\tflags*/\n");
	for (dp = dtab; dp != 0; dp = dp->d_next) {
		mp = dp->d_conn;
		if (mp == 0 || dp->d_type != DEVICE || amigabadslave(mp, dp))
			continue;
		if (mp == TO_NEXUS) {
			if (dp->d_drive != UNKNOWN || dp->d_slave != UNKNOWN) {
				printf("can't specify drive/slave for %s%s\n",
					dp->d_name, awnum(dp->d_unit));
				continue;
			}
			slave = QUES;
			controller = QUES;
		} else {
			if (dp->d_addr != 0) {
				printf("can't specify manufacturer/product for device %s%s\n",
					dp->d_name, awnum(dp->d_unit));
				continue;
			}
			if (mp->d_type == CONTROLLER) {
				if (dp->d_drive == UNKNOWN) {
					printf("must specify drive for %s%s\n",
						dp->d_name, awnum(dp->d_unit));
					continue;
				}
				slave = dp->d_drive;
			} else {
				if (dp->d_slave == UNKNOWN) {
					printf("must specify slave for %s%s\n",
						dp->d_name, awnum(dp->d_unit));
					continue;
				}
				slave = dp->d_slave;
			}
			controller = mp->d_unit;
		}
		fprintf(fp, "{ &%sdriver,\t", dp->d_name);
		if (mp == TO_NEXUS)
			fprintf(fp, "D 0x0,\t");
		else
			fprintf(fp, "&%sdriver,", mp->d_name);
		fprintf(fp, "\t%d,\t%d,\t%d,\tC 0x%x,\t%d,\t0x%x },\n",
			dp->d_unit, controller, slave,
			dp->d_addr, dp->d_dk, dp->d_flags);
	}
	fprintf(fp, "0\n};\n");
	pseudo_init(fp);
	(void) fclose(fp);
}

#define isscsidev(n) (eq(n,"sd") || eq(n,"st"))

amigabadslave(mp, dp)
	register struct device *dp, *mp;
{
	extern char *awnum();

	if (mp == TO_NEXUS && isscsidev(dp->d_name) ||
	    mp != TO_NEXUS && eq(mp->d_name, "scsi") &&
	    !isscsidev(dp->d_name)) {
		printf("%s%s must be attached to a scsi\n",
		       dp->d_name, awnum(dp->d_unit));
		return (1);
	}
	return (0);
}

char *
awnum(num)
{

	if (num == QUES || num == UNKNOWN)
		return ("?");
	(void) sprintf(errbuf, "%d", num);
	return (errbuf);
}
#endif


char *
intv(dev)
register struct device *dev;
{
	static char buf[20];
  
	if (dev->d_vec == 0)
		return ("	0");
	(void) sprintf(buf, "%sint%d", dev->d_name, dev->d_unit);
	return (buf);
}

char *
qu(num)
{
	if (num == QUES)
		return ("'?'");
	if (num == UNKNOWN)
		return (" -1");
	(void) sprintf(errbuf, "%3d", num);
	return (errbuf);
}
