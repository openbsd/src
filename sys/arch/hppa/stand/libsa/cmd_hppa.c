/*	$OpenBSD: cmd_hppa.c,v 1.1 2002/03/02 09:36:43 miod Exp $	*/

/*
 * Copyright (c) 2002 Miodrag Vallat
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*#define	DEBUG*/

#include <sys/param.h>
/* would come from <sys/param.h> if -D_KERNEL */
#define offsetof(s, e) ((size_t)&((s *)0)->e)

#include <machine/iomod.h>
#include <machine/pdc.h>

#include <arch/hppa/dev/cpudevs.h>

#include <libsa.h>
#include "cmd.h"
#include "dev_hppa.h"	/* pdc */

extern struct stable_storage sstor;
extern int sstorsiz;

/* storage sizes we're interested in */
#define	CONSOLEOFFSET \
	offsetof(struct stable_storage, ss_console)
#define	CONSOLESIZE \
	(offsetof(struct stable_storage, ss_console) + \
	 sizeof(struct device_path))

#define	KEYBOARDOFFSET \
	offsetof(struct stable_storage, ss_keyboard)
#define	KEYBOARDSIZE \
	(offsetof(struct stable_storage, ss_keyboard) + \
	 sizeof(struct device_path))

/*
 * Table for the possible console devices found during the device walk.
 */
struct consoledev {
	struct device_path dp;
	int	type;
};

#define	PS2		1
#define	HIL		2
#define	SERIAL		3
#define	GRAPHICS	4

/* max. 2 serial ports (PDC compatibility) */
#define	MAX_SERIALS	2
/* max. HIL and PS2 */
#define	MAX_KEYBOARDS	2
/* max. 3 heads (PDC compatibility) */
#define	MAX_GRAPHICS	3

struct consoledev serials[MAX_SERIALS];
struct consoledev keyboards[MAX_KEYBOARDS];
struct consoledev graphics[MAX_GRAPHICS];

/* Relaxed device comparison */
#define	MATCH(dev1, dev2) \
	(dev1).dp_mod == (dev2).dp_mod && \
	(dev1).dp_bc[4] == (dev2).dp_bc[4] && \
	(dev1).dp_bc[5] == (dev2).dp_bc[5]

int walked;

void bus_walk __P((int));
void register_device __P((struct consoledev *, int,
			  struct device_path *, int, int));

int Xconsole __P((void));
void print_console __P((void));
int set_graphics __P((struct device_path *, int, char *));
int set_serial __P((struct device_path *, int, char *));
int set_console __P((struct device_path *));

int Xkeyboard __P((void));
void print_keyboard __P((void));
int set_keyboard __P((struct device_path *));

struct cmd_table cmd_machine[] = {
	{ "console",	CMDT_CMD,	Xconsole },
	{ "keyboard",	CMDT_CMD,	Xkeyboard },
	{ NULL, },
};

/* value to console speed table */
int i_speeds[] = {
	50,
	75,
	110,
	150,
	300,
	600,
	1200,
	2400,
	4800,
	7200,
	9600,
};

char* c_speeds[] = {
	"50",
	"75",
	"110",
	"150",
	"300",
	"600",
	"1200",
	"2400",
	"4800",
	"7200",
	"9600",
};

/* values to console parity table */
char *parities[] = {
	"none",
	"odd",	
	"<unknown parity>",
	"even",
};

/*
 * C O N S O L E   S E T T I N G S
 */

