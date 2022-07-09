/* Public Domain */

#ifndef _SYS_DEV_FDT_PSCIVAR_H_
#define _SYS_DEV_FDT_PSCIVAR_H_

#define PSCI_SUCCESS		0
#define PSCI_NOT_SUPPORTED	-1

int	psci_can_suspend(void);

int32_t	psci_system_suspend(register_t, register_t);
int32_t	psci_cpu_on(register_t, register_t, register_t);
int32_t	psci_cpu_off(void);
void	psci_flush_bp(void);

#endif /* _SYS_DEV_FDT_PSCIVAR_H_ */
