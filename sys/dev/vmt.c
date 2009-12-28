/*	$OpenBSD: vmt.c,v 1.7 2009/12/28 14:25:34 dlg Exp $ */

/*
 * Copyright (c) 2007 David Crawshaw <david@zentus.com>
 * Copyright (c) 2008 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(__i386__) && !defined(__amd64__)
#error vmt(4) is only supported on i386 and amd64
#endif

#include <dev/vmtvar.h>

/*
 * Protocol reverse engineered by Ken Kato:
 * http://chitchat.at.infoseek.co.jp/vmware/backdoor.html
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/timeout.h>

/* "The" magic number, always occupies the EAX register. */
#define VM_MAGIC			0x564D5868

/* Port numbers, passed on EDX.LOW . */
#define VM_PORT_CMD			0x5658
#define VM_PORT_RPC			0x5659

/* Commands, passed on ECX.LOW. */
#define VM_CMD_GET_SPEED		0x01
#define VM_CMD_APM			0x02
#define VM_CMD_GET_MOUSEPOS		0x04
#define VM_CMD_SET_MOUSEPOS		0x05
#define VM_CMD_GET_CLIPBOARD_LEN	0x06
#define VM_CMD_GET_CLIPBOARD		0x07
#define VM_CMD_SET_CLIPBOARD_LEN	0x08
#define VM_CMD_SET_CLIPBOARD		0x09
#define VM_CMD_GET_VERSION		0x0a
#define  VM_VERSION_UNMANAGED			0x7fffffff
#define VM_CMD_GET_DEVINFO		0x0b
#define VM_CMD_DEV_ADDREMOVE		0x0c
#define VM_CMD_GET_GUI_OPTIONS  	0x0d
#define VM_CMD_SET_GUI_OPTIONS		0x0e
#define VM_CMD_GET_SCREEN_SIZE		0x0f
#define VM_CMD_GET_HWVER		0x11
#define VM_CMD_POPUP_OSNOTFOUND		0x12
#define VM_CMD_GET_BIOS_UUID		0x13
#define VM_CMD_GET_MEM_SIZE		0x14
#define VM_CMD_GET_TIME			0x17
#define VM_CMD_RPC			0x1e

/* RPC sub-commands, passed on ECX.HIGH. */
#define VM_RPC_OPEN			0x00
#define VM_RPC_SET_LENGTH		0x01
#define VM_RPC_SET_DATA			0x02
#define VM_RPC_GET_LENGTH		0x03
#define VM_RPC_GET_DATA			0x04
#define VM_RPC_GET_END			0x05
#define VM_RPC_CLOSE			0x06

/* RPC magic numbers, passed on EBX. */
#define VM_RPC_OPEN_RPCI  	0x49435052UL /* with VM_RPC_OPEN. */
#define VM_RPC_OPEN_RPCI_ENH  	0xC9435052UL /* with VM_RPC_OPEN, enhanced. */
#define VM_RPC_ENH_DATA  	0x00010000UL /* with enhanced RPC data calls. */

/* A register. */
union vm_reg {
	struct {
		uint16_t low;
		uint16_t high;
	} part;
	uint32_t word;
#ifdef __amd64__
	struct {
		uint32_t low;
		uint32_t high;
	} words;
	uint64_t quad;
#endif
} __packed;

/* A register frame. */
struct vm_backdoor {
	union vm_reg eax;
	union vm_reg ebx;
	union vm_reg ecx;
	union vm_reg edx;
	union vm_reg esi;
	union vm_reg edi;
	union vm_reg ebp;
} __packed;

/* RPC context. */
struct vm_rpc {
	uint16_t channel;
	uint32_t cookie1;
	uint32_t cookie2;
};

int	vmt_match(struct device *, void *, void *);
void	vmt_attach(struct device *, struct device *, void *);

struct vmt_softc {
	struct device		sc_dev;

	struct vm_rpc		sc_rpc;
	char			*sc_rpc_buf;
#define VMT_RPC_BUFLEN			256

	struct timeout		sc_tick;
	struct ksensordev	sc_sensordev;
	struct ksensor		sc_sensor;

	char			sc_hostname[MAXHOSTNAMELEN];
};
#define DEVNAME(_s)	((_s)->sc_dev.dv_xname)

struct cfattach vmt_ca = {
	sizeof(struct vmt_softc),
	vmt_match,
	vmt_attach
};
 
