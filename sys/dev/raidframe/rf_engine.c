/*	$OpenBSD: rf_engine.c,v 1.14 2003/01/19 14:32:00 tdeval Exp $	*/
/*	$NetBSD: rf_engine.c,v 1.10 2000/08/20 16:51:03 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II, Mark Holland, Rachad Youssef
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
 * engine.c -- Code for DAG execution engine.                               *
 *                                                                          *
 * Modified to work as follows (holland):                                   *
 *   A user-thread calls into DispatchDAG, which fires off the nodes that   *
 *   are direct successors to the header node. DispatchDAG then returns,    *
 *   and the rest of the I/O continues asynchronously. As each node         *
 *   completes, the node execution function calls FinishNode(). FinishNode  *
 *   scans the list of successors to the node and increments the antecedent *
 *   counts. Each node that becomes enabled is placed on a central node     *
 *   queue. A dedicated dag-execution thread grabs nodes off of this        *
 *   queue and fires them.                                                  *
 *                                                                          *
 *   NULL nodes are never fired.                                            *
 *                                                                          *
 *   Terminator nodes are never fired, but rather cause the callback        *
 *   associated with the DAG to be invoked.                                 *
 *                                                                          *
 *   If a node fails, the dag either rolls forward to the completion or     *
 *   rolls back, undoing previously-completed nodes and fails atomically.   *
 *   The direction of recovery is determined by the location of the failed  *
 *   node in the graph. If the failure occurred before the commit node in   *
 *   the graph, backward recovery is used. Otherwise, forward recovery is   *
 *   used.                                                                  *
 *                                                                          *
 ****************************************************************************/

#include "rf_threadstuff.h"

#include <sys/errno.h>

#include "rf_dag.h"
#include "rf_engine.h"
#include "rf_etimer.h"
#include "rf_general.h"
#include "rf_dagutils.h"
#include "rf_shutdown.h"
#include "rf_raid.h"

int  rf_BranchDone(RF_DagNode_t *);
int  rf_NodeReady(RF_DagNode_t *);
void rf_FireNode(RF_DagNode_t *);
void rf_FireNodeArray(int, RF_DagNode_t **);
void rf_FireNodeList(RF_DagNode_t *);
void rf_PropagateResults(RF_DagNode_t *, int);
void rf_ProcessNode(RF_DagNode_t *, int);

void rf_DAGExecutionThread(RF_ThreadArg_t);
#ifdef	RAID_AUTOCONFIG
#define	RF_ENGINE_PID	10
void rf_DAGExecutionThread_pre(RF_ThreadArg_t);
extern pid_t	  lastpid;
#endif	/* RAID_AUTOCONFIG */
void		**rf_hook_cookies;
extern int	  numraid;

#define	DO_INIT(_l_,_r_)						\
do {									\
	int _rc;							\
	_rc = rf_create_managed_mutex(_l_, &(_r_)->node_queue_mutex);	\
	if (_rc) {							\
		return(_rc);						\
	}								\
	_rc = rf_create_managed_cond(_l_, &(_r_)->node_queue_cond);	\
	if (_rc) {							\
		return(_rc);						\
	}								\
} while (0)

/*
 * Synchronization primitives for this file. DO_WAIT should be enclosed
 * in a while loop.
 */

/*
 * XXX Is this spl-ing really necessary ?
 */
#define	DO_LOCK(_r_)							\
do {									\
	ks = splbio();							\
	RF_LOCK_MUTEX((_r_)->node_queue_mutex);				\
} while (0)

#define	DO_UNLOCK(_r_)							\
do {									\
	RF_UNLOCK_MUTEX((_r_)->node_queue_mutex);			\
	splx(ks);							\
} while (0)

#define	DO_WAIT(_r_)							\
	RF_WAIT_COND((_r_)->node_queue, (_r_)->node_queue_mutex)

/* XXX RF_SIGNAL_COND? */
#define	DO_SIGNAL(_r_)							\
	RF_BROADCAST_COND((_r_)->node_queue)

void rf_ShutdownEngine(void *);

void
rf_ShutdownEngine(void *arg)
{
	RF_Raid_t *raidPtr;

	raidPtr = (RF_Raid_t *) arg;
	raidPtr->shutdown_engine = 1;
	DO_SIGNAL(raidPtr);
}

