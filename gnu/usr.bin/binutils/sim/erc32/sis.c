/*
 * This file is part of SIS.
 * 
 * SIS, SPARC instruction simulator. Copyright (C) 1995 Jiri Gaisler, European
 * Space Agency
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675
 * Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include "sis.h"
#include <dis-asm.h>

#ifndef fprintf
extern          fprintf();
#endif

#define	VAL(x)	strtol(x,(char *)NULL,0)

extern char    *readline(char *prompt);	/* GNU readline function */

/* Command history buffer length - MUST be binary */
#define HIST_LEN	64

extern struct disassemble_info dinfo;
extern struct pstate sregs;
extern struct estate ebase;

extern int      ctrl_c;
extern int      nfp;
extern int      sis_verbose;
extern char    *sis_version;
extern struct estate ebase;
extern struct evcell evbuf[];
extern struct irqcell irqarr[];
extern int      irqpend, ext_irl;
extern char     uart_dev1[];
extern char     uart_dev2[];

#ifdef IUREV0
extern int      iurev0;
#endif

#ifdef MECREV0
extern int      mecrev0;
#endif

run_sim(sregs, go, icount, dis)
    struct pstate  *sregs;
    int             go;
    unsigned int    icount;
    int             dis;
{
    int             mexc, ws;

    sregs->starttime = time(NULL);
    while (!sregs->err_mode & (go || (icount > 0))) {
	if (sregs->bptnum && check_bpt(sregs))
	    return (BPT_HIT);
	sregs->bphit = 0;
	sregs->fhold = 0;
	sregs->hold = 0;
	sregs->icnt = 0;

	sregs->asi = 9 - ((sregs->psr & 0x080) >> 7);

#ifdef IUREV0
	if (iurev0 && sregs->rett_err) {
	    sregs->asi &= ~0x1;
	    sregs->asi |= ((sregs->psr & 0x040) >> 6);
	}
#endif

	mexc = memory_read(sregs->asi, sregs->pc, &sregs->inst, &sregs->hold);

	if (sregs->annul) {
	    sregs->annul = 0;
	    sregs->icnt = 1;
	    sregs->pc = sregs->npc;
	    sregs->npc = sregs->npc + 4;
	    mexc = 0;		/* Traps ignored during annul */
	} else {
	    check_interrupts(sregs);
	    if (sregs->trap) {
		sregs->err_mode = execute_trap(sregs);
	    } else {
		if (mexc) {
		    sregs->trap = I_ACC_EXC;
		} else {
		    if (sregs->histlen) {
			sregs->histbuf[sregs->histind].addr = sregs->pc;
			sregs->histbuf[sregs->histind].time = ebase.simtime;
			sregs->histind++;
			if (sregs->histind >= sregs->histlen)
			    sregs->histind = 0;
		    }
		    if (dis) {
			printf(" %8u ", ebase.simtime);
			dis_mem(sregs->pc, 1, &dinfo);
		    }
		    dispatch_instruction(sregs);
		}
		icount--;
	    }
	    if (sregs->trap) {
		sregs->err_mode = execute_trap(sregs);
	    }
	}
	advance_time(sregs);
	if (ctrl_c) {
	    go = icount = 0;
	}
    }
    sregs->tottime += time(NULL) - sregs->starttime;
    if (sregs->err_mode)
	error_mode(sregs->pc);
    if (sregs->err_mode)
	return (ERROR);
    if (ctrl_c) {
	ctrl_c = 0;
	return (CTRL_C);
    }
    return (TIME_OUT);
}

main(argc, argv)
    int             argc;
    char          **argv;
{

    int             cont = 1;
    int             stat = 1;
    int             freq = 14;
    int             copt = 0;

    char            lastcmd[128] = "reg";
    char           *cmd, *cfile, *bacmd;
    char           *cmdq[HIST_LEN];
    int             cmdi = 0;
    int             i;

    for (i = 0; i < 64; i++)
	cmdq[i] = 0;
    printf("\n SIS - SPARC intruction simulator %s,  copyright Jiri Gaisler 1995\n", sis_version);
    printf(" Bug-reports to jgais@wd.estec.esa.nl\n\n");
    while (stat < argc) {
	if (argv[stat][0] == '-') {
	    if (strcmp(argv[stat], "-v") == 0) {
		sis_verbose = 1;
	    } else if (strcmp(argv[stat], "-c") == 0) {
		if ((stat + 1) < argc) {
		    copt = 1;
		    cfile = argv[++stat];
		}
	    } else if (strcmp(argv[stat], "-nfp") == 0)
		nfp = 1;
#ifdef IUREV0
	    else if (strcmp(argv[stat], "-iurev0") == 0)
		iurev0 = 1;
#endif
#ifdef MECREV0
	    else if (strcmp(argv[stat], "-mecrev0") == 0)
		mecrev0 = 1;
#endif
	    else if (strcmp(argv[stat], "-uart1") == 0) {
		if ((stat + 1) < argc)
		    strcpy(uart_dev1, argv[++stat]);
	    } else if (strcmp(argv[stat], "-uart2") == 0) {
		if ((stat + 1) < argc)
		    strcpy(uart_dev2, argv[++stat]);
	    } else if (strcmp(argv[stat], "-freq") == 0) {
		if ((stat + 1) < argc)
		    freq = VAL(argv[++stat]);
	    } else {
		printf("unknown option %s\n", argv[stat]);
		usage();
		exit(1);
	    }
	} else {
	    bfd_load(argv[stat]);
	}
	stat++;
    }
#ifdef IUREV0
    if (iurev0)
	printf(" simulating IU rev.0 jmpl/restore bug\n");
#endif
#ifdef MECREV0
    if (iurev0)
	printf(" simulating MEC rev.0 timer and uart interrupt bug\n");
#endif
    if (nfp)
	printf("FPU disabled\n");
    sregs.freq = freq;

    INIT_DISASSEMBLE_INFO(dinfo, stdout, fprintf);

    using_history();
    init_signals();
    ebase.simtime = 0;
    reset_all();
    init_bpt(&sregs);
    init_sim();
#ifdef STAT
    reset_stat(&sregs);
#endif

    if (copt) {
	bacmd = (char *) malloc(256);
	strcpy(bacmd, "batch ");
	strcat(bacmd, cfile);
	exec_cmd(&sregs, bacmd);
    }
    while (cont) {

	if (cmdq[cmdi] != 0) {
	    remove_history(cmdq[cmdi]);
	    free(cmdq[cmdi]);
	    cmdq[cmdi] = 0;
	}
	cmdq[cmdi] = readline("sis> ");
	if (cmdq[cmdi] && *cmdq[cmdi])
	    add_history(cmdq[cmdi]);
	if (cmdq[cmdi])
	    stat = exec_cmd(&sregs, cmdq[cmdi]);
	else {
	    puts("\n");
	    exit(0);
	}
	switch (stat) {
	case OK:
	    break;
	case CTRL_C:
	    printf("\b\bInterrupt!\n");
	case TIME_OUT:
	    printf(" Stopped at time %d\n", ebase.simtime);
	    break;
	case BPT_HIT:
	    printf("breakpoint at 0x%08x reached\n", sregs.pc);
	    sregs.bphit = 1;
	    break;
	case ERROR:
	    printf("IU in error mode (%d)\n", sregs.trap);
	    stat = 0;
	    printf(" %8d ", ebase.simtime);
	    dis_mem(sregs.pc, 1, &dinfo);
	    break;
	default:
	    break;
	}
	ctrl_c = 0;
	stat = OK;

	cmdi = (cmdi + 1) & (HIST_LEN - 1);

    }
}