struct cfdriver vmt_cd = {
	NULL,
	"vmt",
	DV_DULL
};

void vm_cmd(struct vm_backdoor *);
void vm_ins(struct vm_backdoor *);
void vm_outs(struct vm_backdoor *);

/* Functions for communicating with the VM Host. */
int vm_rpc_open(struct vm_rpc *);
int vm_rpc_close(struct vm_rpc *);
int vm_rpc_send(const struct vm_rpc *, const uint8_t *, uint32_t);
int vm_rpc_get_length(const struct vm_rpc *, uint32_t *, uint16_t *);
int vm_rpc_get_data(const struct vm_rpc *, char *, uint32_t, uint16_t);

void vmt_tick(void *);

extern char hostname[MAXHOSTNAMELEN];

int
vmt_probe(void)
{
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));

	frame.eax.word = VM_MAGIC;
	frame.ebx.word = ~VM_MAGIC;
	frame.ecx.part.low = VM_CMD_GET_VERSION;
	frame.ecx.part.high = 0xffff;
	frame.edx.part.low  = VM_PORT_CMD;
	frame.edx.part.high = 0;

	vm_cmd(&frame);

	if (frame.eax.word == 0xffffffff ||
	    frame.ebx.word != VM_MAGIC)
		return (0);

	return (1);
}

int
vmt_match(struct device *parent, void *match, void *aux)
{
	/* we cant get here unless vmt_probe previously succeeded */
	return (1);
}

void
vmt_attach(struct device *parent, struct device *self, void *aux)
{
	struct vmt_softc *sc = (struct vmt_softc *)self;
	size_t len;
	u_int32_t rlen;
	u_int16_t ack;

	sc->sc_rpc_buf = malloc(VMT_RPC_BUFLEN, M_DEVBUF, M_NOWAIT);
	if (sc->sc_rpc_buf == NULL) {
		printf(": unable to allocate buffer for RPC\n");
		return;
	}

	if (vm_rpc_open(&sc->sc_rpc) != 0) {
		printf(": failed to open backdoor RPC channel\n");
		goto free;
	}

	len = snprintf(sc->sc_rpc_buf, VMT_RPC_BUFLEN, "tools.set.version %u ",
	    VM_VERSION_UNMANAGED);
#ifdef DIAGNOSTIC
	if (len > VMT_RPC_BUFLEN)
		panic("vmt rpc buffer is too small");
#endif

	if (vm_rpc_send(&sc->sc_rpc, sc->sc_rpc_buf, len) != 0) {
		printf("%s: failed to send version\n", DEVNAME(sc));
		return;
	}

	if (vm_rpc_get_length(&sc->sc_rpc, &rlen, &ack) != 0) {
		printf("%s: failed to get length of version reply\n",
		    DEVNAME(sc));
		return;
	}

	if (rlen > VMT_RPC_BUFLEN) {
		printf("%s: reply is too large for version buffer\n",
		    DEVNAME(sc));
		return;
	}

	bzero(sc->sc_rpc_buf, VMT_RPC_BUFLEN);
	if (vm_rpc_get_data(&sc->sc_rpc, sc->sc_rpc_buf, rlen, ack) != 0) {
		printf("%s: failed to get version reply\n", DEVNAME(sc));
		return;
	}

	if (sc->sc_rpc_buf[0] != '1' && sc->sc_rpc_buf[1] != ' ') {
		printf("%s: setting version failed\n", DEVNAME(sc));
		return;
	}

	strlcpy(sc->sc_sensordev.xname, sc->sc_dev.dv_xname,
	    sizeof(sc->sc_sensordev.xname));

	sc->sc_sensor.type = SENSOR_TIMEDELTA;
	sc->sc_sensor.status = SENSOR_S_UNKNOWN;

	sensor_attach(&sc->sc_sensordev, &sc->sc_sensor);
	sensordev_install(&sc->sc_sensordev);

	timeout_set(&sc->sc_tick, vmt_tick, sc);
	timeout_add_sec(&sc->sc_tick, 1);

	printf("\n");
	return;

free:
	free(sc->sc_rpc_buf, M_DEVBUF);
}