int
rf_ConfigureEngine(RF_ShutdownList_t **listp, RF_Raid_t *raidPtr,
    RF_Config_t *cfgPtr)
{
	int rc;
	char raidname[16];

	DO_INIT(listp, raidPtr);

	raidPtr->node_queue = NULL;
	raidPtr->dags_in_flight = 0;

	rc = rf_init_managed_threadgroup(listp, &raidPtr->engine_tg);
	if (rc)
		return (rc);

	/*
	 * We create the execution thread only once per system boot. No need
	 * to check return code b/c the kernel panics if it can't create the
	 * thread.
	 */
	if (rf_engineDebug) {
		printf("raid%d: %s engine thread\n", raidPtr->raidid,
		    (initproc)?"Starting":"Creating");
	}
	if (rf_hook_cookies == NULL) {
		rf_hook_cookies =
		    malloc(numraid * sizeof(void *),
			   M_RAIDFRAME, M_NOWAIT);
		if (rf_hook_cookies == NULL)
			return (ENOMEM);
		bzero(rf_hook_cookies, numraid * sizeof(void *));
	}
#ifdef	RAID_AUTOCONFIG
	if (initproc == NULL) {
		rf_hook_cookies[raidPtr->raidid] =
			startuphook_establish(rf_DAGExecutionThread_pre,
			    raidPtr);
	} else {
#endif	/* RAID_AUTOCONFIG */
		snprintf(&raidname[0], 16, "raid%d", raidPtr->raidid);
		if (RF_CREATE_THREAD(raidPtr->engine_thread,
		    rf_DAGExecutionThread, raidPtr, &raidname[0])) {
			RF_ERRORMSG("RAIDFRAME: Unable to start engine"
			    " thread\n");
			return (ENOMEM);
		}
		if (rf_engineDebug) {
			printf("raid%d: Engine thread started\n",
			    raidPtr->raidid);
		}
		RF_THREADGROUP_STARTED(&raidPtr->engine_tg);
#ifdef	RAID_AUTOCONFIG
	}
#endif
	/* XXX Something is missing here... */
#ifdef	debug
	printf("Skipping the WAIT_START !!!\n");
#endif
	/* Engine thread is now running and waiting for work. */
	if (rf_engineDebug) {
		printf("raid%d: Engine thread running and waiting for events\n",
		    raidPtr->raidid);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownEngine, raidPtr);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d"
		    " rc=%d\n", __FILE__, __LINE__, rc);
		rf_ShutdownEngine(NULL);
	}
	return (rc);
}

int
rf_BranchDone(RF_DagNode_t *node)
{
	int i;

	/*
	 * Return true if forward execution is completed for a node and it's
	 * succedents.
	 */
	switch (node->status) {
	case rf_wait:
		/* Should never be called in this state. */
		RF_PANIC();
		break;
	case rf_fired:
		/* Node is currently executing, so we're not done. */
		return (RF_FALSE);
	case rf_good:
		/* For each succedent. */
		for (i = 0; i < node->numSuccedents; i++)
			/* Recursively check branch. */
			if (!rf_BranchDone(node->succedents[i]))
				return RF_FALSE;

		return RF_TRUE;	/*
				 * Node and all succedent branches aren't in
				 * fired state.
				 */
		break;
	case rf_bad:
		/* Succedents can't fire. */
		return (RF_TRUE);
	case rf_recover:
		/* Should never be called in this state. */
		RF_PANIC();
		break;
	case rf_undone:
	case rf_panic:
		/* XXX Need to fix this case. */
		/* For now, assume that we're done. */
		return (RF_TRUE);
		break;
	default:
		/* Illegal node status. */
		RF_PANIC();
		break;
	}
}

