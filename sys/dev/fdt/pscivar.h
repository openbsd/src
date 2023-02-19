/* Public Domain */

#ifndef _SYS_DEV_FDT_PSCIVAR_H_
#define _SYS_DEV_FDT_PSCIVAR_H_

#define PSCI_SUCCESS		0
#define PSCI_NOT_SUPPORTED	-1

#define PSCI_METHOD_NONE	0
#define PSCI_METHOD_HVC		1
#define PSCI_METHOD_SMC		2

int	psci_can_suspend(void);

int32_t	psci_system_suspend(register_t, register_t);
int32_t	psci_cpu_on(register_t, register_t, register_t);
int32_t	psci_cpu_off(void);
int32_t	psci_cpu_suspend(register_t, register_t, register_t);
void	psci_flush_bp(void);
int	psci_flush_bp_has_bhb(void);
int	psci_method(void);

int32_t	smccc(uint32_t, register_t, register_t, register_t);

#endif /* _SYS_DEV_FDT_PSCIVAR_H_ */