void
vmt_tick(void *xarg)
{
	struct vmt_softc *sc = xarg;
	struct vm_backdoor frame;
	struct timeval *guest = &sc->sc_sensor.tv;
	struct timeval host, diff;

	size_t len;
	u_int32_t rlen;
	u_int16_t ack;

	microtime(guest);

	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ecx.part.low = VM_CMD_GET_TIME;
	frame.edx.part.low  = VM_PORT_CMD;
	vm_cmd(&frame);

	if (frame.eax.word == 0xffffffff) {
		sc->sc_sensor.status = SENSOR_S_UNKNOWN;
		goto out;
	}

	host.tv_sec = frame.eax.word;
	host.tv_usec = frame.ebx.word;

	timersub(guest, &host, &diff);

	sc->sc_sensor.value = (u_int64_t)diff.tv_sec * 1000000000LL +
	    (u_int64_t)diff.tv_usec * 1000LL;
	sc->sc_sensor.status = SENSOR_S_OK;

	if (strncmp(sc->sc_hostname, hostname, sizeof(sc->sc_hostname)) != 0) {
		strlcpy(sc->sc_hostname, hostname, sizeof(sc->sc_hostname));

		len = snprintf(sc->sc_rpc_buf, VMT_RPC_BUFLEN,
		    "info-set guestinfo.ip %s ", "192.168.1.1");
#ifdef DIAGNOSTIC
		if (len > VMT_RPC_BUFLEN)
			panic("vmt rpc buffer is too small");
#endif

		if (vm_rpc_send(&sc->sc_rpc, sc->sc_rpc_buf, len) != 0) {
			goto out;
		}

		if (vm_rpc_get_length(&sc->sc_rpc, &rlen, &ack) != 0) {
			goto out;
		}

		bzero(sc->sc_rpc_buf, VMT_RPC_BUFLEN);
		if (vm_rpc_get_data(&sc->sc_rpc, sc->sc_rpc_buf,
		    rlen, ack) != 0) {
			return;
		}

		if (sc->sc_rpc_buf[0] != '1' && sc->sc_rpc_buf[1] != ' ') {
			printf("%s: setting hostname failed\n", DEVNAME(sc));
			return;
		}

		printf("%s: hostname set to %s\n", DEVNAME(sc),
		    sc->sc_hostname);
	}

	if (vm_rpc_get_length(&sc->sc_rpc, &rlen, &ack) != 0) {
		printf("%s: failed to get length of version reply\n",
		    DEVNAME(sc));
		return;
	}

	if (rlen == 0)
		goto out;

	if (vm_rpc_get_data(&sc->sc_rpc, sc->sc_rpc_buf, rlen, ack) != 0) {
		printf("%s: failed to get version reply\n", DEVNAME(sc));
		return;
	}

	printf("%s: \"%s\"\n", DEVNAME(sc), sc->sc_rpc_buf);

out:
	timeout_add_sec(&sc->sc_tick, 15);
}

#define BACKDOOR_OP_I386(op, frame)		\
	__asm__ __volatile__ (			\
		"pushal;"			\
		"pushl %%eax;"			\
		"movl 0x18(%%eax), %%ebp;"	\
		"movl 0x14(%%eax), %%edi;"	\
		"movl 0x10(%%eax), %%esi;"	\
		"movl 0x0c(%%eax), %%edx;"	\
		"movl 0x08(%%eax), %%ecx;"	\
		"movl 0x04(%%eax), %%ebx;"	\
		"movl 0x00(%%eax), %%eax;"	\
		op				\
		"xchgl %%eax, 0x00(%%esp);"	\
		"movl %%ebp, 0x18(%%eax);"	\
		"movl %%edi, 0x14(%%eax);"	\
		"movl %%esi, 0x10(%%eax);"	\
		"movl %%edx, 0x0c(%%eax);"	\
		"movl %%ecx, 0x08(%%eax);"	\
		"movl %%ebx, 0x04(%%eax);"	\
		"popl 0x00(%%eax);"		\
		"popal;"			\
		::"a"(frame)			\
	)