int
rf_NodeReady(RF_DagNode_t *node)
{
	int ready;

	switch (node->dagHdr->status) {
	case rf_enable:
	case rf_rollForward:
		if ((node->status == rf_wait) &&
		    (node->numAntecedents == node->numAntDone))
			ready = RF_TRUE;
		else
			ready = RF_FALSE;
		break;
	case rf_rollBackward:
		RF_ASSERT(node->numSuccDone <= node->numSuccedents);
		RF_ASSERT(node->numSuccFired <= node->numSuccedents);
		RF_ASSERT(node->numSuccFired <= node->numSuccDone);
		if ((node->status == rf_good) &&
		    (node->numSuccDone == node->numSuccedents))
			ready = RF_TRUE;
		else
			ready = RF_FALSE;
		break;
	default:
		printf("Execution engine found illegal DAG status"
		    " in rf_NodeReady\n");
		RF_PANIC();
		break;
	}

	return (ready);
}


/*
 * User context and dag-exec-thread context:
 * Fire a node. The node's status field determines which function, do or undo,
 * to be fired.
 * This routine assumes that the node's status field has alread been set to
 * "fired" or "recover" to indicate the direction of execution.
 */
void
rf_FireNode(RF_DagNode_t *node)
{
	switch (node->status) {
	case rf_fired:
		/* Fire the do function of a node. */
		if (rf_engineDebug>1) {
			printf("raid%d: Firing node 0x%lx (%s)\n",
			    node->dagHdr->raidPtr->raidid,
			    (unsigned long) node, node->name);
		}
		if (node->flags & RF_DAGNODE_FLAG_YIELD) {
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
			/* thread_block(); */
			/* printf("Need to block the thread here...\n");  */
			/*
			 * XXX thread_block is actually mentioned in
			 * /usr/include/vm/vm_extern.h
			 */
#else
			thread_block();
#endif
		}
		(*(node->doFunc)) (node);
		break;
	case rf_recover:
		/* Fire the undo function of a node. */
		if (rf_engineDebug>1) {
			printf("raid%d: Firing (undo) node 0x%lx (%s)\n",
			    node->dagHdr->raidPtr->raidid,
			    (unsigned long) node, node->name);
		}
		if (node->flags & RF_DAGNODE_FLAG_YIELD) {
#if (defined(__NetBSD__) || defined(__OpenBSD__)) && defined(_KERNEL)
			/* thread_block(); */
			/* printf("Need to block the thread here...\n"); */
			/*
			 * XXX thread_block is actually mentioned in
			 * /usr/include/vm/vm_extern.h
			 */
#else
			thread_block();
#endif
		}
		(*(node->undoFunc)) (node);
		break;
	default:
		RF_PANIC();
		break;
	}
}


/*
 * User context:
 * Attempt to fire each node in a linear array.
 * The entire list is fired atomically.
 */
void
rf_FireNodeArray(int numNodes, RF_DagNode_t **nodeList)
{
	RF_DagStatus_t dstat;
	RF_DagNode_t *node;
	int i, j;

	/* First, mark all nodes which are ready to be fired. */
	for (i = 0; i < numNodes; i++) {
		node = nodeList[i];
		dstat = node->dagHdr->status;
		RF_ASSERT((node->status == rf_wait) ||
		    (node->status == rf_good));
		if (rf_NodeReady(node)) {
			if ((dstat == rf_enable) || (dstat == rf_rollForward)) {
				RF_ASSERT(node->status == rf_wait);
				if (node->commitNode)
					node->dagHdr->numCommits++;
				node->status = rf_fired;
				for (j = 0; j < node->numAntecedents; j++)
					node->antecedents[j]->numSuccFired++;
			} else {
				RF_ASSERT(dstat == rf_rollBackward);
				RF_ASSERT(node->status == rf_good);
				/* Only one commit node per graph. */
				RF_ASSERT(node->commitNode == RF_FALSE);
				node->status = rf_recover;
			}
		}
	}
	/* Now, fire the nodes. */
	for (i = 0; i < numNodes; i++) {
		if ((nodeList[i]->status == rf_fired) ||
		    (nodeList[i]->status == rf_recover))
			rf_FireNode(nodeList[i]);
	}
}


/*
 * User context:
 * Attempt to fire each node in a linked list.
 * The entire list is fired atomically.
 */
