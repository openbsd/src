#ifdef AFS_HPUX_ENV
#ifdef hp9000s300
/*
#
#	Process assembly language assist for HP 9000 series 300s.
#
*/

	text

/*
#
# struct savearea {
#	char	*topstack;
# }
#
*/

	global	_PRE_Block

	set topstack,0

/* Stuff to allow saving/restoring registers */
	set NREGS,13
	set REGS,0x3ffe			# d1-d7 & a0-a5

/*
# savecontext(f, area1, newsp)
#     int (*f)(); struct savearea *area1; char *newsp;
*/

/* Stack offsets of arguments */
	set f,8
	set area1,12
	set newsp,16

	global	_savecontext
_savecontext:
	mov.b	&1,_PRE_Block		# Dont allow any interrupt finagling
	link	%a6,&-(NREGS*4)		# Save frame pointer & ...
					# ... allocate space for nregs registers
/* Save registers */
	movem.l	&REGS,(%sp)

	mov.l	area1(%a6),%a0		# a0 = base of savearea
	mov.l	%sp,topstack(%a0)	# area->topstack = sp
	mov.l	newsp(%a6),%d0		# Get new sp
	beq.b	l1			# If newsp == 0, no stack switch
	mov.l	%d0,%sp			# Switch to new stack
l1:
	mov.l	f(%a6),%a0		# a0 = f
	jsr	(%a0)			# f()

/* It is impossible to be here, so abort() */

	jsr	_abort

/*
# returnto(area2)
#     struct savearea *area2;
*/

/* Stack offset of argument */
set	area2,8

	global _returnto
_returnto:
	link	%a6,&0
	mov.l	area2(%a6),%a0		# Base of savearea
	mov.l	topstack(%a0),%sp	# Restore sp
/* Restore registers */
	movem.l	(%sp),&REGS

	add.l	&(NREGS*4),%sp
	mov.l	%sp,%a6			# Argghh...be careful here
	unlk	%a6
	clr.b	_PRE_Block
	rts				# Return to previous process
#else

/* start of S700 code */
#ifdef AFS_HPUX_PIC_ENV
/* this code is PIC */
	.CODE

	/*
	; savecontext(f, area1, newsp)
	;    int (*f)();
	;    struct savearea *area1;
	;    char *newsp;
	*/
savecontext
	.PROC
	/*
	; Callinfo sets up register saves using the ENTRY_GR
	; and ENTRY_FR parameters.  ENTRY_FR=21 is only valid
	; for PA 1.1.  (How to deal with this for 800?)
	*/
	.CALLINFO CALLER,FRAME=0,SAVE_RP,ENTRY_GR=18,ENTRY_FR=21
	/*
	; The ENTER statement generates register saves and
	; procedure setup.
	*/
	.ENTER

	LDI   1,%r1				/* Store a (char) 1 in */
	LDW   T%PRE_Block(0,%r19),%r31    	/* global variable */
	STB   %r1,0(0,%r31)			/* PRE_Block. */


	COPY  %arg0,%r22	/* Copy arg0 (f) to dyncall's input register */

	COMIB,= 0,%arg2,L$0001	/* Compare arg2 (newsp) to 0.  Execute the */
				/* next instruction regardless of value. */
	STW   %r30,0(0,%arg1)	/* Store the stack pointer in the first */
				/* element (0th offset) of arg1 (area1). */
	COPY  %arg2,%r30	/* Move arg2 (newsp) into the stack ptr. */

L$0001
	.CALL
	BL    $$dyncall,%r31	/* Dynamic call using pointer in r22. */
	COPY  %r31,%r2
	COPY  %r20, %r19        /* restore link table. */

	.CALL
	BL    exit,%r2		/* Can't get here, so abort. */
	NOP
	.LEAVE
	.PROCEND

	/* returnto(area2)
	 *    struct savearea *area2;
	 */
