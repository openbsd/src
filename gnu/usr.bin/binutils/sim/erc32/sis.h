/*
 * This file is part of SIS.
 * 
 * ERC32SIM, SPARC instruction simulator. Copyright (C) 1995 Jiri Gaisler,
 * European Space Agency
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

#include "ansidecl.h"
#include "remote-sim.h"

#include "end.h"

#define I_ACC_EXC 1

/* Maximum events in event queue */
#define EVENT_MAX	256

/* Maximum # of floating point queue */
#define FPUQN	1

/* Maximum # of breakpoints */
#define BPT_MAX	256

struct histype {
    unsigned        addr;
    unsigned        time;
};

/* type definitions */

typedef short int int16;	/* 16-bit signed int */
typedef unsigned short int uint16;	/* 16-bit unsigned int */
typedef int     int32;		/* 32-bit signed int */
typedef unsigned int uint32;	/* 32-bit unsigned int */
typedef float   float32;	/* 32-bit float */
typedef double  float64;	/* 64-bit float */

struct pstate {

    float64         fd[16];	/* FPU registers */
#ifdef HOST_LITTLE_ENDIAN_FLOAT
    float32         fs[32];
    float32        *fdp;
#else
    float32        *fs;
#endif
    int32          *fsi;
    uint32          fsr;
    int32           fpstate;
    uint32          fpq[FPUQN * 2];
    uint32          fpqn;
    uint32          ftime;
    uint32          flrd;
    uint32          frd;
    uint32          frs1;
    uint32          frs2;
    uint32          fpu_pres;	/* FPU present (0 = No, 1 = Yes) */

    uint32          psr;	/* IU registers */
    uint32          tbr;
    uint32          wim;
    uint32          g[8];
    uint32          r[128];
    uint32          y;
    uint32          pc, npc;


    uint32          trap;	/* Current trap type */
    uint32          annul;	/* Instruction annul */
    uint32          data;	/* Loaded data	     */
    uint32          inst;	/* Current instruction */
    uint32          asi;	/* Current ASI */
    uint32          err_mode;	/* IU error mode */
    uint32          breakpoint;
    uint32          bptnum;
    uint32          bphit;
    uint32          bpts[BPT_MAX];	/* Breakpoints */

    uint32          ltime;	/* Load interlock time */
    uint32          hold;	/* IU hold cycles in current inst */
    uint32          fhold;	/* FPU hold cycles in current inst */
    uint32          icnt;	/* Instruction cycles in curr inst */

    uint32          histlen;	/* Trace history management */
    uint32          histind;
    struct histype *histbuf;
    float32         freq;	/* Simulated processor frequency */


    uint32          tottime;
    uint32          ninst;
    uint32          fholdt;
    uint32          holdt;
    uint32          icntt;
    uint32          finst;
    uint32          simstart;
    uint32          starttime;
    uint32          pwdtime;	/* Cycles in power-down mode */
    uint32          nstore;	/* Number of load instructions */
    uint32          nload;	/* Number of store instructions */
    uint32          nannul;	/* Number of annuled instructions */
    uint32          nbranch;	/* Number of branch instructions */
    uint32          ildreg;	/* Destination of last load instruction */
    uint32          ildtime;	/* Last time point for load dependency */

    int             rett_err;	/* IU in jmpl/restore error state (Rev.0) */
    int             jmpltime;
};

struct evcell {
    void            (*cfunc) ();
    int32           arg;
    uint32          time;
    struct evcell  *nxt;
};

struct estate {
    struct evcell   eq;
    struct evcell  *freeq;
    uint32          simtime;
};

struct irqcell {
    void            (*callback) ();
    int32           arg;
};


#define OK 0
#define TIME_OUT 1
#define BPT_HIT 2
#define ERROR 3
#define CTRL_C 4