void
rf_FireNodeList(RF_DagNode_t *nodeList)
{
	RF_DagNode_t *node, *next;
	RF_DagStatus_t dstat;
	int j;

	if (nodeList) {
		/* First, mark all nodes which are ready to be fired. */
		for (node = nodeList; node; node = next) {
			next = node->next;
			dstat = node->dagHdr->status;
			RF_ASSERT((node->status == rf_wait) ||
			    (node->status == rf_good));
			if (rf_NodeReady(node)) {
				if ((dstat == rf_enable) ||
				    (dstat == rf_rollForward)) {
					RF_ASSERT(node->status == rf_wait);
					if (node->commitNode)
						node->dagHdr->numCommits++;
					node->status = rf_fired;
					for (j = 0; j < node->numAntecedents;
					     j++)
						node->antecedents[j]
						    ->numSuccFired++;
				} else {
					RF_ASSERT(dstat == rf_rollBackward);
					RF_ASSERT(node->status == rf_good);
					/* Only one commit node per graph. */
					RF_ASSERT(node->commitNode == RF_FALSE);
					node->status = rf_recover;
				}
			}
		}
		/* Now, fire the nodes. */
		for (node = nodeList; node; node = next) {
			next = node->next;
			if ((node->status == rf_fired) ||
			    (node->status == rf_recover))
				rf_FireNode(node);
		}
	}
}


/*
 * Interrupt context:
 * For each succedent,
 *    propagate required results from node to succedent.
 *    increment succedent's numAntDone.
 *    place newly-enable nodes on node queue for firing.
 *
 * To save context switches, we don't place NIL nodes on the node queue,
 * but rather just process them as if they had fired. Note that NIL nodes
 * that are the direct successors of the header will actually get fired by
 * DispatchDAG, which is fine because no context switches are involved.
 *
 * Important:  when running at user level, this can be called by any
 * disk thread, and so the increment and check of the antecedent count
 * must be locked. I used the node queue mutex and locked down the
 * entire function, but this is certainly overkill.
 */
