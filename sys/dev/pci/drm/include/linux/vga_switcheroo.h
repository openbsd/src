/* Public domain. */

#ifndef _LINUX_VGA_SWITCHEROO_H
#define _LINUX_VGA_SWITCHEROO_H

#define vga_switcheroo_register_client(a, b, c)	0
#define vga_switcheroo_unregister_client(a)
#define vga_switcheroo_process_delayed_switch()
#define vga_switcheroo_fini_domain_pm_ops(x)
#define vga_switcheroo_lock_ddc(x)
#define vga_switcheroo_unlock_ddc(x)
#define vga_switcheroo_handler_flags()		0
#define vga_switcheroo_client_fb_set(a, b)

#define VGA_SWITCHEROO_CAN_SWITCH_DDC		1

#endif