returnto
	.PROC
	.CALLINFO CALLER,FRAME=0,SAVE_RP,ENTRY_GR=18,ENTRY_FR=21
	/*  No ENTRY is used since this is a magic routine. */
	LDWS  0(0,%arg0),%r30	/* Load the stack pointer from area2 */

	LDW   T%PRE_Block(0,%r19),%r31   /* set PRE_Block = 0; */
	STB   %r0,0(0,%r31)
	.LEAVE
	.PROCEND

	.EXPORT savecontext,ENTRY
	.EXPORT returnto,ENTRY
	.IMPORT PRE_Block,DATA
	.IMPORT $$dyncall,MILLICODE
	.IMPORT exit,CODE

	.END
#else  /* AFS_HPUX_PIC_ENV */
/* non-pic'ified code */
	.CODE

	/*
	; savecontext(f, area1, newsp)
	;    int (*f)();
	;    struct savearea *area1;
	;    char *newsp;
	*/
savecontext
	.PROC
	/*
	; Callinfo sets up register saves using the ENTRY_GR
	; and ENTRY_FR parameters.  ENTRY_FR=21 is only valid
	; for PA 1.1.  (How to deal with this for 800?)
	*/
	.CALLINFO CALLER,FRAME=0,SAVE_RP,ENTRY_GR=18,ENTRY_FR=21
	/* The ENTER statement generates register saves and */
	/* procedure setup. */
	.ENTER

	LDI   1,%r31				/* Store a (char) 1 in */
	ADDIL L%PRE_Block-$global$,%r27		/* global variable */
	STB   %r31,R%PRE_Block-$global$(0,%r1)	/* PRE_Block. */

	COPY  %r26,%r22		/* Copy arg0 (f) to dyncall's input register. */

	COMIB,= 0,%r24,L$0001	/* Compare arg2 (newsp) to 0.  Execute the */
				/* next instruction regardless of value. */
	STWS  %r30,0(0,%r25)	/* Store the stack pointer in the first */
				/* element (0th offset) of arg1 (area1). */
	COPY  %r24,%r30		/* Move arg2 (newsp) into the stack ptr. */

L$0001
	.CALL
	BL    $$dyncall,%r31	/* Dynamic call using pointer in r22. */
	COPY  %r31,%r2

	.CALL
	BL    abort,%r2		/* Can't get here, so abort.
	NOP
	.LEAVE
	.PROCEND

	/*
	; returnto(area2)
	;    struct savearea *area2;
	*/
returnto
	.PROC
	.CALLINFO CALLER,FRAME=0,SAVE_RP,ENTRY_GR=18,ENTRY_FR=21
	/*  No ENTRY is used since this is a magic routine. */
	ADDIL L%PRE_Block-$global$,%r27		/* PRE_Block = 0 */
	STB   %r0,R%PRE_Block-$global$(0,%r1)

	LDWS  0(0,%r26),%r30	/* Load the stack pointer from area2 */
	.LEAVE
	.PROCEND

	.EXPORT savecontext,ENTRY
	.EXPORT returnto,ENTRY
	.IMPORT $global$,DATA
	.IMPORT PRE_Block,DATA
	.IMPORT $$dyncall,MILLICODE
	.IMPORT abort,CODE

	.END
#endif /* AFS_HPUX_PIC_ENV */
#endif /* hp9000s300 */
#endif /* AFS_HPUX_ENV */

#ifdef AFS_BSD_ENV
#ifdef AFS_BSD_PIC_ENV
/* this code is PIC */
	.CODE

	/*
	; savecontext(f, area1, newsp)
	;    int (*f)();
	;    struct savearea *area1;
	;    char *newsp;
	*/
savecontext
	.PROC
	/*
	; Callinfo sets up register saves using the ENTRY_GR
	; and ENTRY_FR parameters.  ENTRY_FR=21 is only valid
	; for PA 1.1.  (How to deal with this for 800?)
	*/
	.CALLINFO CALLER,FRAME=0,SAVE_RP,ENTRY_GR=18,ENTRY_FR=21
	/*
	; The ENTER statement generates register saves and
	; procedure setup.
	*/
	.ENTER

	LDI   1,%r1				/* Store a (char) 1 in */
	LDW   T%PRE_Block(0,%r19),%r31    	/* global variable */
	STB   %r1,0(0,%r31)			/* PRE_Block. */


	COPY  %arg0,%r22	/* Copy arg0 (f) to dyncall's input register */

	COMIB,= 0,%arg2,L$0001	/* Compare arg2 (newsp) to 0.  Execute the */
				/* next instruction regardless of value. */
	STW   %r30,0(0,%arg1)	/* Store the stack pointer in the first */
				/* element (0th offset) of arg1 (area1). */
	COPY  %arg2,%r30	/* Move arg2 (newsp) into the stack ptr. */