void
rf_PropagateResults(RF_DagNode_t *node, int context)
{
	RF_DagNode_t *s, *a;
	RF_Raid_t *raidPtr;
	int i, ks;
	/* A list of NIL nodes to be finished. */
	RF_DagNode_t *finishlist = NULL;
	/* List of nodes with failed truedata antecedents. */
	RF_DagNode_t *skiplist = NULL;
	RF_DagNode_t *firelist = NULL;	/* A list of nodes to be fired. */
	RF_DagNode_t *q = NULL, *qh = NULL, *next;
	int j, skipNode;

	raidPtr = node->dagHdr->raidPtr;

	DO_LOCK(raidPtr);

	/* Debug - validate fire counts. */
	for (i = 0; i < node->numAntecedents; i++) {
		a = *(node->antecedents + i);
		RF_ASSERT(a->numSuccFired >= a->numSuccDone);
		RF_ASSERT(a->numSuccFired <= a->numSuccedents);
		a->numSuccDone++;
	}

	switch (node->dagHdr->status) {
	case rf_enable:
	case rf_rollForward:
		for (i = 0; i < node->numSuccedents; i++) {
			s = *(node->succedents + i);
			RF_ASSERT(s->status == rf_wait);
			(s->numAntDone)++;
			if (s->numAntDone == s->numAntecedents) {
				/* Look for NIL nodes. */
				if (s->doFunc == rf_NullNodeFunc) {
					/*
					 * Don't fire NIL nodes, just process
					 * them.
					 */
					s->next = finishlist;
					finishlist = s;
				} else {
					/*
					 * Look to see if the node is to be
					 * skipped.
					 */
					skipNode = RF_FALSE;
					for (j = 0; j < s->numAntecedents; j++)
						if ((s->antType[j] ==
						     rf_trueData) &&
						    (s->antecedents[j]->status
						     == rf_bad))
							skipNode = RF_TRUE;
					if (skipNode) {
						/*
						 * This node has one or more
						 * failed true data
						 * dependencies, so skip it.
						 */
						s->next = skiplist;
						skiplist = s;
					} else {
						/*
						 * Add s to list of nodes (q)
						 * to execute.
						 */
						if (context != RF_INTR_CONTEXT)
						{
							/*
							 * We only have to
							 * enqueue if we're at
							 * intr context.
							 */
							/*
							 * Put node on a list to
							 * be fired after we
							 * unlock.
							 */
							s->next = firelist;
							firelist = s;
						} else {
							/*
							 * Enqueue the node for
							 * the dag exec thread
							 * to fire.
							 */
						     RF_ASSERT(rf_NodeReady(s));
							if (q) {
								q->next = s;
								q = s;
							} else {
								qh = q = s;
								qh->next = NULL;
							}
						}
					}
				}
			}
		}

		if (q) {
			/*
			 * Transfer our local list of nodes to the node
			 * queue.
			 */
			q->next = raidPtr->node_queue;
			raidPtr->node_queue = qh;
			DO_SIGNAL(raidPtr);
		}
		DO_UNLOCK(raidPtr);

		for (; skiplist; skiplist = next) {
			next = skiplist->next;
			skiplist->status = rf_skipped;
			for (i = 0; i < skiplist->numAntecedents; i++) {
				skiplist->antecedents[i]->numSuccFired++;
			}
			if (skiplist->commitNode) {
				skiplist->dagHdr->numCommits++;
			}
			rf_FinishNode(skiplist, context);
		}
		for (; finishlist; finishlist = next) {
			/* NIL nodes: no need to fire them. */
			next = finishlist->next;
			finishlist->status = rf_good;
			for (i = 0; i < finishlist->numAntecedents; i++) {
				finishlist->antecedents[i]->numSuccFired++;
			}
			if (finishlist->commitNode)
				finishlist->dagHdr->numCommits++;
			/*
			 * Okay, here we're calling rf_FinishNode() on nodes
			 * that have the null function as their work proc.
			 * Such a node could be the terminal node in a DAG.
			 * If so, it will cause the DAG to complete, which will
			 * in turn free memory used by the DAG, which includes
			 * the node in question.
			 * Thus, we must avoid referencing the node at all
			 * after calling rf_FinishNode() on it.
			 */
			/* Recursive call. */
			rf_FinishNode(finishlist, context);
		}
		/* Fire all nodes in firelist. */
		rf_FireNodeList(firelist);
		break;

	case rf_rollBackward:
		for (i = 0; i < node->numAntecedents; i++) {
			a = *(node->antecedents + i);
			RF_ASSERT(a->status == rf_good);
			RF_ASSERT(a->numSuccDone <= a->numSuccedents);
			RF_ASSERT(a->numSuccDone <= a->numSuccFired);

			if (a->numSuccDone == a->numSuccFired) {
				if (a->undoFunc == rf_NullNodeFunc) {
					/*
					 * Don't fire NIL nodes, just process
					 * them.
					 */
					a->next = finishlist;
					finishlist = a;
				} else {
					if (context != RF_INTR_CONTEXT) {
						/*
						 * We only have to enqueue if
						 * we're at intr context.
						 */
						/*
						 * Put node on a list to
						 * be fired after we
						 * unlock.
						 */
						a->next = firelist;
						firelist = a;
					} else {
						/*
						 * Enqueue the node for
						 * the dag exec thread
						 * to fire.
						 */
						RF_ASSERT(rf_NodeReady(a));
						if (q) {
							q->next = a;
							q = a;
						} else {
							qh = q = a;
							qh->next = NULL;
						}
					}
				}
			}
		}
		if (q) {
			/*
			 * Transfer our local list of nodes to the node
			 * queue.
			 */
			q->next = raidPtr->node_queue;
			raidPtr->node_queue = qh;
			DO_SIGNAL(raidPtr);
		}
		DO_UNLOCK(raidPtr);
		for (; finishlist; finishlist = next) {
			/* NIL nodes: no need to fire them. */
			next = finishlist->next;
			finishlist->status = rf_good;
			/*
			 * Okay, here we're calling rf_FinishNode() on nodes
			 * that have the null function as their work proc.
			 * Such a node could be the first node in a DAG.
			 * If so, it will cause the DAG to complete, which will
			 * in turn free memory used by the DAG, which includes
			 * the node in question.
			 * Thus, we must avoid referencing the node at all
			 * after calling rf_FinishNode() on it.
			 */
			rf_FinishNode(finishlist, context);
			/* Recursive call. */
		}
		/* Fire all nodes in firelist. */
		rf_FireNodeList(firelist);

		break;
	default:
		printf("Engine found illegal DAG status in"
		    " rf_PropagateResults()\n");
		RF_PANIC();
		break;
	}
}


/*
 * Process a fired node which has completed.
 */
