/* 
 * tkTextBTree.c --
 *
 *	This file contains code that manages the B-tree representation
 *	of text for Tk's text widget and implements character and
 *	toggle segment types.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkTextBTree.c 1.35 96/03/21 15:51:39
 */

#include "tkInt.h"
#include "tkPort.h"
#include "tkText.h"

/*
 * The data structure below keeps summary information about one tag as part
 * of the tag information in a node.
 */

typedef struct Summary {
    TkTextTag *tagPtr;			/* Handle for tag. */
    int toggleCount;			/* Number of transitions into or
					 * out of this tag that occur in
					 * the subtree rooted at this node. */
    struct Summary *nextPtr;		/* Next in list of all tags for same
					 * node, or NULL if at end of list. */
} Summary;

/*
 * The data structure below defines a node in the B-tree.
 */

typedef struct Node {
    struct Node *parentPtr;		/* Pointer to parent node, or NULL if
					 * this is the root. */
    struct Node *nextPtr;		/* Next in list of siblings with the
					 * same parent node, or NULL for end
					 * of list. */
    Summary *summaryPtr;		/* First in malloc-ed list of info
					 * about tags in this subtree (NULL if
					 * no tag info in the subtree). */
    int level;				/* Level of this node in the B-tree.
					 * 0 refers to the bottom of the tree
					 * (children are lines, not nodes). */
    union {				/* First in linked list of children. */
	struct Node *nodePtr;		/* Used if level > 0. */
	TkTextLine *linePtr;		/* Used if level == 0. */
    } children;
    int numChildren;			/* Number of children of this node. */
    int numLines;			/* Total number of lines (leaves) in
					 * the subtree rooted here. */
} Node;

/*
 * Upper and lower bounds on how many children a node may have:
 * rebalance when either of these limits is exceeded.  MAX_CHILDREN
 * should be twice MIN_CHILDREN and MIN_CHILDREN must be >= 2.
 */

#define MAX_CHILDREN 12
#define MIN_CHILDREN 6

/*
 * The data structure below defines an entire B-tree.
 */

typedef struct BTree {
    Node *rootPtr;			/* Pointer to root of B-tree. */
    TkText *textPtr;			/* Used to find tagTable in consistency
					 * checking code */
} BTree;

/*
 * The structure below is used to pass information between
 * TkBTreeGetTags and IncCount:
 */

typedef struct TagInfo {
    int numTags;			/* Number of tags for which there
					 * is currently information in
					 * tags and counts. */
    int arraySize;			/* Number of entries allocated for
					 * tags and counts. */
    TkTextTag **tagPtrs;		/* Array of tags seen so far.
					 * Malloc-ed. */
    int *counts;			/* Toggle count (so far) for each
					 * entry in tags.  Malloc-ed. */
} TagInfo;

/*
 * Variable that indicates whether to enable consistency checks for
 * debugging.
 */

int tkBTreeDebug = 0;

/*
 * Macros that determine how much space to allocate for new segments:
 */

#define CSEG_SIZE(chars) ((unsigned) (Tk_Offset(TkTextSegment, body) \
	+ 1 + (chars)))
#define TSEG_SIZE ((unsigned) (Tk_Offset(TkTextSegment, body) \
	+ sizeof(TkTextToggle)))

/*
 * Forward declarations for procedures defined in this file:
 */

static void		ChangeNodeToggleCount _ANSI_ARGS_((Node *nodePtr,
			    TkTextTag *tagPtr, int delta));
static void		CharCheckProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static int		CharDeleteProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr, int treeGone));
static TkTextSegment *	CharCleanupProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static TkTextSegment *	CharSplitProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    int index));
static void		CheckNodeConsistency _ANSI_ARGS_((Node *nodePtr));
static void		CleanupLine _ANSI_ARGS_((TkTextLine *linePtr));
static void		DeleteSummaries _ANSI_ARGS_((Summary *tagPtr));
static void		DestroyNode _ANSI_ARGS_((Node *nodePtr));
static void		IncCount _ANSI_ARGS_((TkTextTag *tagPtr, int inc,
			    TagInfo *tagInfoPtr));
static void		Rebalance _ANSI_ARGS_((BTree *treePtr, Node *nodePtr));
static void		RecomputeNodeCounts _ANSI_ARGS_((Node *nodePtr));
static TkTextSegment *	SplitSeg _ANSI_ARGS_((TkTextIndex *indexPtr));
static void		ToggleCheckProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static TkTextSegment *	ToggleCleanupProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static int		ToggleDeleteProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr, int treeGone));
static void		ToggleLineChangeProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static TkTextSegment *	FindTagStart _ANSI_ARGS_((TkTextBTree tree,
			    TkTextTag *tagPtr, TkTextIndex *indexPtr));

/*
 * Type record for character segments:
 */

Tk_SegType tkTextCharType = {
    "character",				/* name */
    0,						/* leftGravity */
    CharSplitProc,				/* splitProc */
    CharDeleteProc,				/* deleteProc */
    CharCleanupProc,				/* cleanupProc */
    (Tk_SegLineChangeProc *) NULL,		/* lineChangeProc */
    TkTextCharLayoutProc,			/* layoutProc */
    CharCheckProc				/* checkProc */
};

/*
 * Type record for segments marking the beginning of a tagged
 * range:
 */

Tk_SegType tkTextToggleOnType = {
    "toggleOn",					/* name */
    0,						/* leftGravity */
    (Tk_SegSplitProc *) NULL,			/* splitProc */
    ToggleDeleteProc,				/* deleteProc */
    ToggleCleanupProc,				/* cleanupProc */
    ToggleLineChangeProc,			/* lineChangeProc */
    (Tk_SegLayoutProc *) NULL,			/* layoutProc */
    ToggleCheckProc				/* checkProc */
};

/*
 * Type record for segments marking the end of a tagged
 * range:
 */

Tk_SegType tkTextToggleOffType = {
    "toggleOff",				/* name */
    1,						/* leftGravity */
    (Tk_SegSplitProc *) NULL,			/* splitProc */
    ToggleDeleteProc,				/* deleteProc */
    ToggleCleanupProc,				/* cleanupProc */
    ToggleLineChangeProc,			/* lineChangeProc */
    (Tk_SegLayoutProc *) NULL,			/* layoutProc */
    ToggleCheckProc				/* checkProc */
};

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeCreate --
 *
 *	This procedure is called to create a new text B-tree.
 *
 * Results:
 *	The return value is a pointer to a new B-tree containing
 *	one line with nothing but a newline character.
 *
 * Side effects:
 *	Memory is allocated and initialized.
 *
 *----------------------------------------------------------------------
 */

