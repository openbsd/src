/*	$NetBSD: bounds.h,v 1.3 1995/04/21 02:47:47 briggs Exp $	*/

#if defined(CHECKBOUNDS)

#undef CHECKBOUNDS

/* This requires ANSI C stringification. */
#define CHECKBOUNDS(a, i) {						\
	if ( (((a) + (i)) < (a)) ||					\
	     (((a) + (i)) >= ((a) + (sizeof(a) / sizeof(*(a))))) ) {	\
		printf("index " #i " (%d) exceeded bounds of " #a	\
			", '%s' line %d.\n", (i), __FILE__, __LINE__);	\
		printf("halting...\n");					\
		/*asm("	stop	#0x2700");*/				\
	}								\
}

#define CHECKPOINTER(a, p) {						\
	if ( ((p) < (a)) ||						\
	     ((p) >= ((a) + (sizeof(a) / sizeof(*(a))))) ) {		\
		printf("pointer " #p " (0x%X) exceeded bounds of " #a	\
			" (0x%X), '%s' line %d.\n",			\
			(p), (a), __FILE__, __LINE__);			\
		printf("halting...\n");					\
		/*asm("	stop	#0x2700");*/				\
	}								\
}

#else				/* !defined(CHECKBOUNDS) */

#define CHECKBOUNDS(a, i)
#define CHECKPOINTER(a, p)

#endif				/* defined(CHECKBOUNDS) */
