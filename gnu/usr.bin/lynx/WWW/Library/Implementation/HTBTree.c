/*                  Binary Tree for sorting things
 *                  ==============================
 *                      Author: Arthur Secret
 *
 *       4 March 94: Bug fixed in the balancing procedure
 *
 */

#include <HTUtils.h>
#include <HTBTree.h>

#define MAXIMUM(a,b) ((a)>(b)?(a):(b))

#include <LYLeaks.h>

/*********************************************************
 * This function returns an HTBTree with memory allocated
 * for it when given a mean to compare things
 */
HTBTree *HTBTree_new(HTComparer comp)
{
    HTBTree *tree = typeMalloc(HTBTree);

    if (tree == NULL)
	outofmem(__FILE__, "HTBTree_new");

    assert(tree != NULL);

    tree->compare = comp;
    tree->top = NULL;

    return tree;
}

/*********************************************************
 * This void will free the memory allocated for one element
 */
static void HTBTElement_free(HTBTElement *element)
{
    if (element) {
	if (element->left != NULL)
	    HTBTElement_free(element->left);
	if (element->right != NULL)
	    HTBTElement_free(element->right);
	FREE(element);
    }
}

/*************************************************************
 * This void will free the memory allocated for the whole tree
 */
void HTBTree_free(HTBTree *tree)
{
    HTBTElement_free(tree->top);
    FREE(tree);
}

/*********************************************************
 * This void will free the memory allocated for one element
 */
static void HTBTElementAndObject_free(HTBTElement *element)
{
    if (element) {		/* Just in case nothing was in the tree anyway */
	if (element->left != NULL)
	    HTBTElementAndObject_free(element->left);
	if (element->right != NULL)
	    HTBTElementAndObject_free(element->right);
	FREE(element->object);
	FREE(element);
    }
}

/*************************************************************
 * This void will free the memory allocated for the whole tree
 */
void HTBTreeAndObject_free(HTBTree *tree)
{
    HTBTElementAndObject_free(tree->top);
    FREE(tree);
}

/*********************************************************************
 * Returns a pointer to equivalent object in a tree or NULL if none.
 */
void *HTBTree_search(HTBTree *tree,
		     void *object)
{
    HTBTElement *cur = tree->top;
    int res;

    while (cur != NULL) {
	res = tree->compare(object, cur->object);

	if (res == 0)
	    return cur->object;
	else if (res < 0)
	    cur = cur->left;
	else if (res > 0)
	    cur = cur->right;
    }
    return NULL;
}

/*********************************************************************
 * This void is the core of HTBTree.c . It will
 *       1/ add a new element to the tree at the right place
 *		so that the tree remains sorted
 *       2/ balance the tree to be as fast as possible when reading it
 */