void
print_console()
{
	int port, mode, speed, parity, bits;
	int i;

#ifdef DEBUG
	printf("console flags %x mod %x bc %d/%d/%d/%d/%d/%d\n",
	    sstor.ss_console.dp_flags,
	    sstor.ss_console.dp_mod,
	    sstor.ss_console.dp_bc[0],
	    sstor.ss_console.dp_bc[1],
	    sstor.ss_console.dp_bc[2],
	    sstor.ss_console.dp_bc[3],
	    sstor.ss_console.dp_bc[4],
	    sstor.ss_console.dp_bc[5]);

	printf("console path %x/%x/%x/%x/%x/%x\n",
	    sstor.ss_console.dp_layers[0],
	    sstor.ss_console.dp_layers[1],
	    sstor.ss_console.dp_layers[2],
	    sstor.ss_console.dp_layers[3],
	    sstor.ss_console.dp_layers[4],
	    sstor.ss_console.dp_layers[5]);
#endif

	printf("Console path: ");

	/* look for a serial console */
	for (port = i = 0; i < MAX_SERIALS; i++)
		if (MATCH(serials[i].dp, sstor.ss_console)) {
			port = i + 1;
			break;
		}

	if (port == 0) {
		/*
		 * Graphics console
		 */

		for (port = i = 0; i < MAX_GRAPHICS; i++)
			if (MATCH(graphics[i].dp, sstor.ss_console)) {
				port = i;
				break;
			}

		/*
		 * If the console could still not be identified, consider
		 * it is a simplified encoding for teh default graphics
		 * console. Hence port == 0, no need to check.
		 */
		if (port == 0)
			printf("graphics");
		else
			printf("graphics_%d", port);

		mode = sstor.ss_console.dp_layers[0];
		if (mode != 0)
			printf(".%d", mode);
	} else {
		/*
		 * Serial console
		 */

		printf("rs232%s",
		    port == 2 ? "_2" : "");

		speed = PZL_SPEED(sstor.ss_console.dp_layers[0]);
		if (speed < NENTS(i_speeds)) {
			speed = i_speeds[speed];
			printf(".%d", speed);
		} else
			printf(".<unknown speed>");

		bits = PZL_BITS(sstor.ss_console.dp_layers[0]);
		printf(".%d", bits);

		parity = PZL_PARITY(sstor.ss_console.dp_layers[0]);
		printf(".%s", parities[parity]);
	}

	printf("\n");
}

int
set_graphics(console, port, arg)
	struct device_path *console;
	int port;
	char *arg;
{
	int maxmode, mode = 0;
	char *digit;

	/* head */
	if (graphics[port].type == 0) {
		printf("no such device found\n");
		return 0;
	}

	/* mode */
	if (arg != NULL) {
		for (digit = arg; *digit != '\0'; digit++) {
			if (*digit >= '0' && *digit <= '9')
				mode = 10 * mode + (*digit - '0');
			else {
				printf("invalid mode specification, %s\n",
				    arg);
				return 0;
			}
		}

		if (mode <= 0) {
			printf("invalid mode specification, %s\n",
			    arg);
			return 0;
		}
	}

	/*
	 * If we are just changing the mode of the same graphics
	 * console, check that our mode is in the valid range.
	 */
	if (MATCH(graphics[port].dp, sstor.ss_console)) {
		maxmode = sstor.ss_console.dp_layers[1];

		/* pick back same mode if unspecified */
		if (mode == 0)
			mode = sstor.ss_console.dp_layers[0];

		if (mode > maxmode) {
			printf("invalid mode value, available range is 1-%d\n",
			    maxmode);
			return 0;
		}
	} else {
		if (mode == 0)
			mode = 1;
		maxmode = mode;
	}

	*console = graphics[port].dp;
	console->dp_layers[0] = mode;
	console->dp_layers[1] = maxmode;
	console->dp_layers[2] = console->dp_layers[3] =
	console->dp_layers[4] = console->dp_layers[5] = 0;

	return 1;
}

