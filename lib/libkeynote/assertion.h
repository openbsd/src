/* $OpenBSD: assertion.h,v 1.1.1.1 1999/05/23 22:11:03 angelos Exp $ */

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

#ifndef __ASSERTION_H__
#define __ASSERTION_H__

#include "keynote.h"

struct keylist
{
    int             key_alg;
    void           *key_key;
    char           *key_stringkey;
    struct keylist *key_next;
};

struct assertion 
{
    void               *as_authorizer;
    char               *as_buf;
    char               *as_signature;
    char	       *as_authorizer_string_s;
    char               *as_authorizer_string_e;
    char               *as_keypred_s; 
    char               *as_keypred_e;
    char               *as_conditions_s;
    char               *as_conditions_e;
    char               *as_signature_string_s;
    char               *as_signature_string_e;
    char	       *as_comment_s;
    char	       *as_comment_e;
    char	       *as_startofsignature;
    char	       *as_allbutsignature;
    int                 as_id;
    int			as_signeralgorithm;
    int                 as_result;
    int			as_error;
    u_char		as_flags;
    u_char 		as_internalflags;
    char		as_kresult;
    char                as_sigresult;
    struct keylist     *as_keylist;
    struct environment *as_env;
    struct assertion   *as_next;
};

/* Internal flags */
#define ASSERT_IFLAG_WEIRDLICS   0x0001  /* Needs Licensees re-processing */
#define ASSERT_IFLAG_WEIRDAUTH   0x0002  /* Needs Authorizer re-processing */
#define ASSERT_IFLAG_WEIRDSIG	 0x0004  /* Needs Signature re-processing */
#define ASSERT_IFLAG_NEEDPROC    0x0008  /* Needs "key field" processing */
#define ASSERT_IFLAG_PROCESSED   0x0010  /* Handled repositioning already */

extern struct assertion *keynote_current_assertion;

#define KRESULT_UNTOUCHED	0
#define KRESULT_IN_PROGRESS	1	/* For cycle detection */
#define KRESULT_DONE            2

#define KEYWORD_VERSION		1
#define KEYWORD_LOCALINIT      	2
#define KEYWORD_AUTHORIZER     	3
#define KEYWORD_LICENSEES	4
#define KEYWORD_CONDITIONS	5
#define KEYWORD_SIGNATURE	6
#define KEYWORD_COMMENT		7

#define KEYNOTE_FLAG_EXPORTALL	0x1

#define LEXTYPE_CHAR		0x1

struct keylist *keynote_keylist_find(struct keylist *, char *);
struct assertion *keynote_parse_assertion(char *, int, int);
int    keynote_evaluate_authorizer(struct assertion *, int);
struct assertion *keynote_find_assertion(void *, int, int);
int    keynote_evaluate_assertion(struct assertion *);
int    keynote_parse_keypred(struct assertion *, int);
int    keynote_keylist_add(struct keylist **, char *);
int    keynote_add_htable(struct assertion *, int);
void   keynote_free_assertion(struct assertion *);
void   keynote_keylist_free(struct keylist *);
int    keynote_in_authorizers(void *, int);
char  *keynote_get_private_key(char *);
int    keynote_evaluate_query(void);
int    keynote_lex_add(void *, int);
void   keynote_lex_remove(void *);
#endif /* __ASSERTION_H__ */