void HTBTree_add(HTBTree *tree,
		 void *object)
{
    HTBTElement *father_of_element;
    HTBTElement *added_element;
    HTBTElement *forefather_of_element;
    HTBTElement *father_of_forefather;
    BOOL father_found, top_found;
    int depth, depth2, corrections;

    /* father_of_element is a pointer to the structure that is the father of
     * the new object "object".  added_element is a pointer to the structure
     * that contains or will contain the new object "object". 
     * father_of_forefather and forefather_of_element are pointers that are
     * used to modify the depths of upper elements, when needed.
     *
     * father_found indicates by a value NO when the future father of "object"
     * is found.  top_found indicates by a value NO when, in case of a
     * difference of depths < 2, the top of the tree is encountered and forbids
     * any further try to balance the tree.  corrections is an integer used to
     * avoid infinite loops in cases such as:
     *
     *             3                        3
     *          4                              4
     *           5                            5
     *
     * 3 is used here to show that it need not be the top of the tree.
     */

    /*
     * 1/ Adding of the element to the binary tree
     */

    if (tree->top == NULL) {
	tree->top = typeMalloc(HTBTElement);

	if (tree->top == NULL)
	    outofmem(__FILE__, "HTBTree_add");

	assert(tree->top != NULL);

	tree->top->up = NULL;
	tree->top->object = object;
	tree->top->left = NULL;
	tree->top->left_depth = 0;
	tree->top->right = NULL;
	tree->top->right_depth = 0;
    } else {
	father_found = YES;
	father_of_element = tree->top;
	added_element = NULL;
	father_of_forefather = NULL;
	forefather_of_element = NULL;
	while (father_found) {
	    int res = tree->compare(object, father_of_element->object);

	    if (res < 0) {
		if (father_of_element->left != NULL)
		    father_of_element = father_of_element->left;
		else {
		    father_found = NO;
		    father_of_element->left = typeMalloc(HTBTElement);

		    if (father_of_element->left == NULL)
			outofmem(__FILE__, "HTBTree_add");

		    assert(father_of_element->left != NULL);

		    added_element = father_of_element->left;
		    added_element->up = father_of_element;
		    added_element->object = object;
		    added_element->left = NULL;
		    added_element->left_depth = 0;
		    added_element->right = NULL;
		    added_element->right_depth = 0;
		}
	    } else {		/* res >= 0 */
		if (father_of_element->right != NULL) {
		    father_of_element = father_of_element->right;
		} else {
		    father_found = NO;
		    father_of_element->right = typeMalloc(HTBTElement);

		    if (father_of_element->right == NULL)
			outofmem(__FILE__, "HTBTree_add");
		    assert(father_of_element->right != NULL);

		    added_element = father_of_element->right;
		    added_element->up = father_of_element;
		    added_element->object = object;
		    added_element->left = NULL;
		    added_element->left_depth = 0;
		    added_element->right = NULL;
		    added_element->right_depth = 0;
		}
	    }
	}

	/*
	 * Changing of all depths that need to be changed
	 */
	father_of_forefather = father_of_element;
	forefather_of_element = added_element;
	do {
	    if (father_of_forefather->left == forefather_of_element) {
		depth = father_of_forefather->left_depth;
		father_of_forefather->left_depth = 1
		    + MAXIMUM(forefather_of_element->right_depth,
			      forefather_of_element->left_depth);
		depth2 = father_of_forefather->left_depth;
	    } else {
		depth = father_of_forefather->right_depth;
		father_of_forefather->right_depth = 1
		    + MAXIMUM(forefather_of_element->right_depth,
			      forefather_of_element->left_depth);
		depth2 = father_of_forefather->right_depth;
	    }
	    forefather_of_element = father_of_forefather;
	    father_of_forefather = father_of_forefather->up;
	} while ((depth != depth2) && (father_of_forefather != NULL));

	/*
	 * 2/ Balancing the binary tree, if necessary
	 */
	top_found = YES;
	corrections = 0;
	while ((top_found) && (corrections < 7)) {
	    if ((abs(father_of_element->left_depth
		     - father_of_element->right_depth)) < 2) {
		if (father_of_element->up != NULL)
		    father_of_element = father_of_element->up;
		else
		    top_found = NO;
	    } else {		/* We start the process of balancing */

		corrections = corrections + 1;
		/*
		 * corrections is an integer used to avoid infinite
		 * loops in cases such as:
		 *
		 *             3                        3
		 *          4                              4
		 *           5                            5
		 *
		 * 3 is used to show that it need not be the top of the tree
		 * But let's avoid these two exceptions anyhow
		 * with the two following conditions (4 March 94 - AS)
		 */

		if (father_of_element->left == NULL) {
		    if ((father_of_element->right != NULL)
			&& (father_of_element->right->right == NULL)
			&& (father_of_element->right->left != NULL)
			&& (father_of_element->right->left->left == NULL)
			&& (father_of_element->right->left->right == NULL)) {
			corrections = 7;
		    }
		} else {
		    if ((father_of_element->right == NULL)
			&& (father_of_element->left->left == NULL)
			&& (father_of_element->left->right != NULL)
			&& (father_of_element->left->right->right == NULL)
			&& (father_of_element->left->right->left == NULL)) {
			corrections = 7;
		    }
		}

		if ((father_of_element->left != NULL)
		    && (father_of_element->left_depth > father_of_element->right_depth)) {
		    added_element = father_of_element->left;
		    father_of_element->left_depth = added_element->right_depth;
		    added_element->right_depth = 1
			+ MAXIMUM(father_of_element->right_depth,
				  father_of_element->left_depth);
		    if (father_of_element->up != NULL) {
			/* Bug fixed in March 94  -  AS */
			BOOL first_time;

			father_of_forefather = father_of_element->up;
			forefather_of_element = added_element;
			first_time = YES;
			do {
			    if (father_of_forefather->left
				== forefather_of_element->up) {
				depth = father_of_forefather->left_depth;
				if (first_time) {
				    father_of_forefather->left_depth = 1
					+ MAXIMUM(forefather_of_element->left_depth,
						  forefather_of_element->right_depth);
				    first_time = NO;
				} else
				    father_of_forefather->left_depth = 1
					+ MAXIMUM(forefather_of_element->up->left_depth,
						  forefather_of_element->up->right_depth);

				depth2 = father_of_forefather->left_depth;
			    } else {
				depth = father_of_forefather->right_depth;
				if (first_time) {
				    father_of_forefather->right_depth = 1
					+ MAXIMUM(forefather_of_element->left_depth,
						  forefather_of_element->right_depth);
				    first_time = NO;
				} else
				    father_of_forefather->right_depth = 1
					+ MAXIMUM(forefather_of_element->up->left_depth,
						  forefather_of_element->up->right_depth);
				depth2 = father_of_forefather->right_depth;
			    }
			    forefather_of_element = forefather_of_element->up;
			    father_of_forefather = father_of_forefather->up;
			} while ((depth != depth2) &&
				 (father_of_forefather != NULL));
			father_of_forefather = father_of_element->up;
			if (father_of_forefather->left == father_of_element) {
			    /*
			     *                   3                       3
			     *               4                       5
			     * When tree   5   6        becomes    7    4
			     *            7 8                          8 6
			     *
			     * 3 is used to show that it may not be the top of the
			     * tree.
			     */
			    father_of_forefather->left = added_element;
			    father_of_element->left = added_element->right;
			    added_element->right = father_of_element;
			}
			if (father_of_forefather->right == father_of_element) {
			    /*
			     *          3                       3
			     *               4                       5
			     * When tree   5   6        becomes    7    4
			     *            7 8                          8 6
			     *
			     * 3 is used to show that it may not be the top of the
			     * tree
			     */
			    father_of_forefather->right = added_element;
			    father_of_element->left = added_element->right;
			    added_element->right = father_of_element;
			}
			added_element->up = father_of_forefather;
		    } else {
			/*

			 *               1                       2
			 * When tree   2   3        becomes    4    1
			 *            4 5                          5 3
			 *
			 * 1 is used to show that it is the top of the tree
			 */
			added_element->up = NULL;
			father_of_element->left = added_element->right;
			added_element->right = father_of_element;
		    }
		    father_of_element->up = added_element;
		    if (father_of_element->left != NULL)
			father_of_element->left->up = father_of_element;
		} else if (father_of_element->right != NULL) {
		    added_element = father_of_element->right;
		    father_of_element->right_depth = added_element->left_depth;
		    added_element->left_depth = 1 +
			MAXIMUM(father_of_element->right_depth,
				father_of_element->left_depth);
		    if (father_of_element->up != NULL)
			/* Bug fixed in March 94  -  AS */
		    {
			BOOL first_time;

			father_of_forefather = father_of_element->up;
			forefather_of_element = added_element;
			first_time = YES;
			do {
			    if (father_of_forefather->left
				== forefather_of_element->up) {
				depth = father_of_forefather->left_depth;
				if (first_time) {
				    father_of_forefather->left_depth = 1
					+ MAXIMUM(forefather_of_element->left_depth,
						  forefather_of_element->right_depth);
				    first_time = NO;
				} else
				    father_of_forefather->left_depth = 1
					+ MAXIMUM(forefather_of_element->up->left_depth,
						  forefather_of_element->up->right_depth);
				depth2 = father_of_forefather->left_depth;
			    } else {
				depth = father_of_forefather->right_depth;
				if (first_time) {
				    father_of_forefather->right_depth = 1
					+ MAXIMUM(forefather_of_element->left_depth,
						  forefather_of_element->right_depth);
				    first_time = NO;
				} else
				    father_of_forefather->right_depth = 1
					+ MAXIMUM(forefather_of_element->up->left_depth,
						  forefather_of_element->up->right_depth);
				depth2 = father_of_forefather->right_depth;
			    }
			    father_of_forefather = father_of_forefather->up;
			    forefather_of_element = forefather_of_element->up;
			} while ((depth != depth2) &&
				 (father_of_forefather != NULL));
			father_of_forefather = father_of_element->up;
			if (father_of_forefather->left == father_of_element) {
			    /*
			     *                    3                       3
			     *               4                       6
			     * When tree   5   6        becomes    4    8
			     *                7 8                 5 7
			     *
			     * 3 is used to show that it may not be the top of the
			     * tree.
			     */
			    father_of_forefather->left = added_element;
			    father_of_element->right = added_element->left;
			    added_element->left = father_of_element;
			}
			if (father_of_forefather->right == father_of_element) {
			    /*
			     *           3                      3
			     *               4                       6
			     * When tree   5   6        becomes    4    8
			     *                7 8                 5 7
			     *
			     * 3 is used to show that it may not be the top of the
			     * tree
			     */
			    father_of_forefather->right = added_element;
			    father_of_element->right = added_element->left;
			    added_element->left = father_of_element;
			}
			added_element->up = father_of_forefather;
		    } else {
			/*

			 *               1                       3
			 * When tree   2   3        becomes    1    5
			 *                4 5                 2 4
			 *
			 * 1 is used to show that it is the top of the tree.
			 */
			added_element->up = NULL;
			father_of_element->right = added_element->left;
			added_element->left = father_of_element;
		    }
		    father_of_element->up = added_element;
		    if (father_of_element->right != NULL)
			father_of_element->right->up = father_of_element;
		}
	    }
	}
	while (father_of_element->up != NULL) {
	    father_of_element = father_of_element->up;
	}
	tree->top = father_of_element;
    }
}

