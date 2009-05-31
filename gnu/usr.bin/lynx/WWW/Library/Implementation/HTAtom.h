/*  */

/*                      Atoms: Names to numbers                 HTAtom.h
 *                      =======================
 *
 *      Atoms are names which are given representative pointer values
 *      so that they can be stored more efficiently, and compaisons
 *      for equality done more efficiently.
 *
 *      HTAtom_for(string) returns a representative value such that it
 *      will always (within one run of the program) return the same
 *      value for the same given string.
 *
 * Authors:
 *      TBL     Tim Berners-Lee, WorldWideWeb project, CERN
 *
 *      (c) Copyright CERN 1991 - See Copyright.html
 *
 */

#ifndef HTATOM_H
#define HTATOM_H

#include <HTList.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct _HTAtom HTAtom;

    struct _HTAtom {
	HTAtom *next;
	char *name;
    };				/* struct _HTAtom */

    extern HTAtom *HTAtom_for(const char *string);
    extern HTList *HTAtom_templateMatches(const char *templ);

#define HTAtom_name(a) ((a)->name)

/*

The HTFormat type

   We use the HTAtom object for holding representations.  This allows faster manipulation
   (comparison and copying) that if we stayed with strings.

 */
    typedef HTAtom *HTFormat;

#ifdef __cplusplus
}
#endif
#endif				/* HTATOM_H */