int
set_serial(console, port, arg)
	struct device_path *console;
	int port;
	char *arg;
{
	char *dot;
	int i;
	int speed, parity, bits;

	/* port */
	port--;
	if (serials[port].type == 0) {
		printf("no such device found\n");
		return 0;
	}

	/* speed */
	dot = strchr(arg, '.');
	if (dot != NULL)
		*dot++ = '\0';

	if (arg == NULL || *arg == '\0') {
		speed = 0;	/* kill warning */
		for (i = 0; i < NENTS(i_speeds); i++)
			if (i_speeds[i] == 9600) {
				speed = i;
				break;
			}
	} else {
		speed = 0;
		for (i = 0; i < NENTS(c_speeds); i++)
			if (strcmp(arg, c_speeds[i]) == 0) {
				speed = i;
				break;
			}
		if (speed == 0) {
			printf("invalid speed specification, %s\n", arg);
			return 0;
		}
	}

	/* data bits */
	arg = dot;
	dot = strchr(arg, '.');

	if (arg == NULL || *arg == '\0')
		bits = 8;
	else {
		if (dot == arg + 1)
			bits = *arg - '0';
		else
			bits = 0;

		if (bits < 5 || bits > 8) {
			printf("invalid bits specification, %s\n", arg);
			return 0;
		}
	}
	if (dot != NULL)
		*dot++ = '\0';

	/* parity */
	arg = dot;
	if (arg == NULL || *arg == '\0')
		parity = 0;	/* none */
	else {
		parity = -1;
		for (i = 0; i <= 3; i++)
			if (strcmp(arg, parities[i]) == 0) {
				parity = i;
				break;
			}
		if (parity == 2)
			parity = -1;	/* unknown parity */
	}
	if (parity < 0) {
		printf("invalid parity specification, %s\n", arg);
		return 0;
	}

	*console = serials[port].dp;
	console->dp_layers[0] = PZL_ENCODE(bits, parity, speed);

	return 1;
}

int
set_console(console)
	struct device_path *console;
{
	char *arg = cmd.argv[1], *dot;

	/* extract first word */
	dot = strchr(arg, '.');
	if (dot != NULL)
		*dot++ = '\0';

	/*
	 * Graphics console
	 */
	if (strcmp(arg, "graphics") == 0)
		return set_graphics(console, 0, dot);

	if (strcmp(arg, "graphics_1") == 0)
		return set_graphics(console, 1, dot);
	if (strcmp(arg, "graphics_2") == 0)
		return set_graphics(console, 2, dot);

	/*
	 * Serial console
	 */
	if (strcmp(arg, "rs232") == 0)
		return set_serial(console, 1, dot);

	if (strcmp(arg, "rs232_2") == 0)
		return set_serial(console, 2, dot);

	printf("invalid device specification, %s\n", arg);
	return 0;
}

int
Xconsole()
{
	struct device_path console;
	int rc;

	/* walk the device list if not already done */
	if (walked == 0) {
		bus_walk(-1);
		walked++;
	}

	if (sstorsiz < CONSOLESIZE) {
		printf("no console information in stable storage\n");
		return 0;
	}

	if (cmd.argc == 1) {
		print_console();
	} else {
		console = sstor.ss_console;
		if (set_console(&console)) {
			if (memcmp(&sstor.ss_console, &console,
			    sizeof console) != 0) {
				sstor.ss_console = console;

				/* alea jacta est */
				rc = (*pdc)(PDC_STABLE, PDC_STABLE_WRITE,
				    CONSOLEOFFSET, &sstor.ss_console,
				    sizeof(sstor.ss_console));
				if (rc != 0) {
					printf("failed to save console settings, error %d\n",
					    rc);
					/* read sstor again for safety */
					(*pdc)(PDC_STABLE, PDC_STABLE_READ,
					    CONSOLEOFFSET, &sstor.ss_console,
					    sizeof(sstor.ss_console));
				} else
					printf("you will need to power-cycle "
					       "your machine for the changes "
					       "to take effect.\n");
			}
			print_console();
		}
	}

	return 0;
}

/*
 * K E Y B O A R D   S E T T I N G S
 */

void
print_keyboard()
{
	int type;
	int i;

#ifdef DEBUG
	printf("keyboard flags %x mod %x bc %d/%d/%d/%d/%d/%d\n",
	    sstor.ss_keyboard.dp_flags,
	    sstor.ss_keyboard.dp_mod,
	    sstor.ss_keyboard.dp_bc[0],
	    sstor.ss_keyboard.dp_bc[1],
	    sstor.ss_keyboard.dp_bc[2],
	    sstor.ss_keyboard.dp_bc[3],
	    sstor.ss_keyboard.dp_bc[4],
	    sstor.ss_keyboard.dp_bc[5]);

	printf("keyboard path %x/%x/%x/%x/%x/%x\n",
	    sstor.ss_keyboard.dp_layers[0],
	    sstor.ss_keyboard.dp_layers[1],
	    sstor.ss_keyboard.dp_layers[2],
	    sstor.ss_keyboard.dp_layers[3],
	    sstor.ss_keyboard.dp_layers[4],
	    sstor.ss_keyboard.dp_layers[5]);
#endif

	printf("Keyboard path: ");

	for (type = i = 0; i < MAX_KEYBOARDS; i++)
		if (MATCH(keyboards[i].dp, sstor.ss_keyboard)) {
			type = keyboards[i].type;
			break;
		}

	switch (type) {
	case HIL:
		printf("hil");
		break;
	case PS2:
		printf("ps2");
		break;
	default:
		printf("unknown");
		break;
	}

	printf("\n");
}