TkTextBTree
TkBTreeCreate(textPtr)
    TkText *textPtr;
{
    register BTree *treePtr;
    register Node *rootPtr;
    register TkTextLine *linePtr, *linePtr2;
    register TkTextSegment *segPtr;

    /*
     * The tree will initially have two empty lines.  The second line
     * isn't actually part of the tree's contents, but its presence
     * makes several operations easier.  The tree will have one node,
     * which is also the root of the tree.
     */

    rootPtr = (Node *) ckalloc(sizeof(Node));
    linePtr = (TkTextLine *) ckalloc(sizeof(TkTextLine));
    linePtr2 = (TkTextLine *) ckalloc(sizeof(TkTextLine));
    rootPtr->parentPtr = NULL;
    rootPtr->nextPtr = NULL;
    rootPtr->summaryPtr = NULL;
    rootPtr->level = 0;
    rootPtr->children.linePtr = linePtr;
    rootPtr->numChildren = 2;
    rootPtr->numLines = 2;

    linePtr->parentPtr = rootPtr;
    linePtr->nextPtr = linePtr2;
    segPtr = (TkTextSegment *) ckalloc(CSEG_SIZE(1));
    linePtr->segPtr = segPtr;
    segPtr->typePtr = &tkTextCharType;
    segPtr->nextPtr = NULL;
    segPtr->size = 1;
    segPtr->body.chars[0] = '\n';
    segPtr->body.chars[1] = 0;

    linePtr2->parentPtr = rootPtr;
    linePtr2->nextPtr = NULL;
    segPtr = (TkTextSegment *) ckalloc(CSEG_SIZE(1));
    linePtr2->segPtr = segPtr;
    segPtr->typePtr = &tkTextCharType;
    segPtr->nextPtr = NULL;
    segPtr->size = 1;
    segPtr->body.chars[0] = '\n';
    segPtr->body.chars[1] = 0;

    treePtr = (BTree *) ckalloc(sizeof(BTree));
    treePtr->rootPtr = rootPtr;
    treePtr->textPtr = textPtr;

    return (TkTextBTree) treePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeDestroy --
 *
 *	Delete a B-tree, recycling all of the storage it contains.
 *
 * Results:
 *	The tree given by treePtr is deleted.  TreePtr should never
 *	again be used.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TkBTreeDestroy(tree)
    TkTextBTree tree;			/* Pointer to tree to delete. */ 
{
    BTree *treePtr = (BTree *) tree;

    DestroyNode(treePtr->rootPtr);
    ckfree((char *) treePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyNode --
 *
 *	This is a recursive utility procedure used during the deletion
 *	of a B-tree.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All the storage for nodePtr and its descendants is freed.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyNode(nodePtr)
    register Node *nodePtr;
{
    if (nodePtr->level == 0) {
	TkTextLine *linePtr;
	TkTextSegment *segPtr;

	while (nodePtr->children.linePtr != NULL) {
	    linePtr = nodePtr->children.linePtr;
	    nodePtr->children.linePtr = linePtr->nextPtr;
	    while (linePtr->segPtr != NULL) {
		segPtr = linePtr->segPtr;
		linePtr->segPtr = segPtr->nextPtr;
		(*segPtr->typePtr->deleteProc)(segPtr, linePtr, 1);
	    }
	    ckfree((char *) linePtr);
	}
    } else {
	register Node *childPtr;

	while (nodePtr->children.nodePtr != NULL) {
	    childPtr = nodePtr->children.nodePtr;
	    nodePtr->children.nodePtr = childPtr->nextPtr;
	    DestroyNode(childPtr);
	}
    }
    DeleteSummaries(nodePtr->summaryPtr);
    ckfree((char *) nodePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteSummaries --
 *
 *	Free up all of the memory in a list of tag summaries associated
 *	with a node.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage is released.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteSummaries(summaryPtr)
    register Summary *summaryPtr;	/* First in list of node's tag
					 * summaries. */
{
    register Summary *nextPtr;
    while (summaryPtr != NULL) {
	nextPtr = summaryPtr->nextPtr;
	ckfree((char *) summaryPtr);
	summaryPtr = nextPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeInsertChars --
 *
 *	Insert characters at a given position in a B-tree.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Characters are added to the B-tree at the given position.
 *	If the string contains newlines, new lines will be added,
 *	which could cause the structure of the B-tree to change.
 *
 *----------------------------------------------------------------------
 */

void
TkBTreeInsertChars(indexPtr, string)
    register TkTextIndex *indexPtr;	/* Indicates where to insert text.
					 * When the procedure returns, this
					 * index is no longer valid because
					 * of changes to the segment
					 * structure. */
    char *string;			/* Pointer to bytes to insert (may
					 * contain newlines, must be null-
					 * terminated). */
{
    register Node *nodePtr;
    register TkTextSegment *prevPtr;	/* The segment just before the first
					 * new segment (NULL means new segment
					 * is at beginning of line). */
    TkTextSegment *curPtr;		/* Current segment;  new characters
					 * are inserted just after this one. 
					 * NULL means insert at beginning of
					 * line. */
    TkTextLine *linePtr;		/* Current line (new segments are
					 * added to this line). */
    register TkTextSegment *segPtr;
    TkTextLine *newLinePtr;
    int chunkSize;			/* # characters in current chunk. */
    register char *eol;			/* Pointer to character just after last
					 * one in current chunk. */
    int changeToLineCount;		/* Counts change to total number of
					 * lines in file. */

    prevPtr = SplitSeg(indexPtr);
    linePtr = indexPtr->linePtr;
    curPtr = prevPtr;

    /*
     * Chop the string up into lines and create a new segment for
     * each line, plus a new line for the leftovers from the
     * previous line.
     */

    changeToLineCount = 0;
    while (*string != 0) {
	for (eol = string; *eol != 0; eol++) {
	    if (*eol == '\n') {
		eol++;
		break;
	    }
	}
	chunkSize = eol-string;
	segPtr = (TkTextSegment *) ckalloc(CSEG_SIZE(chunkSize));
	segPtr->typePtr = &tkTextCharType;
	if (curPtr == NULL) {
	    segPtr->nextPtr = linePtr->segPtr;
	    linePtr->segPtr = segPtr;
	} else {
	    segPtr->nextPtr = curPtr->nextPtr;
	    curPtr->nextPtr = segPtr;
	}
	segPtr->size = chunkSize;
	strncpy(segPtr->body.chars, string, (size_t) chunkSize);
	segPtr->body.chars[chunkSize] = 0;
	curPtr = segPtr;

	if (eol[-1] != '\n') {
	    break;
	}

	/*
	 * The chunk ended with a newline, so create a new TkTextLine
	 * and move the remainder of the old line to it.
	 */

	newLinePtr = (TkTextLine *) ckalloc(sizeof(TkTextLine));
	newLinePtr->parentPtr = linePtr->parentPtr;
	newLinePtr->nextPtr = linePtr->nextPtr;
	linePtr->nextPtr = newLinePtr;
	newLinePtr->segPtr = segPtr->nextPtr;
	segPtr->nextPtr = NULL;
	linePtr = newLinePtr;
	curPtr = NULL;
	changeToLineCount++;

	string = eol;
    }

    /*
     * Cleanup the starting line for the insertion, plus the ending
     * line if it's different.
     */

    CleanupLine(indexPtr->linePtr);
    if (linePtr != indexPtr->linePtr) {
	CleanupLine(linePtr);
    }

    /*
     * Increment the line counts in all the parent nodes of the insertion
     * point, then rebalance the tree if necessary.
     */

    for (nodePtr = linePtr->parentPtr ; nodePtr != NULL;
	    nodePtr = nodePtr->parentPtr) {
	nodePtr->numLines += changeToLineCount;
    }
    nodePtr = linePtr->parentPtr;
    nodePtr->numChildren += changeToLineCount;
    if (nodePtr->numChildren > MAX_CHILDREN) {
	Rebalance((BTree *) indexPtr->tree, nodePtr);
    }

    if (tkBTreeDebug) {
	TkBTreeCheck(indexPtr->tree);
    }
}

/*
 *--------------------------------------------------------------
 *
 * SplitSeg --
 *
 *	This procedure is called before adding or deleting
 *	segments.  It does three things: (a) it finds the segment
 *	containing indexPtr;  (b) if there are several such
 *	segments (because some segments have zero length) then
 *	it picks the first segment that does not have left
 *	gravity;  (c) if the index refers to the middle of
 *	a segment then it splits the segment so that the
 *	index now refers to the beginning of a segment.
 *
 * Results:
 *	The return value is a pointer to the segment just
 *	before the segment corresponding to indexPtr (as
 *	described above).  If the segment corresponding to
 *	indexPtr is the first in its line then the return
 *	value is NULL.
 *
 * Side effects:
 *	The segment referred to by indexPtr is split unless
 *	indexPtr refers to its first character.
 *
 *--------------------------------------------------------------
 */

static TkTextSegment *
SplitSeg(indexPtr)
    TkTextIndex *indexPtr;		/* Index identifying position
					 * at which to split a segment. */
{
    TkTextSegment *prevPtr, *segPtr;
    int count;

    for (count = indexPtr->charIndex, prevPtr = NULL,
	    segPtr = indexPtr->linePtr->segPtr; segPtr != NULL;
	    count -= segPtr->size, prevPtr = segPtr, segPtr = segPtr->nextPtr) {
	if (segPtr->size > count) {
	    if (count == 0) {
		return prevPtr;
	    }
	    segPtr = (*segPtr->typePtr->splitProc)(segPtr, count);
	    if (prevPtr == NULL) {
		indexPtr->linePtr->segPtr = segPtr;
	    } else {
		prevPtr->nextPtr = segPtr;
	    }
	    return segPtr;
	} else if ((segPtr->size == 0) && (count == 0)
		&& !segPtr->typePtr->leftGravity) {
	    return prevPtr;
	}
    }
    panic("SplitSeg reached end of line!");
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * CleanupLine --
 *
 *	This procedure is called after modifications have been
 *	made to a line.  It scans over all of the segments in
 *	the line, giving each a chance to clean itself up, e.g.
 *	by merging with the following segments, updating internal
 *	information, etc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what the segment-specific cleanup procedures do.
 *
 *--------------------------------------------------------------
 */

static void
CleanupLine(linePtr)
    TkTextLine *linePtr;		/* Line to be cleaned up. */
{
    TkTextSegment *segPtr, **prevPtrPtr;
    int anyChanges;

    /*
     * Make a pass over all of the segments in the line, giving each
     * a chance to clean itself up.  This could potentially change
     * the structure of the line, e.g. by merging two segments
     * together or having two segments cancel themselves;  if so,
     * then repeat the whole process again, since the first structure
     * change might make other structure changes possible.  Repeat
     * until eventually there are no changes.
     */

    while (1) {
	anyChanges = 0;
	for (prevPtrPtr = &linePtr->segPtr, segPtr = *prevPtrPtr;
		segPtr != NULL;
		prevPtrPtr = &(*prevPtrPtr)->nextPtr, segPtr = *prevPtrPtr) {
	    if (segPtr->typePtr->cleanupProc != NULL) {
		*prevPtrPtr = (*segPtr->typePtr->cleanupProc)(segPtr, linePtr);
		if (segPtr != *prevPtrPtr) {
		    anyChanges = 1;
		}
	    }
	}
	if (!anyChanges) {
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeDeleteChars --
 *
 *	Delete a range of characters from a B-tree.  The caller
 *	must make sure that the final newline of the B-tree is
 *	never deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information is deleted from the B-tree.  This can cause the
 *	internal structure of the B-tree to change.  Note: because
 *	of changes to the B-tree structure, the indices pointed
 *	to by index1Ptr and index2Ptr should not be used after this
 *	procedure returns.
 *
 *----------------------------------------------------------------------
 */

void
TkBTreeDeleteChars(index1Ptr, index2Ptr)
    register TkTextIndex *index1Ptr;	/* Indicates first character that is
					 * to be deleted. */
    register TkTextIndex *index2Ptr;	/* Indicates character just after the
					 * last one that is to be deleted. */
{
    TkTextSegment *prevPtr;		/* The segment just before the start
					 * of the deletion range. */
    TkTextSegment *lastPtr;		/* The segment just after the end
					 * of the deletion range. */
    TkTextSegment *segPtr, *nextPtr;
    TkTextLine *curLinePtr;
    Node *curNodePtr, *nodePtr;

    /*
     * Tricky point:  split at index2Ptr first;  otherwise the split
     * at index2Ptr may invalidate segPtr and/or prevPtr.
     */

    lastPtr = SplitSeg(index2Ptr);
    if (lastPtr != NULL) {
	lastPtr = lastPtr->nextPtr;
    }  else {
	lastPtr = index2Ptr->linePtr->segPtr;
    }
    prevPtr = SplitSeg(index1Ptr);
    if (prevPtr != NULL) {
	segPtr = prevPtr->nextPtr;
	prevPtr->nextPtr = lastPtr;
    } else {
	segPtr = index1Ptr->linePtr->segPtr;
	index1Ptr->linePtr->segPtr = lastPtr;
    }

    /*
     * Delete all of the segments between prevPtr and lastPtr.
     */

    curLinePtr = index1Ptr->linePtr;
    curNodePtr = curLinePtr->parentPtr;
    while (segPtr != lastPtr) {
	if (segPtr == NULL) {
	    TkTextLine *nextLinePtr;

	    /*
	     * We just ran off the end of a line.  First find the
	     * next line, then go back to the old line and delete it
	     * (unless it's the starting line for the range).
	     */

	    nextLinePtr = TkBTreeNextLine(curLinePtr);
	    if (curLinePtr != index1Ptr->linePtr) {
		if (curNodePtr == index1Ptr->linePtr->parentPtr) {
		    index1Ptr->linePtr->nextPtr = curLinePtr->nextPtr;
		} else {
		    curNodePtr->children.linePtr = curLinePtr->nextPtr;
		}
		for (nodePtr = curNodePtr; nodePtr != NULL;
			nodePtr = nodePtr->parentPtr) {
		    nodePtr->numLines--;
		}
		curNodePtr->numChildren--;
		ckfree((char *) curLinePtr);
	    }
	    curLinePtr = nextLinePtr;
	    segPtr = curLinePtr->segPtr;

	    /*
	     * If the node is empty then delete it and its parents,
	     * recursively upwards until a non-empty node is found.
	     */

	    while (curNodePtr->numChildren == 0) {
		Node *parentPtr;

		parentPtr = curNodePtr->parentPtr;
		if (parentPtr->children.nodePtr == curNodePtr) {
		    parentPtr->children.nodePtr = curNodePtr->nextPtr;
		} else {
		    Node *prevNodePtr = parentPtr->children.nodePtr;
		    while (prevNodePtr->nextPtr != curNodePtr) {
			prevNodePtr = prevNodePtr->nextPtr;
		    }
		    prevNodePtr->nextPtr = curNodePtr->nextPtr;
		}
		parentPtr->numChildren--;
		ckfree((char *) curNodePtr);
		curNodePtr = parentPtr;
	    }
	    curNodePtr = curLinePtr->parentPtr;
	    continue;
	}

	nextPtr = segPtr->nextPtr;
	if ((*segPtr->typePtr->deleteProc)(segPtr, curLinePtr, 0) != 0) {
	    /*
	     * This segment refuses to die.  Move it to prevPtr and
	     * advance prevPtr if the segment has left gravity.
	     */

	    if (prevPtr == NULL) {
		segPtr->nextPtr = index1Ptr->linePtr->segPtr;
		index1Ptr->linePtr->segPtr = segPtr;
	    } else {
		segPtr->nextPtr = prevPtr->nextPtr;
		prevPtr->nextPtr = segPtr;
	    }
	    if (segPtr->typePtr->leftGravity) {
		prevPtr = segPtr;
	    }
	}
	segPtr = nextPtr;
    }

    /*
     * If the beginning and end of the deletion range are in different
     * lines, join the two lines together and discard the ending line.
     */

    if (index1Ptr->linePtr != index2Ptr->linePtr) {
	TkTextLine *prevLinePtr;

	for (segPtr = lastPtr; segPtr != NULL;
		segPtr = segPtr->nextPtr) {
	    if (segPtr->typePtr->lineChangeProc != NULL) {
		(*segPtr->typePtr->lineChangeProc)(segPtr, index2Ptr->linePtr);
	    }
	}
	curNodePtr = index2Ptr->linePtr->parentPtr;
	for (nodePtr = curNodePtr; nodePtr != NULL;
		nodePtr = nodePtr->parentPtr) {
	    nodePtr->numLines--;
	}
	curNodePtr->numChildren--;
	prevLinePtr = curNodePtr->children.linePtr;
	if (prevLinePtr == index2Ptr->linePtr) {
	    curNodePtr->children.linePtr = index2Ptr->linePtr->nextPtr;
	} else {
	    while (prevLinePtr->nextPtr != index2Ptr->linePtr) {
		prevLinePtr = prevLinePtr->nextPtr;
	    }
	    prevLinePtr->nextPtr = index2Ptr->linePtr->nextPtr;
	}
	ckfree((char *) index2Ptr->linePtr);
	Rebalance((BTree *) index2Ptr->tree, curNodePtr);
    }

    /*
     * Cleanup the segments in the new line.
     */

    CleanupLine(index1Ptr->linePtr);

    /*
     * Lastly, rebalance the first node of the range.
     */

    Rebalance((BTree *) index1Ptr->tree, index1Ptr->linePtr->parentPtr);
    if (tkBTreeDebug) {
	TkBTreeCheck(index1Ptr->tree);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeFindLine --
 *
 *	Find a particular line in a B-tree based on its line number.
 *
 * Results:
 *	The return value is a pointer to the line structure for the
 *	line whose index is "line", or NULL if no such line exists.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkTextLine *
TkBTreeFindLine(tree, line)
    TkTextBTree tree;			/* B-tree in which to find line. */
    int line;				/* Index of desired line. */
{
    BTree *treePtr = (BTree *) tree;
    register Node *nodePtr;
    register TkTextLine *linePtr;
    int linesLeft;

    nodePtr = treePtr->rootPtr;
    linesLeft = line;
    if ((line < 0) || (line >= nodePtr->numLines)) {
	return NULL;
    }

    /*
     * Work down through levels of the tree until a node is found at
     * level 0.
     */

    while (nodePtr->level != 0) {
	for (nodePtr = nodePtr->children.nodePtr;
		nodePtr->numLines <= linesLeft;
		nodePtr = nodePtr->nextPtr) {
	    if (nodePtr == NULL) {
		panic("TkBTreeFindLine ran out of nodes");
	    }
	    linesLeft -= nodePtr->numLines;
	}
    }

    /*
     * Work through the lines attached to the level-0 node.
     */

    for (linePtr = nodePtr->children.linePtr; linesLeft > 0;
	    linePtr = linePtr->nextPtr) {
	if (linePtr == NULL) {
	    panic("TkBTreeFindLine ran out of lines");
	}
	linesLeft -= 1;
    }
    return linePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeNextLine --
 *
 *	Given an existing line in a B-tree, this procedure locates the
 *	next line in the B-tree.  This procedure is used for scanning
 *	through the B-tree.
 *
 * Results:
 *	The return value is a pointer to the line that immediately
 *	follows linePtr, or NULL if there is no such line.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkTextLine *
TkBTreeNextLine(linePtr)
    register TkTextLine *linePtr;	/* Pointer to existing line in
					 * B-tree. */
{
    register Node *nodePtr;

    if (linePtr->nextPtr != NULL) {
	return linePtr->nextPtr;
    }

    /*
     * This was the last line associated with the particular parent node.
     * Search up the tree for the next node, then search down from that
     * node to find the first line.
     */

    for (nodePtr = linePtr->parentPtr; ; nodePtr = nodePtr->parentPtr) {
	if (nodePtr->nextPtr != NULL) {
	    nodePtr = nodePtr->nextPtr;
	    break;
	}
	if (nodePtr->parentPtr == NULL) {
	    return (TkTextLine *) NULL;
	}
    }
    while (nodePtr->level > 0) {
	nodePtr = nodePtr->children.nodePtr;
    }
    return nodePtr->children.linePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreePreviousLine --
 *
 *	Given an existing line in a B-tree, this procedure locates the
 *	previous line in the B-tree.  This procedure is used for scanning
 *	through the B-tree in the reverse direction.
 *
 * Results:
 *	The return value is a pointer to the line that immediately
 *	preceeds linePtr, or NULL if there is no such line.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkTextLine *
TkBTreePreviousLine(linePtr)
    register TkTextLine *linePtr;	/* Pointer to existing line in
					 * B-tree. */
{
    register Node *nodePtr;
    register Node *node2Ptr;
    register TkTextLine *prevPtr;

    /*
     * Find the line under this node just before the starting line.
     */
    prevPtr = linePtr->parentPtr->children.linePtr;	/* First line at leaf */
    while (prevPtr != linePtr) {
	if (prevPtr->nextPtr == linePtr) {
	    return prevPtr;
	}
	prevPtr = prevPtr->nextPtr;
	if (prevPtr == (TkTextLine *) NULL) {
	    panic("TkBTreePreviousLine ran out of lines");
	}
    }

    /*
     * This was the first line associated with the particular parent node.
     * Search up the tree for the previous node, then search down from that
     * node to find its last line.
     */
    for (nodePtr = linePtr->parentPtr; ; nodePtr = nodePtr->parentPtr) {
	if (nodePtr == (Node *) NULL || nodePtr->parentPtr == (Node *) NULL) {
	    return (TkTextLine *) NULL;
	}
	if (nodePtr != nodePtr->parentPtr->children.nodePtr) {
	    break;
	}
    }
    for (node2Ptr = nodePtr->parentPtr->children.nodePtr; ; 
	    node2Ptr = node2Ptr->children.nodePtr) {
	while (node2Ptr->nextPtr != nodePtr) {
	    node2Ptr = node2Ptr->nextPtr;
	}
	if (node2Ptr->level == 0) {
	    break;
	}
	nodePtr = (Node *)NULL;
    }
    for (prevPtr = node2Ptr->children.linePtr ; ; prevPtr = prevPtr->nextPtr) {
	if (prevPtr->nextPtr == (TkTextLine *) NULL) {
	    return prevPtr;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeLineIndex --
 *
 *	Given a pointer to a line in a B-tree, return the numerical
 *	index of that line.
 *
 * Results:
 *	The result is the index of linePtr within the tree, where 0
 *	corresponds to the first line in the tree.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkBTreeLineIndex(linePtr)
    TkTextLine *linePtr;		/* Pointer to existing line in
					 * B-tree. */
{
    register TkTextLine *linePtr2;
    register Node *nodePtr, *parentPtr, *nodePtr2;
    int index;

    /*
     * First count how many lines precede this one in its level-0
     * node.
     */

    nodePtr = linePtr->parentPtr;
    index = 0;
    for (linePtr2 = nodePtr->children.linePtr; linePtr2 != linePtr;
	    linePtr2 = linePtr2->nextPtr) {
	if (linePtr2 == NULL) {
	    panic("TkBTreeLineIndex couldn't find line");
	}
	index += 1;
    }

    /*
     * Now work up through the levels of the tree one at a time,
     * counting how many lines are in nodes preceding the current
     * node.
     */

    for (parentPtr = nodePtr->parentPtr ; parentPtr != NULL;
	    nodePtr = parentPtr, parentPtr = parentPtr->parentPtr) {
	for (nodePtr2 = parentPtr->children.nodePtr; nodePtr2 != nodePtr;
		nodePtr2 = nodePtr2->nextPtr) {
	    if (nodePtr2 == NULL) {
		panic("TkBTreeLineIndex couldn't find node");
	    }
	    index += nodePtr2->numLines;
	}
    }
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeLinkSegment --
 *
 *	This procedure adds a new segment to a B-tree at a given
 *	location.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	SegPtr will be linked into its tree.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
void
TkBTreeLinkSegment(segPtr, indexPtr)
    TkTextSegment *segPtr;	/* Pointer to new segment to be added to
				 * B-tree.  Should be completely initialized
				 * by caller except for nextPtr field. */
    TkTextIndex *indexPtr;	/* Where to add segment:  it gets linked
				 * in just before the segment indicated
				 * here. */
{
    register TkTextSegment *prevPtr;

    prevPtr = SplitSeg(indexPtr);
    if (prevPtr == NULL) {
	segPtr->nextPtr = indexPtr->linePtr->segPtr;
	indexPtr->linePtr->segPtr = segPtr;
    } else {
	segPtr->nextPtr = prevPtr->nextPtr;
	prevPtr->nextPtr = segPtr;
    }
    CleanupLine(indexPtr->linePtr);
    if (tkBTreeDebug) {
	TkBTreeCheck(indexPtr->tree);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeUnlinkSegment --
 *
 *	This procedure unlinks a segment from its line in a B-tree.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	SegPtr will be unlinked from linePtr.  The segment itself
 *	isn't modified by this procedure.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
void
TkBTreeUnlinkSegment(tree, segPtr, linePtr)
    TkTextBTree tree;			/* Tree containing segment. */
    TkTextSegment *segPtr;		/* Segment to be unlinked. */
    TkTextLine *linePtr;		/* Line that currently contains
					 * segment. */
{
    register TkTextSegment *prevPtr;

    if (linePtr->segPtr == segPtr) {
	linePtr->segPtr = segPtr->nextPtr;
    } else {
	for (prevPtr = linePtr->segPtr; prevPtr->nextPtr != segPtr;
		prevPtr = prevPtr->nextPtr) {
	    /* Empty loop body. */
	}
	prevPtr->nextPtr = segPtr->nextPtr;
    }
    CleanupLine(linePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeTag --
 *
 *	Turn a given tag on or off for a given range of characters in
 *	a B-tree of text.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given tag is added to the given range of characters
 *	in the tree or removed from all those characters, depending
 *	on the "add" argument.  The structure of the btree is modified
 *	enough that index1Ptr and index2Ptr are no longer valid after
 *	this procedure returns, and the indexes may be modified by
 *	this procedure.
 *
 *----------------------------------------------------------------------
 */

void
TkBTreeTag(index1Ptr, index2Ptr, tagPtr, add)
    register TkTextIndex *index1Ptr;	/* Indicates first character in
					 * range. */
    register TkTextIndex *index2Ptr;	/* Indicates character just after the
					 * last one in range. */
    TkTextTag *tagPtr;			/* Tag to add or remove. */
    int add;				/* One means add tag to the given
					 * range of characters;  zero means
					 * remove the tag from the range. */
{
    TkTextSegment *segPtr, *prevPtr;
    TkTextSearch search;
    TkTextLine *cleanupLinePtr;
    int oldState;
    int changed;

    /*
     * See whether the tag is present at the start of the range.  If
     * the state doesn't already match what we want then add a toggle
     * there.
     */

    oldState = TkBTreeCharTagged(index1Ptr, tagPtr);
    if ((add != 0) ^ oldState) {
	segPtr = (TkTextSegment *) ckalloc(TSEG_SIZE);
	segPtr->typePtr = (add) ? &tkTextToggleOnType : &tkTextToggleOffType;
	prevPtr = SplitSeg(index1Ptr);
	if (prevPtr == NULL) {
	    segPtr->nextPtr = index1Ptr->linePtr->segPtr;
	    index1Ptr->linePtr->segPtr = segPtr;
	} else {
	    segPtr->nextPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = segPtr;
	}
	segPtr->size = 0;
	segPtr->body.toggle.tagPtr = tagPtr;
	segPtr->body.toggle.inNodeCounts = 0;
    }

    /*
     * Scan the range of characters and delete any internal tag
     * transitions.  Keep track of what the old state was at the end
     * of the range, and add a toggle there if it's needed.
     */

    TkBTreeStartSearch(index1Ptr, index2Ptr, tagPtr, &search);
    cleanupLinePtr = index1Ptr->linePtr;
    while (TkBTreeNextTag(&search)) {
	oldState ^= 1;
	segPtr = search.segPtr;
	prevPtr = search.curIndex.linePtr->segPtr;
	if (prevPtr == segPtr) {
	    search.curIndex.linePtr->segPtr = segPtr->nextPtr;
	} else {
	    while (prevPtr->nextPtr != segPtr) {
		prevPtr = prevPtr->nextPtr;
	    }
	    prevPtr->nextPtr = segPtr->nextPtr;
	}
	if (segPtr->body.toggle.inNodeCounts) {
	    ChangeNodeToggleCount(search.curIndex.linePtr->parentPtr,
		    segPtr->body.toggle.tagPtr, -1);
	    segPtr->body.toggle.inNodeCounts = 0;
	    changed = 1;
	} else {
	    changed = 0;
	}
	ckfree((char *) segPtr);

	/*
	 * The code below is a bit tricky.  After deleting a toggle
	 * we eventually have to call CleanupLine, in order to allow
	 * character segments to be merged together.  To do this, we
	 * remember in cleanupLinePtr a line that needs to be
	 * cleaned up, but we don't clean it up until we've moved
	 * on to a different line.  That way the cleanup process
	 * won't goof up segPtr.
	 */

	if (cleanupLinePtr != search.curIndex.linePtr) {
	    CleanupLine(cleanupLinePtr);
	    cleanupLinePtr = search.curIndex.linePtr;
	}
	/*
	 * Quick hack.  ChangeNodeToggleCount may move the tag's root
	 * location around and leave the search in the void.  This resets
	 * the search.
	 */
	if (changed) {
	    TkBTreeStartSearch(index1Ptr, index2Ptr, tagPtr, &search);
	}
    }
    if ((add != 0) ^ oldState) {
	segPtr = (TkTextSegment *) ckalloc(TSEG_SIZE);
	segPtr->typePtr = (add) ? &tkTextToggleOffType : &tkTextToggleOnType;
	prevPtr = SplitSeg(index2Ptr);
	if (prevPtr == NULL) {
	    segPtr->nextPtr = index2Ptr->linePtr->segPtr;
	    index2Ptr->linePtr->segPtr = segPtr;
	} else {
	    segPtr->nextPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = segPtr;
	}
	segPtr->size = 0;
	segPtr->body.toggle.tagPtr = tagPtr;
	segPtr->body.toggle.inNodeCounts = 0;
    }

    /*
     * Cleanup cleanupLinePtr and the last line of the range, if
     * these are different.
     */

    CleanupLine(cleanupLinePtr);
    if (cleanupLinePtr != index2Ptr->linePtr) {
	CleanupLine(index2Ptr->linePtr);
    }

    if (tkBTreeDebug) {
	TkBTreeCheck(index1Ptr->tree);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChangeNodeToggleCount --
 *
 *	This procedure increments or decrements the toggle count for
 *	a particular tag in a particular node and all its ancestors
 *	up to the per-tag root node.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The toggle count for tag is adjusted up or down by "delta" in
 *	nodePtr.  This routine maintains the tagRootPtr that identifies
 *	the root node for the tag, moving it up or down the tree as needed.
 *
 *----------------------------------------------------------------------
 */

static void
ChangeNodeToggleCount(nodePtr, tagPtr, delta)
    register Node *nodePtr;		/* Node whose toggle count for a tag
					 * must be changed. */
    TkTextTag *tagPtr;			/* Information about tag. */
    int delta;				/* Amount to add to current toggle
					 * count for tag (may be negative). */
{
    register Summary *summaryPtr, *prevPtr;
    register Node *node2Ptr;
    int rootLevel;			/* Level of original tag root */

    tagPtr->toggleCount += delta;
    if (tagPtr->tagRootPtr == (Node *) NULL) {
	tagPtr->tagRootPtr = nodePtr;
	return;
    }

    /*
     * Note the level of the existing root for the tag so we can detect
     * if it needs to be moved because of the toggle count change.
     */

    rootLevel = tagPtr->tagRootPtr->level;

    /*
     * Iterate over the node and its ancestors up to the tag root, adjusting
     * summary counts at each node and moving the tag's root upwards if
     * necessary.
     */

    for ( ; nodePtr != tagPtr->tagRootPtr; nodePtr = nodePtr->parentPtr) {
	/*
	 * See if there's already an entry for this tag for this node.  If so,
	 * perhaps all we have to do is adjust its count.
	 */
    
	for (prevPtr = NULL, summaryPtr = nodePtr->summaryPtr;
		summaryPtr != NULL;
		prevPtr = summaryPtr, summaryPtr = summaryPtr->nextPtr) {
	    if (summaryPtr->tagPtr == tagPtr) {
		break;
	    }
	}
	if (summaryPtr != NULL) {
	    summaryPtr->toggleCount += delta;
	    if (summaryPtr->toggleCount > 0 &&
		    summaryPtr->toggleCount < tagPtr->toggleCount) {
		continue;
	    }
	    if (summaryPtr->toggleCount != 0) {
		/*
		 * Should never find a node with max toggle count at this
		 * point (there shouldn't have been a summary entry in the
		 * first place).
		 */

		panic("ChangeNodeToggleCount: bad toggle count (%d) max (%d)",
		    summaryPtr->toggleCount, tagPtr->toggleCount);
	    }
    
	    /*
	     * Zero toggle count;  must remove this tag from the list.
	     */

	    if (prevPtr == NULL) {
		nodePtr->summaryPtr = summaryPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = summaryPtr->nextPtr;
	    }
	    ckfree((char *) summaryPtr);
	} else {
	    /*
	     * This tag isn't currently in the summary information list.
	     */
    
	    if (rootLevel == nodePtr->level) {
    
		/*
		 * The old tag root is at the same level in the tree as this
		 * node, but it isn't at this node.  Move the tag root up
		 * a level, in the hopes that it will now cover this node
		 * as well as the old root (if not, we'll move it up again
		 * the next time through the loop).  To push it up one level
		 * we copy the original toggle count into the summary
		 * information at the old root and change the root to its
		 * parent node.
		 */
    
		Node *rootNodePtr = tagPtr->tagRootPtr;
		summaryPtr = (Summary *) ckalloc(sizeof(Summary));
		summaryPtr->tagPtr = tagPtr;
		summaryPtr->toggleCount = tagPtr->toggleCount - delta;
		summaryPtr->nextPtr = rootNodePtr->summaryPtr;
		rootNodePtr->summaryPtr = summaryPtr;
		rootNodePtr = rootNodePtr->parentPtr;
		rootLevel = rootNodePtr->level;
		tagPtr->tagRootPtr = rootNodePtr;
	    }
	    summaryPtr = (Summary *) ckalloc(sizeof(Summary));
	    summaryPtr->tagPtr = tagPtr;
	    summaryPtr->toggleCount = delta;
	    summaryPtr->nextPtr = nodePtr->summaryPtr;
	    nodePtr->summaryPtr = summaryPtr;
	}
    }

    /*
     * If we've decremented the toggle count, then it may be necessary
     * to push the tag root down one or more levels.
     */

    if (delta >= 0) {
	return;
    }
    if (tagPtr->toggleCount == 0) {
	tagPtr->tagRootPtr = (Node *) NULL;
	return;
    }
    nodePtr = tagPtr->tagRootPtr;
    while (nodePtr->level > 0) {
	/*
	 * See if a single child node accounts for all of the tag's
	 * toggles.  If so, push the root down one level.
	 */

	for (node2Ptr = nodePtr->children.nodePtr;
		node2Ptr != (Node *)NULL ;
		node2Ptr = node2Ptr->nextPtr) {
	    for (prevPtr = NULL, summaryPtr = node2Ptr->summaryPtr;
		    summaryPtr != NULL;
		    prevPtr = summaryPtr, summaryPtr = summaryPtr->nextPtr) {
		if (summaryPtr->tagPtr == tagPtr) {
		    break;
		}
	    }
	    if (summaryPtr == NULL) {
		continue;
	    }
	    if (summaryPtr->toggleCount != tagPtr->toggleCount) {
		/*
		 * No node has all toggles, so the root is still valid.
		 */

		return;
	    }

	    /*
	     * This node has all the toggles, so push down the root.
	     */

	    if (prevPtr == NULL) {
		node2Ptr->summaryPtr = summaryPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = summaryPtr->nextPtr;
	    }
	    ckfree((char *) summaryPtr);
	    tagPtr->tagRootPtr = node2Ptr;
	    break;
	}
	nodePtr = tagPtr->tagRootPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FindTagStart --
 *
 *	Find the start of the first range of a tag.
 *
 * Results:
 *	The return value is a pointer to the first tag toggle segment
 *	for the tag.  This can be either a tagon or tagoff segments because
 *	of the way TkBTreeAdd removes a tag.
 *	Sets *indexPtr to be the index of the tag toggle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TkTextSegment *
FindTagStart(tree, tagPtr, indexPtr)
    TkTextBTree tree;			/* Tree to search within */
    TkTextTag *tagPtr;			/* Tag to search for. */
    TkTextIndex *indexPtr;		/* Return - index information */
{
    register Node *nodePtr;
    register TkTextLine *linePtr;
    register TkTextSegment *segPtr;
    register Summary *summaryPtr;
    int offset = 0;

    nodePtr = tagPtr->tagRootPtr;
    if (nodePtr == (Node *) NULL) {
	return NULL;
    }

    /*
     * Search from the root of the subtree that contains the tag down
     * to the level 0 node.
     */

    while (nodePtr->level > 0) {
	for (nodePtr = nodePtr->children.nodePtr ; nodePtr != (Node *) NULL;
		nodePtr = nodePtr->nextPtr) {
	    for (summaryPtr = nodePtr->summaryPtr ; summaryPtr != NULL;
		    summaryPtr = summaryPtr->nextPtr) {
		if (summaryPtr->tagPtr == tagPtr) {
		    goto gotNodeWithTag;
		}
	    }
	}
	gotNodeWithTag:
	continue;
    }

    /*
     * Work through the lines attached to the level-0 node.
     */

    for (linePtr = nodePtr->children.linePtr; linePtr != (TkTextLine *) NULL;
	    linePtr = linePtr->nextPtr) {
	for (offset = 0, segPtr = linePtr->segPtr ; segPtr != NULL;
		offset += segPtr->size, segPtr = segPtr->nextPtr) {
	    if (((segPtr->typePtr == &tkTextToggleOnType)
		    || (segPtr->typePtr == &tkTextToggleOffType))
		    && (segPtr->body.toggle.tagPtr == tagPtr)) {
		/*
		 * It is possible that this is a tagoff tag, but that
		 * gets cleaned up later.
		 */
		indexPtr->tree = tree;
		indexPtr->linePtr = linePtr;
		indexPtr->charIndex = offset;
		return segPtr;
	    }
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * FindTagEnd --
 *
 *	Find the end of the last range of a tag.
 *
 * Results:
 *	The return value is a pointer to the last tag toggle segment
 *	for the tag.  This can be either a tagon or tagoff segments because
 *	of the way TkBTreeAdd removes a tag.
 *	Sets *indexPtr to be the index of the tag toggle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TkTextSegment *
FindTagEnd(tree, tagPtr, indexPtr)
    TkTextBTree tree;			/* Tree to search within */
    TkTextTag *tagPtr;			/* Tag to search for. */
    TkTextIndex *indexPtr;		/* Return - index information */
{
    register Node *nodePtr, *lastNodePtr;
    register TkTextLine *linePtr ,*lastLinePtr;
    register TkTextSegment *segPtr, *lastSegPtr, *last2SegPtr;
    register Summary *summaryPtr;
    int lastoffset, lastoffset2, offset = 0;

    nodePtr = tagPtr->tagRootPtr;
    if (nodePtr == (Node *) NULL) {
	return NULL;
    }

    /*
     * Search from the root of the subtree that contains the tag down
     * to the level 0 node.
     */

    while (nodePtr->level > 0) {
	for (lastNodePtr = NULL, nodePtr = nodePtr->children.nodePtr ;
		nodePtr != (Node *) NULL; nodePtr = nodePtr->nextPtr) {
	    for (summaryPtr = nodePtr->summaryPtr ; summaryPtr != NULL;
		    summaryPtr = summaryPtr->nextPtr) {
		if (summaryPtr->tagPtr == tagPtr) {
		    lastNodePtr = nodePtr;
		    break;
		}
	    }
	}
	nodePtr = lastNodePtr;
    }

    /*
     * Work through the lines attached to the level-0 node.
     */
    last2SegPtr = NULL;
    lastoffset2 = 0;
    lastoffset = 0;
    for (lastLinePtr = NULL, linePtr = nodePtr->children.linePtr;
	    linePtr != (TkTextLine *) NULL; linePtr = linePtr->nextPtr) {
	for (offset = 0, lastSegPtr = NULL, segPtr = linePtr->segPtr ;
		segPtr != NULL; 
		offset += segPtr->size, segPtr = segPtr->nextPtr) {
	    if (((segPtr->typePtr == &tkTextToggleOnType)
		    || (segPtr->typePtr == &tkTextToggleOffType))
		    && (segPtr->body.toggle.tagPtr == tagPtr)) {
		lastSegPtr = segPtr;
		lastoffset = offset;
	    }
	}
	if (lastSegPtr != NULL) {
	    lastLinePtr = linePtr;
	    last2SegPtr = lastSegPtr;
	    lastoffset2 = lastoffset;
	}
    }
    indexPtr->tree = tree;
    indexPtr->linePtr = lastLinePtr;
    indexPtr->charIndex = lastoffset2;
    return last2SegPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeStartSearch --
 *
 *	This procedure sets up a search for tag transitions involving
 *	a given tag (or all tags) in a given range of the text.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The information at *searchPtr is set up so that subsequent calls
 *	to TkBTreeNextTag or TkBTreePrevTag will return information about the
 *	locations of tag transitions.  Note that TkBTreeNextTag or 
 *	TkBTreePrevTag must be called to get the first transition.
 *	Note: unlike TkBTreeNextTag and TkBTreePrevTag, this routine does not
 *	guarantee that searchPtr->curIndex is equal to *index1Ptr.  It may be
 *	greater than that if *index1Ptr is less than the first tag transition.
 *
 *----------------------------------------------------------------------
 */

void
TkBTreeStartSearch(index1Ptr, index2Ptr, tagPtr, searchPtr)
    TkTextIndex *index1Ptr;		/* Search starts here.  Tag toggles
					 * at this position will not be
					 * returned. */
    TkTextIndex *index2Ptr;		/* Search stops here.  Tag toggles
					 * at this position *will* be
					 * returned. */
    TkTextTag *tagPtr;			/* Tag to search for.  NULL means
					 * search for any tag. */
    register TkTextSearch *searchPtr;	/* Where to store information about
					 * search's progress. */
{
    int offset;
    TkTextIndex index0;		/* First index of the tag */
    TkTextSegment *seg0Ptr;	/* First segment of the tag */

    /*
     * Find the segment that contains the first toggle for the tag.  This
     * may become the starting point in the search.
     */

    seg0Ptr = FindTagStart(index1Ptr->tree, tagPtr, &index0);
    if (seg0Ptr == (TkTextSegment *) NULL) {
	/*
	 * Even though there are no toggles, the display code still
	 * uses the search curIndex, so initialize that anyway.
	 */

	searchPtr->linesLeft = 0;
	searchPtr->curIndex = *index1Ptr;
	searchPtr->segPtr = NULL;
	searchPtr->nextPtr = NULL;
	return;
    }
    if (TkTextIndexCmp(index1Ptr, &index0) < 0) {
	/*
	 * Adjust start of search up to the first range of the tag
	 */

	searchPtr->curIndex = index0;
	searchPtr->segPtr = NULL;
	searchPtr->nextPtr = seg0Ptr;	/* Will be returned by NextTag */
	index1Ptr = &index0;
    } else {
	searchPtr->curIndex = *index1Ptr;
	searchPtr->segPtr = NULL;
	searchPtr->nextPtr = TkTextIndexToSeg(index1Ptr, &offset);
	searchPtr->curIndex.charIndex -= offset;
    }
    searchPtr->lastPtr = TkTextIndexToSeg(index2Ptr, (int *) NULL);
    searchPtr->tagPtr = tagPtr;
    searchPtr->linesLeft = TkBTreeLineIndex(index2Ptr->linePtr) + 1
	    - TkBTreeLineIndex(index1Ptr->linePtr);
    searchPtr->allTags = (tagPtr == NULL);
    if (searchPtr->linesLeft == 1) {
	/*
	 * Starting and stopping segments are in the same line; mark the
	 * search as over immediately if the second segment is before the
	 * first.  A search does not return a toggle at the very start of
	 * the range, unless the range is artificially moved up to index0.
	 */
	if (((index1Ptr == &index0) && 
		(index1Ptr->charIndex > index2Ptr->charIndex)) ||
	    ((index1Ptr != &index0) && 
		(index1Ptr->charIndex >= index2Ptr->charIndex))) {
		searchPtr->linesLeft = 0;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeStartSearchBack --
 *
 *	This procedure sets up a search backwards for tag transitions involving
 *	a given tag (or all tags) in a given range of the text.  In the
 *	normal case the first index (*index1Ptr) is beyond the second
 *	index (*index2Ptr).
 *	
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The information at *searchPtr is set up so that subsequent calls
 *	to TkBTreePrevTag will return information about the
 *	locations of tag transitions.  Note that TkBTreePrevTag must be called
 *	to get the first transition.
 *	Note: unlike TkBTreeNextTag and TkBTreePrevTag, this routine does not
 *	guarantee that searchPtr->curIndex is equal to *index1Ptr.  It may be
 *	less than that if *index1Ptr is greater than the last tag transition.
 *
 *----------------------------------------------------------------------
 */

void
TkBTreeStartSearchBack(index1Ptr, index2Ptr, tagPtr, searchPtr)
    TkTextIndex *index1Ptr;		/* Search starts here.  Tag toggles
					 * at this position will not be
					 * returned. */
    TkTextIndex *index2Ptr;		/* Search stops here.  Tag toggles
					 * at this position *will* be
					 * returned. */
    TkTextTag *tagPtr;			/* Tag to search for.  NULL means
					 * search for any tag. */
    register TkTextSearch *searchPtr;	/* Where to store information about
					 * search's progress. */
{
    int offset;
    TkTextIndex index0;		/* Last index of the tag */
    TkTextIndex backOne;	/* One character before starting index */
    TkTextSegment *seg0Ptr;	/* Last segment of the tag */

    /*
     * Find the segment that contains the last toggle for the tag.  This
     * may become the starting point in the search.
     */

    seg0Ptr = FindTagEnd(index1Ptr->tree, tagPtr, &index0);
    if (seg0Ptr == (TkTextSegment *) NULL) {
	/*
	 * Even though there are no toggles, the display code still
	 * uses the search curIndex, so initialize that anyway.
	 */

	searchPtr->linesLeft = 0;
	searchPtr->curIndex = *index1Ptr;
	searchPtr->segPtr = NULL;
	searchPtr->nextPtr = NULL;
	return;
    }

    /*
     * Adjust the start of the search so it doesn't find any tag toggles
     * that are right at the index specified by the user.
     */

    if (TkTextIndexCmp(index1Ptr, &index0) > 0) {
	searchPtr->curIndex = index0;
	index1Ptr = &index0;
    } else {
	TkTextIndexBackChars(index1Ptr, 1, &searchPtr->curIndex);
    }
    searchPtr->segPtr = NULL;
    searchPtr->nextPtr = TkTextIndexToSeg(&searchPtr->curIndex, &offset);
    searchPtr->curIndex.charIndex -= offset;

    /*
     * Adjust the end of the search so it does find toggles that are right
     * at the second index specified by the user.
     */

    if ((TkBTreeLineIndex(index2Ptr->linePtr) == 0) &&
	    (index2Ptr->charIndex == 0)) {
	backOne = *index2Ptr;
	searchPtr->lastPtr = NULL;	/* Signals special case for 1.0 */
    } else {
	TkTextIndexBackChars(index2Ptr, 1, &backOne);
	searchPtr->lastPtr = TkTextIndexToSeg(&backOne, (int *) NULL);
    }
    searchPtr->tagPtr = tagPtr;
    searchPtr->linesLeft = TkBTreeLineIndex(index1Ptr->linePtr) + 1
	    - TkBTreeLineIndex(backOne.linePtr);
    searchPtr->allTags = (tagPtr == NULL);
    if (searchPtr->linesLeft == 1) {
	/*
	 * Starting and stopping segments are in the same line; mark the
	 * search as over immediately if the second segment is after the
	 * first.
	 */

	if (index1Ptr->charIndex <= backOne.charIndex) {
	    searchPtr->linesLeft = 0;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeNextTag --
 *
 *	Once a tag search has begun, successive calls to this procedure
 *	return successive tag toggles.  Note:  it is NOT SAFE to call this
 *	procedure if characters have been inserted into or deleted from
 *	the B-tree since the call to TkBTreeStartSearch.
 *
 * Results:
 *	The return value is 1 if another toggle was found that met the
 *	criteria specified in the call to TkBTreeStartSearch;  in this
 *	case searchPtr->curIndex gives the toggle's position and
 *	searchPtr->curTagPtr points to its segment.  0 is returned if
 *	no more matching tag transitions were found; in this case
 *	searchPtr->curIndex is the same as searchPtr->stopIndex.
 *
 * Side effects:
 *	Information in *searchPtr is modified to update the state of the
 *	search and indicate where the next tag toggle is located.
 *
 *----------------------------------------------------------------------
 */

int
TkBTreeNextTag(searchPtr)
    register TkTextSearch *searchPtr;	/* Information about search in
					 * progress;  must have been set up by
					 * call to TkBTreeStartSearch. */
{
    register TkTextSegment *segPtr;
    register Node *nodePtr;
    register Summary *summaryPtr;

    if (searchPtr->linesLeft <= 0) {
	goto searchOver;
    }

    /*
     * The outermost loop iterates over lines that may potentially contain
     * a relevant tag transition, starting from the current segment in
     * the current line.
     */

    segPtr = searchPtr->nextPtr;
    while (1) {
	/*
	 * Check for more tags on the current line.
	 */

	for ( ; segPtr != NULL; segPtr = segPtr->nextPtr) {
	    if (segPtr == searchPtr->lastPtr) {
		goto searchOver;
	    }
	    if (((segPtr->typePtr == &tkTextToggleOnType)
		    || (segPtr->typePtr == &tkTextToggleOffType))
		    && (searchPtr->allTags
		    || (segPtr->body.toggle.tagPtr == searchPtr->tagPtr))) {
		searchPtr->segPtr = segPtr;
		searchPtr->nextPtr = segPtr->nextPtr;
		searchPtr->tagPtr = segPtr->body.toggle.tagPtr;
		return 1;
	    }
	    searchPtr->curIndex.charIndex += segPtr->size;
	}
    
	/*
	 * See if there are more lines associated with the current parent
	 * node.  If so, go back to the top of the loop to search the next
	 * one.
	 */

	nodePtr = searchPtr->curIndex.linePtr->parentPtr;
	searchPtr->curIndex.linePtr = searchPtr->curIndex.linePtr->nextPtr;
	searchPtr->linesLeft--;
	if (searchPtr->linesLeft <= 0) {
	    goto searchOver;
	}
	if (searchPtr->curIndex.linePtr != NULL) {
	    segPtr = searchPtr->curIndex.linePtr->segPtr;
	    searchPtr->curIndex.charIndex = 0;
	    continue;
	}
	if (nodePtr == searchPtr->tagPtr->tagRootPtr) {
	    goto searchOver;
	}
    
	/*
	 * Search across and up through the B-tree's node hierarchy looking
	 * for the next node that has a relevant tag transition somewhere in
	 * its subtree.  Be sure to update linesLeft as we skip over large
	 * chunks of lines.
	 */
    
	while (1) {
	    while (nodePtr->nextPtr == NULL) {
		if (nodePtr->parentPtr == NULL ||
		    nodePtr->parentPtr == searchPtr->tagPtr->tagRootPtr) {
		    goto searchOver;
		}
		nodePtr = nodePtr->parentPtr;
	    }
	    nodePtr = nodePtr->nextPtr;
	    for (summaryPtr = nodePtr->summaryPtr; summaryPtr != NULL;
		    summaryPtr = summaryPtr->nextPtr) {
		if ((searchPtr->allTags) ||
			(summaryPtr->tagPtr == searchPtr->tagPtr)) {
		    goto gotNodeWithTag;
		}
	    }
	    searchPtr->linesLeft -= nodePtr->numLines;
	}
    
	/*
	 * At this point we've found a subtree that has a relevant tag
	 * transition.  Now search down (and across) through that subtree
	 * to find the first level-0 node that has a relevant tag transition.
	 */
    
	gotNodeWithTag:
	while (nodePtr->level > 0) {
	    for (nodePtr = nodePtr->children.nodePtr; ;
		    nodePtr = nodePtr->nextPtr) {
		for (summaryPtr = nodePtr->summaryPtr; summaryPtr != NULL;
			summaryPtr = summaryPtr->nextPtr) {
		    if ((searchPtr->allTags)
			    || (summaryPtr->tagPtr == searchPtr->tagPtr)) {
			goto nextChild;
		    }
		}
		searchPtr->linesLeft -= nodePtr->numLines;
		if (nodePtr->nextPtr == NULL) {
		    panic("TkBTreeNextTag found incorrect tag summary info.");
		}
	    }
	    nextChild:
	    continue;
	}
    
	/*
	 * Now we're down to a level-0 node that contains a line that contains
	 * a relevant tag transition.  Set up line information and go back to
	 * the beginning of the loop to search through lines.
	 */

	searchPtr->curIndex.linePtr = nodePtr->children.linePtr;
	searchPtr->curIndex.charIndex = 0;
	segPtr = searchPtr->curIndex.linePtr->segPtr;
	if (searchPtr->linesLeft <= 0) {
	    goto searchOver;
	}
	continue;
    }

    searchOver:
    searchPtr->linesLeft = 0;
    searchPtr->segPtr = NULL;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreePrevTag --
 *
 *	Once a tag search has begun, successive calls to this procedure
 *	return successive tag toggles in the reverse direction.
 *	Note:  it is NOT SAFE to call this
 *	procedure if characters have been inserted into or deleted from
 *	the B-tree since the call to TkBTreeStartSearch.
 *
 * Results:
 *	The return value is 1 if another toggle was found that met the
 *	criteria specified in the call to TkBTreeStartSearch;  in this
 *	case searchPtr->curIndex gives the toggle's position and
 *	searchPtr->curTagPtr points to its segment.  0 is returned if
 *	no more matching tag transitions were found; in this case
 *	searchPtr->curIndex is the same as searchPtr->stopIndex.
 *
 * Side effects:
 *	Information in *searchPtr is modified to update the state of the
 *	search and indicate where the next tag toggle is located.
 *
 *----------------------------------------------------------------------
 */

int
TkBTreePrevTag(searchPtr)
    register TkTextSearch *searchPtr;	/* Information about search in
					 * progress;  must have been set up by
					 * call to TkBTreeStartSearch. */
{
    register TkTextSegment *segPtr, *prevPtr;
    register TkTextLine *linePtr, *prevLinePtr;
    register Node *nodePtr, *node2Ptr, *prevNodePtr;
    register Summary *summaryPtr;
    int charIndex;
    int pastLast;			/* Saw last marker during scan */
    int linesSkipped;

    if (searchPtr->linesLeft <= 0) {
	goto searchOver;
    }

    /*
     * The outermost loop iterates over lines that may potentially contain
     * a relevant tag transition, starting from the current segment in
     * the current line.  "nextPtr" is maintained as the last segment in
     * a line that we can look at. 
     */

    while (1) {
	/*
	 * Check for the last toggle before the current segment on this line.
	 */
	charIndex = 0;
	if (searchPtr->lastPtr == NULL) {
	    /* 
	     * Search back to the very beginning, so pastLast is irrelevent.
	     */
	    pastLast = 1; 
	} else {
	    pastLast = 0;
	}
	for (prevPtr = NULL, segPtr = searchPtr->curIndex.linePtr->segPtr ;
		segPtr != NULL && segPtr != searchPtr->nextPtr;
		segPtr = segPtr->nextPtr) {
	    if (((segPtr->typePtr == &tkTextToggleOnType)
		    || (segPtr->typePtr == &tkTextToggleOffType))
		    && (searchPtr->allTags
		    || (segPtr->body.toggle.tagPtr == searchPtr->tagPtr))) {
		prevPtr = segPtr;
		searchPtr->curIndex.charIndex = charIndex;
	    }
	    if (segPtr == searchPtr->lastPtr) {
	        prevPtr = NULL;   /* Segments earlier than last don't count */
		pastLast = 1;
	    }
	    charIndex += segPtr->size;
	}
	if (prevPtr != NULL) {
	    if (searchPtr->linesLeft == 1 && !pastLast) {
		/*
		 * We found a segment that is before the stopping index.
		 * Note that it is OK if prevPtr == lastPtr.
		 */
		goto searchOver;
	    }
	    searchPtr->segPtr = prevPtr;
	    searchPtr->nextPtr = prevPtr;
	    searchPtr->tagPtr = prevPtr->body.toggle.tagPtr;
	    return 1;
	}
    
	searchPtr->linesLeft--;
	if (searchPtr->linesLeft <= 0) {
	    goto searchOver;
	}

	/*
	 * See if there are more lines associated with the current parent
	 * node.  If so, go back to the top of the loop to search the previous
	 * one.
	 */

	nodePtr = searchPtr->curIndex.linePtr->parentPtr;
	for (prevLinePtr = NULL, linePtr = nodePtr->children.linePtr;
		linePtr != NULL && linePtr != searchPtr->curIndex.linePtr;
		prevLinePtr = linePtr, linePtr = linePtr->nextPtr) {
	    /* empty loop body */ ;
	}
	if (prevLinePtr != NULL) {
	    searchPtr->curIndex.linePtr = prevLinePtr;
	    searchPtr->nextPtr = NULL;
	    continue;
	}
	if (nodePtr == searchPtr->tagPtr->tagRootPtr) {
	    goto searchOver;
	}
    
	/*
	 * Search across and up through the B-tree's node hierarchy looking
	 * for the previous node that has a relevant tag transition somewhere in
	 * its subtree.  The search and line counting is trickier with/out
	 * back pointers. We'll scan all the nodes under a parent up to
	 * the current node, searching all of them for tag state.  The last
	 * one we find, if any, is recorded in prevNodePtr, and any nodes
	 * past prevNodePtr that don't have tag state increment linesSkipped.
	 */
    
	while (1) {
	    for (prevNodePtr = NULL, linesSkipped = 0,
		    node2Ptr = nodePtr->parentPtr->children.nodePtr ;
		    node2Ptr != nodePtr;  node2Ptr = node2Ptr->nextPtr) {
		for (summaryPtr = node2Ptr->summaryPtr; summaryPtr != NULL;
			summaryPtr = summaryPtr->nextPtr) {
		    if ((searchPtr->allTags) ||
			    (summaryPtr->tagPtr == searchPtr->tagPtr)) {
			prevNodePtr = node2Ptr;
			linesSkipped = 0;
			goto keepLooking;
		    }
		}
		linesSkipped += node2Ptr->numLines;

		keepLooking:
		continue;
	    }
	    if (prevNodePtr != NULL) {
		nodePtr = prevNodePtr;
		searchPtr->linesLeft -= linesSkipped;
		goto gotNodeWithTag;
	    }
	    nodePtr = nodePtr->parentPtr;
	    if (nodePtr->parentPtr == NULL ||
		nodePtr == searchPtr->tagPtr->tagRootPtr) {
		goto searchOver;
	    }
	}
    
	/*
	 * At this point we've found a subtree that has a relevant tag
	 * transition.  Now search down (and across) through that subtree
	 * to find the last level-0 node that has a relevant tag transition.
	 */
    
	gotNodeWithTag:
	while (nodePtr->level > 0) {
	    for (linesSkipped = 0, prevNodePtr = NULL,
		    nodePtr = nodePtr->children.nodePtr; nodePtr != NULL ;
		    nodePtr = nodePtr->nextPtr) {
		for (summaryPtr = nodePtr->summaryPtr; summaryPtr != NULL;
			summaryPtr = summaryPtr->nextPtr) {
		    if ((searchPtr->allTags)
			    || (summaryPtr->tagPtr == searchPtr->tagPtr)) {
			prevNodePtr = nodePtr;
			linesSkipped = 0;
			goto keepLooking2;
		    }
		}
		linesSkipped += nodePtr->numLines;

		keepLooking2:
		continue;
	    }
	    if (prevNodePtr == NULL) {
		panic("TkBTreePrevTag found incorrect tag summary info.");
	    }
	    searchPtr->linesLeft -= linesSkipped;
	    nodePtr = prevNodePtr;
	}
    
	/*
	 * Now we're down to a level-0 node that contains a line that contains
	 * a relevant tag transition.  Set up line information and go back to
	 * the beginning of the loop to search through lines.  We start with
	 * the last line below the node.
	 */

	for (prevLinePtr = NULL, linePtr = nodePtr->children.linePtr;
		linePtr != NULL ;
		prevLinePtr = linePtr, linePtr = linePtr->nextPtr) {
	    /* empty loop body */ ;
	}
	searchPtr->curIndex.linePtr = prevLinePtr;
	searchPtr->curIndex.charIndex = 0;
	if (searchPtr->linesLeft <= 0) {
	    goto searchOver;
	}
	continue;
    }

    searchOver:
    searchPtr->linesLeft = 0;
    searchPtr->segPtr = NULL;
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeCharTagged --
 *
 *	Determine whether a particular character has a particular tag.
 *
 * Results:
 *	The return value is 1 if the given tag is in effect at the
 *	character given by linePtr and ch, and 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkBTreeCharTagged(indexPtr, tagPtr)
    TkTextIndex *indexPtr;		/* Indicates a character position at
					 * which to check for a tag. */
    TkTextTag *tagPtr;			/* Tag of interest. */
{
    register Node *nodePtr;
    register TkTextLine *siblingLinePtr;
    register TkTextSegment *segPtr;
    TkTextSegment *toggleSegPtr;
    int toggles, index;

    /* 
     * Check for toggles for the tag in indexPtr's line but before
     * indexPtr.  If there is one, its type indicates whether or
     * not the character is tagged.
     */

    toggleSegPtr = NULL;
    for (index = 0, segPtr = indexPtr->linePtr->segPtr;
	    (index + segPtr->size) <= indexPtr->charIndex;
	    index += segPtr->size, segPtr = segPtr->nextPtr) {
	if (((segPtr->typePtr == &tkTextToggleOnType)
		|| (segPtr->typePtr == &tkTextToggleOffType))
		&& (segPtr->body.toggle.tagPtr == tagPtr)) {
	    toggleSegPtr = segPtr;
	}
    }
    if (toggleSegPtr != NULL) {
	return (toggleSegPtr->typePtr == &tkTextToggleOnType);
    }

    /*
     * No toggle in this line.  Look for toggles for the tag in lines
     * that are predecessors of indexPtr->linePtr but under the same
     * level-0 node.
     */

    toggles = 0;
    for (siblingLinePtr = indexPtr->linePtr->parentPtr->children.linePtr;
	    siblingLinePtr != indexPtr->linePtr;
	    siblingLinePtr = siblingLinePtr->nextPtr) {
	for (segPtr = siblingLinePtr->segPtr; segPtr != NULL;
		segPtr = segPtr->nextPtr) {
	    if (((segPtr->typePtr == &tkTextToggleOnType)
		    || (segPtr->typePtr == &tkTextToggleOffType))
		    && (segPtr->body.toggle.tagPtr == tagPtr)) {
		toggleSegPtr = segPtr;
	    }
	}
    }
    if (toggleSegPtr != NULL) {
	return (toggleSegPtr->typePtr == &tkTextToggleOnType);
    }

    /*
     * No toggle in this node.  Scan upwards through the ancestors of
     * this node, counting the number of toggles of the given tag in
     * siblings that precede that node.
     */

    toggles = 0;
    for (nodePtr = indexPtr->linePtr->parentPtr; nodePtr->parentPtr != NULL;
	    nodePtr = nodePtr->parentPtr) {
	register Node *siblingPtr;
	register Summary *summaryPtr;

	for (siblingPtr = nodePtr->parentPtr->children.nodePtr; 
		siblingPtr != nodePtr; siblingPtr = siblingPtr->nextPtr) {
	    for (summaryPtr = siblingPtr->summaryPtr; summaryPtr != NULL;
		    summaryPtr = summaryPtr->nextPtr) {
		if (summaryPtr->tagPtr == tagPtr) {
		    toggles += summaryPtr->toggleCount;
		}
	    }
	}
	if (nodePtr == tagPtr->tagRootPtr) {
	    break;
	}
    }

    /*
     * An odd number of toggles means that the tag is present at the
     * given point.
     */

    return toggles & 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeGetTags --
 *
 *	Return information about all of the tags that are associated
 *	with a particular character in a B-tree of text.
 *
 * Results:
 *	The return value is a malloc-ed array containing pointers to
 *	information for each of the tags that is associated with
 *	the character at the position given by linePtr and ch.  The
 *	word at *numTagsPtr is filled in with the number of pointers
 *	in the array.  It is up to the caller to free the array by
 *	passing it to free.  If there are no tags at the given character
 *	then a NULL pointer is returned and *numTagsPtr will be set to 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
TkTextTag **
TkBTreeGetTags(indexPtr, numTagsPtr)
    TkTextIndex *indexPtr;	/* Indicates a particular position in
				 * the B-tree. */
    int *numTagsPtr;		/* Store number of tags found at this
				 * location. */
{
    register Node *nodePtr;
    register TkTextLine *siblingLinePtr;
    register TkTextSegment *segPtr;
    int src, dst, index;
    TagInfo tagInfo;
#define NUM_TAG_INFOS 10

    tagInfo.numTags = 0;
    tagInfo.arraySize = NUM_TAG_INFOS;
    tagInfo.tagPtrs = (TkTextTag **) ckalloc((unsigned)
	    NUM_TAG_INFOS*sizeof(TkTextTag *));
    tagInfo.counts = (int *) ckalloc((unsigned)
	    NUM_TAG_INFOS*sizeof(int));

    /*
     * Record tag toggles within the line of indexPtr but preceding
     * indexPtr.
     */

    for (index = 0, segPtr = indexPtr->linePtr->segPtr;
	    (index + segPtr->size) <= indexPtr->charIndex;
	    index += segPtr->size, segPtr = segPtr->nextPtr) {
	if ((segPtr->typePtr == &tkTextToggleOnType)
		|| (segPtr->typePtr == &tkTextToggleOffType)) {
	    IncCount(segPtr->body.toggle.tagPtr, 1, &tagInfo);
	}
    }

    /*
     * Record toggles for tags in lines that are predecessors of
     * indexPtr->linePtr but under the same level-0 node.
     */

    for (siblingLinePtr = indexPtr->linePtr->parentPtr->children.linePtr;
	    siblingLinePtr != indexPtr->linePtr;
	    siblingLinePtr = siblingLinePtr->nextPtr) {
	for (segPtr = siblingLinePtr->segPtr; segPtr != NULL;
		segPtr = segPtr->nextPtr) {
	    if ((segPtr->typePtr == &tkTextToggleOnType)
		    || (segPtr->typePtr == &tkTextToggleOffType)) {
		IncCount(segPtr->body.toggle.tagPtr, 1, &tagInfo);
	    }
	}
    }

    /*
     * For each node in the ancestry of this line, record tag toggles
     * for all siblings that precede that node.
     */

    for (nodePtr = indexPtr->linePtr->parentPtr; nodePtr->parentPtr != NULL;
	    nodePtr = nodePtr->parentPtr) {
	register Node *siblingPtr;
	register Summary *summaryPtr;

	for (siblingPtr = nodePtr->parentPtr->children.nodePtr; 
		siblingPtr != nodePtr; siblingPtr = siblingPtr->nextPtr) {
	    for (summaryPtr = siblingPtr->summaryPtr; summaryPtr != NULL;
		    summaryPtr = summaryPtr->nextPtr) {
		if (summaryPtr->toggleCount & 1) {
		    IncCount(summaryPtr->tagPtr, summaryPtr->toggleCount,
			    &tagInfo);
		}
	    }
	}
    }

    /*
     * Go through the tag information and squash out all of the tags
     * that have even toggle counts (these tags exist before the point
     * of interest, but not at the desired character itself).
     */

    for (src = 0, dst = 0; src < tagInfo.numTags; src++) {
	if (tagInfo.counts[src] & 1) {
	    tagInfo.tagPtrs[dst] = tagInfo.tagPtrs[src];
	    dst++;
	}
    }
    *numTagsPtr = dst;
    ckfree((char *) tagInfo.counts);
    if (dst == 0) {
	ckfree((char *) tagInfo.tagPtrs);
	return NULL;
    }
    return tagInfo.tagPtrs;
}

/*
 *----------------------------------------------------------------------
 *
 * IncCount --
 *
 *	This is a utility procedure used by TkBTreeGetTags.  It
 *	increments the count for a particular tag, adding a new
 *	entry for that tag if there wasn't one previously.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The information at *tagInfoPtr may be modified, and the arrays
 *	may be reallocated to make them larger.
 *
 *----------------------------------------------------------------------
 */

static void
IncCount(tagPtr, inc, tagInfoPtr)
    TkTextTag *tagPtr;		/* Handle for tag. */
    int inc;			/* Amount by which to increment tag count. */
    TagInfo *tagInfoPtr;	/* Holds cumulative information about tags;
				 * increment count here. */
{
    register TkTextTag **tagPtrPtr;
    int count;

    for (tagPtrPtr = tagInfoPtr->tagPtrs, count = tagInfoPtr->numTags;
	    count > 0; tagPtrPtr++, count--) {
	if (*tagPtrPtr == tagPtr) {
	    tagInfoPtr->counts[tagInfoPtr->numTags-count] += inc;
	    return;
	}
    }

    /*
     * There isn't currently an entry for this tag, so we have to
     * make a new one.  If the arrays are full, then enlarge the
     * arrays first.
     */

    if (tagInfoPtr->numTags == tagInfoPtr->arraySize) {
	TkTextTag **newTags;
	int *newCounts, newSize;

	newSize = 2*tagInfoPtr->arraySize;
	newTags = (TkTextTag **) ckalloc((unsigned)
		(newSize*sizeof(TkTextTag *)));
	memcpy((VOID *) newTags, (VOID *) tagInfoPtr->tagPtrs,
		tagInfoPtr->arraySize * sizeof(TkTextTag *));
	ckfree((char *) tagInfoPtr->tagPtrs);
	tagInfoPtr->tagPtrs = newTags;
	newCounts = (int *) ckalloc((unsigned) (newSize*sizeof(int)));
	memcpy((VOID *) newCounts, (VOID *) tagInfoPtr->counts,
		tagInfoPtr->arraySize * sizeof(int));
	ckfree((char *) tagInfoPtr->counts);
	tagInfoPtr->counts = newCounts;
	tagInfoPtr->arraySize = newSize;
    }

    tagInfoPtr->tagPtrs[tagInfoPtr->numTags] = tagPtr;
    tagInfoPtr->counts[tagInfoPtr->numTags] = inc;
    tagInfoPtr->numTags++;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeCheck --
 *
 *	This procedure runs a set of consistency checks over a B-tree
 *	and panics if any inconsistencies are found.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a structural defect is found, the procedure panics with an
 *	error message.
 *
 *----------------------------------------------------------------------
 */

void
TkBTreeCheck(tree)
    TkTextBTree tree;		/* Tree to check. */
{
    BTree *treePtr = (BTree *) tree;
    register Summary *summaryPtr;
    register Node *nodePtr;
    register TkTextLine *linePtr;
    register TkTextSegment *segPtr;
    register TkTextTag *tagPtr;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    int count;

    /*
     * Make sure that the tag toggle counts and the tag root pointers are OK.
     */
    for (entryPtr = Tcl_FirstHashEntry(&treePtr->textPtr->tagTable, &search);
	    entryPtr != NULL ; entryPtr = Tcl_NextHashEntry(&search)) {
	tagPtr = (TkTextTag *) Tcl_GetHashValue(entryPtr);
	nodePtr = tagPtr->tagRootPtr;
	if (nodePtr == (Node *) NULL) {
	    if (tagPtr->toggleCount != 0) {
		panic("TkBTreeCheck found \"%s\" with toggles (%d) but no root",
		    tagPtr->name, tagPtr->toggleCount);
	    }
	    continue;		/* no ranges for the tag */
	} else if (tagPtr->toggleCount == 0) {
	    panic("TkBTreeCheck found root for \"%s\" with no toggles",
		    tagPtr->name);
	} else if (tagPtr->toggleCount & 1) {
	    panic("TkBTreeCheck found odd toggle count for \"%s\" (%d)",
		    tagPtr->name, tagPtr->toggleCount);
	}
	for (summaryPtr = nodePtr->summaryPtr; summaryPtr != NULL;
		summaryPtr = summaryPtr->nextPtr) {
	    if (summaryPtr->tagPtr == tagPtr) {
		panic("TkBTreeCheck found root node with summary info");
	    }
	}
	count = 0;
	if (nodePtr->level > 0) {
	    for (nodePtr = nodePtr->children.nodePtr ; nodePtr != NULL ;
		    nodePtr = nodePtr->nextPtr) {
		for (summaryPtr = nodePtr->summaryPtr; summaryPtr != NULL;
			summaryPtr = summaryPtr->nextPtr) {
		    if (summaryPtr->tagPtr == tagPtr) {
			count += summaryPtr->toggleCount;
		    }
		}
	    }
	} else {
	    for (linePtr = nodePtr->children.linePtr ; linePtr != NULL ;
		    linePtr = linePtr->nextPtr) {
		for (segPtr = linePtr->segPtr; segPtr != NULL;
			segPtr = segPtr->nextPtr) {
		    if ((segPtr->typePtr == &tkTextToggleOnType ||
			 segPtr->typePtr == &tkTextToggleOffType) &&
			 segPtr->body.toggle.tagPtr == tagPtr) {
			count++;
		    }
		}
	    }
	}
	if (count != tagPtr->toggleCount) {
	    panic("TkBTreeCheck toggleCount (%d) wrong for \"%s\" should be (%d)",
		tagPtr->toggleCount, tagPtr->name, count);
	}
    }

    /*
     * Call a recursive procedure to do the main body of checks.
     */

    nodePtr = treePtr->rootPtr;
    CheckNodeConsistency(treePtr->rootPtr);

    /*
     * Make sure that there are at least two lines in the text and
     * that the last line has no characters except a newline.
     */

    if (nodePtr->numLines < 2) {
	panic("TkBTreeCheck: less than 2 lines in tree");
    }
    while (nodePtr->level > 0) {
	nodePtr = nodePtr->children.nodePtr;
	while (nodePtr->nextPtr != NULL) {
	    nodePtr = nodePtr->nextPtr;
	}
    }
    linePtr = nodePtr->children.linePtr;
    while (linePtr->nextPtr != NULL) {
	linePtr = linePtr->nextPtr;
    }
    segPtr = linePtr->segPtr;
    while ((segPtr->typePtr == &tkTextToggleOffType)
	    || (segPtr->typePtr == &tkTextRightMarkType)
	    || (segPtr->typePtr == &tkTextLeftMarkType)) {
	/*
	 * It's OK to toggle a tag off in the last line, but
	 * not to start a new range.  It's also OK to have marks
	 * in the last line.
	 */

	segPtr = segPtr->nextPtr;
    }
    if (segPtr->typePtr != &tkTextCharType) {
	panic("TkBTreeCheck: last line has bogus segment type");
    }
    if (segPtr->nextPtr != NULL) {
	panic("TkBTreeCheck: last line has too many segments");
    }
    if (segPtr->size != 1) {
	panic("TkBTreeCheck: last line has wrong # characters: %d",
		segPtr->size);
    }
    if ((segPtr->body.chars[0] != '\n') || (segPtr->body.chars[1] != 0)) {
	panic("TkBTreeCheck: last line had bad value: %s",
		segPtr->body.chars);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CheckNodeConsistency --
 *
 *	This procedure is called as part of consistency checking for
 *	B-trees:  it checks several aspects of a node and also runs
 *	checks recursively on the node's children.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If anything suspicious is found in the tree structure, the
 *	procedure panics.
 *
 *----------------------------------------------------------------------
 */

static void
CheckNodeConsistency(nodePtr)
    register Node *nodePtr;		/* Node whose subtree should be
					 * checked. */
{
    register Node *childNodePtr;
    register Summary *summaryPtr, *summaryPtr2;
    register TkTextLine *linePtr;
    register TkTextSegment *segPtr;
    int numChildren, numLines, toggleCount, minChildren;

    if (nodePtr->parentPtr != NULL) {
	minChildren = MIN_CHILDREN;
    } else if (nodePtr->level > 0) {
	minChildren = 2;
    } else  {
	minChildren = 1;
    }
    if ((nodePtr->numChildren < minChildren)
	    || (nodePtr->numChildren > MAX_CHILDREN)) {
	panic("CheckNodeConsistency: bad child count (%d)",
		nodePtr->numChildren);
    }

    numChildren = 0;
    numLines = 0;
    if (nodePtr->level == 0) {
	for (linePtr = nodePtr->children.linePtr; linePtr != NULL;
		linePtr = linePtr->nextPtr) {
	    if (linePtr->parentPtr != nodePtr) {
		panic("CheckNodeConsistency: line doesn't point to parent");
	    }
	    if (linePtr->segPtr == NULL) {
		panic("CheckNodeConsistency: line has no segments");
	    }
	    for (segPtr = linePtr->segPtr; segPtr != NULL;
		    segPtr = segPtr->nextPtr) {
		if (segPtr->typePtr->checkProc != NULL) {
		    (*segPtr->typePtr->checkProc)(segPtr, linePtr);
		}
		if ((segPtr->size == 0) && (!segPtr->typePtr->leftGravity)
			&& (segPtr->nextPtr != NULL)
			&& (segPtr->nextPtr->size == 0)
			&& (segPtr->nextPtr->typePtr->leftGravity)) {
		    panic("CheckNodeConsistency: wrong segment order for gravity");
		}
		if ((segPtr->nextPtr == NULL)
			&& (segPtr->typePtr != &tkTextCharType)) {
		    panic("CheckNodeConsistency: line ended with wrong type");
		}
	    }
	    numChildren++;
	    numLines++;
	}
    } else {
	for (childNodePtr = nodePtr->children.nodePtr; childNodePtr != NULL;
		childNodePtr = childNodePtr->nextPtr) {
	    if (childNodePtr->parentPtr != nodePtr) {
		panic("CheckNodeConsistency: node doesn't point to parent");
	    }
	    if (childNodePtr->level != (nodePtr->level-1)) {
		panic("CheckNodeConsistency: level mismatch (%d %d)",
			nodePtr->level, childNodePtr->level);
	    }
	    CheckNodeConsistency(childNodePtr);
	    for (summaryPtr = childNodePtr->summaryPtr; summaryPtr != NULL;
			summaryPtr = summaryPtr->nextPtr) {
		for (summaryPtr2 = nodePtr->summaryPtr; ;
			summaryPtr2 = summaryPtr2->nextPtr) {
		    if (summaryPtr2 == NULL) {
			if (summaryPtr->tagPtr->tagRootPtr == nodePtr) {
			    break;
			}
			panic("CheckNodeConsistency: node tag \"%s\" not %s",
				summaryPtr->tagPtr->name,
				"present in parent summaries");
		    }
		    if (summaryPtr->tagPtr == summaryPtr2->tagPtr) {
			break;
		    }
		}
	    }
	    numChildren++;
	    numLines += childNodePtr->numLines;
	}
    }
    if (numChildren != nodePtr->numChildren) {
	panic("CheckNodeConsistency: mismatch in numChildren (%d %d)",
		numChildren, nodePtr->numChildren);
    }
    if (numLines != nodePtr->numLines) {
	panic("CheckNodeConsistency: mismatch in numLines (%d %d)",
		numLines, nodePtr->numLines);
    }

    for (summaryPtr = nodePtr->summaryPtr; summaryPtr != NULL;
	    summaryPtr = summaryPtr->nextPtr) {
	if (summaryPtr->tagPtr->toggleCount == summaryPtr->toggleCount) {
	    panic("CheckNodeConsistency: found unpruned root for \"%s\"",
		summaryPtr->tagPtr->name);
	}
	toggleCount = 0;
	if (nodePtr->level == 0) {
	    for (linePtr = nodePtr->children.linePtr; linePtr != NULL;
		    linePtr = linePtr->nextPtr) {
		for (segPtr = linePtr->segPtr; segPtr != NULL;
			segPtr = segPtr->nextPtr) {
		    if ((segPtr->typePtr != &tkTextToggleOnType)
			    && (segPtr->typePtr != &tkTextToggleOffType)) {
			continue;
		    }
		    if (segPtr->body.toggle.tagPtr == summaryPtr->tagPtr) {
			toggleCount ++;
		    }
		}
	    }
	} else {
	    for (childNodePtr = nodePtr->children.nodePtr;
		    childNodePtr != NULL;
		    childNodePtr = childNodePtr->nextPtr) {
		for (summaryPtr2 = childNodePtr->summaryPtr;
			summaryPtr2 != NULL;
			summaryPtr2 = summaryPtr2->nextPtr) {
		    if (summaryPtr2->tagPtr == summaryPtr->tagPtr) {
			toggleCount += summaryPtr2->toggleCount;
		    }
		}
	    }
	}
	if (toggleCount != summaryPtr->toggleCount) {
	    panic("CheckNodeConsistency: mismatch in toggleCount (%d %d)",
		    toggleCount, summaryPtr->toggleCount);
	}
	for (summaryPtr2 = summaryPtr->nextPtr; summaryPtr2 != NULL;
		summaryPtr2 = summaryPtr2->nextPtr) {
	    if (summaryPtr2->tagPtr == summaryPtr->tagPtr) {
		panic("CheckNodeConsistency: duplicated node tag: %s",
			summaryPtr->tagPtr->name);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Rebalance --
 *
 *	This procedure is called when a node of a B-tree appears to be
 *	out of balance (too many children, or too few).  It rebalances
 *	that node and all of its ancestors in the tree.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The internal structure of treePtr may change.
 *
 *----------------------------------------------------------------------
 */

static void
Rebalance(treePtr, nodePtr)
    BTree *treePtr;			/* Tree that is being rebalanced. */
    register Node *nodePtr;		/* Node that may be out of balance. */
{
    /*
     * Loop over the entire ancestral chain of the node, working up
     * through the tree one node at a time until the root node has
     * been processed.
     */

    for ( ; nodePtr != NULL; nodePtr = nodePtr->parentPtr) {
	register Node *newPtr, *childPtr;
	register TkTextLine *linePtr;
	int i;

	/*
	 * Check to see if the node has too many children.  If it does,
	 * then split off all but the first MIN_CHILDREN into a separate
	 * node following the original one.  Then repeat until the
	 * node has a decent size.
	 */

	if (nodePtr->numChildren > MAX_CHILDREN) {
	    while (1) {
		/*
		 * If the node being split is the root node, then make a
		 * new root node above it first.
		 */
    
		if (nodePtr->parentPtr == NULL) {
		    newPtr = (Node *) ckalloc(sizeof(Node));
		    newPtr->parentPtr = NULL;
		    newPtr->nextPtr = NULL;
		    newPtr->summaryPtr = NULL;
		    newPtr->level = nodePtr->level + 1;
		    newPtr->children.nodePtr = nodePtr;
		    newPtr->numChildren = 1;
		    newPtr->numLines = nodePtr->numLines;
		    RecomputeNodeCounts(newPtr);
		    treePtr->rootPtr = newPtr;
		}
		newPtr = (Node *) ckalloc(sizeof(Node));
		newPtr->parentPtr = nodePtr->parentPtr;
		newPtr->nextPtr = nodePtr->nextPtr;
		nodePtr->nextPtr = newPtr;
		newPtr->summaryPtr = NULL;
		newPtr->level = nodePtr->level;
		newPtr->numChildren = nodePtr->numChildren - MIN_CHILDREN;
		if (nodePtr->level == 0) {
		    for (i = MIN_CHILDREN-1,
			    linePtr = nodePtr->children.linePtr;
			    i > 0; i--, linePtr = linePtr->nextPtr) {
			/* Empty loop body. */
		    }
		    newPtr->children.linePtr = linePtr->nextPtr;
		    linePtr->nextPtr = NULL;
		} else {
		    for (i = MIN_CHILDREN-1,
			    childPtr = nodePtr->children.nodePtr;
			    i > 0; i--, childPtr = childPtr->nextPtr) {
			/* Empty loop body. */
		    }
		    newPtr->children.nodePtr = childPtr->nextPtr;
		    childPtr->nextPtr = NULL;
		}
		RecomputeNodeCounts(nodePtr);
		nodePtr->parentPtr->numChildren++;
		nodePtr = newPtr;
		if (nodePtr->numChildren <= MAX_CHILDREN) {
		    RecomputeNodeCounts(nodePtr);
		    break;
		}
	    }
	}

	while (nodePtr->numChildren < MIN_CHILDREN) {
	    register Node *otherPtr;
	    Node *halfwayNodePtr = NULL;	/* Initialization needed only */
	    TkTextLine *halfwayLinePtr = NULL;	/* to prevent cc warnings. */
	    int totalChildren, firstChildren, i;

	    /*
	     * Too few children for this node.  If this is the root then,
	     * it's OK for it to have less than MIN_CHILDREN children
	     * as long as it's got at least two.  If it has only one
	     * (and isn't at level 0), then chop the root node out of
	     * the tree and use its child as the new root.
	     */

	    if (nodePtr->parentPtr == NULL) {
		if ((nodePtr->numChildren == 1) && (nodePtr->level > 0)) {
		    treePtr->rootPtr = nodePtr->children.nodePtr;
		    treePtr->rootPtr->parentPtr = NULL;
		    DeleteSummaries(nodePtr->summaryPtr);
		    ckfree((char *) nodePtr);
		}
		return;
	    }

	    /*
	     * Not the root.  Make sure that there are siblings to
	     * balance with.
	     */

	    if (nodePtr->parentPtr->numChildren < 2) {
		Rebalance(treePtr, nodePtr->parentPtr);
		continue;
	    }

	    /*
	     * Find a sibling neighbor to borrow from, and arrange for
	     * nodePtr to be the earlier of the pair.
	     */

	    if (nodePtr->nextPtr == NULL) {
		for (otherPtr = nodePtr->parentPtr->children.nodePtr;
			otherPtr->nextPtr != nodePtr;
			otherPtr = otherPtr->nextPtr) {
		    /* Empty loop body. */
		}
		nodePtr = otherPtr;
	    }
	    otherPtr = nodePtr->nextPtr;

	    /*
	     * We're going to either merge the two siblings together
	     * into one node or redivide the children among them to
	     * balance their loads.  As preparation, join their two
	     * child lists into a single list and remember the half-way
	     * point in the list.
	     */

	    totalChildren = nodePtr->numChildren + otherPtr->numChildren;
	    firstChildren = totalChildren/2;
	    if (nodePtr->children.nodePtr == NULL) {
		nodePtr->children = otherPtr->children;
		otherPtr->children.nodePtr = NULL;
		otherPtr->children.linePtr = NULL;
	    }
	    if (nodePtr->level == 0) {
		register TkTextLine *linePtr;

		for (linePtr = nodePtr->children.linePtr, i = 1;
			linePtr->nextPtr != NULL;
			linePtr = linePtr->nextPtr, i++) {
		    if (i == firstChildren) {
			halfwayLinePtr = linePtr;
		    }
		}
		linePtr->nextPtr = otherPtr->children.linePtr;
		while (i <= firstChildren) {
		    halfwayLinePtr = linePtr;
		    linePtr = linePtr->nextPtr;
		    i++;
		}
	    } else {
		register Node *childPtr;

		for (childPtr = nodePtr->children.nodePtr, i = 1;
			childPtr->nextPtr != NULL;
			childPtr = childPtr->nextPtr, i++) {
		    if (i <= firstChildren) {
			if (i == firstChildren) {
			    halfwayNodePtr = childPtr;
			}
		    }
		}
		childPtr->nextPtr = otherPtr->children.nodePtr;
		while (i <= firstChildren) {
		    halfwayNodePtr = childPtr;
		    childPtr = childPtr->nextPtr;
		    i++;
		}
	    }

	    /*
	     * If the two siblings can simply be merged together, do it.
	     */

	    if (totalChildren <= MAX_CHILDREN) {
		RecomputeNodeCounts(nodePtr);
		nodePtr->nextPtr = otherPtr->nextPtr;
		nodePtr->parentPtr->numChildren--;
		DeleteSummaries(otherPtr->summaryPtr);
		ckfree((char *) otherPtr);
		continue;
	    }

	    /*
	     * The siblings can't be merged, so just divide their
	     * children evenly between them.
	     */

	    if (nodePtr->level == 0) {
		otherPtr->children.linePtr = halfwayLinePtr->nextPtr;
		halfwayLinePtr->nextPtr = NULL;
	    } else {
		otherPtr->children.nodePtr = halfwayNodePtr->nextPtr;
		halfwayNodePtr->nextPtr = NULL;
	    }
	    RecomputeNodeCounts(nodePtr);
	    RecomputeNodeCounts(otherPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RecomputeNodeCounts --
 *
 *	This procedure is called to recompute all the counts in a node
 *	(tags, child information, etc.) by scanning the information in
 *	its descendants.  This procedure is called during rebalancing
 *	when a node's child structure has changed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The tag counts for nodePtr are modified to reflect its current
 *	child structure, as are its numChildren and numLines fields.
 *	Also, all of the childrens' parentPtr fields are made to point
 *	to nodePtr.
 *
 *----------------------------------------------------------------------
 */

static void
RecomputeNodeCounts(nodePtr)
    register Node *nodePtr;		/* Node whose tag summary information
					 * must be recomputed. */
{
    register Summary *summaryPtr, *summaryPtr2;
    register Node *childPtr;
    register TkTextLine *linePtr;
    register TkTextSegment *segPtr;
    TkTextTag *tagPtr;

    /*
     * Zero out all the existing counts for the node, but don't delete
     * the existing Summary records (most of them will probably be reused).
     */

    for (summaryPtr = nodePtr->summaryPtr; summaryPtr != NULL;
	    summaryPtr = summaryPtr->nextPtr) {
	summaryPtr->toggleCount = 0;
    }
    nodePtr->numChildren = 0;
    nodePtr->numLines = 0;

    /*
     * Scan through the children, adding the childrens' tag counts into
     * the node's tag counts and adding new Summary structures if
     * necessary.
     */

    if (nodePtr->level == 0) {
	for (linePtr = nodePtr->children.linePtr; linePtr != NULL;
		linePtr = linePtr->nextPtr) {
	    nodePtr->numChildren++;
	    nodePtr->numLines++;
	    linePtr->parentPtr = nodePtr;
	    for (segPtr = linePtr->segPtr; segPtr != NULL;
		    segPtr = segPtr->nextPtr) {
		if (((segPtr->typePtr != &tkTextToggleOnType)
			&& (segPtr->typePtr != &tkTextToggleOffType))
			|| !(segPtr->body.toggle.inNodeCounts)) {
		    continue;
		}
		tagPtr = segPtr->body.toggle.tagPtr;
		for (summaryPtr = nodePtr->summaryPtr; ;
			summaryPtr = summaryPtr->nextPtr) {
		    if (summaryPtr == NULL) {
			summaryPtr = (Summary *) ckalloc(sizeof(Summary));
			summaryPtr->tagPtr = tagPtr;
			summaryPtr->toggleCount = 1;
			summaryPtr->nextPtr = nodePtr->summaryPtr;
			nodePtr->summaryPtr = summaryPtr;
			break;
		    }
		    if (summaryPtr->tagPtr == tagPtr) {
			summaryPtr->toggleCount++;
			break;
		    }
		}
	    }
	}
    } else {
	for (childPtr = nodePtr->children.nodePtr; childPtr != NULL;
		childPtr = childPtr->nextPtr) {
	    nodePtr->numChildren++;
	    nodePtr->numLines += childPtr->numLines;
	    childPtr->parentPtr = nodePtr;
	    for (summaryPtr2 = childPtr->summaryPtr; summaryPtr2 != NULL;
		    summaryPtr2 = summaryPtr2->nextPtr) {
		for (summaryPtr = nodePtr->summaryPtr; ;
			summaryPtr = summaryPtr->nextPtr) {
		    if (summaryPtr == NULL) {
			summaryPtr = (Summary *) ckalloc(sizeof(Summary));
			summaryPtr->tagPtr = summaryPtr2->tagPtr;
			summaryPtr->toggleCount = summaryPtr2->toggleCount;
			summaryPtr->nextPtr = nodePtr->summaryPtr;
			nodePtr->summaryPtr = summaryPtr;
			break;
		    }
		    if (summaryPtr->tagPtr == summaryPtr2->tagPtr) {
			summaryPtr->toggleCount += summaryPtr2->toggleCount;
			break;
		    }
		}
	    }
	}
    }

    /*
     * Scan through the node's tag records again and delete any Summary
     * records that still have a zero count, or that have all the toggles.
     * The node with the children that account for all the tags toggles
     * have no summary information, and they become the tagRootPtr for the tag.
     */

    summaryPtr2 = NULL;
    for (summaryPtr = nodePtr->summaryPtr; summaryPtr != NULL; ) {
	if (summaryPtr->toggleCount > 0 && 
		summaryPtr->toggleCount < summaryPtr->tagPtr->toggleCount) {
	    if (nodePtr->level == summaryPtr->tagPtr->tagRootPtr->level) {
		/*
		 * The tag's root node split and some toggles left.
		 * The tag root must move up a level.
		 */
		summaryPtr->tagPtr->tagRootPtr = nodePtr->parentPtr;
	    }
	    summaryPtr2 = summaryPtr;
	    summaryPtr = summaryPtr->nextPtr;
	    continue;
	}
	if (summaryPtr->toggleCount == summaryPtr->tagPtr->toggleCount) {
	    /*
	     * A node merge has collected all the toggles under one node.
	     * Push the root down to this level.
	     */
	    summaryPtr->tagPtr->tagRootPtr = nodePtr;
	}
	if (summaryPtr2 != NULL) {
	    summaryPtr2->nextPtr = summaryPtr->nextPtr;
	    ckfree((char *) summaryPtr);
	    summaryPtr = summaryPtr2->nextPtr;
	} else {
	    nodePtr->summaryPtr = summaryPtr->nextPtr;
	    ckfree((char *) summaryPtr);
	    summaryPtr = nodePtr->summaryPtr;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeNumLines --
 *
 *	This procedure returns a count of the number of lines of
 *	text present in a given B-tree.
 *
 * Results:
 *	The return value is a count of the number of usable lines
 *	in tree (i.e. it doesn't include the dummy line that is just
 * 	used to mark the end of the tree).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkBTreeNumLines(tree)
    TkTextBTree tree;			/* Information about tree. */
{
    BTree *treePtr = (BTree *) tree;
    return treePtr->rootPtr->numLines - 1;
}

/*
 *--------------------------------------------------------------
 *
 * CharSplitProc --
 *
 *	This procedure implements splitting for character segments.
 *
 * Results:
 *	The return value is a pointer to a chain of two segments
 *	that have the same characters as segPtr except split
 *	among the two segments.
 *
 * Side effects:
 *	Storage for segPtr is freed.
 *
 *--------------------------------------------------------------
 */

static TkTextSegment *
CharSplitProc(segPtr, index)
    TkTextSegment *segPtr;		/* Pointer to segment to split. */
    int index;				/* Position within segment at which
					 * to split. */
{
    TkTextSegment *newPtr1, *newPtr2;

    newPtr1 = (TkTextSegment *) ckalloc(CSEG_SIZE(index));
    newPtr2 = (TkTextSegment *) ckalloc(
	    CSEG_SIZE(segPtr->size - index));
    newPtr1->typePtr = &tkTextCharType;
    newPtr1->nextPtr = newPtr2;
    newPtr1->size = index;
    strncpy(newPtr1->body.chars, segPtr->body.chars, (size_t) index);
    newPtr1->body.chars[index] = 0;
    newPtr2->typePtr = &tkTextCharType;
    newPtr2->nextPtr = segPtr->nextPtr;
    newPtr2->size = segPtr->size - index;
    strcpy(newPtr2->body.chars, segPtr->body.chars + index);
    ckfree((char*) segPtr);
    return newPtr1;
}

/*
 *--------------------------------------------------------------
 *
 * CharCleanupProc --
 *
 *	This procedure merges adjacent character segments into
 *	a single character segment, if possible.
 *
 * Results:
 *	The return value is a pointer to the first segment in
 *	the (new) list of segments that used to start with segPtr.
 *
 * Side effects:
 *	Storage for the segments may be allocated and freed.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static TkTextSegment *
CharCleanupProc(segPtr, linePtr)
    TkTextSegment *segPtr;		/* Pointer to first of two adjacent
					 * segments to join. */
    TkTextLine *linePtr;		/* Line containing segments (not
					 * used). */
{
    TkTextSegment *segPtr2, *newPtr;

    segPtr2 = segPtr->nextPtr;
    if ((segPtr2 == NULL) || (segPtr2->typePtr != &tkTextCharType)) {
	return segPtr;
    }
    newPtr = (TkTextSegment *) ckalloc(CSEG_SIZE(
	    segPtr->size + segPtr2->size));
    newPtr->typePtr = &tkTextCharType;
    newPtr->nextPtr = segPtr2->nextPtr;
    newPtr->size = segPtr->size + segPtr2->size;
    strcpy(newPtr->body.chars, segPtr->body.chars);
    strcpy(newPtr->body.chars + segPtr->size, segPtr2->body.chars);
    ckfree((char*) segPtr);
    ckfree((char*) segPtr2);
    return newPtr;
}

/*
 *--------------------------------------------------------------
 *
 * CharDeleteProc --
 *
 *	This procedure is invoked to delete a character segment.
 *
 * Results:
 *	Always returns 0 to indicate that the segment was deleted.
 *
 * Side effects:
 *	Storage for the segment is freed.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
CharDeleteProc(segPtr, linePtr, treeGone)
    TkTextSegment *segPtr;		/* Segment to delete. */
    TkTextLine *linePtr;		/* Line containing segment. */
    int treeGone;			/* Non-zero means the entire tree is
					 * being deleted, so everything must
					 * get cleaned up. */
{
    ckfree((char*) segPtr);
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * CharCheckProc --
 *
 *	This procedure is invoked to perform consistency checks
 *	on character segments.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the segment isn't inconsistent then the procedure
 *	panics.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
CharCheckProc(segPtr, linePtr)
    TkTextSegment *segPtr;		/* Segment to check. */
    TkTextLine *linePtr;		/* Line containing segment. */
{
    /*
     * Make sure that the segment contains the number of
     * characters indicated by its header, and that the last
     * segment in a line ends in a newline.  Also make sure
     * that there aren't ever two character segments adjacent
     * to each other:  they should be merged together.
     */

    if (segPtr->size <= 0) {
	panic("CharCheckProc: segment has size <= 0");
    }
    if (strlen(segPtr->body.chars) != segPtr->size) {
	panic("CharCheckProc: segment has wrong size");
    }
    if (segPtr->nextPtr == NULL) {
	if (segPtr->body.chars[segPtr->size-1] != '\n') {
	    panic("CharCheckProc: line doesn't end with newline");
	}
    } else {
	if (segPtr->nextPtr->typePtr == &tkTextCharType) {
	    panic("CharCheckProc: adjacent character segments weren't merged");
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * ToggleDeleteProc --
 *
 *	This procedure is invoked to delete toggle segments.
 *
 * Results:
 *	Returns 1 to indicate that the segment may not be deleted,
 *	unless the entire B-tree is going away.
 *
 * Side effects:
 *	If the tree is going away then the toggle's memory is
 *	freed;  otherwise the toggle counts in nodes above the
 *	segment get updated.
 *
 *--------------------------------------------------------------
 */

static int
ToggleDeleteProc(segPtr, linePtr, treeGone)
    TkTextSegment *segPtr;		/* Segment to check. */
    TkTextLine *linePtr;		/* Line containing segment. */
    int treeGone;			/* Non-zero means the entire tree is
					 * being deleted, so everything must
					 * get cleaned up. */
{
    if (treeGone) {
	ckfree((char *) segPtr);
	return 0;
    }

    /*
     * This toggle is in the middle of a range of characters that's
     * being deleted.  Refuse to die.  We'll be moved to the end of
     * the deleted range and our cleanup procedure will be called
     * later.  Decrement node toggle counts here, and set a flag
     * so we'll re-increment them in the cleanup procedure.
     */

    if (segPtr->body.toggle.inNodeCounts) {
	ChangeNodeToggleCount(linePtr->parentPtr,
		segPtr->body.toggle.tagPtr, -1);
	segPtr->body.toggle.inNodeCounts = 0;
    }
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * ToggleCleanupProc --
 *
 *	This procedure is called when a toggle is part of a line that's
 *	been modified in some way.  It's invoked after the
 *	modifications are complete.
 *
 * Results:
 *	The return value is the head segment in a new list
 *	that is to replace the tail of the line that used to
 *	start at segPtr.  This allows the procedure to delete
 *	or modify segPtr.
 *
 * Side effects:
 *	Toggle counts in the nodes above the new line will be
 *	updated if they're not already.  Toggles may be collapsed
 *	if there are duplicate toggles at the same position.
 *
 *--------------------------------------------------------------
 */

static TkTextSegment *
ToggleCleanupProc(segPtr, linePtr)
    TkTextSegment *segPtr;	/* Segment to check. */
    TkTextLine *linePtr;	/* Line that now contains segment. */
{
    TkTextSegment *segPtr2, *prevPtr;
    int counts;

    /*
     * If this is a toggle-off segment, look ahead through the next
     * segments to see if there's a toggle-on segment for the same tag
     * before any segments with non-zero size.  If so then the two
     * toggles cancel each other;  remove them both.
     */

    if (segPtr->typePtr == &tkTextToggleOffType) {
	for (prevPtr = segPtr, segPtr2 = prevPtr->nextPtr;
		(segPtr2 != NULL) && (segPtr2->size == 0);
		prevPtr = segPtr2, segPtr2 = prevPtr->nextPtr) {
	    if (segPtr2->typePtr != &tkTextToggleOnType) {
		continue;
	    }
	    if (segPtr2->body.toggle.tagPtr != segPtr->body.toggle.tagPtr) {
		continue;
	    }
	    counts = segPtr->body.toggle.inNodeCounts
		    + segPtr2->body.toggle.inNodeCounts;
	    if (counts != 0) {
		ChangeNodeToggleCount(linePtr->parentPtr,
			segPtr->body.toggle.tagPtr, -counts);
	    }
	    prevPtr->nextPtr = segPtr2->nextPtr;
	    ckfree((char *) segPtr2);
	    segPtr2 = segPtr->nextPtr;
	    ckfree((char *) segPtr);
	    return segPtr2;
	}
    }

    if (!segPtr->body.toggle.inNodeCounts) {
	ChangeNodeToggleCount(linePtr->parentPtr,
		segPtr->body.toggle.tagPtr, 1);
	segPtr->body.toggle.inNodeCounts = 1;
    }
    return segPtr;
}

/*
 *--------------------------------------------------------------
 *
 * ToggleLineChangeProc --
 *
 *	This procedure is invoked when a toggle segment is about
 *	to move from one line to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Toggle counts are decremented in the nodes above the line.
 *
 *--------------------------------------------------------------
 */

static void
ToggleLineChangeProc(segPtr, linePtr)
    TkTextSegment *segPtr;	/* Segment to check. */
    TkTextLine *linePtr;	/* Line that used to contain segment. */
{
    if (segPtr->body.toggle.inNodeCounts) {
	ChangeNodeToggleCount(linePtr->parentPtr,
		segPtr->body.toggle.tagPtr, -1);
	segPtr->body.toggle.inNodeCounts = 0;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ToggleCheckProc --
 *
 *	This procedure is invoked to perform consistency checks
 *	on toggle segments.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a consistency problem is found the procedure panics.
 *
 *--------------------------------------------------------------
 */

static void
ToggleCheckProc(segPtr, linePtr)
    TkTextSegment *segPtr;		/* Segment to check. */
    TkTextLine *linePtr;		/* Line containing segment. */
{
    register Summary *summaryPtr;
    int needSummary;

    if (segPtr->size != 0) {
	panic("ToggleCheckProc: segment had non-zero size");
    }
    if (!segPtr->body.toggle.inNodeCounts) {
	panic("ToggleCheckProc: toggle counts not updated in nodes");
    }
    needSummary = (segPtr->body.toggle.tagPtr->tagRootPtr != linePtr->parentPtr);
    for (summaryPtr = linePtr->parentPtr->summaryPtr; ;
	    summaryPtr = summaryPtr->nextPtr) {
	if (summaryPtr == NULL) {
	    if (needSummary) {
		panic("ToggleCheckProc: tag not present in node");
	    } else {
		break;
	    }
	}
	if (summaryPtr->tagPtr == segPtr->body.toggle.tagPtr) {
	    if (!needSummary) {
		panic("ToggleCheckProc: tag present in root node summary");
	    }
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkBTreeCharsInLine --
 *
 *	This procedure returns a count of the number of characters
 *	in a given line.
 *
 * Results:
 *	The return value is the character count for linePtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkBTreeCharsInLine(linePtr)
    TkTextLine *linePtr;		/* Line whose characters should be
					 * counted. */
{
    TkTextSegment *segPtr;
    int count;

    count = 0;
    for (segPtr = linePtr->segPtr; segPtr != NULL; segPtr = segPtr->nextPtr) {
	count += segPtr->size;
    }
    return count;
}
