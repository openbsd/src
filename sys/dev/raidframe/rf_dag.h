/*	$OpenBSD: rf_dag.h,v 1.1 1999/01/11 14:29:06 niklas Exp $	*/
/*	$NetBSD: rf_dag.h,v 1.1 1998/11/13 04:20:27 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II, Mark Holland
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

/****************************************************************************
 *                                                                          *
 * dag.h -- header file for DAG-related data structures                     *
 *                                                                          *
 ****************************************************************************/
/*
 *
 * :  
 * Log: rf_dag.h,v 
 * Revision 1.35  1996/11/05 18:38:37  jimz
 * add patch from galvarez@cs.ucsd.edu (Guillermo Alvarez)
 * to fix dag_params memory-sizing problem (should be an array
 * of the type, not an array of pointers to the type)
 *
 * Revision 1.34  1996/07/28  20:31:39  jimz
 * i386netbsd port
 * true/false fixup
 *
 * Revision 1.33  1996/07/22  19:52:16  jimz
 * switched node params to RF_DagParam_t, a union of
 * a 64-bit int and a void *, for better portability
 * attempted hpux port, but failed partway through for
 * lack of a single C compiler capable of compiling all
 * source files
 *
 * Revision 1.32  1996/06/10  22:22:13  wvcii
 * added two node status types for use in backward error
 * recovery experiments.
 *
 * Revision 1.31  1996/06/09  02:36:46  jimz
 * lots of little crufty cleanup- fixup whitespace
 * issues, comment #ifdefs, improve typing in some
 * places (esp size-related)
 *
 * Revision 1.30  1996/06/07  22:49:18  jimz
 * fix up raidPtr typing
 *
 * Revision 1.29  1996/05/27  18:56:37  jimz
 * more code cleanup
 * better typing
 * compiles in all 3 environments
 *
 * Revision 1.28  1996/05/24  22:17:04  jimz
 * continue code + namespace cleanup
 * typed a bunch of flags
 *
 * Revision 1.27  1996/05/24  04:28:55  jimz
 * release cleanup ckpt
 *
 * Revision 1.26  1996/05/23  21:46:35  jimz
 * checkpoint in code cleanup (release prep)
 * lots of types, function names have been fixed
 *
 * Revision 1.25  1996/05/23  00:33:23  jimz
 * code cleanup: move all debug decls to rf_options.c, all extern
 * debug decls to rf_options.h, all debug vars preceded by rf_
 *
 * Revision 1.24  1996/05/18  19:51:34  jimz
 * major code cleanup- fix syntax, make some types consistent,
 * add prototypes, clean out dead code, et cetera
 *
 * Revision 1.23  1996/05/16  23:05:20  jimz
 * Added dag_ptrs field, RF_DAG_PTRCACHESIZE
 *
 * The dag_ptrs field of the node is basically some scribble
 * space to be used here. We could get rid of it, and always
 * allocate the range of pointers, but that's expensive. So,
 * we pick a "common case" size for the pointer cache. Hopefully,
 * we'll find that:
 * (1) Generally, nptrs doesn't exceed RF_DAG_PTRCACHESIZE by
 *     only a little bit (least efficient case)
 * (2) Generally, ntprs isn't a lot less than RF_DAG_PTRCACHESIZE
 *     (wasted memory)
 *
 * Revision 1.22  1996/05/08  21:01:24  jimz
 * fixed up enum type names that were conflicting with other
 * enums and function names (ie, "panic")
 * future naming trends will be towards RF_ and rf_ for
 * everything raidframe-related
 *
 * Revision 1.21  1996/05/08  15:23:47  wvcii
 * added new node states:  undone, recover, panic
 *
 * Revision 1.20  1995/12/01  14:59:19  root
 * increased MAX_ANTECEDENTS from 10 to 20
 * should consider getting rid of this (eliminate static array)
 *
 * Revision 1.19  1995/11/30  15:58:59  wvcii
 * added copyright info
 *
 * Revision 1.18  1995/11/19  16:27:03  wvcii
 * created struct dagList
 *
 * Revision 1.17  1995/11/07  15:43:01  wvcii
 * added static array to DAGnode: antType
 * added commitNode type
 * added commit node counts to dag header
 * added ptr (firstDag) to support multi-dag requests
 * added succedent done/fired counts to nodes to support rollback
 * added node status type "skipped"
 * added hdr status types "rollForward, rollBackward"
 * deleted hdr status type "disable"
 * updated ResetNode & ResetDAGHeader to zero new fields
 *
 */

