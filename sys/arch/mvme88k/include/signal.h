/* Stolen from SVR4 (/usr/include/sys/signal.h) */

typedef int sig_atomic_t;

/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler. It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 *
 * All machines must have an sc_onstack and sc_mask.
 */
struct  sigcontext {
        int     sc_onstack;             /* sigstack state to restore */
        int     sc_mask;                /* signal mask to restore */
	/* begin machine dependent portion */
	int	sc_regs[32];
#define	sc_sp	sc_regs[31]
	int	sc_xip;
	int	sc_nip;
	int	sc_fip;
	int	sc_ps;
	int	sc_fpsr;
	int	sc_fpcr;
	int	sc_ssbr;
	int	sc_dmt0;
	int	sc_dmd0;
	int	sc_dma0;
	int	sc_dmt1;
	int	sc_dmd1;
	int	sc_dma1;
	int	sc_dmt2;
	int	sc_dmd2;
	int	sc_dma2;
	int	sc_fpecr;
	int	sc_fphs1;
	int	sc_fpls1;
	int	sc_fphs2;
	int	sc_fpls2;
	int	sc_fppt;
	int	sc_fprh;
	int	sc_fprl;
	int	sc_fpit;
};
