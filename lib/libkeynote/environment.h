/* $OpenBSD: environment.h,v 1.1.1.1 1999/05/23 22:11:04 angelos Exp $ */

/*
 * The author of this code is Angelos D. Keromytis (angelos@dsl.cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Philadelphia, PA, USA,
 * in April-May 1998
 *
 * Copyright (C) 1998, 1999 by Angelos D. Keromytis.
 *	
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, THE AUTHORS MAKES NO
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#ifndef __ENVIRONMENT_H__
#define __ENVIRONMENT_H__

#include "keynote.h"

#define KEYNOTE_RAND_INIT_LEN 		1024

/*
 * These can be changed to reflect more assertions/session or more
 * sessions respectively
 */
#define HASHTABLESIZE   		37
#define SESSIONTABLESIZE                37

struct keynote_session
{
    int                     ks_id;
    int                     ks_assertioncounter;
    int                     ks_values_num;
    struct environment     *ks_env_table[HASHTABLESIZE];
    struct environment     *ks_env_regex;
    struct keylist         *ks_action_authorizers;
    struct assertion       *ks_assertion_table[HASHTABLESIZE];
    char                  **ks_values;
    char                   *ks_authorizers_cache;
    char                   *ks_values_cache;
    struct keynote_session *ks_prev;
    struct keynote_session *ks_next;
};

int   keynote_env_add(char *, char *, struct environment **, u_int, int);
char *keynote_env_lookup(char *, struct environment **, u_int);
int   keynote_env_delete(char *, struct environment **, u_int);
struct environment *keynote_get_envlist(char *, char *, int);
void  keynote_env_cleanup(struct environment **, u_int); 
struct keynote_session *keynote_find_session(int);
void  keynote_free_env(struct environment *);
int   keynote_sremove_assertion(int, int);
u_int keynote_stringhash(char *, u_int);
void  keynote_cleanup_kth(void);
int   keynote_retindex(char *);

extern struct keynote_session *keynote_current_session;
extern int   keynote_returnvalue;
extern int   keynote_justrecord;
#endif /* __ENVIRONMENT_H__ */
