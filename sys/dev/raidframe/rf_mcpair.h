/*	$OpenBSD: rf_mcpair.h,v 1.1 1999/01/11 14:29:29 niklas Exp $	*/
/*	$NetBSD: rf_mcpair.h,v 1.1 1998/11/13 04:20:31 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/* rf_mcpair.h
 * see comments in rf_mcpair.c
 */

#ifndef _RF__RF_MCPAIR_H_
#define _RF__RF_MCPAIR_H_

#include "rf_types.h"
#include "rf_threadstuff.h"

struct RF_MCPair_s {
  RF_DECLARE_MUTEX(mutex)
  RF_DECLARE_COND(cond)
  int           flag;
  RF_MCPair_t  *next;
};

#ifdef KERNEL
#if !defined(__NetBSD__) && !defined(__OpenBSD__)
#define RF_WAIT_MCPAIR(_mcp)  mpsleep(&((_mcp)->flag), PZERO, "mcpair", 0, (void *) simple_lock_addr((_mcp)->mutex), MS_LOCK_SIMPLE)
#else
#define RF_WAIT_MCPAIR(_mcp)  tsleep(&((_mcp)->flag), PRIBIO | PCATCH, "mcpair", 0)
#endif
#else /* KERNEL */
#define RF_WAIT_MCPAIR(_mcp)  RF_WAIT_COND((_mcp)->cond, (_mcp)->mutex)
#endif /* KERNEL */

int rf_ConfigureMCPair(RF_ShutdownList_t **listp);
RF_MCPair_t *rf_AllocMCPair(void);
void rf_FreeMCPair(RF_MCPair_t *t);
void rf_MCPairWakeupFunc(RF_MCPair_t *t);

#endif /* !_RF__RF_MCPAIR_H_ */
