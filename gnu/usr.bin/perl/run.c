/*    run.c
 *
 *    Copyright (c) 1991-1994, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 */

#include "EXTERN.h"
#include "perl.h"

/*
 * "Away now, Shadowfax!  Run, greatheart, run as you have never run before!
 * Now we are come to the lands where you were foaled, and every stone you
 * know.  Run now!  Hope is in speed!"  --Gandalf
 */

dEXT char **watchaddr = 0;
dEXT char *watchok;

#ifndef DEBUGGING

int
runops() {
    SAVEI32(runlevel);
    runlevel++;

    while ( op = (*op->op_ppaddr)() ) ;
    return 0;
}

#else

static void debprof _((OP*op));

int
runops() {
    if (!op) {
	warn("NULL OP IN RUN");
	return 0;
    }

    SAVEI32(runlevel);
    runlevel++;

    do {
	if (debug) {
	    if (watchaddr != 0 && *watchaddr != watchok)
		fprintf(stderr, "WARNING: %lx changed from %lx to %lx\n",
		    (long)watchaddr, (long)watchok, (long)*watchaddr);
	    DEBUG_s(debstack());
	    DEBUG_t(debop(op));
	    DEBUG_P(debprof(op));
	}
    } while ( op = (*op->op_ppaddr)() );
    return 0;
}

I32
debop(op)
OP *op;
{
    SV *sv;
    deb("%s", op_name[op->op_type]);
    switch (op->op_type) {
    case OP_CONST:
	fprintf(stderr, "(%s)", SvPEEK(cSVOP->op_sv));
	break;
    case OP_GVSV:
    case OP_GV:
	if (cGVOP->op_gv) {
	    sv = NEWSV(0,0);
	    gv_fullname(sv, cGVOP->op_gv);
	    fprintf(stderr, "(%s)", SvPV(sv, na));
	    SvREFCNT_dec(sv);
	}
	else
	    fprintf(stderr, "(NULL)");
	break;
    default:
	break;
    }
    fprintf(stderr, "\n");
    return 0;
}

void
watch(addr)
char **addr;
{
    watchaddr = addr;
    watchok = *addr;
    fprintf(stderr, "WATCHING, %lx is currently %lx\n",
	(long)watchaddr, (long)watchok);
}

static void
debprof(op)
OP* op;
{
    if (!profiledata)
	New(000, profiledata, MAXO, U32);
    ++profiledata[op->op_type];
}

void
debprofdump()
{
    U32 i;
    if (!profiledata)
	return;
    for (i = 0; i < MAXO; i++) {
	if (profiledata[i])
	    fprintf(stderr, "%d\t%lu\n", i, profiledata[i]);
    }
}

#endif

