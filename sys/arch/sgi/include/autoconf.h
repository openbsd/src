/*	$OpenBSD: autoconf.h,v 1.39 2015/12/25 08:34:50 visa Exp $ */

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Definitions used by autoconfiguration.
 */

#ifndef _MACHINE_AUTOCONF_H_
#define _MACHINE_AUTOCONF_H_

#include <machine/bus.h>

/*
 * Structure holding all misc config information.
 */
struct cpu_info;

struct sys_rec {
	int	system_type;
	int	system_subtype;		/* IP35 only */

	/* Serial console configuration. */
	struct mips_bus_space console_io;
};

extern struct sys_rec sys_config;

/*
 * Attachment information for mainbus child devices.
 */
struct mainbus_attach_args {
	const char	*maa_name;
	int16_t		 maa_nasid;
	int16_t		 maa_physid;
};

/*
 * Device physical location information.  Used to match console and boot
 * devices on IP27 and IP30 kernels.
 */
struct sgi_device_location {
	int16_t		nasid;		/* node identifier */
	uint		widget;		/* widget number */

	int		bus;		/* bus number if connected to PIC */
	int		device;		/* device number if PCI */
	int		fn;		/* function number if PCI */

	uint32_t	specific;	/* port on dual-scsibus controllers,
					   device id on serial controllers */
};

#include <mips64/autoconf.h>

void	enaddr_aton(const char *, u_int8_t *);
u_long	bios_getenvint(const char *);

struct device;

void	arcs_device_register(struct device *, void *);
void	dksc_device_register(struct device *, void *);
extern	void (*_device_register)(struct device *, void *);

void	ip22_setup(void);
void	ip22_post_autoconf(void);
void	ip27_setup(void);
void	ip27_autoconf(struct device *);
void	ip30_setup(void);
void	ip30_autoconf(struct device *);
void	ip32_setup(void);

extern char osloadpartition[256];
extern char osloadoptions[129];
extern int16_t masternasid;
extern int16_t currentnasid;

extern struct sgi_device_location console_output, console_input;

int	location_match(struct sgi_device_location *,
	    struct sgi_device_location *);

extern void (*md_halt)(int);
void	arcbios_halt(int);

#endif /* _MACHINE_AUTOCONF_H_ */
