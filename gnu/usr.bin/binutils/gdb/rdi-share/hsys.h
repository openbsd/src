/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/*
 * Host C library support header file.
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 *
 */

#ifndef angsd_hsys_h
#define angsd_hsys_h

#define HSYS_FOPEN_MAX 256
#define NONHANDLE -1
#define UNIQUETEMPS 256

#include "dbg_hif.h"
#include "hostchan.h"

typedef struct {
  FILE *FileTable[HSYS_FOPEN_MAX] ;
  char FileFlags[HSYS_FOPEN_MAX] ;
  char *TempNames[UNIQUETEMPS];
} OSblock;

#define NOOP 0
#define BINARY 1
#define READOP 2
#define WRITEOP 4

typedef struct {
  const struct Dbg_HostosInterface *hostif;  /* Interface to debug toolkit. */
  int last_errno;                              /* Number of the last error. */
  OSblock *OSptr;
  char **CommandLine ;           /* Ptr to cmd line d`string held by ardi.c */
} hsys_state;

/*
 *  Function: HostSysInit
 *   Purpose: Set up the state block, filetable and register the and C lib
 *            callback fn 
 *
 *    Params:
 *       Input: hostif, the host interface from the debug toolbox
 *              cmdline, the command line used to call the image
 *              state, the status block for the C lib 
 *
 *   Returns:
 *          OK: an RDIError_* valuee
 */
extern int HostSysInit(
  const struct Dbg_HostosInterface *hostif, char **cmdline, hsys_state **state
);

/*
 *  Function: HostSysExit
 *   Purpose: Close down the host side C library support
 *
 *    Params:
 *       Input: hstate, the status block for the C lib 
 *
 *    Returns:  an RDIError_* valuee
 */
extern int HostSysExit(hsys_state *hstate);

/*
 *  Function: HandleSysMessage
 *   Purpose: Handle an incoming C library message as a callback
 *
 *    Params:
 *       Input: packet is the incoming data packet as described in devsw.h
 *       hstate, the status block for the C lib
 *
 *    Returns:  an RDIError_* valuee
 */
extern int HandleSysMessage(Packet *packet, hsys_state* stateptr);

/*
 *  Function: panic
 *   Purpose: Print a fatal error message
 *
 *    Params:
 *       Input: format  printf() style message describing the problem
 *              ...     extra arguments for printf().
 *
 *   Returns: This routine does not return
 *
 * Post-conditions: Will have called exit(1);
 */
extern void panic(const char *format, ...);

#endif /* ndef angsd_hsys_h */

/* EOF hsys.h */
