/*      $NetBSD: nm.h,v 1.1 1995/06/05 15:23:03 ragge Exp $       */

#define PTRACE_ARG3_TYPE        caddr_t

#define ATTACH_DETACH

#define FETCH_INFERIOR_REGISTERS

#define KERNEL_U_ADDR USRSTACK

/*
 * Here is something very foolish! trapframes, signal stacks,
 * assembly routines and coredumps should be synced to work together!
 * Must clean this up someday. /ragge
 */

#define REGISTER_U_ADDR(addr, blockend, regno)                          \
{                                                                       \
	switch (regno) {						\
	case 12:							\
		addr = 4;						\
		break;							\
									\
	case 13:							\
		addr = 0;						\
		break;							\
									\
	case 16:							\
		addr = 68;						\
		break;							\
									\
	case 15:							\
		addr = 64;						\
		break;							\
									\
	case 14:							\
		addr = 60;						\
		break;							\
									\
	default: /* Reg 0-11 */						\
		addr = regno * 4 + 8;					\
		break;							\
									\
	}								\
}