/*************************************************************************
 * this function returns a pointer to the leftmost element if ele is NULL,
 * and to the next object to the right otherwise.
 * If no elements left, returns a pointer to NULL.
 */
HTBTElement *HTBTree_next(HTBTree *tree,
			  HTBTElement *ele)
{
    HTBTElement *father_of_element;
    HTBTElement *father_of_forefather;

    if (ele == NULL) {
	father_of_element = tree->top;
	if (father_of_element != NULL)
	    while (father_of_element->left != NULL)
		father_of_element = father_of_element->left;
    } else {
	father_of_element = ele;
	if (father_of_element->right != NULL) {
	    father_of_element = father_of_element->right;
	    while (father_of_element->left != NULL)
		father_of_element = father_of_element->left;
	} else {
	    father_of_forefather = father_of_element->up;
	    while (father_of_forefather &&
		   (father_of_forefather->right == father_of_element)) {
		father_of_element = father_of_forefather;
		father_of_forefather = father_of_element->up;
	    }
	    father_of_element = father_of_forefather;
	}
    }
#ifdef BTREE_TRACE
    /* The option -DBTREE_TRACE will give much more information
     * about the way the process is running, for debugging matters
     */
    if (father_of_element != NULL) {
	printf("\nObject = %s\t", (char *) father_of_element->object);
	if (father_of_element->up != NULL)
	    printf("Objet du pere = %s\n",
		   (char *) father_of_element->up->object);
	else
	    printf("Pas de Pere\n");
	if (father_of_element->left != NULL)
	    printf("Objet du fils gauche = %s\t",
		   (char *) father_of_element->left->object);
	else
	    printf("Pas de fils gauche\t");
	if (father_of_element->right != NULL)
	    printf("Objet du fils droit = %s\n",
		   (char *) father_of_element->right->object);
	else
	    printf("Pas de fils droit\n");
	printf("Profondeur gauche = %d\t", father_of_element->left_depth);
	printf("Profondeur droite = %d\n", father_of_element->right_depth);
	printf("      **************\n");
    }
#endif
    return father_of_element;
}

