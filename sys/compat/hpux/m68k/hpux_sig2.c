#	$OpenBSD: hpux_sig2.c,v 1.1 2004/09/19 21:56:18 mickey Exp $
#
# Config file description for machine-independent HPUX compat code.
# Included by ports that need it.

# ports should define any machine-specific files they need in their
# own file lists.

file	compat/hpux/hpux_compat.c		compat_hpux
file	compat/hpux/hpux_file.c			compat_hpux
file	compat/hpux/hpux_tty.c			compat_hpux
file	compat/hpux/hpux_sig.c			compat_hpux
file	compat/hpux/m68k/hpux_sig2.c		compat_hpux
file	compat/hpux/m68k/hpux_exec.c		compat_hpux
file	compat/hpux/m68k/hpux_net.c		compat_hpux
file	compat/hpux/m68k/hpux_syscalls.c	compat_hpux & syscall_debug
file	compat/hpux/m68k/hpux_sysent.c		compat_hpux