L$0001
	.CALL
	BL    $$dyncall,%r31	/* Dynamic call using pointer in r22. */
	COPY  %r31,%r2
	COPY  %r20, %r19        /* restore link table. */

	.CALL
	BL    exit,%r2		/* Can't get here, so abort. */
	NOP
	.LEAVE
	.PROCEND

	/* returnto(area2)
	 *    struct savearea *area2;
	 */
returnto
	.PROC
	.CALLINFO CALLER,FRAME=0,SAVE_RP,ENTRY_GR=18,ENTRY_FR=21
	/*  No ENTRY is used since this is a magic routine. */
	LDWS  0(0,%arg0),%r30	/* Load the stack pointer from area2 */

	LDW   T%PRE_Block(0,%r19),%r31   /* set PRE_Block = 0; */
	STB   %r0,0(0,%r31)
	.LEAVE
	.PROCEND

	.EXPORT savecontext,ENTRY
	.EXPORT returnto,ENTRY
	.IMPORT PRE_Block,DATA
	.IMPORT $$dyncall,MILLICODE
	.IMPORT exit,CODE

	.END
#else  /* AFS_BSD_PIC_ENV */
/* non-pic'ified code */
	.IMPORT $global$,DATA
	.IMPORT PRE_Block,DATA
	.IMPORT $$dyncall,MILLICODE
	.IMPORT abort,CODE

	.text

	/*
	; savecontext(f, area1, newsp)
	;    int (*f)();
	;    struct savearea *area1;
	;    char *newsp;
	*/
	.EXPORT savecontext,ENTRY
savecontext
	.PROC
	/*
	; Callinfo sets up register saves using the ENTRY_GR
	; and ENTRY_FR parameters.  ENTRY_FR=21 is only valid
	; for PA 1.1.  (How to deal with this for 800?)
	*/
	.CALLINFO CALLER,FRAME=0,SAVE_RP,ENTRY_GR=18,ENTRY_FR=21
	/* The ENTER statement generates register saves and */
	/* procedure setup. */
	.ENTRY

	LDI   1,%r31				/* Store a (char) 1 in */
	ADDIL L%PRE_Block-$global$,%r27		/* global variable */
	STB   %r31,R%PRE_Block-$global$(0,%r1)	/* PRE_Block. */

	COPY  %r26,%r22		/* Copy arg0 (f) to dyncall's input register. */

	COMIB,= 0,%r24,L$0001	/* Compare arg2 (newsp) to 0.  Execute the */
				/* next instruction regardless of value. */
	STWS  %r30,0(0,%r25)	/* Store the stack pointer in the first */
				/* element (0th offset) of arg1 (area1). */
	COPY  %r24,%r30		/* Move arg2 (newsp) into the stack ptr. */

L$0001
	.CALL
	BL    $$dyncall,%r31	/* Dynamic call using pointer in r22. */
	COPY  %r31,%r2

	.CALL
	BL    abort,%r2		/* Can't get here, so abort. */
	NOP
	.EXIT
	.PROCEND

	.text

	/*
	; returnto(area2)
	;    struct savearea *area2;
	*/
	.EXPORT returnto,ENTRY
returnto
	.PROC
	.CALLINFO CALLER,FRAME=0,SAVE_RP,ENTRY_GR=18,ENTRY_FR=21
	.ENTRY
	ADDIL L%PRE_Block-$global$,%r27		/* PRE_Block = 0 */
	STB   %r0,R%PRE_Block-$global$(0,%r1)

	LDWS  0(0,%r26),%r30	/* Load the stack pointer from area2 */
	.EXIT
	.PROCEND

	.END
#endif /* AFS_BSD_PIC_ENV */
#endif /* AFS_BSD_ENV */