#ifndef _RF__RF_DAG_H_
#define _RF__RF_DAG_H_

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_alloclist.h"
#include "rf_stripelocks.h"
#include "rf_layout.h"
#include "rf_dagflags.h"
#include "rf_acctrace.h"
#include "rf_memchunk.h"

#define RF_THREAD_CONTEXT   0 /* we were invoked from thread context */
#define RF_INTR_CONTEXT     1 /* we were invoked from interrupt context */
#define RF_MAX_ANTECEDENTS 20 /* max num of antecedents a node may posses */

#ifdef KERNEL
#include <sys/buf.h>
#endif /* KERNEL */

struct RF_PropHeader_s { /* structure for propagation of results */
  int               resultNum; /* bind result # resultNum */
  int               paramNum;  /* to parameter # paramNum */
  RF_PropHeader_t  *next;      /* linked list for multiple results/params */
};

typedef enum RF_NodeStatus_e {
  rf_bwd1,    /* node is ready for undo logging (backward error recovery only) */
  rf_bwd2,    /* node has completed undo logging (backward error recovery only) */
  rf_wait,    /* node is waiting to be executed */
  rf_fired,   /* node is currently executing its do function */
  rf_good,    /* node successfully completed execution of its do function */
  rf_bad,     /* node failed to successfully execute its do function */
  rf_skipped, /* not used anymore, used to imply a node was not executed */
  rf_recover, /* node is currently executing its undo function */
  rf_panic,   /* node failed to successfully execute its undo function */
  rf_undone   /* node successfully executed its undo function */
} RF_NodeStatus_t;

/*
 * These were used to control skipping a node.
 * Now, these are only used as comments.
 */
typedef enum RF_AntecedentType_e {
  rf_trueData,
  rf_antiData,
  rf_outputData,
  rf_control
} RF_AntecedentType_t;

#define RF_DAG_PTRCACHESIZE   40
#define RF_DAG_PARAMCACHESIZE 12

typedef RF_uint8 RF_DagNodeFlags_t;

struct RF_DagNode_s {
  RF_NodeStatus_t status;          /* current status of this node */
  int (*doFunc)(RF_DagNode_t *);   /* normal function */
  int (*undoFunc)(RF_DagNode_t *); /* func to remove effect of doFunc */
  int (*wakeFunc)(RF_DagNode_t *, int status);  /* func called when the node completes an I/O */
  int numParams;                /* number of parameters required by *funcPtr */
  int numResults;               /* number of results produced by *funcPtr */
  int numAntecedents;           /* number of antecedents */
  int numAntDone;               /* number of antecedents which have finished */
  int numSuccedents;            /* number of succedents */
  int numSuccFired;             /* incremented when a succedent is fired during forward execution */
  int numSuccDone;              /* incremented when a succedent finishes during rollBackward */
  int commitNode;               /* boolean flag - if true, this is a commit node */
  RF_DagNode_t **succedents;    /* succedents, array size numSuccedents */
  RF_DagNode_t **antecedents;   /* antecedents, array size numAntecedents */
  RF_AntecedentType_t antType[RF_MAX_ANTECEDENTS]; /* type of each antecedent */
  void **results;               /* array of results produced by *funcPtr */
  RF_DagParam_t *params;        /* array of parameters required by *funcPtr */
  RF_PropHeader_t **propList;   /* propagation list, size numSuccedents */
  RF_DagHeader_t *dagHdr;       /* ptr to head of dag containing this node */
  void *dagFuncData;            /* dag execution func uses this for whatever it wants */
  RF_DagNode_t *next;
  int nodeNum;                  /* used by PrintDAG for debug only */
  int visited;                  /* used to avoid re-visiting nodes on DAG walks */
                                /* ANY CODE THAT USES THIS FIELD MUST MAINTAIN THE PROPERTY
                                 * THAT AFTER IT FINISHES, ALL VISITED FLAGS IN THE DAG ARE IDENTICAL */
  char *name;                   /* debug only */
  RF_DagNodeFlags_t flags;      /* see below */
  RF_DagNode_t  *dag_ptrs[RF_DAG_PTRCACHESIZE];     /* cache for performance */
  RF_DagParam_t  dag_params[RF_DAG_PARAMCACHESIZE]; /* cache for performance */
};

/*
 * Bit values for flags field of RF_DagNode_t
 */
