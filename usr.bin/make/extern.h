/*	$OpenBSD: extern.h,v 1.36 2000/11/24 14:36:34 espie Exp $	*/
/*	$NetBSD: nonints.h,v 1.12 1996/11/06 17:59:19 christos Exp $	*/

/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)nonints.h	8.3 (Berkeley) 3/19/94
 */

/* arch.c */
extern ReturnStatus Arch_ParseArchive __P((char **, Lst, SymTable *));
extern void Arch_Touch __P((GNode *));
extern void Arch_TouchLib __P((GNode *));
extern TIMESTAMP Arch_MTime __P((GNode *));
extern TIMESTAMP Arch_MemMTime __P((GNode *));
extern void Arch_FindLib __P((GNode *, Lst));
extern Boolean Arch_LibOODate __P((GNode *));
extern void Arch_Init __P((void));
extern void Arch_End __P((void));
extern int Arch_IsLib __P((GNode *));

/* compat.c */
extern void Compat_Run __P((Lst));

/* cond.c */
extern int Cond_Eval __P((char *));
extern void Cond_End __P((void));

#include "error.h"

/* for.c */
typedef struct For_ For;
extern For *For_Eval __P((char *));
extern Boolean For_Accumulate __P((For *, const char *));
extern void For_Run  __P((For *));

/* main.c */
extern void Main_ParseArgLine __P((char *));
extern char *Cmd_Exec __P((char *, char **));
extern void Error __P((char *, ...));
extern void Fatal __P((char *, ...));
extern void Punt __P((char *, ...));
extern void DieHorribly __P((void));
extern void PrintAddr __P((void *));
extern void Finish __P((int));

/* make.c */
extern void Make_TimeStamp __P((GNode *, GNode *));
extern Boolean Make_OODate __P((GNode *));
extern void Make_HandleUse __P((GNode *, GNode *));
extern void Make_Update __P((GNode *));
extern void Make_DoAllVar __P((GNode *));
extern Boolean Make_Run __P((Lst));

/* parse.c */
extern void Parse_Error __P((int, char *, ...));
extern Boolean Parse_AnyExport __P((void));
extern Boolean Parse_IsVar __P((char *));
extern void Parse_DoVar __P((char *, GSymT *));
extern void Parse_AddIncludeDir __P((char *));
extern void Parse_File __P((char *, FILE *));
extern void Parse_Init __P((void));
extern void Parse_End __P((void));
extern void Parse_FromString __P((char *, unsigned long));
extern void Parse_MainName __P((Lst));
extern unsigned long Parse_Getlineno __P((void));
extern const char *Parse_Getfilename __P((void));

/* str.c */
extern void str_init __P((void));
extern void str_end __P((void));
extern char *str_concat __P((const char *, const char *, char));
extern char **brk_string __P((const char *, int *, Boolean, char **));
extern const char *iterate_words __P((const char **));
extern int Str_Match __P((const char *, const char *));
extern const char *Str_SYSVMatch __P((const char *, const char *, size_t *len));
extern void Str_SYSVSubst __P((Buffer, const char *, const char *, size_t));
extern char *interval_dup __P((const char *begin, const char *end));
extern char *escape_dup __P((const char *, const char *, const char *));

/* suff.c */
extern void Suff_ClearSuffixes __P((void));
extern Boolean Suff_IsTransform __P((char *));
extern GNode *Suff_AddTransform __P((char *));
extern void Suff_EndTransform __P((void *));
extern void Suff_AddSuffix __P((char *));
extern Lst Suff_GetPath __P((char *));
extern void Suff_DoPaths __P((void));
extern void Suff_AddInclude __P((char *));
extern void Suff_AddLib __P((char *));
extern void Suff_FindDeps __P((GNode *));
extern void Suff_SetNull __P((char *));
extern void Suff_Init __P((void));
extern void Suff_End __P((void));
extern void Suff_PrintAll __P((void));

/* targ.c */
extern void Targ_Init __P((void));
extern void Targ_End __P((void));
extern GNode *Targ_NewGN __P((const char *, const char *));
extern GNode *Targ_FindNode __P((const char *, int));
extern void Targ_FindList __P((Lst, Lst));
extern Boolean Targ_Ignore __P((GNode *));
extern Boolean Targ_Silent __P((GNode *));
extern Boolean Targ_Precious __P((GNode *));
extern void Targ_SetMain __P((GNode *));
extern void Targ_PrintCmd __P((void *));
extern char *Targ_FmtTime __P((TIMESTAMP));
extern void Targ_PrintType __P((int));
extern void Targ_PrintGraph __P((int));

/* var.c */
extern void Var_Delete __P((const char *, GSymT *));
extern void Var_Set __P((const char *, const char *, GSymT *));
extern void Varq_Set __P((int, const char *, GNode *));
extern void Var_Append __P((const char *, const char *, GSymT *));
extern void Varq_Append __P((int, const char *, GNode *));
extern Boolean Var_Exists __P((const char *, GSymT *));
extern Boolean Varq_Exists __P((int, GNode *));
extern char *Var_Value __P((const char *, GSymT *));
extern char *Varq_Value __P((int,  GNode *));
extern char *Var_Parse __P((char *, SymTable *, Boolean, size_t *, Boolean *));
extern char *Var_Subst __P((char *, SymTable *, Boolean));
extern char *Var_SubstVar __P((const char *, const char *, const char *, 
	size_t));
extern char *Var_GetTail __P((char *));
extern char *Var_GetHead __P((char *));
extern void Var_Init __P((void));
extern void Var_End __P((void));
extern void Var_Dump __P((GSymT *));
extern void SymTable_Init __P((SymTable *));
extern void SymTable_Destroy __P((SymTable *));
extern void Var_AddCmdline __P((const char *));
