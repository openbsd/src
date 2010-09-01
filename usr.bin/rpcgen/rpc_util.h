/*	$OpenBSD: rpc_util.h,v 1.15 2010/09/01 14:43:34 millert Exp $	*/
/*	$NetBSD: rpc_util.h,v 1.3 1995/06/11 21:50:10 pk Exp $	*/

/*
 * Copyright (c) 2010, Oracle America, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the "Oracle America, Inc." nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *   INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 *   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*      @(#)rpc_util.h  1.5  90/08/29  (C) 1987 SMI   */

/*
 * rpc_util.h, Useful definitions for the RPC protocol compiler 
 */

#define alloc(size)		(void *)malloc((unsigned)(size))
#define ALLOC(object)   (object *) malloc(sizeof(object))

struct list {
	definition *val;
	struct list *next;
};
typedef struct list list;

#define PUT 1
#define GET 2

/*
 * Global variables 
 */
#define MAXLINESIZE 1024
extern char curline[MAXLINESIZE];
extern char *where;
extern int linenum;

extern char *infilename;
extern FILE *fout;
extern FILE *fin;

extern list *defined;


extern bas_type *typ_list_h;
extern bas_type *typ_list_t;

/*
 * All the option flags
 */
extern int inetdflag;
extern int pmflag;
extern int tblflag;
extern int logflag;
extern int newstyle;
extern int Cflag;     /* C++ flag */
extern int tirpcflag; /* flag for generating tirpc code */
extern int doinline; /* if this is 0, then do not generate inline code */
extern int callerflag;

/*
 * Other flags related with inetd jumpstart.
 */
extern int indefinitewait;
extern int exitnow;
extern int timerflag;

extern int nonfatalerrors;

/*
 * rpc_util routines 
 */
void storeval(list **, definition *);

#define STOREVAL(list,item)	\
	storeval(list,item)

definition *findval(list *, char *, int (*)(definition *, char *));

#define FINDVAL(list,item,finder) \
	findval(list, item, finder)

void crash(void);
char *fixtype(char *);
char *stringfix(char *);
char *locase(char *);
void pvname_svc(char *, char *);
void pvname(char *, char *);
void ptype(char *, char *, int);
int isvectordef(char *, relation);
int streq(char *, char *);
void error(char *);
void tabify(FILE *, int);
void record_open(char *);
bas_type *find_type(char *);
char *make_argname(char *, char *);
/*
 * rpc_cout routines 
 */
void emit(definition *);

/*
 * rpc_hout routines 
 */
void print_datadef(definition *);
void print_funcdef(definition *);

/*
 * rpc_svcout routines 
 */
void write_most(char *, int, int);
void write_rest(void);
void write_programs(char *);
void write_svc_aux(int);
void write_inetd_register(char *);
void write_netid_register(char *);
void write_nettype_register(char *);
int nullproc(proc_list *);

/*
 * rpc_clntout routines
 */
void write_stubs(void);
void printarglist(proc_list *, char *, char *);


/*
 * rpc_tblout routines
 */
void write_tables(void);

/*
 * rpc_sample routines
 */
void write_sample_svc(definition *);
int write_sample_clnt(definition *);
void write_sample_clnt_main(void);

void add_type(int len, char *type);

void pdeclaration(char *, declaration *, int, char *);
void add_sample_msg(void);