void
rf_ProcessNode(RF_DagNode_t *node, int context)
{
	RF_Raid_t *raidPtr;

	raidPtr = node->dagHdr->raidPtr;

	switch (node->status) {
	case rf_good:
		/* Normal case, don't need to do anything. */
		break;
	case rf_bad:
		if ((node->dagHdr->numCommits > 0) ||
		    (node->dagHdr->numCommitNodes == 0)) {
			/* Crossed commit barrier. */
			node->dagHdr->status = rf_rollForward;
			if (rf_engineDebug || 1) {
				printf("raid%d: node (%s) returned fail,"
				    " rolling forward\n", raidPtr->raidid,
				    node->name);
			}
		} else {
			/* Never reached commit barrier. */
			node->dagHdr->status = rf_rollBackward;
			if (rf_engineDebug || 1) {
				printf("raid%d: node (%s) returned fail,"
				    " rolling backward\n", raidPtr->raidid,
				    node->name);
			}
		}
		break;
	case rf_undone:
		/* Normal rollBackward case, don't need to do anything. */
		break;
	case rf_panic:
		/* An undo node failed !!! */
		printf("UNDO of a node failed !!!/n");
		break;
	default:
		printf("node finished execution with an illegal status !!!\n");
		RF_PANIC();
		break;
	}

	/*
	 * Enqueue node's succedents (antecedents if rollBackward) for
	 * execution.
	 */
	rf_PropagateResults(node, context);
}


/*
 * User context or dag-exec-thread context:
 * This is the first step in post-processing a newly-completed node.
 * This routine is called by each node execution function to mark the node
 * as complete and fire off any successors that have been enabled.
 */
int
rf_FinishNode(RF_DagNode_t *node, int context)
{
	/* As far as I can tell, retcode is not used -wvcii. */
	int retcode = RF_FALSE;
	node->dagHdr->numNodesCompleted++;
	rf_ProcessNode(node, context);

	return (retcode);
}


/*
 * User context:
 * Submit dag for execution, return non-zero if we have to wait for completion.
 * If and only if we return non-zero, we'll cause cbFunc to get invoked with
 * cbArg when the DAG has completed.
 *
 * For now we always return 1. If the DAG does not cause any I/O, then the
 * callback may get invoked before DispatchDAG returns. There's code in state
 * 5 of ContinueRaidAccess to handle this.
 *
 * All we do here is fire the direct successors of the header node. The DAG
 * execution thread does the rest of the dag processing.
 */
int
rf_DispatchDAG(RF_DagHeader_t *dag, void (*cbFunc) (void *), void *cbArg)
{
	RF_Raid_t *raidPtr;

	raidPtr = dag->raidPtr;
	if (dag->tracerec) {
		RF_ETIMER_START(dag->tracerec->timer);
	}
	if (rf_engineDebug || rf_validateDAGDebug) {
		if (rf_ValidateDAG(dag))
			RF_PANIC();
	}
	if (rf_engineDebug>1) {
		printf("raid%d: Entering DispatchDAG\n", raidPtr->raidid);
	}
	raidPtr->dags_in_flight++;	/*
					 * Debug only:  blow off proper
					 * locking.
					 */
	dag->cbFunc = cbFunc;
	dag->cbArg = cbArg;
	dag->numNodesCompleted = 0;
	dag->status = rf_enable;
	rf_FireNodeArray(dag->numSuccedents, dag->succedents);
	return (1);
}


/*
 * Dedicated kernel thread:
 * The thread that handles all DAG node firing.
 * To minimize locking and unlocking, we grab a copy of the entire node queue
 * and then set the node queue to NULL before doing any firing of nodes.
 * This way we only have to release the lock once. Of course, it's probably
 * rare that there's more than one node in the queue at any one time, but it
 * sometimes happens.
 *
 * In the kernel, this thread runs at spl0 and is not swappable. I copied these
 * characteristics from the aio_completion_thread.
 */

