/*	$OpenPackages$ */
/*	$OpenBSD: extern.h,v 1.37 2001/05/03 13:41:05 espie Exp $	*/
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
extern ReturnStatus Arch_ParseArchive(char **, Lst, SymTable *);
extern void Arch_Touch(GNode *);
extern void Arch_TouchLib(GNode *);
extern TIMESTAMP Arch_MTime(GNode *);
extern TIMESTAMP Arch_MemMTime(GNode *);
extern void Arch_FindLib(GNode *, Lst);
extern Boolean Arch_LibOODate(GNode *);
extern void Arch_Init(void);
extern void Arch_End(void);
extern Boolean Arch_IsLib(GNode *);

/* compat.c */
extern void Compat_Run(Lst);

/* cond.c */
extern int Cond_Eval(char *);
extern void Cond_End(void);

#include "error.h"

/* for.c */
typedef struct For_ For;
extern For *For_Eval(const char *);
extern Boolean For_Accumulate(For *, const char *);
extern void For_Run (For *);

/* main.c */
extern void Main_ParseArgLine(char *);
extern char *Cmd_Exec(const char *, char **);
extern void Error(char *, ...);
extern void Fatal(char *, ...);
extern void Punt(char *, ...);
extern void DieHorribly(void);
extern void PrintAddr(void *);
extern void Finish(int);

/* make.c */
extern void Make_TimeStamp(GNode *, GNode *);
extern Boolean Make_OODate(GNode *);
extern void Make_HandleUse(GNode *, GNode *);
extern void Make_Update(GNode *);
extern void Make_DoAllVar(GNode *);
extern Boolean Make_Run(Lst);

/* parse.c */
extern void Parse_Error(int, char *, ...);
extern Boolean Parse_AnyExport(void);
extern Boolean Parse_IsVar(char *);
extern void Parse_DoVar(const char *, GSymT *);
extern void Parse_AddIncludeDir(const char *);
extern void Parse_File(char *, FILE *);
extern void Parse_Init(void);
extern void Parse_End(void);
extern void Parse_FromString(char *, unsigned long);
extern unsigned long Parse_Getlineno(void);
extern const char *Parse_Getfilename(void);
extern void Parse_MainName(Lst);

/* stats.h */
extern void Init_Stats(void);

/* str.c */
extern void str_init(void);
extern void str_end(void);
extern char *lastchar(const char *, const char *, int);
extern char *str_concati(const char *, const char *, const char *, int);
#define str_concat(s1, s2, sep) str_concati(s1, s2, strchr(s2, '\0'), sep)
extern char **brk_string(const char *, int *, char **);
extern const char *iterate_words(const char **);
extern Boolean Str_Matchi(const char *, const char *, const char *);
#define Str_Match(string, pattern) \
	Str_Matchi(string, pattern, strchr(pattern, '\0'))
extern const char *Str_SYSVMatch(const char *, const char *, size_t *);
extern void Str_SYSVSubst(Buffer, const char *, const char *, size_t);
extern char *interval_dup(const char *, const char *);
extern char *escape_dup(const char *, const char *, const char *);

/* suff.c */
extern void Suff_ClearSuffixes(void);
extern Boolean Suff_IsTransform(const char *);
extern GNode *Suff_AddTransform(const char *);
extern void Suff_EndTransform(void *);
extern void Suff_AddSuffix(char *);
extern Lst Suff_GetPath(char *);
extern void Suff_DoPaths(void);
extern void Suff_AddInclude(char *);
extern void Suff_AddLib(char *);
extern void Suff_FindDeps(GNode *);
extern void Suff_SetNull(char *);
extern void Suff_Init(void);
extern void Suff_End(void);
extern void Suff_PrintAll(void);

/* targ.c */
extern void Targ_Init(void);
extern void Targ_End(void);
extern GNode *Targ_NewGN(const char *, const char *);
extern GNode *Targ_FindNode(const char *, const char *, int);
extern void Targ_FindList(Lst, Lst);
extern Boolean Targ_Ignore(GNode *);
extern Boolean Targ_Silent(GNode *);
extern Boolean Targ_Precious(GNode *);
extern void Targ_SetMain(GNode *);
extern void Targ_PrintCmd(void *);
extern char *Targ_FmtTime(TIMESTAMP);
extern void Targ_PrintType(int);
extern void Targ_PrintGraph(int);

/* var.c */
extern void Var_Delete(const char *);
extern void Var_Set_interval(const char *, const char *, const char *,
	GSymT *);
extern void Varq_Set(int, const char *, GNode *);
extern void Var_Append_interval(const char *, const char *,
	const char *, GSymT  *);
extern void Varq_Append(int, const char *, GNode *);
extern char *Var_Value_interval(const char *, const char *);
extern char *Varq_Value(int,  GNode *);
extern char *Var_Parse(const char *, SymTable *, Boolean, size_t *,
	Boolean *);
extern size_t Var_ParseSkip(const char *, SymTable *, ReturnStatus *);
extern ReturnStatus Var_ParseBuffer(Buffer, const char *, SymTable *,
	Boolean, size_t *);
extern char *Var_Subst(const char *, SymTable *, Boolean);
extern void Var_SubstVar(Buffer, const char *, const char *, const char *);
extern void Var_Init(void);
extern void Var_End(void);
extern void Var_Dump(void);
extern void SymTable_Init(SymTable *);
extern void SymTable_Destroy(SymTable *);
#define Var_Set(n, v, ctxt)	Var_Set_interval(n, NULL, v, ctxt)
#define Var_Append(n, v, ctxt)	Var_Append_interval(n, NULL, v, ctxt)
#define Var_Value(n)	Var_Value_interval(n, NULL)
extern void Var_AddCmdline(const char *);

/* Used to store temporary names, after $ expansion */
struct Name {
	const char 	*s;
	const char 	*e;
	Boolean		tofree;
};

extern const char *Var_Name_Get(const char *, struct Name *, SymTable *, 
    Boolean, const char *(*)(const char *));
extern void Var_Name_Free(struct Name *);