#ifdef TEST
/*****************************************************
 * This is just a test to show how to handle HTBTree.c
 */
main()
{
    HTBTree *tree;
    HTBTElement *next_element;

    tree = HTBTree_new((HTComparer) strcasecomp);
    HTBTree_add(tree, "hypertext");
    HTBTree_add(tree, "Addressing");
    HTBTree_add(tree, "X11");
    HTBTree_add(tree, "Tools");
    HTBTree_add(tree, "Proposal.wn");
    HTBTree_add(tree, "Protocols");
    HTBTree_add(tree, "NeXT");
    HTBTree_add(tree, "Daemon");
    HTBTree_add(tree, "Test");
    HTBTree_add(tree, "Administration");
    HTBTree_add(tree, "LineMode");
    HTBTree_add(tree, "DesignIssues");
    HTBTree_add(tree, "MarkUp");
    HTBTree_add(tree, "Macintosh");
    HTBTree_add(tree, "Proposal.rtf.wn");
    HTBTree_add(tree, "FIND");
    HTBTree_add(tree, "Paper");
    HTBTree_add(tree, "Tcl");
    HTBTree_add(tree, "Talks");
    HTBTree_add(tree, "Architecture");
    HTBTree_add(tree, "VMSHelp");
    HTBTree_add(tree, "Provider");
    HTBTree_add(tree, "Archive");
    HTBTree_add(tree, "SLAC");
    HTBTree_add(tree, "Project");
    HTBTree_add(tree, "News");
    HTBTree_add(tree, "Viola");
    HTBTree_add(tree, "Users");
    HTBTree_add(tree, "FAQ");
    HTBTree_add(tree, "WorkingNotes");
    HTBTree_add(tree, "Windows");
    HTBTree_add(tree, "FineWWW");
    HTBTree_add(tree, "Frame");
    HTBTree_add(tree, "XMosaic");
    HTBTree_add(tree, "People");
    HTBTree_add(tree, "All");
    HTBTree_add(tree, "Curses");
    HTBTree_add(tree, "Erwise");
    HTBTree_add(tree, "Carl");
    HTBTree_add(tree, "MidasWWW");
    HTBTree_add(tree, "XPM");
    HTBTree_add(tree, "MailRobot");
    HTBTree_add(tree, "Illustrations");
    HTBTree_add(tree, "VMClient");
    HTBTree_add(tree, "XPA");
    HTBTree_add(tree, "Clients.html");
    HTBTree_add(tree, "Library");
    HTBTree_add(tree, "CERNLIB_Distribution");
    HTBTree_add(tree, "libHTML");
    HTBTree_add(tree, "WindowsPC");
    HTBTree_add(tree, "tkWWW");
    HTBTree_add(tree, "tk2.3");
    HTBTree_add(tree, "CVS-RCS");
    HTBTree_add(tree, "DecnetSockets");
    HTBTree_add(tree, "SGMLStream");
    HTBTree_add(tree, "NextStep");
    HTBTree_add(tree, "CVSRepository_old");
    HTBTree_add(tree, "ArthurSecret");
    HTBTree_add(tree, "CVSROOT");
    HTBTree_add(tree, "HytelnetGate");
    HTBTree_add(tree, "cern.www.new.src");
    HTBTree_add(tree, "Conditions");
    HTBTree_add(tree, "HTMLGate");
    HTBTree_add(tree, "Makefile");
    HTBTree_add(tree, "Newsgroups.html");
    HTBTree_add(tree, "People.html");
    HTBTree_add(tree, "Bugs.html");
    HTBTree_add(tree, "Summary.html");
    HTBTree_add(tree, "zDesignIssues.wn");
    HTBTree_add(tree, "HT.draw");
    HTBTree_add(tree, "HTandCERN.wn");
    HTBTree_add(tree, "Ideas.wn");
    HTBTree_add(tree, "MarkUp.wn");
    HTBTree_add(tree, "Proposal.html");
    HTBTree_add(tree, "SearchPanel.draw");
    HTBTree_add(tree, "Comments.wn");
    HTBTree_add(tree, "Xanadu.html");
    HTBTree_add(tree, "Storinglinks.html");
    HTBTree_add(tree, "TheW3Book.html");
    HTBTree_add(tree, "Talk_Feb-91.html");
    HTBTree_add(tree, "JFosterEntry.txt");
    HTBTree_add(tree, "Summary.txt");
    HTBTree_add(tree, "Bibliography.html");
    HTBTree_add(tree, "HTandCern.txt");
    HTBTree_add(tree, "Talk.draw");
    HTBTree_add(tree, "zDesignNotes.html");
    HTBTree_add(tree, "Link.html");
    HTBTree_add(tree, "Status.html");
    HTBTree_add(tree, "http.txt");
    HTBTree_add(tree, "People.html~");
    HTBTree_add(tree, "TAGS");
    HTBTree_add(tree, "summary.txt");
    HTBTree_add(tree, "Technical.html");
    HTBTree_add(tree, "Terms.html");
    HTBTree_add(tree, "JANETAccess.html");
    HTBTree_add(tree, "People.txt");
    HTBTree_add(tree, "README.txt");
    HTBTree_add(tree, "CodingStandards.html");
    HTBTree_add(tree, "Copyright.txt");
    HTBTree_add(tree, "Status_old.html");
    HTBTree_add(tree, "patches~");
    HTBTree_add(tree, "RelatedProducts.html");
    HTBTree_add(tree, "Implementation");
    HTBTree_add(tree, "History.html");
    HTBTree_add(tree, "Makefile.bak");
    HTBTree_add(tree, "Makefile.old");
    HTBTree_add(tree, "Policy.html");
    HTBTree_add(tree, "WhatIs.html");
    HTBTree_add(tree, "TheProject.html");
    HTBTree_add(tree, "Notation.html");
    HTBTree_add(tree, "Helping.html");
    HTBTree_add(tree, "Cyber-WWW.sit.Hqx");
    HTBTree_add(tree, "Glossary.html");
    HTBTree_add(tree, "maketags.html");
    HTBTree_add(tree, "IntroCS.html");
    HTBTree_add(tree, "Contrib");
    HTBTree_add(tree, "Help.html");
    HTBTree_add(tree, "CodeManagExec");
    HTBTree_add(tree, "HT-0.1draz");
    HTBTree_add(tree, "Cello");
    HTBTree_add(tree, "TOPUB");
    HTBTree_add(tree, "BUILD");
    HTBTree_add(tree, "BUILDALL");
    HTBTree_add(tree, "Lynx");
    HTBTree_add(tree, "ArthurLibrary");
    HTBTree_add(tree, "RashtyClient");
    HTBTree_add(tree, "#History.html#");
    HTBTree_add(tree, "PerlServers");
    HTBTree_add(tree, "modules");
    HTBTree_add(tree, "NCSA_httpd");
    HTBTree_add(tree, "MAIL2HTML");
    HTBTree_add(tree, "core");
    HTBTree_add(tree, "EmacsWWW");
#ifdef BTREE_TRACE
    printf("\nTreeTopObject=%s\n\n", tree->top->object);
#endif
    next_element = HTBTree_next(tree, NULL);
    while (next_element != NULL) {
#ifndef BTREE_TRACE
	printf("The next element is %s\n", next_element->object);
#endif
	next_element = HTBTree_next(tree, next_element);
    }
    HTBTree_free(tree);
}

#endif