#define BACKDOOR_OP_AMD64(op, frame)		\
	__asm__ __volatile__ (			\
		"pushq %%rbp;			\n\t" \
		"pushq %%rax;			\n\t" \
		"movq 0x30(%%rax), %%rbp;	\n\t" \
		"movq 0x28(%%rax), %%rdi;	\n\t" \
		"movq 0x20(%%rax), %%rsi;	\n\t" \
		"movq 0x18(%%rax), %%rdx;	\n\t" \
		"movq 0x10(%%rax), %%rcx;	\n\t" \
		"movq 0x08(%%rax), %%rbx;	\n\t" \
		"movq 0x00(%%rax), %%rax;	\n\t" \
		op				"\n\t" \
		"xchgq %%rax, 0x00(%%rsp);	\n\t" \
		"movq %%rbp, 0x30(%%rax);	\n\t" \
		"movq %%rdi, 0x28(%%rax);	\n\t" \
		"movq %%rsi, 0x20(%%rax);	\n\t" \
		"movq %%rdx, 0x18(%%rax);	\n\t" \
		"movq %%rcx, 0x10(%%rax);	\n\t" \
		"movq %%rbx, 0x08(%%rax);	\n\t" \
		"popq 0x00(%%rax);		\n\t" \
		"popq %%rbp;			\n\t" \
		: /* No outputs. */ : "a" (frame) \
		  /* No pushal on amd64 so warn gcc about the clobbered registers. */ \
		: "rbx", "rcx", "rdx", "rdi", "rsi", "cc", "memory" \
	)


#ifdef __i386__
#define BACKDOOR_OP(op, frame) BACKDOOR_OP_I386(op, frame)
#else
#define BACKDOOR_OP(op, frame) BACKDOOR_OP_AMD64(op, frame)
#endif

void
vm_cmd(struct vm_backdoor *frame)
{
	BACKDOOR_OP("inl %%dx, %%eax;", frame);
}

void
vm_ins(struct vm_backdoor *frame)
{
	BACKDOOR_OP("cld;\n\trep insb;", frame);
}

void
vm_outs(struct vm_backdoor *frame)
{
	BACKDOOR_OP("cld;\n\trep outsb;", frame);
}

int
vm_rpc_open(struct vm_rpc *rpc)
{
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));
	frame.eax.word      = VM_MAGIC;
	frame.ebx.word      = VM_RPC_OPEN_RPCI_ENH;
	frame.ecx.part.low  = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_OPEN;
	frame.edx.part.low  = VM_PORT_CMD;
	frame.edx.part.high = 0;

	vm_cmd(&frame);

	if (frame.ecx.part.high != 1 || frame.edx.part.low != 0) {
		printf("vmware: open failed, eax=%08x, ecx=%08x, edx=%08x\n",
			frame.eax.word, frame.ecx.word, frame.edx.word);
		return EIO;
	}

	rpc->channel = frame.edx.part.high;
	rpc->cookie1 = frame.esi.word;
	rpc->cookie2 = frame.edi.word;

	return 0;
}

int
vm_rpc_close(struct vm_rpc *rpc)
{
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));
	frame.eax.word      = VM_MAGIC;
	frame.ebx.word      = 0;
	frame.ecx.part.low  = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_CLOSE;
	frame.edx.part.low  = VM_PORT_CMD;
	frame.edx.part.high = rpc->channel;
	frame.edi.word      = rpc->cookie2;
	frame.esi.word      = rpc->cookie1;

	vm_cmd(&frame);

	if (frame.ecx.part.high == 0 || frame.ecx.part.low != 0) {
		printf("vmware: close failed, eax=%08x, ecx=%08x\n",
				frame.eax.word, frame.ecx.word);
		return EIO;
	}

	rpc->channel = 0;
	rpc->cookie1 = 0;
	rpc->cookie2 = 0;

	return 0;
}

int
vm_rpc_send(const struct vm_rpc *rpc, const uint8_t *buf, uint32_t length)
{
	struct vm_backdoor frame;

	/* Send the length of the command. */
	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = length;
	frame.ecx.part.low  = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_SET_LENGTH;
	frame.edx.part.low  = VM_PORT_CMD;
	frame.edx.part.high = rpc->channel;
	frame.esi.word = rpc->cookie1;
	frame.edi.word = rpc->cookie2;

	vm_cmd(&frame);

	if (frame.ecx.part.high == 0) {
		printf("vmware: sending length failed, eax=%08x, ecx=%08x\n",
				frame.eax.word, frame.ecx.word);
		return EIO;
	}

	if (length == 0)
		return 0; /* Only need to poke once if command is null. */

	/* Send the command using enhanced RPC. */
	bzero(&frame, sizeof(frame));
	frame.eax.word = VM_MAGIC;
	frame.ebx.word = VM_RPC_ENH_DATA;
	frame.ecx.word = length;
	frame.edx.part.low  = VM_PORT_RPC;  // XXX we are here
	frame.edx.part.high = rpc->channel;
	frame.ebp.word = rpc->cookie1;
	frame.edi.word = rpc->cookie2;
#ifdef __amd64__
	frame.esi.quad = (uint64_t)buf;
#else
	frame.esi.word = (uint32_t)buf;
#endif

	vm_outs(&frame);

	if (frame.ebx.word != VM_RPC_ENH_DATA) {
		printf("vmware: send failed, ebx=%08x\n", frame.ebx.word);
		return EIO;
	}

	return 0;
}

