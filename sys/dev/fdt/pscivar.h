/* Public Domain */

#ifndef _SYS_DEV_FDT_PSCIVAR_H_
#define _SYS_DEV_FDT_PSCIVAR_H_

#define PSCI_SUCCESS		0
#define PSCI_NOT_SUPPORTED	-1

int32_t psci_cpu_on(register_t, register_t, register_t);
void	psci_flush_bp(void);

#endif /* _SYS_DEV_FDT_PSCIVAR_H_ */