#define RF_DAGNODE_FLAG_NONE  0x00
#define RF_DAGNODE_FLAG_YIELD 0x01 /* in the kernel, yield the processor before firing this node */

/* enable - DAG ready for normal execution, no errors encountered
 * rollForward - DAG encountered an error after commit point, rolling forward
 * rollBackward - DAG encountered an error prior to commit point, rolling backward
 */
typedef enum RF_DagStatus_e {
  rf_enable,
  rf_rollForward,
  rf_rollBackward
} RF_DagStatus_t;

#define RF_MAX_HDR_SUCC 1

#define RF_MAXCHUNKS 10

struct RF_DagHeader_s {
  RF_DagStatus_t status;               /* status of this DAG */
  int numSuccedents;                   /* DAG may be a tree, i.e. may have > 1 root */
  int numCommitNodes;                  /* number of commit nodes in graph */
  int numCommits;                      /* number of commit nodes which have been fired  */
  RF_DagNode_t *succedents[RF_MAX_HDR_SUCC]; /* array of succedents, size numSuccedents */
  RF_DagHeader_t *next;                /* ptr to allow a list of dags */
  RF_AllocListElem_t *allocList;       /* ptr to list of ptrs to be freed prior to freeing DAG */
  RF_AccessStripeMapHeader_t *asmList; /* list of access stripe maps to be freed */
  int nodeNum;                         /* used by PrintDAG for debug only */
  int numNodesCompleted;
  RF_AccTraceEntry_t *tracerec;        /* perf mon only */

  void (*cbFunc)(void *);        /* function to call when the dag completes */
  void *cbArg;                         /* argument for cbFunc */
  char *creator;                       /* name of function used to create this dag */

  RF_Raid_t *raidPtr;                  /* the descriptor for the RAID device this DAG is for */
  void *bp;                            /* the bp for this I/O passed down from the file system. ignored outside kernel */

  RF_ChunkDesc_t *memChunk[RF_MAXCHUNKS]; /* experimental- Chunks of memory to be retained upon DAG free for re-use */
  int chunkIndex;                      /* the idea is to avoid calls to alloc and free */

  RF_ChunkDesc_t **xtraMemChunk;       /* escape hatch which allows SelectAlgorithm to merge memChunks from several dags */
  int xtraChunkIndex;                  /* number of ptrs to valid chunks */
  int xtraChunkCnt;                    /* number of ptrs to chunks allocated */

#ifdef SIMULATE
  int done;                            /* Tag to tell if termination node has been fired */
#endif /* SIMULATE */
};

struct RF_DagList_s {
  /* common info for a list of dags which will be fired sequentially */
  int numDags;                 /* number of dags in the list */
  int numDagsFired;            /* number of dags in list which have initiated execution */
  int numDagsDone;             /* number of dags in list which have completed execution */
  RF_DagHeader_t *dags;        /* list of dags */
  RF_RaidAccessDesc_t *desc;   /* ptr to descriptor for this access */
  RF_AccTraceEntry_t tracerec; /* perf mon info for dags (not user info) */
};

/* resets a node so that it can be fired again */
#define RF_ResetNode(_n_)  { \
  (_n_)->status = rf_wait;   \
  (_n_)->numAntDone = 0;     \
  (_n_)->numSuccFired = 0;   \
  (_n_)->numSuccDone = 0;    \
  (_n_)->next = NULL;        \
}

#ifdef SIMULATE
#define RF_ResetDagHeader(_h_) { \
  (_h_)->done = RF_FALSE;           \
  (_h_)->numNodesCompleted = 0;  \
  (_h_)->numCommits = 0;         \
  (_h_)->status = rf_enable;     \
}
#else /* SIMULATE */
#define RF_ResetDagHeader(_h_) { \
  (_h_)->numNodesCompleted = 0;  \
  (_h_)->numCommits = 0;         \
  (_h_)->status = rf_enable;     \
}
#endif /* SIMULATE */

/* convience macro for declaring a create dag function */

#define RF_CREATE_DAG_FUNC_DECL(_name_) \
void _name_ ( \
	RF_Raid_t             *raidPtr, \
	RF_AccessStripeMap_t  *asmap, \
	RF_DagHeader_t        *dag_h, \
	void                  *bp, \
	RF_RaidAccessFlags_t   flags, \
	RF_AllocListElem_t    *allocList)

#endif /* !_RF__RF_DAG_H_ */