int
vm_rpc_get_data(const struct vm_rpc *rpc, char *data, uint32_t length,
    uint16_t dataid)
{
	struct vm_backdoor frame;

	/* Get data using enhanced RPC. */
	bzero(&frame, sizeof(frame));
	frame.eax.word      = VM_MAGIC;
	frame.ebx.word      = VM_RPC_ENH_DATA;
	frame.ecx.word      = length;
	frame.edx.part.low  = VM_PORT_RPC;
	frame.edx.part.high = rpc->channel;
	frame.esi.word      = rpc->cookie1;
#ifdef __amd64__
	frame.edi.quad      = (uint64_t)data;
#else
	frame.edi.word      = (uint32_t)data;
#endif
	frame.ebp.word      = rpc->cookie2;

	vm_ins(&frame);

	if (frame.ebx.word != VM_RPC_ENH_DATA) {
		printf("vmware: get data failed, ebx=%08x\n",
				frame.ebx.word);
		return EIO;
	}

	/* Acknowledge data received. */
	bzero(&frame, sizeof(frame));
	frame.eax.word      = VM_MAGIC;
	frame.ebx.word      = dataid;
	frame.ecx.part.low  = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_GET_END;
	frame.edx.part.low  = VM_PORT_CMD;
	frame.edx.part.high = rpc->channel;
	frame.esi.word      = rpc->cookie1;
	frame.edi.word      = rpc->cookie2;

	vm_cmd(&frame);

	if (frame.ecx.part.high == 0) {
		printf("vmware: ack data failed, eax=%08x, ecx=%08x\n",
				frame.eax.word, frame.ecx.word);
		return EIO;
	}

	return 0;
}

int
vm_rpc_get_length(const struct vm_rpc *rpc, uint32_t *length, uint16_t *dataid)
{
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));
	frame.eax.word      = VM_MAGIC;
	frame.ebx.word      = 0;
	frame.ecx.part.low  = VM_CMD_RPC;
	frame.ecx.part.high = VM_RPC_GET_LENGTH;
	frame.edx.part.low  = VM_PORT_CMD;
	frame.edx.part.high = rpc->channel;
	frame.esi.word      = rpc->cookie1;
	frame.edi.word      = rpc->cookie2;

	vm_cmd(&frame);

	if (frame.ecx.part.high == 0) {
		printf("vmware: get length failed, eax=%08x, ecx=%08x\n",
				frame.eax.word, frame.ecx.word);
		return EIO;
	}

	*length = frame.ebx.word;
	*dataid = frame.edx.part.high;

	return 0;
}


#if 0
	struct vm_backdoor frame;

	bzero(&frame, sizeof(frame));

	frame.eax.word = VM_MAGIC;
	frame.ecx.part.low = VM_CMD_GET_VERSION;
	frame.edx.part.low  = VM_PORT_CMD;

	printf("\n");
	printf("eax 0x%08x\n", frame.eax.word);
	printf("ebx 0x%08x\n", frame.ebx.word);
	printf("ecx 0x%08x\n", frame.ecx.word);
	printf("edx 0x%08x\n", frame.edx.word);
	printf("ebp 0x%08x\n", frame.ebp.word);
	printf("edi 0x%08x\n", frame.edi.word);
	printf("esi 0x%08x\n", frame.esi.word);

	vm_cmd(&frame);

	printf("-\n");
	printf("eax 0x%08x\n", frame.eax.word);
	printf("ebx 0x%08x\n", frame.ebx.word);
	printf("ecx 0x%08x\n", frame.ecx.word);
	printf("edx 0x%08x\n", frame.edx.word);
	printf("ebp 0x%08x\n", frame.ebp.word);
	printf("edi 0x%08x\n", frame.edi.word);
	printf("esi 0x%08x\n", frame.esi.word);
#endif
