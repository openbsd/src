#ifndef _MACHINE_FPU_H
#define _MACHINE_FPU_H

#ifdef _KERNEL

void	save_vsx(struct proc *);
void	restore_vsx(struct proc *);

int	fpu_sigcode(struct proc *);

#endif

#endif /* _MACHINE_FPU_H_ */
