/*	$OpenBSD: rf_dag.h,v 1.4 2002/12/16 07:01:03 tdeval Exp $	*/
/*	$NetBSD: rf_dag.h,v 1.3 1999/02/05 00:06:07 oster Exp $	*/

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

#ifndef	_RF__RF_DAG_H_
#define	_RF__RF_DAG_H_

#include "rf_types.h"
#include "rf_threadstuff.h"
#include "rf_alloclist.h"
#include "rf_stripelocks.h"
#include "rf_layout.h"
#include "rf_dagflags.h"
#include "rf_acctrace.h"
#include "rf_memchunk.h"

#define	RF_THREAD_CONTEXT   0	/* We were invoked from thread context. */
#define	RF_INTR_CONTEXT     1	/* We were invoked from interrupt context. */
#define	RF_MAX_ANTECEDENTS 20	/* Max num of antecedents a node may possess. */

#include <sys/buf.h>

struct RF_PropHeader_s {	/* Structure for propagation of results. */
	int		 resultNum;	/* Bind result # resultNum. */
	int		 paramNum;	/* To parameter # paramNum. */
	RF_PropHeader_t	*next;		/* Linked list for multiple
					 * results/params. */
};

typedef enum RF_NodeStatus_e {
	rf_bwd1,	/*
			 * Node is ready for undo logging
			 * (backward error recovery only).
			 */
	rf_bwd2,	/*
			 * Node has completed undo logging
			 * (backward error recovery only).
			 */
	rf_wait,	/* Node is waiting to be executed. */
	rf_fired,	/* Node is currently executing its do function. */
	rf_good,	/*
			 * Node successfully completed execution
			 * of its do function.
			 */
	rf_bad,		/*
			 * Node failed to successfully execute
			 * its do function.
			 */
	rf_skipped,	/*
			 * Not used anymore, used to imply a node
			 * was not executed.
			 */
	rf_recover,	/* Node is currently executing its undo function. */
	rf_panic,	/*
			 * Node failed to successfully execute
			 * its undo function.
			 */
	rf_undone	/* Node successfully executed its undo function. */
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
#define	RF_DAG_PTRCACHESIZE	40
#define	RF_DAG_PARAMCACHESIZE	12

typedef RF_uint8 RF_DagNodeFlags_t;

struct RF_DagNode_s {
	RF_NodeStatus_t	  status;	/* Current status of this node. */
	int		(*doFunc) (RF_DagNode_t *);
					/* Normal function. */
	int		(*undoFunc) (RF_DagNode_t *);
					/* Func to remove effect of doFunc. */
	int		(*wakeFunc) (RF_DagNode_t *, int status);
					/*
					 * Func called when the node completes
					 * an I/O.
					 */
	int		  numParams;	/*
					 * Number of parameters required
					 * by *funcPtr.
					 */
	int		  numResults;	/*
					 * Number of results produced
					 * by *funcPtr.
					 */
	int		  numAntecedents; /* Number of antecedents. */
	int		  numAntDone;	/*
					 * Number of antecedents that
					 * have finished.
					 */
	int		  numSuccedents; /* Number of succedents. */
	int		  numSuccFired;	/*
					 * Incremented when a succedent
					 * is fired during forward execution.
					 */
	int		  numSuccDone;	/*
					 * Incremented when a succedent
					 * finishes during rollBackward.
					 */
	int		  commitNode;	/*
					 * Boolean flag - if true, this is
					 * a commit node.
					 */
	RF_DagNode_t	**succedents;	/*
					 * Succedents, array size
					 * numSuccedents.
					 */
	RF_DagNode_t	**antecedents;	/*
					 * Antecedents, array size
					 * numAntecedents.
					 */
	RF_AntecedentType_t antType[RF_MAX_ANTECEDENTS];
					/* Type of each antecedent. */
	void		**results;	/*
					 * Array of results produced
					 * by *funcPtr.
					 */
	RF_DagParam_t	 *params;	/*
					 * Array of parameters required
					 * by *funcPtr.
					 */
	RF_PropHeader_t	**propList;	/*
					 * Propagation list,
					 * size numSuccedents.
					 */
	RF_DagHeader_t	 *dagHdr;	/*
					 * Ptr to head of dag containing
					 * this node.
					 */
	void		 *dagFuncData;	/*
					 * Dag execution func uses this
					 * for whatever it wants.
					 */
	RF_DagNode_t	 *next;
	int		  nodeNum;	/* Used by PrintDAG for debug only. */
	int		  visited;	/*
					 * Used to avoid re-visiting
					 * nodes on DAG walks.
					 */
	/*
	 * ANY CODE THAT USES THIS FIELD MUST MAINTAIN THE PROPERTY THAT
	 * AFTER IT FINISHES, ALL VISITED FLAGS IN THE DAG ARE IDENTICAL.
	 */

	char		 *name;		/* Debug only. */
	RF_DagNodeFlags_t flags;	/* See below. */
	RF_DagNode_t	 *dag_ptrs[RF_DAG_PTRCACHESIZE];
					/* Cache for performance. */
	RF_DagParam_t	  dag_params[RF_DAG_PARAMCACHESIZE];
					/* Cache for performance. */
};

/*
 * Bit values for flags field of RF_DagNode_t.
 */
#define	RF_DAGNODE_FLAG_NONE	0x00
#define	RF_DAGNODE_FLAG_YIELD	0x01	/*
					 * In the kernel, yield the processor
					 * before firing this node.
					 */

/*
 * rf_enable	   - DAG ready for normal execution, no errors encountered.
 * rf_rollForward  - DAG encountered an error after commit point, rolling
 *		     forward.
 * rf_rollBackward - DAG encountered an error prior to commit point, rolling
 *		     backward.
 */
typedef enum RF_DagStatus_e {
	rf_enable,
	rf_rollForward,
	rf_rollBackward
} RF_DagStatus_t;

#define	RF_MAX_HDR_SUCC		 1

#define	RF_MAXCHUNKS		10

struct RF_DagHeader_s {
	RF_DagStatus_t	  status;		/* Status of this DAG. */
	int		  numSuccedents;	/*
						 * DAG may be a tree,
						 * i.e. may have > 1 root.
						 */
	int		  numCommitNodes;	/*
						 * Number of commit nodes
						 * in graph.
						 */
	int		  numCommits;		/*
						 * Number of commit nodes
						 * that have been fired.
						 */
	RF_DagNode_t	 *succedents[RF_MAX_HDR_SUCC];	/*
							 * Array of succedents,
							 * size numSuccedents.
							 */
	RF_DagHeader_t	 *next;			/*
						 * Ptr to allow a list
						 * of dags.
						 */
	RF_AllocListElem_t *allocList;		/*
						 * Ptr to list of ptrs
						 * to be freed prior to
						 * freeing DAG.
						 */
	RF_AccessStripeMapHeader_t *asmList;	/*
						 * List of access stripe maps
						 * to be freed.
						 */
	int		  nodeNum;		/*
						 * Used by PrintDAG for
						 * debug only.
						 */
	int		  numNodesCompleted;
	RF_AccTraceEntry_t *tracerec;		/* Perf mon only. */

	void		(*cbFunc) (void *);	/*
						 * Function to call when
						 * the dag completes.
						 */
	void		 *cbArg;		/* Argument for cbFunc. */
	char		 *creator;		/*
						 * Name of function used
						 * to create this dag.
						 */

	RF_Raid_t	 *raidPtr;		/*
						 * The descriptor for the
						 * RAID device this DAG
						 * is for.
						 */
	void		 *bp;			/*
						 * The bp for this I/O passed
						 * down from the file system.
						 * ignored outside kernel.
						 */

	RF_ChunkDesc_t	 *memChunk[RF_MAXCHUNKS]; /*
						 * Experimental- Chunks of
						 * memory to be retained upon
						 * DAG free for re-use.
						 */
	int		  chunkIndex;		/*
						 * The idea is to avoid calls
						 * to alloc and free.
						 */

	RF_ChunkDesc_t	**xtraMemChunk;		/*
						 * Escape hatch that allows
						 * SelectAlgorithm to merge
						 * memChunks from several dags.
						 */
	int		  xtraChunkIndex;	/*
						 * Number of ptrs to valid
						 * chunks.
						 */
	int		  xtraChunkCnt;		/*
						 * Number of ptrs to chunks
						 * allocated.
						 */
};

struct RF_DagList_s {
	/* Common info for a list of dags that will be fired sequentially. */
	int		 numDags;	/* Number of dags in the list. */
	int		 numDagsFired;	/*
					 * Number of dags in list that
					 * have initiated execution.
					 */
	int		 numDagsDone;	/*
					 * Number of dags in list that
					 * have completed execution.
					 */
	RF_DagHeader_t	*dags;		/* List of dags. */
	RF_RaidAccessDesc_t *desc;	/* Ptr to descriptor for this access. */
	RF_AccTraceEntry_t tracerec;	/*
					 * Perf mon info for dags (not user
					 * info).
					 */
};

/* Reset a node so that it can be fired again. */
#define	RF_ResetNode(_n_)	do {					\
	(_n_)->status = rf_wait;					\
	(_n_)->numAntDone = 0;						\
	(_n_)->numSuccFired = 0;					\
	(_n_)->numSuccDone = 0;						\
	(_n_)->next = NULL;						\
} while (0)

#define	RF_ResetDagHeader(_h_)	do {					\
	(_h_)->numNodesCompleted = 0;					\
	(_h_)->numCommits = 0;						\
	(_h_)->status = rf_enable;					\
} while (0)

/* Convenience macro for declaring a create dag function. */
#define	RF_CREATE_DAG_FUNC_DECL(_name_)					\
void _name_ (RF_Raid_t *, RF_AccessStripeMap_t *, RF_DagHeader_t *,	\
    void *, RF_RaidAccessFlags_t, RF_AllocListElem_t *);		\
void _name_ (								\
	RF_Raid_t		*raidPtr,				\
	RF_AccessStripeMap_t	*asmap,					\
	RF_DagHeader_t		*dag_h,					\
	void			*bp,					\
	RF_RaidAccessFlags_t	 flags,					\
	RF_AllocListElem_t	*allocList				\
)

#endif	/* !_RF__RF_DAG_H_ */
