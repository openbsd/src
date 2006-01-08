/*	$OpenBSD: adb.c,v 1.16 2006/01/08 17:25:05 miod Exp $	*/
/*	$NetBSD: adb.c,v 1.47 2005/06/16 22:43:36 jmc Exp $	*/

/*
 * Copyright (C) 1994	Bradley A. Grantham
 * All rights reserved.
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
 *	This product includes software developed by Bradley A. Grantham.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mac68k/dev/adbvar.h>

/*
 * Function declarations.
 */
int	adbmatch(struct device *, void *, void *);
void	adbattach(struct device *, struct device *, void *);
int	adbprint(void *, const char *);
void	adb_attach_deferred(void *);

extern void	adb_jadbproc(void);

/*
 * Global variables.
 */
int	adb_polling = 0;	/* Are we polling?  (Debugger mode) */
#ifdef ADB_DEBUG
int	adb_debug = 0;		/* Output debugging messages */
#endif /* ADB_DEBUG */

extern struct	mac68k_machine_S mac68k_machine;
extern int	adbHardware;
extern char	*adbHardwareDescr[];

/*
 * Driver definition.
 */
struct cfattach adb_ca = {
	sizeof(struct device), adbmatch, adbattach
};
struct cfdriver adb_cd = {
	NULL, "adb", DV_DULL
};

int
adbmatch(struct device *parent, void *vcf, void *aux)
{
	static int adb_matched = 0;

	/* Allow only one instance. */
	if (adb_matched)
		return (0);

	adb_matched = 1;
	return (1);
}

void
adbattach(struct device *parent, struct device *self, void *aux)
{
	printf("\n");
	startuphook_establish(adb_attach_deferred, self);
}

void
adb_attach_deferred(void *v)
{
	struct device *self = v;
	ADBDataBlock adbdata;
	struct adb_attach_args aa_args;
	int totaladbs;
	int adbindex, adbaddr;

	printf("%s", self->dv_xname);
	adb_polling = 1;

#ifdef MRG_ADB
	if (!mrg_romready()) {
		printf(": no ROM ADB driver in this kernel for this machine\n");
		return;
	}

#ifdef ADB_DEBUG
	if (adb_debug)
		printf("adb: call mrg_initadbintr\n");
#endif

	mrg_initadbintr();	/* Mac ROM Glue okay to do ROM intr */
#ifdef ADB_DEBUG
	if (adb_debug)
		printf("adb: returned from mrg_initadbintr\n");
#endif

	/* ADBReInit pre/post-processing */
	JADBProc = adb_jadbproc;

	/* Initialize ADB */
#ifdef ADB_DEBUG
	if (adb_debug)
		printf("adb: calling ADBAlternateInit.\n");
#endif

	printf(": mrg");
	ADBAlternateInit();
#else
	ADBReInit();
	printf(": %s", adbHardwareDescr[adbHardware]);

#ifdef ADB_DEBUG
	if (adb_debug)
		printf("adb: done with ADBReInit\n");
#endif

#endif /* MRG_ADB */

	totaladbs = CountADBs();

	printf(", %d target%s\n", totaladbs, (totaladbs == 1) ? "" : "s");

	/* for each ADB device */
	for (adbindex = 1; adbindex <= totaladbs; adbindex++) {
		/* Get the ADB information */
		adbaddr = GetIndADB(&adbdata, adbindex);

		aa_args.origaddr = (int)(adbdata.origADBAddr);
		aa_args.adbaddr = adbaddr;
		aa_args.handler_id = (int)(adbdata.devType);

		(void)config_found(self, &aa_args, adbprint);
	}
	adb_polling = 0;
}


int
adbprint(void *args, const char *name)
{
	struct adb_attach_args *aa_args = (struct adb_attach_args *)args;
	int rv = UNCONF;

	if (name) {	/* no configured device matched */
		rv = UNSUPP; /* most ADB device types are unsupported */

		/* print out what kind of ADB device we have found */
		switch(aa_args->origaddr) {
#ifdef ADBVERBOSE
		case ADBADDR_SECURE:
			printf("security dongle (%d)",
			    aa_args->handler_id);
			break;
#endif
		case ADBADDR_MAP:
			printf("mapped device (%d)",
			    aa_args->handler_id);
			rv = UNCONF;
			break;
		case ADBADDR_REL:
			printf("relative positioning device (%d)",
			    aa_args->handler_id);
			rv = UNCONF;
			break;
#ifdef ADBVERBOSE
		case ADBADDR_ABS:
			switch (aa_args->handler_id) {
			case ADB_ARTPAD:
				printf("WACOM ArtPad II");
				break;
			default:
				printf("absolute positioning device (%d)",
				    aa_args->handler_id);
				break;
			}
			break;
		case ADBADDR_DATATX:
			printf("data transfer device (modem?) (%d)",
			    aa_args->handler_id);
			break;
		case ADBADDR_MISC:
			switch (aa_args->handler_id) {
			case ADB_POWERKEY:
				printf("Sophisticated Circuits PowerKey");
				break;
			default:
				printf("misc. device (remote control?) (%d)",
				    aa_args->handler_id);
				break;
			}
			break;
		default:
			printf("unknown type %d device, (handler %d)",
			    aa_args->origaddr, aa_args->handler_id);
			break;
#endif /* ADBVERBOSE */
		}
		printf(" at %s", name);
	}

	printf(" addr %d", aa_args->adbaddr);

	return (rv);
}


/*
 * adb_op_sync
 *
 * This routine does exactly what the adb_op routine does, except that after
 * the adb_op is called, it waits until the return value is present before
 * returning.
 *
 * NOTE: The user specified compRout is ignored, since this routine specifies
 * it's own to adb_op, which is why you really called this in the first place
 * anyway.
 */
int
adb_op_sync(Ptr buffer, Ptr compRout, Ptr data, short command)
{
	int tmout;
	int result;
	volatile int flag = 0;

	result = ADBOp(buffer, (void *)adb_op_comprout, (Ptr)&flag, 
	    command);	/* send command */
	if (result == 0) {		/* send ok? */
		/*
		 * Total time to wait is calculated as follows:
		 *  - Tlt (stop to start time): 260 usec
		 *  - start bit: 100 usec
		 *  - up to 8 data bytes: 64 * 100 usec = 6400 usec
		 *  - stop bit (with SRQ): 140 usec
		 * Total: 6900 usec
		 *
		 * This is the total time allowed by the specification.  Any
		 * device that doesn't conform to this will fail to operate
		 * properly on some Apple systems.  In spite of this we
		 * double the time to wait; some Cuda-based apparently
		 * queues some commands and allows the main CPU to continue
		 * processing (radical concept, eh?).  To be safe, allow
		 * time for two complete ADB transactions to occur.
		 */
		for (tmout = 13800; !flag && tmout >= 10; tmout -= 10)
			delay(10);
		if (!flag && tmout > 0)
			delay(tmout);

		if (!flag)
			result = -2;
	}

	return result;
}


/*
 * adb_op_comprout
 *
 * This function is used by the adb_op_sync routine so it knows when the
 * function is done.
 */
void 
adb_op_comprout(void)
{
	asm("movw	#1,a2@			| update flag value");
}