int
set_keyboard(keyboard)
	struct device_path *keyboard;
{
	int i;
	char *arg = cmd.argv[1];
	int type;

	if (strcmp(arg, "hil") == 0)
		type = HIL;
	else if (strcmp(arg, "ps2") == 0)
		type = PS2;
	else {
		printf("invalid device specification, %s\n", arg);
		return 0;
	}

	for (i = 0; i < MAX_KEYBOARDS; i++)
		if (keyboards[i].type == type) {
			*keyboard = keyboards[i].dp;
			return 1;
		}

	printf("no such device found\n");
	return 0;
}

int
Xkeyboard()
{
	struct device_path keyboard;
	int rc;

	/* walk the device list if not already done */
	if (walked == 0) {
		bus_walk(-1);
		walked++;
	}

	if (sstorsiz < KEYBOARDSIZE) {
		printf("no keyboard information in stable storage\n");
		return 0;
	}

	if (cmd.argc == 1) {
		print_keyboard();
	} else {
		keyboard = sstor.ss_keyboard;
		if (set_keyboard(&keyboard)) {
			if (memcmp(&sstor.ss_keyboard, &keyboard,
			    sizeof keyboard) != 0) {
				sstor.ss_keyboard = keyboard;

				/* alea jacta est */
				rc = (*pdc)(PDC_STABLE, PDC_STABLE_WRITE,
				    KEYBOARDOFFSET, &sstor.ss_keyboard,
				    sizeof(sstor.ss_keyboard));
				if (rc != 0) {
					printf("failed to save keyboard settings, error %d\n",
					    rc);
					/* read sstor again for safety */
					(*pdc)(PDC_STABLE, PDC_STABLE_READ,
					    KEYBOARDOFFSET, &sstor.ss_keyboard,
					    sizeof(sstor.ss_keyboard));
				} else
					printf("you will need to power-cycle "
					       "your machine for the changes "
					       "to take effect.\n");
			}
			print_keyboard();
		}
	}

	return 0;
}

/*
 * U T I L I T I E S
 */

/*
 * Bus walker.
 * This routine will walk all the modules on a given bus, registering
 * serial ports, keyboard and graphics devices as they are found.
 */
