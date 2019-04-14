/* Public domain. */

#ifndef _ASM_IOSF_MBI_H
#define _ASM_IOSF_MBI_H

#define MBI_PMIC_BUS_ACCESS_BEGIN	1
#define MBI_PMIC_BUS_ACCESS_END		2

#define iosf_mbi_assert_punit_acquired()
#define iosf_mbi_punit_acquire()
#define iosf_mbi_punit_release()
#define iosf_mbi_register_pmic_bus_access_notifier(x)			0
#define iosf_mbi_unregister_pmic_bus_access_notifier_unlocked(x)	0

#endif