#ifdef	RAID_AUTOCONFIG
void
rf_DAGExecutionThread_pre(RF_ThreadArg_t arg)
{
	RF_Raid_t *raidPtr;
	char raidname[16];
	int len;
	pid_t oldpid = lastpid;

	raidPtr = (RF_Raid_t *) arg;

	if (rf_engineDebug) {
		printf("raid%d: Starting engine thread\n", raidPtr->raidid);
	}

	lastpid = RF_ENGINE_PID + raidPtr->raidid - 1;
	len = sprintf(&raidname[0], "raid%d", raidPtr->raidid);
#ifdef	DIAGNOSTIC
	if (len >= sizeof(raidname))
		panic("raidname expansion too long.");
#endif	/* DIAGNOSTIC */

	if (RF_CREATE_THREAD(raidPtr->engine_thread, rf_DAGExecutionThread,
	    raidPtr, &raidname[0])) {
		RF_ERRORMSG("RAIDFRAME: Unable to start engine thread\n");
		return;
	}

	lastpid = oldpid;
	if (rf_engineDebug) {
		printf("raid%d: Engine thread started\n", raidPtr->raidid);
	}
	RF_THREADGROUP_STARTED(&raidPtr->engine_tg);
}
#endif	/* RAID_AUTOCONFIG */

void
rf_DAGExecutionThread(RF_ThreadArg_t arg)
{
	RF_DagNode_t *nd, *local_nq, *term_nq, *fire_nq;
	RF_Raid_t *raidPtr;
	int ks;
	int s;

	raidPtr = (RF_Raid_t *) arg;

	while (!(&raidPtr->engine_tg)->created)
		(void) tsleep((void *)&(&raidPtr->engine_tg)->created, PWAIT,
				"raidinit", 0);

	if (rf_engineDebug) {
		printf("raid%d: Engine thread is running\n", raidPtr->raidid);
	}
	/* XXX What to put here ? XXX */

	s = splbio();

	RF_THREADGROUP_RUNNING(&raidPtr->engine_tg);

	rf_hook_cookies[raidPtr->raidid] =
		shutdownhook_establish(rf_shutdown_hook, (void *)raidPtr);

	DO_LOCK(raidPtr);
	while (!raidPtr->shutdown_engine) {

		while (raidPtr->node_queue != NULL) {
			local_nq = raidPtr->node_queue;
			fire_nq = NULL;
			term_nq = NULL;
			raidPtr->node_queue = NULL;
			DO_UNLOCK(raidPtr);

			/* First, strip out the terminal nodes. */
			while (local_nq) {
				nd = local_nq;
				local_nq = local_nq->next;
				switch (nd->dagHdr->status) {
				case rf_enable:
				case rf_rollForward:
					if (nd->numSuccedents == 0) {
						/*
						 * End of the dag, add to
						 * callback list.
						 */
						nd->next = term_nq;
						term_nq = nd;
					} else {
						/*
						 * Not the end, add to the
						 * fire queue.
						 */
						nd->next = fire_nq;
						fire_nq = nd;
					}
					break;
				case rf_rollBackward:
					if (nd->numAntecedents == 0) {
						/*
						 * End of the dag, add to the
						 * callback list.
						 */
						nd->next = term_nq;
						term_nq = nd;
					} else {
						/*
						 * Not the end, add to the
						 * fire queue.
						 */
						nd->next = fire_nq;
						fire_nq = nd;
					}
					break;
				default:
					RF_PANIC();
					break;
				}
			}

			/*
			 * Execute callback of dags which have reached the
			 * terminal node.
			 */
			while (term_nq) {
				nd = term_nq;
				term_nq = term_nq->next;
				nd->next = NULL;
				(nd->dagHdr->cbFunc) (nd->dagHdr->cbArg);
				raidPtr->dags_in_flight--; /* Debug only. */
			}

			/* Fire remaining nodes. */
			rf_FireNodeList(fire_nq);

			DO_LOCK(raidPtr);
		}
		while (!raidPtr->shutdown_engine && raidPtr->node_queue == NULL)
			DO_WAIT(raidPtr);
	}
	DO_UNLOCK(raidPtr);

	if (rf_hook_cookies && rf_hook_cookies[raidPtr->raidid] != NULL) {
		shutdownhook_disestablish(rf_hook_cookies[raidPtr->raidid]);
		rf_hook_cookies[raidPtr->raidid] = NULL;
	}

	RF_THREADGROUP_DONE(&raidPtr->engine_tg);

	splx(s);
	kthread_exit(0);
}
