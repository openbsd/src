/*
 * Declarations for the Decstation 3100 and 2100
 */

#ifndef _KN01VAR_H
#define _KN01VAR_H

extern int kn01_intr __P((unsigned mask, unsigned pc,
			  unsigned statusReg, unsigned causeReg));

/*
 * old-style, 4.4bsd kn01 interrupt establish function
 */
void kn01_enable_intr __P((u_int slotno,
			   int (*handler) __P((void* softc)),
			   void *sc,  int onoff));


/*
 * new-style kn01 interrupt establish function (not yet finished)
 */
void
kn01_intr_establish __P((struct device *parent, void * cookie, int level,
			 int (*handler) __P((intr_arg_t)), intr_arg_t arg));
#endif	/* _KN01VAR_H */
