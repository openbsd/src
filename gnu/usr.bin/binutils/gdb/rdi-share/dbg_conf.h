/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * ARM symbolic debugger toolbox: dbg_conf.h
 */

/*
 * RCS $Revision: 1.3 $
 * Checkin $Date: 2004/12/27 14:00:54 $
 */

#ifndef Dbg_Conf__h

#define Dbg_Conf__h

typedef struct Dbg_ConfigBlock {
    int bytesex;
    int fpe;               /* Target should initialise FPE */
    long memorysize;
    unsigned long cpu_speed;/* Cpu speed (HZ) */
    int serialport;        /*) remote connection parameters */
    int seriallinespeed;   /*) (serial connection) */
    int parallelport;      /*) ditto */
    int parallellinespeed; /*) (parallel connection) */
    char *ethernettarget;  /* name of remote ethernet target */
    int processor;         /* processor the armulator is to emulate (eg ARM60) */
    int rditype;           /* armulator / remote processor */
    int heartbeat_on;  /* angel heartbeat */
    int drivertype;        /* parallel / serial / etc */
    char const *configtoload;
    char const *memconfigtoload;
    int flags;
} Dbg_ConfigBlock;

#define Dbg_ConfigFlag_Reset 1
#define Dbg_ConfigFlag_LLSymsNeedPrefix 2

typedef struct Dbg_HostosInterface Dbg_HostosInterface;
/* This structure allows access by the (host-independent) C-library support
   module of armulator or pisd (armos.c) to host-dependent functions for
   which there is no host-independent interface.  Its contents are unknown
   to the debugger toolbox.
   The assumption is that, in a windowed system, fputc(stderr) for example
   may not achieve the desired effect of the character appearing in some
   window.
 */

#endif