void
bus_walk(bus)
	int bus;
{
	struct device_path dp;
	struct pdc_memmap memmap;
	struct iodc_data mptr;
	struct iomod *io;
	iodcio_t iodc;
	int err;
	int i;
	int kluge_ps2 = 0;	/* kluge, see below */

	iodc = (iodcio_t)(PAGE0->mem_free + IODC_MAXSIZE);
		
	for (i = 0; i < MAXMODBUS; i++) {
		dp.dp_bc[0] = dp.dp_bc[1] =
		dp.dp_bc[2] = dp.dp_bc[3] = -1;
		dp.dp_bc[4] = bus;
		dp.dp_bc[5] = bus < 0 ? -1 : 0;
		dp.dp_mod = i;
		if ((pdc)(PDC_MEMMAP, PDC_MEMMAP_HPA, &memmap,
			  &dp) < 0)
			continue;

		io = (struct iomod *) memmap.hpa;

		if ((err = (pdc)(PDC_IODC, PDC_IODC_READ, &pdcbuf, io,
			   IODC_DATA, &mptr, sizeof(mptr))) < 0) {
			continue;
		}

#ifdef DEBUG
		printf("device %d/%d flags %d mod %x type %x model %x\n",
		    dp.dp_bc[4], dp.dp_bc[5],
		    dp.dp_flags, dp.dp_mod,
		    mptr.iodc_type, mptr.iodc_sv_model);
#endif

		/*
		 * If the device can be considered as a valid rs232,
		 * graphics console or keyboard, register it.
		 *
		 * GIO devices are registered as ``main'' devices, since
		 * in the PDC's view of the world, they are, yet other
		 * devices can be found before them...
		 */
		switch (mptr.iodc_type) {
		case HPPA_TYPE_ADIRECT:
			switch (mptr.iodc_sv_model) {
			case HPPA_ADIRECT_RS232:
				register_device(serials, MAX_SERIALS,
				    &dp, SERIAL, 0);
				break;
			case HPPA_ADIRECT_HIL:
				register_device(keyboards, MAX_KEYBOARDS,
				    &dp, HIL, 0);
				break;
			case HPPA_ADIRECT_PEACOCK:
			case HPPA_ADIRECT_LEONARDO:
				register_device(graphics, MAX_GRAPHICS,
				    &dp, GRAPHICS, 0);
				break;
			}
			break;
		case HPPA_TYPE_FIO:
			switch (mptr.iodc_sv_model) {
			case HPPA_FIO_HIL:
				register_device(keyboards, MAX_KEYBOARDS,
				    &dp, HIL, 0);
				break;
			case HPPA_FIO_RS232:
				register_device(serials, MAX_SERIALS,
				    &dp, SERIAL, 0);
				break;
			case HPPA_FIO_DINOPCK:
				register_device(keyboards, MAX_KEYBOARDS,
				    &dp, PS2, 0);
				break;
			case HPPA_FIO_GPCIO:
				/*
				 * KLUGE! At this point, there is no way to
				 * know if this port is the keyboard port or
				 * the mouse port.
				 * Let's assume the first port found is the
				 * keyboard, and ignore the others.
				 */
				if (kluge_ps2 != 0)
					break;
				register_device(keyboards, MAX_KEYBOARDS,
				    &dp, PS2, 1);
				kluge_ps2++;
				break;
			case HPPA_FIO_GRS232:
				register_device(serials, MAX_SERIALS,
				    &dp, SERIAL, 1);
				break;
			case HPPA_FIO_SGC:
				register_device(graphics, MAX_GRAPHICS,
				    &dp, GRAPHICS, 0);
				break;
			case HPPA_FIO_GSGC:
				register_device(graphics, MAX_GRAPHICS,
				    &dp, GRAPHICS, 1);
				break;
			}
			break;
		}
	}

	/*
	 * Do a second pass to scan all child busses
	 */
	for (i = 0; i < MAXMODBUS; i++) {
		dp.dp_bc[0] = dp.dp_bc[1] =
		dp.dp_bc[2] = dp.dp_bc[3] = -1;
		dp.dp_bc[4] = bus;
		dp.dp_bc[5] = bus < 0 ? -1 : 0;
		dp.dp_mod = i;
		if ((pdc)(PDC_MEMMAP, PDC_MEMMAP_HPA, &memmap,
			  &dp) < 0)
			continue;

		io = (struct iomod *) memmap.hpa;

		if ((err = (pdc)(PDC_IODC, PDC_IODC_READ, &pdcbuf, io,
			   IODC_DATA, &mptr, sizeof(mptr))) < 0) {
			continue;
		}

		switch (mptr.iodc_type) {
		case HPPA_TYPE_BHA:
		case HPPA_TYPE_BRIDGE:
			bus_walk(dp.dp_mod);
			break;
		}
	}
}

void
register_device(devlist, cnt, dp, type, first) 
	struct consoledev *devlist;
	int cnt;
	struct device_path *dp;
	int type;
	int first;
{
	int i;
	struct consoledev *dev;

	for (i = 0, dev = devlist; i < cnt; i++, dev++)
		if (dev->type == 0)
			break;

	if (i == cnt) {
#ifdef DEBUG
		printf("can't register device, need more room!\n");
#endif
		return;
	}

	/*
	 * If this is supposedly the main device, insert on top
	 */
	if (first != 0) {
		memcpy(devlist + 1, devlist,
		    (cnt - 1) * sizeof(struct consoledev));
		dev = devlist;
	}

	dev->dp = *dp;
	dev->type = type;

#ifdef DEBUG
	printf("(registered as type %d)\n", type);
#endif
}
