/*	$OpenBSD: x86_mmio.c,v 1.6 2026/09/18 21:26:16 dv Exp $	*/
/*
 * Copyright (c) 2022 Dave Voutila <dv@openbsd.org>
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

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <machine/psl.h>
#include <machine/specialreg.h>

#include "vmd.h"
#include "mmio.h"
#include "pci.h"
#include "x86_mmio.h"
#include "x86_vm.h"

#ifdef DPRINTF
#undef DPRINTF
#endif
#if MMIO_DEBUG
static void dump_regs(struct vcpu_reg_state *);
static void dump_insn(struct x86_insn *);
#define DPRINTF			log_debug
#define DUMP_REGS		dump_regs
#define DUMP_INSN		dump_insn
#else
#define DPRINTF(x...)		do {} while(0)
#define DUMP_REGS(x...)		do {} while(0)
#define DUMP_INSN(x...)		do {} while(0)
#endif	/* MMIO_DEBUG */

extern char* __progname;

struct x86_decode_state {
	uint8_t	s_bytes[15];
	size_t	s_len;
	size_t	s_idx;
};

enum decode_result {
	DECODE_ERROR = 0,	/* Something went wrong. */
	DECODE_DONE,		/* Decode success and no more work needed. */
	DECODE_MORE,		/* Decode success and more work required. */
};

const char *str_cpu_mode(int);
const char *str_decode_res(enum decode_result);
const char *str_opcode(struct x86_opcode *);
const char *str_operand_enc(struct x86_opcode *);
const char *str_reg(int);
const char *str_sreg(int);

static enum decode_result decode_prefix(struct x86_decode_state *,
    struct x86_insn *);
static enum decode_result decode_opcode(struct x86_decode_state *,
    struct x86_insn *);
static enum decode_result decode_modrm(struct x86_decode_state *,
    struct x86_insn *);
static int get_modrm_reg(struct x86_insn *);
static int get_modrm_addr(struct x86_insn *, struct vcpu_reg_state *);
static enum decode_result decode_disp(struct x86_decode_state *,
    struct vcpu_reg_state *, struct x86_insn *);
static enum decode_result decode_sib(struct x86_decode_state *,
    struct vcpu_reg_state *, struct x86_insn *);
static enum decode_result decode_imm(struct x86_decode_state *,
    struct x86_insn *);
static int get_operand_size(struct x86_insn *);

static enum decode_result peek_byte(struct x86_decode_state *, uint8_t *);
static enum decode_result next_byte(struct x86_decode_state *, uint8_t *);
static enum decode_result next_value(struct x86_decode_state *, size_t,
    uint64_t *);
static int is_valid_state(struct x86_decode_state *, const char *);
__dead static void mmio_fatal(struct x86_insn *, struct vm_exit *, uint64_t);

static int emulate_add(struct x86_insn *, struct vm_exit *, uint32_t);
static void emulate_add_flags(struct vm_exit *, uint64_t, uint64_t, uint64_t,
    int);
static int emulate_and(struct x86_insn *, struct vm_exit *, uint32_t);
static int emulate_cmp(struct x86_insn *, struct vm_exit *, uint32_t);
static void emulate_logic_flags(struct vm_exit *, uint64_t, int);
static int emulate_mov(struct x86_insn *, struct vm_exit *, uint32_t);
static int emulate_movs(struct x86_insn *, struct vm_exit *, uint32_t);
static int emulate_movzx(struct x86_insn *, struct vm_exit *, uint32_t);
static int emulate_pop(struct x86_insn *, struct vm_exit *, uint32_t);
static int emulate_push(struct x86_insn *, struct vm_exit *, uint32_t);
static int emulate_sub(struct x86_insn *, struct vm_exit *, uint32_t);
static void emulate_sub_flags(struct vm_exit *, uint64_t, uint64_t, uint64_t,
    int);
static int emulate_test(struct x86_insn *, struct vm_exit *, uint32_t);

SLIST_HEAD(mmio_dev_head, mmio_dev) mmio_devs;

/* Lookup table for 1-byte opcodes, in opcode alphabetical order. */
const enum x86_opcode_type x86_1byte_opcode_tbl[256] = {
	/* ADD r/m to register */
	[0x03] = OP_ADD,
	/* AND r/m to register */
	[0x23] = OP_AND,
	/* SUB r/m from register */
	[0x2B] = OP_SUB,
	/* CMP r/m against register */
	[0x80] = OP_CMP,
	[0x81] = OP_CMP,
	[0x83] = OP_CMP,
	[0x3B] = OP_CMP,

	/* MOV */
	[0x88] = OP_MOV,
	[0x89] = OP_MOV,
	[0x8A] = OP_MOV,
	[0x8B] = OP_MOV,
	[0x8C] = OP_MOV,
	[0xA0] = OP_MOV,
	[0xA1] = OP_MOV,
	[0xA2] = OP_MOV,
	[0xA3] = OP_MOV,
	[0xC6] = OP_MOV,
	[0xC7] = OP_MOV,

	/* POP r/m (group 1A, /0) */
	[0x8F] = OP_POP,

	/* PUSH r/m (group 5, /6) */
	[0xFF] = OP_PUSH,

	/* TEST immediate against r/m */
	[0xF7] = OP_TEST,

	/* MOVS */
	[0xA4] = OP_MOVS,
	[0xA5] = OP_MOVS,

	[ESCAPE] = OP_TWO_BYTE,
};

/* Lookup table for 1-byte operand encodings, in opcode alphabetical order. */
const enum x86_operand_enc x86_1byte_operand_enc_tbl[256] = {
	/* ADD r/m to register */
	[0x03] = OP_ENC_RM,
	/* AND r/m to register */
	[0x23] = OP_ENC_RM,
	/* SUB r/m from register */
	[0x2B] = OP_ENC_RM,
	/* CMP r/m against register */
	[0x80] = OP_ENC_MI,
	[0x81] = OP_ENC_MI,
	[0x83] = OP_ENC_MI,
	[0x3B] = OP_ENC_RM,

	/* MOV */
	[0x88] = OP_ENC_MR,
	[0x89] = OP_ENC_MR,
	[0x8A] = OP_ENC_RM,
	[0x8B] = OP_ENC_RM,
	[0x8C] = OP_ENC_MR,
	[0xA0] = OP_ENC_FD,
	[0xA1] = OP_ENC_FD,
	[0xA2] = OP_ENC_TD,
	[0xA3] = OP_ENC_TD,
	[0xC6] = OP_ENC_MI,
	[0xC7] = OP_ENC_MI,

	/* POP r/m (group 1A, /0) */
	[0x8F] = OP_ENC_M,

	/* PUSH r/m (group 5, /6) */
	[0xFF] = OP_ENC_M,

	/* TEST immediate against r/m */
	[0xF7] = OP_ENC_MI,

	/* MOVS */
	[0xA4] = OP_ENC_ZO,
	[0xA5] = OP_ENC_ZO,
};

const enum x86_opcode_type x86_2byte_opcode_tbl[256] = {
	/* MOVZX */
	[0xB6] = OP_MOVZX,
	[0xB7] = OP_MOVZX,
};

const enum x86_operand_enc x86_2byte_operand_enc_table[256] = {
	/* MOVZX */
	[0xB6] = OP_ENC_RM,
	[0xB7] = OP_ENC_RM,
};

static void
mmio_fatal(struct x86_insn *insn, struct vm_exit *exit, uint64_t gpa)
{
	uint64_t *r = exit->vrs.vrs_gprs;

	fatalx("invalid MMIO address: rip=0x%llx gva=0x%lx gpa=0x%llx",
	    r[VCPU_REGS_RIP], insn->insn_gva, gpa);
}

/*
 * peek_byte
 *
 * Fetch the next byte from the instruction bytes without advancing the
 * position in the stream.
 *
 * Return values:
 *  DECODE_DONE: byte was found and is the last in the stream
 *  DECODE_MORE: byte was found and there are more remaining to be read
 *  DECODE_ERROR: state is invalid and not byte was found, *byte left unchanged
 */
static enum decode_result
peek_byte(struct x86_decode_state *state, uint8_t *byte)
{
	enum decode_result res;

	if (state == NULL)
		return (DECODE_ERROR);

	if (state->s_idx == state->s_len)
		return (DECODE_ERROR);

	if (state->s_idx + 1 == state->s_len)
		res = DECODE_DONE;
	else
		res = DECODE_MORE;

	if (byte != NULL)
		*byte = state->s_bytes[state->s_idx];
	return (res);
}

/*
 * next_byte
 *
 * Fetch the next byte from the instruction bytes, advancing the position in the
 * stream and mutating decode state.
 *
 * Return values:
 *  DECODE_DONE: byte was found and is the last in the stream
 *  DECODE_MORE: byte was found and there are more remaining to be read
 *  DECODE_ERROR: state is invalid and not byte was found, *byte left unchanged
 */
static enum decode_result
next_byte(struct x86_decode_state *state, uint8_t *byte)
{
	uint8_t next;

	/* Cheat and see if we're going to fail. */
	if (peek_byte(state, &next) == DECODE_ERROR)
		return (DECODE_ERROR);

	if (byte != NULL)
		*byte = next;
	state->s_idx++;

	return (state->s_idx < state->s_len ? DECODE_MORE : DECODE_DONE);
}

/*
 * Fetch the next `n' bytes as a single uint64_t value.
 */
static enum decode_result
next_value(struct x86_decode_state *state, size_t n, uint64_t *value)
{
	uint8_t bytes[8];
	size_t i;
	enum decode_result res;

	if (value == NULL)
		return (DECODE_ERROR);

	if (n == 0 || n > sizeof(bytes))
		return (DECODE_ERROR);

	memset(bytes, 0, sizeof(bytes));
	for (i = 0; i < n; i++)
		if ((res = next_byte(state, &bytes[i])) == DECODE_ERROR)
			return (DECODE_ERROR);

	*value = *((uint64_t*)bytes);

	return (res);
}

/*
 * is_valid_state
 *
 * Validate the decode state looks viable.
 *
 * Returns:
 *  1: if state is valid
 *  0: if an invariant is detected
 */
static int
is_valid_state(struct x86_decode_state *state, const char *fn_name)
{
	const char *s = (fn_name != NULL) ? fn_name : __func__;

	if (state == NULL) {
		log_warnx("%s: null state", s);
		return (0);
	}
	if (state->s_len > sizeof(state->s_bytes)) {
		log_warnx("%s: invalid length", s);
		return (0);
	}
	if (state->s_idx + 1 > state->s_len) {
		log_warnx("%s: invalid index", s);
		return (0);
	}

	return (1);
}

#ifdef MMIO_DEBUG
static void
dump_regs(struct vcpu_reg_state *vrs)
{
	size_t i;
	struct vcpu_segment_info *vsi;

	for (i = 0; i < VCPU_REGS_NGPRS; i++)
		log_debug("%s: %s 0x%llx", __progname, str_reg(i),
		    vrs->vrs_gprs[i]);

	for (i = 0; i < VCPU_REGS_NSREGS; i++) {
		vsi = &vrs->vrs_sregs[i];
		log_debug("%s: %s { sel: 0x%04x, lim: 0x%08x, ar: 0x%08x, "
		    "base: 0x%llx }", __progname, str_sreg(i),
		    vsi->vsi_sel, vsi->vsi_limit, vsi->vsi_ar, vsi->vsi_base);
	}
}

static void
dump_insn(struct x86_insn *insn)
{
	log_debug("instruction { %s, enc=%s, len=%d, mod=0x%02x, ("
	    "reg=%s, addr=0x%lx) sib=0x%02x }",
	    str_opcode(&insn->insn_opcode),
	    str_operand_enc(&insn->insn_opcode), insn->insn_bytes_len,
	    insn->insn_modrm, str_reg(insn->insn_reg),
	    insn->insn_gva, insn->insn_sib);
}
#endif /* MMIO_DEBUG */

const char *
str_cpu_mode(int mode)
{
	switch (mode) {
	case VMM_CPU_MODE_REAL: return "REAL";
	case VMM_CPU_MODE_PROT: return "PROT";
	case VMM_CPU_MODE_PROT32: return "PROT32";
	case VMM_CPU_MODE_COMPAT: return "COMPAT";
	case VMM_CPU_MODE_LONG: return "LONG";
	default: return "UNKNOWN";
	}
}

const char *
str_decode_res(enum decode_result res) {
	switch (res) {
	case DECODE_DONE: return "DONE";
	case DECODE_MORE: return "MORE";
	case DECODE_ERROR: return "ERROR";
	default: return "UNKNOWN";
	}
}

const char *
str_opcode(struct x86_opcode *opcode)
{
	switch (opcode->op_type) {
	case OP_ADD: return "ADD";
	case OP_AND: return "AND";
	case OP_CMP: return "CMP";
	case OP_IN: return "IN";
	case OP_INS: return "INS";
	case OP_MOV: return "MOV";
	case OP_MOVS: return "MOVS";
	case OP_MOVZX: return "MOVZX";
	case OP_OUT: return "OUT";
	case OP_OUTS: return "OUTS";
	case OP_POP: return "POP";
	case OP_PUSH: return "PUSH";
	case OP_SUB: return "SUB";
	case OP_TEST: return "TEST";
	case OP_UNSUPPORTED: return "UNSUPPORTED";
	default: return "UNKNOWN";
	}
}

const char *
str_operand_enc(struct x86_opcode *opcode)
{
	switch (opcode->op_encoding) {
	case OP_ENC_I: return "I";
	case OP_ENC_M: return "M";
	case OP_ENC_MI: return "MI";
	case OP_ENC_MR: return "MR";
	case OP_ENC_RM: return "RM";
	case OP_ENC_FD: return "FD";
	case OP_ENC_TD: return "TD";
	case OP_ENC_OI: return "OI";
	case OP_ENC_ZO: return "ZO";
	default: return "UNKNOWN";
	}
}

const char *
str_reg(int reg) {
	switch (reg) {
	case VCPU_REGS_RAX: return "RAX";
	case VCPU_REGS_RCX: return "RCX";
	case VCPU_REGS_RDX: return "RDX";
	case VCPU_REGS_RBX: return "RBX";
	case VCPU_REGS_RSI: return "RSI";
	case VCPU_REGS_RDI: return "RDI";
	case VCPU_REGS_R8:  return " R8";
	case VCPU_REGS_R9:  return " R9";
	case VCPU_REGS_R10: return "R10";
	case VCPU_REGS_R11: return "R11";
	case VCPU_REGS_R12: return "R12";
	case VCPU_REGS_R13: return "R13";
	case VCPU_REGS_R14: return "R14";
	case VCPU_REGS_R15: return "R15";
	case VCPU_REGS_RSP: return "RSP";
	case VCPU_REGS_RBP: return "RBP";
	case VCPU_REGS_RIP: return "RIP";
	case VCPU_REGS_RFLAGS: return "RFLAGS";
	default: return "UNKNOWN";
	}
}

const char *
str_sreg(int sreg) {
	switch (sreg) {
	case VCPU_REGS_CS: return "CS";
	case VCPU_REGS_DS: return "DS";
	case VCPU_REGS_ES: return "ES";
	case VCPU_REGS_FS: return "FS";
	case VCPU_REGS_GS: return "GS";
	case VCPU_REGS_SS: return "GS";
	case VCPU_REGS_LDTR: return "LDTR";
	case VCPU_REGS_TR: return "TR";
	default: return "UNKNOWN";
	}
}

int
detect_cpu_mode(struct vcpu_reg_state *vrs)
{
	uint64_t cr0, cr4, cs, efer, rflags;

	/* Is protected mode enabled? */
	cr0 = vrs->vrs_crs[VCPU_REGS_CR0];
	if (!(cr0 & CR0_PE))
		return (VMM_CPU_MODE_REAL);

	cr4 = vrs->vrs_crs[VCPU_REGS_CR4];
	cs = vrs->vrs_sregs[VCPU_REGS_CS].vsi_ar;
	efer = vrs->vrs_msrs[VCPU_REGS_EFER];
	rflags = vrs->vrs_gprs[VCPU_REGS_RFLAGS];

	/* Check for Long modes. */
	if ((efer & EFER_LME) && (cr4 & CR4_PAE) && (cr0 & CR0_PG)) {
		if (cs & CS_L) {
			/* Long Modes */
			if (!(cs & CS_D))
				return (VMM_CPU_MODE_LONG);
			log_warnx("%s: invalid cpu mode", __progname);
			return (VMM_CPU_MODE_UNKNOWN);
		} else {
			/* Compatibility Modes */
			if (cs & CS_D) /* XXX Add Compat32 mode */
				return (VMM_CPU_MODE_UNKNOWN);
			return (VMM_CPU_MODE_COMPAT);
		}
	}

	/* Check for 32-bit Protected Mode. */
	if (cs & CS_D)
		return (VMM_CPU_MODE_PROT32);

	/* Check for virtual 8086 mode. */
	if (rflags & EFLAGS_VM) {
		/* XXX add Virtual8086 mode */
		log_warnx("%s: Virtual 8086 mode", __progname);
		return (VMM_CPU_MODE_UNKNOWN);
	}

	/* Can't determine mode. */
	log_warnx("%s: invalid cpu mode", __progname);
	return (VMM_CPU_MODE_UNKNOWN);
}

static enum decode_result
decode_prefix(struct x86_decode_state *state, struct x86_insn *insn)
{
	enum decode_result res = DECODE_ERROR;
	struct x86_prefix *prefix;
	uint8_t byte;

	if (!is_valid_state(state, __func__) || insn == NULL)
		return (-1);

	prefix = &insn->insn_prefix;
	memset(prefix, 0, sizeof(*prefix));

	/*
	 * Decode prefixes. The last of its kind wins. The behavior is undefined
	 * in the Intel SDM (see Vol 2, 2.1.1 Instruction Prefixes.)
	 */
	while ((res = peek_byte(state, &byte)) != DECODE_ERROR) {
		switch (byte) {
		case LEG_1_LOCK:
		case LEG_1_REPNE:
		case LEG_1_REP:
			prefix->pfx_group1 = byte;
			break;
		case LEG_2_CS:
		case LEG_2_SS:
		case LEG_2_DS:
		case LEG_2_ES:
		case LEG_2_FS:
		case LEG_2_GS:
			prefix->pfx_group2 = byte;
			break;
		case LEG_3_OPSZ:
			prefix->pfx_group3 = byte;
			break;
		case LEG_4_ADDRSZ:
			prefix->pfx_group4 = byte;
			break;
		case REX_BASE...REX_BASE + 0x0F:
			if (insn->insn_cpu_mode == VMM_CPU_MODE_LONG)
				prefix->pfx_rex = byte;
			else /* INC encountered */
				return (DECODE_ERROR);
			break;
		case VEX_2_BYTE:
		case VEX_3_BYTE:
			log_warnx("%s: VEX not supported", __func__);
			return (DECODE_ERROR);
		default:
			/* Something other than a valid prefix. */
			return (DECODE_MORE);
		}
		/* Advance our position. */
		next_byte(state, NULL);
	}

	return (res);
}

static enum decode_result
decode_modrm(struct x86_decode_state *state, struct x86_insn *insn)
{
	enum decode_result res;
	uint8_t byte = 0;

	if (!is_valid_state(state, __func__) || insn == NULL) {
		log_warnx("%s: invalid state or null insn", __func__);
		return (DECODE_ERROR);
	}

	insn->insn_modrm_valid = 0;

	/* Check the operand encoding to see if we fetch a byte or abort. */
	switch (insn->insn_opcode.op_encoding) {
	case OP_ENC_M:
	case OP_ENC_MR:
	case OP_ENC_RM:
	case OP_ENC_MI:
		res = next_byte(state, &byte);
		if (res == DECODE_ERROR) {
			log_warnx("%s: failed to get modrm byte", __func__);
			break;
		}
		insn->insn_modrm = byte;
		insn->insn_modrm_valid = 1;
		break;
	case OP_ENC_I:
	case OP_ENC_OI:
		log_warnx("%s: instruction does not need memory assist",
		    __func__);
		res = DECODE_ERROR;
		break;
	case OP_ENC_ZO:
		res = DECODE_DONE;
		break;
	default:
		/* Peek to see if we're done decode. */
		res = peek_byte(state, NULL);
		DPRINTF("%s: decoding modrm res=%s", __func__,
		    str_decode_res(res));
	}

	return (res);
}

static int
get_modrm_reg(struct x86_insn *insn)
{
	if (insn == NULL)
		return (-1);

	if (insn->insn_modrm_valid) {
		switch (MODRM_REGOP(insn->insn_modrm)) {
		case 0:
			insn->insn_reg = VCPU_REGS_RAX;
			break;
		case 1:
			insn->insn_reg = VCPU_REGS_RCX;
			break;
		case 2:
			insn->insn_reg = VCPU_REGS_RDX;
			break;
		case 3:
			insn->insn_reg = VCPU_REGS_RBX;
			break;
		case 4:
			insn->insn_reg = VCPU_REGS_RSP;
			break;
		case 5:
			insn->insn_reg = VCPU_REGS_RBP;
			break;
		case 6:
			insn->insn_reg = VCPU_REGS_RSI;
			break;
		case 7:
			insn->insn_reg = VCPU_REGS_RDI;
			break;
		}
	}

	/* REX R bit selects extended registers in LONG mode. */
	if (insn->insn_prefix.pfx_rex & REX_R)
		insn->insn_reg += 8;

	return (0);
}

static int
get_modrm_addr(struct x86_insn *insn, struct vcpu_reg_state *vrs)
{
	uint8_t mod, reg, rm;
	vaddr_t addr = 0x0UL;

	if (insn == NULL || vrs == NULL)
		return (-1);

	if (insn->insn_modrm_valid) {
		rm = MODRM_RM(insn->insn_modrm);
		mod = MODRM_MOD(insn->insn_modrm);

		/* r/m=100 selects a SIB byte except for register operands. */
		if (!(rm == 0b100 && mod != 0b11) &&
		    /* mod=00, r/m=101 is RIP-relative. */
		    !(rm == 0b101 && mod == 0b00)) {
			reg = rm;
			if (insn->insn_prefix.pfx_rex & REX_B)
				reg += 8;
			addr = vrs->vrs_gprs[reg];
		}

		DPRINTF("%s: computed register-based addr=0x%lx", __func__,
		    addr);
		insn->insn_gva = addr;
	}

	return (0);
}

static enum decode_result
decode_disp(struct x86_decode_state *state, struct vcpu_reg_state *vrs,
    struct x86_insn *insn)
{
	enum decode_result res = DECODE_ERROR;
	int64_t disp = 0;

	if (!is_valid_state(state, __func__) || insn == NULL) {
		log_warnx("%s: invalid state", __func__);
		return (DECODE_ERROR);
	}

	if (insn->insn_opcode.op_encoding == OP_ENC_FD ||
	    insn->insn_opcode.op_encoding == OP_ENC_TD) {
		/* XXX rex prefix override needs handling here */
		/*     cannot count on processor mode to determine */
		/*     op size */
		switch (insn->insn_cpu_mode) {
		case VMM_CPU_MODE_PROT32:
			insn->insn_disp_type = DISP_4;
			res = next_value(state, 4, &disp);
			if (res == DECODE_ERROR) {
				log_warnx("%s: decode error in next_value for "
				    "disp %d", __func__, insn->insn_disp_type);
				return (res);
			}
			insn->insn_disp = disp;
			insn->insn_gva = disp;
			return (res);
		case VMM_CPU_MODE_LONG:
		case VMM_CPU_MODE_COMPAT:
		default:
			log_warnx("%s: unimplemented displacement decode",
			    __func__);
			return (DECODE_ERROR);
		}
	}

	if (!insn->insn_modrm_valid) {
		log_warnx("%s: invalid modrm", __func__);
		return (DECODE_ERROR);
	}

	/*
	 * In 32- and 64-bit addressing, mod=00 with a SIB base of 101
	 * denotes a base-less disp32 address. This is the encoding used by
	 * openbsd amd64 for writes to the fixed LAPIC mapping (for example,
	 * `movl $0, local_apic+LAPIC_EOI').
	 */
	if (insn->insn_sib_valid && MODRM_MOD(insn->insn_modrm) == 0 &&
	    SIB_BASE(insn->insn_sib) == 5) {
		insn->insn_disp_type = DISP_4;
		res = next_value(state, 4, &disp);
		if (res == DECODE_ERROR) {
			log_warnx("%s: decode error in SIB disp32 processing",
			    __func__);
			return (res);
		}
		insn->insn_disp = disp;
		if (insn->insn_cpu_mode == VMM_CPU_MODE_LONG)
			insn->insn_disp = (int64_t)(int32_t)insn->insn_disp;
		insn->insn_gva += insn->insn_disp;
		return (res);
	}

	/* Disp8 / Disp32 / %rip + Disp32 displacement */
	if (MODRM_RM(insn->insn_modrm) == 0x5) {
		if (MODRM_MOD(insn->insn_modrm) == 1) {
			/* Disp8 */
			insn->insn_disp_type = DISP_1;
			res = next_value(state, 1, &disp);
		} else {
			/* Disp32 */
			insn->insn_disp_type = DISP_4;
			res = next_value(state, 4, &disp);
		}

		if (res == DECODE_ERROR) {
			log_warnx("%s: decode error in Disp32 processing",
			    __func__);
			return (res);
		}
		insn->insn_disp = disp;

		/* Sign-extend 32-bit displacement to 64 bits in long mode */
		if (insn->insn_disp_type == DISP_4 &&
		    insn->insn_cpu_mode == VMM_CPU_MODE_LONG)
			insn->insn_disp = (int64_t)(int32_t)insn->insn_disp;

		if (insn->insn_cpu_mode == VMM_CPU_MODE_LONG &&
		    MODRM_MOD(insn->insn_modrm) == 0) {
			insn->insn_disp += vrs->vrs_gprs[VCPU_REGS_RIP];

			/*
			 * we dont yet know how long the instructions is
			 * so defer adding the fixup based on %rip until
			 * we do (at the end of insn_decode()
			 */
			insn->insn_needs_rip_fixup = 1;
			insn->insn_gva += (int32_t)insn->insn_disp;
			return (res);
		}

		insn->insn_gva += insn->insn_disp;

		return (res);
	}

	DPRINTF("%s: mod = %d", __func__, MODRM_MOD(insn->insn_modrm));

	switch (MODRM_MOD(insn->insn_modrm)) {
	case 0x00:
		insn->insn_disp_type = DISP_0;
		res = DECODE_MORE;
		DPRINTF("%s: returning DECODE_MORE", __func__);
		break;
	case 0x01:
		insn->insn_disp_type = DISP_1;
		res = next_value(state, 1, &disp);
		if (res == DECODE_ERROR) {
			log_warnx("%s: decode error in next_value for disp 0x1",
			    __func__);
			return (res);
		}
		insn->insn_disp = disp;
		break;
	case 0x02:
		if (insn->insn_prefix.pfx_group4 == LEG_4_ADDRSZ) {
			insn->insn_disp_type = DISP_2;
			res = next_value(state, 2, &disp);
		} else {
			insn->insn_disp_type = DISP_4;
			res = next_value(state, 4, &disp);
		}
		if (res == DECODE_ERROR) {
			log_warnx("%s: decode error in next_value for disp %d",
			    __func__, insn->insn_disp_type);
			return (res);
		}
		insn->insn_disp = disp;
		/* Sign-extend 32-bit displacement to 64 bits in long mode */
		if (insn->insn_disp_type == DISP_4 &&
		    insn->insn_cpu_mode == VMM_CPU_MODE_LONG)
			insn->insn_disp = (int64_t)(int32_t)insn->insn_disp;
		break;
	default:
		insn->insn_disp_type = DISP_NONE;
		res = DECODE_MORE;
		log_warnx("%s: ?? DISP_NONE fallthrough", __func__);
	}

	insn->insn_gva += insn->insn_disp;

	DPRINTF("%s: returning calculated displacement of 0x%llx", __func__,
	    insn->insn_disp);

	return (res);
}

static enum decode_result
decode_opcode(struct x86_decode_state *state, struct x86_insn *insn)
{
	enum decode_result res;
	enum x86_opcode_type type;
	enum x86_operand_enc enc;
	struct x86_opcode *opcode = &insn->insn_opcode;
	uint8_t byte, byte2;

	if (!is_valid_state(state, __func__) || insn == NULL)
		return (-1);

	memset(opcode, 0, sizeof(*opcode));

	res = next_byte(state, &byte);
	if (res == DECODE_ERROR)
		return (res);

	type = x86_1byte_opcode_tbl[byte];
	switch(type) {
	case OP_UNKNOWN:
	case OP_UNSUPPORTED:
		log_warnx("%s: unsupported opcode 0x%02x", __func__, byte);
		return (DECODE_ERROR);

	case OP_TWO_BYTE:
		res = next_byte(state, &byte2);
		if (res == DECODE_ERROR)
			return (res);

		type = x86_2byte_opcode_tbl[byte2];
		if (type == OP_UNKNOWN || type == OP_UNSUPPORTED) {
			log_warnx("%s: unsupported 2-byte opcode 0x0f%02x",
			    __func__, byte2);
			return (DECODE_ERROR);
		}

		opcode->op_bytes[0] = byte;
		opcode->op_bytes[1] = byte2;
		opcode->op_bytes_len = 2;
		enc = x86_2byte_operand_enc_table[byte2];
		break;

	default:
		/* We've potentially got a known 1-byte opcode. */
		opcode->op_bytes[0] = byte;
		opcode->op_bytes_len = 1;
		enc = x86_1byte_operand_enc_tbl[byte];
	}

	if (enc == OP_ENC_UNKNOWN)
		return (DECODE_ERROR);

	opcode->op_type = type;
	opcode->op_encoding = enc;

	return (res);
}

static enum decode_result
decode_sib(struct x86_decode_state *state, struct vcpu_reg_state *vrs,
    struct x86_insn *insn)
{
	enum decode_result res;
	uint8_t byte, mod, scale, index, base, index_reg, base_reg;
	uint64_t scale_val;
	vaddr_t addr = 0;

	if (!is_valid_state(state, __func__) || insn == NULL)
		return (-1);

	/* SIB is optional, so assume we will be continuing. */
	res = DECODE_MORE;

	insn->insn_sib_valid = 0;
	if (!insn->insn_modrm_valid)
		return (res);

	mod = MODRM_MOD(insn->insn_modrm);

	/* XXX is SIB valid in all cpu modes? */
	if (MODRM_RM(insn->insn_modrm) == 0b100) {
		res = next_byte(state, &byte);
		if (res != DECODE_ERROR) {
			insn->insn_sib_valid = 1;
			insn->insn_sib = byte;

			scale = SIB_SCALE(byte);
			index = SIB_INDEX(byte);
			base = SIB_BASE(byte);
			base_reg = base;
			if (insn->insn_prefix.pfx_rex & REX_B)
				base_reg += 8;
			index_reg = index;
			if (insn->insn_prefix.pfx_rex & REX_X)
				index_reg += 8;

			/* Calculate scale factor: 0->1, 1->2, 2->4, 3->8 */
			scale_val = 1ULL << scale;

			/* Add base register value (unless special case) */
			if (base != 0b101 || mod != 0b00) {
				addr += vrs->vrs_gprs[base_reg];
			}

			/* index=100 is no index unless REX.X extends it to R12. */
			if (index != 0b100 ||
			    (insn->insn_prefix.pfx_rex & REX_X)) {
				addr += vrs->vrs_gprs[index_reg] * scale_val;
			}

			insn->insn_gva = addr;

			DPRINTF("%s: SIB calc: scale=%llu, index=%s, base=%s, "
			    "addr=0x%lx", __func__, scale_val,
			    index == 0b100 &&
			    !(insn->insn_prefix.pfx_rex & REX_X) ? "none" :
			    str_reg(index_reg),
			    base == 0b101 && mod == 0b00 ? "none" :
			    str_reg(base_reg), addr);
		}
	}

	return (res);
}

static enum decode_result
decode_imm(struct x86_decode_state *state, struct x86_insn *insn)
{
	enum decode_result res;
	size_t num_bytes;
	uint64_t value;

	if (!is_valid_state(state, __func__) || insn == NULL)
		return (DECODE_ERROR);

	/* Only handle MI encoded instructions. Others shouldn't need assist. */
	if (insn->insn_opcode.op_encoding != OP_ENC_MI)
		return (DECODE_DONE);

	/* Exceptions related to MOV and group-3 TEST instructions. */
	if (insn->insn_opcode.op_type == OP_MOV) {
		switch (insn->insn_opcode.op_bytes[0]) {
		case 0xC6:
			num_bytes = 1;
			break;
		case 0xC7:
			if (insn->insn_cpu_mode == VMM_CPU_MODE_REAL)
				num_bytes = 2;
			else
				num_bytes = 4;
			break;
		default:
			log_warnx("%s: cannot decode immediate bytes for MOV",
			    __func__);
			return (DECODE_ERROR);
		}
	} else if (insn->insn_opcode.op_type == OP_CMP) {
		if (!insn->insn_modrm_valid ||
		    MODRM_REGOP(insn->insn_modrm) != 7) {
			log_warnx("%s: unsupported CMP group operation /%u",
			    __func__, MODRM_REGOP(insn->insn_modrm));
			return (DECODE_ERROR);
		}
		switch (insn->insn_opcode.op_bytes[0]) {
		case 0x80:
		case 0x83:
			num_bytes = 1;
			break;
		case 0x81:
			num_bytes = get_operand_size(insn) == 2 ? 2 : 4;
			break;
		default:
			return (DECODE_ERROR);
		}
	} else if (insn->insn_opcode.op_type == OP_TEST) {
		if (insn->insn_opcode.op_bytes[0] != 0xf7 ||
		    !insn->insn_modrm_valid ||
		    MODRM_REGOP(insn->insn_modrm) != 0) {
			log_warnx("%s: unsupported F7 group operation /%u",
			    __func__, MODRM_REGOP(insn->insn_modrm));
			return (DECODE_ERROR);
		}
		num_bytes = get_operand_size(insn) == 2 ? 2 : 4;
	} else {
		/* Fallback to interpreting based on cpu mode and REX. */
		if (insn->insn_cpu_mode == VMM_CPU_MODE_REAL)
			num_bytes = 2;
		else if (insn->insn_prefix.pfx_rex == REX_NONE)
			num_bytes = 4;
		else
			num_bytes = 8;
	}

	res = next_value(state, num_bytes, &value);
	if (res != DECODE_ERROR) {
		insn->insn_immediate = value;
		insn->insn_immediate_len = num_bytes;
	}

	return (res);
}


/*
 * insn_decode
 *
 * Decode an x86 instruction from the provided instruction bytes.
 *
 * Return values:
 *  0: successful decode
 *  Non-zero: an exception occurred during decode
 */
int
insn_decode(struct vm_exit *exit, struct x86_insn *insn)
{
	enum decode_result res;
	struct vcpu_reg_state *vrs = &exit->vrs;
	struct x86_decode_state state;
	uint8_t *bytes, len;
	int mode;

	if (exit == NULL || insn == NULL) {
		log_warnx("%s: invalid input", __func__);
		return (DECODE_ERROR);
	}

	bytes = exit->vee.vee_insn_bytes;
	len = exit->vee.vee_insn_len;

	/* 0. Initialize state and instruction objects. */
	memset(insn, 0, sizeof(*insn));
	memset(&state, 0, sizeof(state));
	state.s_len = len;
	memcpy(&state.s_bytes, bytes, len);

	/* 1. Detect CPU mode. */
	mode = detect_cpu_mode(vrs);
	if (mode == VMM_CPU_MODE_UNKNOWN) {
		log_warnx("%s: failed to identify cpu mode", __func__);
		DUMP_REGS(vrs);
		return (-1);
	}
	insn->insn_cpu_mode = mode;

#ifdef MMIO_DEBUG
	DPRINTF("%s: cpu mode %s detected", __progname, str_cpu_mode(mode));
	char _bytes[128] = { 0 };
	char *p = _bytes;
	for (int i = 0; i < len; i++) {
		p += snprintf(p, 6, "%02x ", bytes[i]);
	}
	DPRINTF("%s: got bytes [ %s]", __func__, _bytes);
#endif

	/* 2. Decode prefixes. */
	res = decode_prefix(&state, insn);
	if (res == DECODE_ERROR) {
		log_warnx("%s: error decoding prefixes", __func__);
		goto err;
	} else if (res == DECODE_DONE)
		goto done;

	DPRINTF("%s: prefixes {g1: 0x%02x, g2: 0x%02x, g3: 0x%02x, g4: 0x%02x,"
	    " rex: 0x%02x }", __progname, insn->insn_prefix.pfx_group1,
	    insn->insn_prefix.pfx_group2, insn->insn_prefix.pfx_group3,
	    insn->insn_prefix.pfx_group4, insn->insn_prefix.pfx_rex);

	/* 3. Pick apart opcode. Here we can start short-circuiting. */
	res = decode_opcode(&state, insn);
	if (res == DECODE_ERROR) {
		log_warnx("%s: error decoding opcode", __func__);
		goto err;
	} else if (res == DECODE_DONE)
		goto done;

	DPRINTF("%s: found opcode %s (operand encoding %s) (%s)", __progname,
	    str_opcode(&insn->insn_opcode), str_operand_enc(&insn->insn_opcode),
	    str_decode_res(res));

	/* Process optional ModR/M byte. */
	res = decode_modrm(&state, insn);
	if (res == DECODE_ERROR) {
		log_warnx("%s: error decoding modrm", __func__);
		goto err;
	}
	if (get_modrm_addr(insn, vrs) != 0)
		goto err;
	if (get_modrm_reg(insn) != 0)
		goto err;
	if (res == DECODE_DONE)
		goto done;

	if (insn->insn_modrm_valid)
		DPRINTF("%s: found ModRM 0x%02x (%s)", __progname,
		    insn->insn_modrm, str_decode_res(res));

	/* Process optional SIB byte. */
	res = decode_sib(&state, vrs, insn);
	if (res == DECODE_ERROR) {
		log_warnx("%s: error decoding sib", __func__);
		goto err;
	} else if (res == DECODE_DONE)
		goto done;

	if (insn->insn_sib_valid)
		DPRINTF("%s: found SIB 0x%02x (%s)", __progname,
		    insn->insn_sib, str_decode_res(res));

	/* Process any Displacement bytes. */
	res = decode_disp(&state, vrs, insn);
	if (res == DECODE_ERROR) {
		log_warnx("%s: error decoding displacement", __func__);
		goto err;
	} else if (res == DECODE_DONE)
		goto done;

	/* Process any Immediate data bytes. */
	res = decode_imm(&state, insn);
	if (res == DECODE_ERROR) {
		log_warnx("%s: error decoding immediate bytes", __func__);
		goto err;
	}

done:
	insn->insn_bytes_len = state.s_idx;

	if (insn->insn_needs_rip_fixup) {
		insn->insn_gva += insn->insn_bytes_len;
	}

	DPRINTF("%s: final instruction length is %u", __func__,
		insn->insn_bytes_len);
	DUMP_INSN(insn);
	DPRINTF("%s: modrm: {mod: %d, regop: %d, rm: %d}", __func__,
	    MODRM_MOD(insn->insn_modrm), MODRM_REGOP(insn->insn_modrm),
	    MODRM_RM(insn->insn_modrm));
	DUMP_REGS(vrs);
	return (0);

err:
	DUMP_INSN(insn);
	DPRINTF("%s: modrm: {mod: %d, regop: %d, rm: %d}", __func__,
	    MODRM_MOD(insn->insn_modrm), MODRM_REGOP(insn->insn_modrm),
	    MODRM_RM(insn->insn_modrm));
	DUMP_REGS(vrs);
	return (-1);
}

static int
get_operand_size(struct x86_insn *insn)
{
	uint8_t opcode;

	opcode = insn->insn_opcode.op_bytes[
	    insn->insn_opcode.op_bytes_len - 1];
	if (insn->insn_opcode.op_type == OP_MOV &&
	    (opcode == 0x88 || opcode == 0x8a || opcode == 0xa0 ||
	    opcode == 0xa2 || opcode == 0xc6))
		return (1);
	if (insn->insn_opcode.op_type == OP_CMP && opcode == 0x80)
		return (1);
	if (insn->insn_opcode.op_type == OP_MOVS && opcode == 0xa4)
		return (1);

	if (insn->insn_cpu_mode == VMM_CPU_MODE_LONG) {
		if (insn->insn_prefix.pfx_rex & REX_W)
			return 8;
		if (insn->insn_prefix.pfx_group3 == LEG_3_OPSZ)
			return 2;
		return 4;
	} else if (insn->insn_cpu_mode == VMM_CPU_MODE_PROT32) {
		if (insn->insn_prefix.pfx_group3 == LEG_3_OPSZ)
			return 2;
		return 4;
	}
	return 2;
}

/* Set the status flags shared by AND and TEST. */
static void
emulate_logic_flags(struct vm_exit *exit, uint64_t result, int opsz)
{
	uint64_t rflags, sign;

	rflags = exit->vrs.vrs_gprs[VCPU_REGS_RFLAGS];
	rflags &= ~(PSL_C | PSL_PF | PSL_AF | PSL_Z | PSL_N | PSL_V);
	if (result == 0)
		rflags |= PSL_Z;
	sign = 1ULL << (opsz * 8 - 1);
	if (result & sign)
		rflags |= PSL_N;
	if (__builtin_parity((unsigned int)(result & 0xff)) == 0)
		rflags |= PSL_PF;
	exit->vrs.vrs_gprs[VCPU_REGS_RFLAGS] = rflags;
}

static int
emulate_add(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	uint64_t data, gpa, lhs, mask, result, rhs;
	mmio_dev_fn_t mmio_fn;
	int opsz, ret;

	if (insn->insn_opcode.op_encoding != OP_ENC_RM)
		return (EINVAL);

	if (!(exit->vee.vee_insn_info & VEE_GPA_VALID))
		fatalx("%s: exit information missing GPA", __func__);
	gpa = exit->vee.vee_gpa;

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	opsz = get_operand_size(insn);
	data = 0;
	if ((ret = mmio_fn(vcpu_id, MMIO_DIR_READ, gpa, opsz, &data)) != 0)
		return (ret);

	switch (opsz) {
	case 2:
		mask = 0xffff;
		break;
	case 4:
		mask = 0xffffffff;
		break;
	case 8:
		mask = UINT64_MAX;
		break;
	default:
		return (EINVAL);
	}
	lhs = exit->vrs.vrs_gprs[insn->insn_reg] & mask;
	rhs = data & mask;
	result = (lhs + rhs) & mask;
	if (opsz == 2)
		exit->vrs.vrs_gprs[insn->insn_reg] =
		    (exit->vrs.vrs_gprs[insn->insn_reg] & ~mask) | result;
	else
		exit->vrs.vrs_gprs[insn->insn_reg] = result;
	emulate_add_flags(exit, lhs, rhs, result, opsz);
	return (0);
}

/* Intel SDM Vol. 2: ADD sets OF, SF, ZF, AF, PF and CF. */
static void
emulate_add_flags(struct vm_exit *exit, uint64_t lhs, uint64_t rhs,
    uint64_t result, int opsz)
{
	uint64_t rflags, sign;

	rflags = exit->vrs.vrs_gprs[VCPU_REGS_RFLAGS];
	rflags &= ~(PSL_C | PSL_PF | PSL_AF | PSL_Z | PSL_N | PSL_V);
	sign = 1ULL << (opsz * 8 - 1);
	if (result < lhs)
		rflags |= PSL_C;
	if (__builtin_parity((unsigned int)(result & 0xff)) == 0)
		rflags |= PSL_PF;
	if ((lhs ^ rhs ^ result) & 0x10)
		rflags |= PSL_AF;
	if (result == 0)
		rflags |= PSL_Z;
	if (result & sign)
		rflags |= PSL_N;
	if (~(lhs ^ rhs) & (lhs ^ result) & sign)
		rflags |= PSL_V;
	exit->vrs.vrs_gprs[VCPU_REGS_RFLAGS] = rflags;
}

static int
emulate_and(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	uint64_t data, gpa, mask, old, result;
	mmio_dev_fn_t mmio_fn;
	int opsz, ret;

	if (insn->insn_opcode.op_encoding != OP_ENC_RM) {
		log_warnx("%s: unsupported encoding %s", __func__,
		    str_operand_enc(&insn->insn_opcode));
		return (EINVAL);
	}

	if (!(exit->vee.vee_insn_info & VEE_GPA_VALID))
		fatalx("%s: exit information missing GPA", __func__);
	gpa = exit->vee.vee_gpa;

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	opsz = get_operand_size(insn);
	data = 0;
	ret = mmio_fn(vcpu_id, MMIO_DIR_READ, gpa, opsz, &data);
	if (ret) {
		log_warnx("%s: mmio function indicated failure", __func__);
		return (0);
	}

	switch (opsz) {
	case 2:
		mask = 0xffff;
		break;
	case 4:
		mask = 0xffffffff;
		break;
	case 8:
		mask = 0xffffffffffffffffULL;
		break;
	default:
		fatalx("invalid AND operand size %d", opsz);
	}

	old = exit->vrs.vrs_gprs[insn->insn_reg];
	result = (old & data) & mask;
	if (opsz == 2)
		exit->vrs.vrs_gprs[insn->insn_reg] =
		    (old & ~mask) | result;
	else
		exit->vrs.vrs_gprs[insn->insn_reg] = result;

	/* AND clears CF and OF; AF is undefined and is cleared here. */
	emulate_logic_flags(exit, result, opsz);

	return (0);
}

static int
emulate_test(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	uint64_t data, gpa, immediate, mask, result;
	mmio_dev_fn_t mmio_fn;
	int opsz, ret;

	if (insn->insn_opcode.op_encoding != OP_ENC_MI ||
	    !insn->insn_modrm_valid || MODRM_REGOP(insn->insn_modrm) != 0) {
		log_warnx("%s: unsupported encoding or F7 group operation",
		    __func__);
		return (EINVAL);
	}

	if (!(exit->vee.vee_insn_info & VEE_GPA_VALID))
		fatalx("%s: exit information missing GPA", __func__);
	gpa = exit->vee.vee_gpa;

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	opsz = get_operand_size(insn);
	data = 0;
	ret = mmio_fn(vcpu_id, MMIO_DIR_READ, gpa, opsz, &data);
	if (ret != 0) {
		log_warnx("%s: mmio function indicated failure", __func__);
		return (0);
	}

	switch (opsz) {
	case 2:
		mask = 0xffff;
		immediate = insn->insn_immediate & mask;
		break;
	case 4:
		mask = 0xffffffff;
		immediate = insn->insn_immediate & mask;
		break;
	case 8:
		mask = 0xffffffffffffffffULL;
		immediate = (uint64_t)(int64_t)(int32_t)
		    insn->insn_immediate;
		break;
	default:
		fatalx("invalid TEST operand size %d", opsz);
	}
	result = (data & immediate) & mask;

	/* TEST has AND's flags semantics without writing either operand */
	emulate_logic_flags(exit, result, opsz);

	return (0);
}

static int
emulate_cmp(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	uint64_t data, gpa, lhs, mask, result, rhs;
	uint8_t opcode;
	mmio_dev_fn_t mmio_fn;
	int opsz, ret;

	if (insn->insn_opcode.op_encoding != OP_ENC_RM &&
	    insn->insn_opcode.op_encoding != OP_ENC_MI) {
		log_warnx("%s: unsupported encoding %s", __func__,
		    str_operand_enc(&insn->insn_opcode));
		return (EINVAL);
	}
	if (insn->insn_opcode.op_encoding == OP_ENC_MI &&
	    (!insn->insn_modrm_valid ||
	    MODRM_REGOP(insn->insn_modrm) != 7))
		return (EINVAL);

	if (!(exit->vee.vee_insn_info & VEE_GPA_VALID))
		fatalx("%s: exit information missing GPA", __func__);
	gpa = exit->vee.vee_gpa;

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	opsz = get_operand_size(insn);
	data = 0;
	ret = mmio_fn(vcpu_id, MMIO_DIR_READ, gpa, opsz, &data);
	if (ret != 0) {
		log_warnx("%s: mmio function indicated failure", __func__);
		return (ret);
	}

	switch (opsz) {
	case 1:
		mask = 0xff;
		break;
	case 2:
		mask = 0xffff;
		break;
	case 4:
		mask = 0xffffffff;
		break;
	case 8:
		mask = UINT64_MAX;
		break;
	default:
		fatalx("invalid CMP operand size %d", opsz);
	}

	if (insn->insn_opcode.op_encoding == OP_ENC_MI) {
		lhs = data & mask;
		opcode = insn->insn_opcode.op_bytes[0];
		if (opcode == 0x83)
			rhs = (int8_t)insn->insn_immediate & mask;
		else if (opcode == 0x81 && opsz == 8)
			rhs = (int32_t)insn->insn_immediate & mask;
		else
			rhs = insn->insn_immediate & mask;
	} else {
		lhs = exit->vrs.vrs_gprs[insn->insn_reg] & mask;
		rhs = data & mask;
	}
	result = (lhs - rhs) & mask;
	emulate_sub_flags(exit, lhs, rhs, result, opsz);
	return (0);
}

static int
emulate_pop(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	struct vcpu_reg_state *vrs = &exit->vrs;
	struct vcpu_segment_info *ss = &vrs->vrs_sregs[VCPU_REGS_SS];
	uint64_t data, gpa, new_sp, old_sp, stack_gva, stack_gpa;
	mmio_dev_fn_t mmio_fn;
	int opsz, ret;

	if (insn->insn_opcode.op_encoding != OP_ENC_M ||
	    !insn->insn_modrm_valid || MODRM_REGOP(insn->insn_modrm) != 0 ||
	    MODRM_MOD(insn->insn_modrm) == 3) {
		log_warnx("%s: unsupported encoding or 8F group operation",
		    __func__);
		return (EINVAL);
	}

	/* This is the 32-bit protected-mode form in openbsd i386 kernels */
	if (insn->insn_cpu_mode != VMM_CPU_MODE_PROT32) {
		log_warnx("%s: unsupported cpu mode %s", __func__,
		    str_cpu_mode(insn->insn_cpu_mode));
		return (ENOTSUP);
	}

	opsz = get_operand_size(insn);
	if (opsz != 2 && opsz != 4) {
		log_warnx("%s: invalid operand size %d", __func__, opsz);
		return (EINVAL);
	}

	/* read SS:ESP before incrementing ESP */
	old_sp = vrs->vrs_gprs[VCPU_REGS_RSP];
	stack_gva = (uint32_t)(ss->vsi_base + (uint32_t)old_sp);
	ret = translate_gva(exit, stack_gva, &stack_gpa, PROT_READ);
	if (ret != 0) {
		log_warnx("%s: error translating stack gva 0x%llx: %s",
		    __func__, stack_gva, strerror(ret));
		return (ret);
	}
	data = 0;
	ret = read_mem(stack_gpa, &data, opsz);
	if (ret != 0) {
		log_warnx("%s: error reading stack gpa 0x%llx: %s",
		    __func__, stack_gpa, strerror(ret));
		return (ret);
	}

	new_sp = (uint32_t)(old_sp + opsz);
	ret = translate_gva(exit, insn->insn_gva, &gpa, PROT_WRITE);
	if (ret != 0) {
		log_warnx("%s: error translating destination gva 0x%lx: %s",
		    __func__, insn->insn_gva, strerror(ret));
		return (ret);
	}

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	ret = mmio_fn(vcpu_id, MMIO_DIR_WRITE, gpa, opsz, &data);
	if (ret != 0) {
		log_warnx("%s: mmio function indicated failure", __func__);
		return (ret);
	}

	vrs->vrs_gprs[VCPU_REGS_RSP] =
	    (old_sp & 0xffffffff00000000ULL) | new_sp;
	return (0);
}

static int
emulate_push(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	struct vcpu_reg_state *vrs = &exit->vrs;
	struct vcpu_segment_info *ss = &vrs->vrs_sregs[VCPU_REGS_SS];
	uint64_t data, gpa, new_sp, old_sp, stack_gva, stack_gpa;
	mmio_dev_fn_t mmio_fn;
	int opsz, ret;

	if (insn->insn_opcode.op_encoding != OP_ENC_M ||
	    !insn->insn_modrm_valid || MODRM_REGOP(insn->insn_modrm) != 6) {
		log_warnx("%s: unsupported encoding or FF group operation",
		    __func__);
		return (EINVAL);
	}

	/* This is the 32-bit protected-mode form in openbsd i386 kernels */
	if (insn->insn_cpu_mode != VMM_CPU_MODE_PROT32) {
		log_warnx("%s: unsupported cpu mode %s", __func__,
		    str_cpu_mode(insn->insn_cpu_mode));
		return (ENOTSUP);
	}

	opsz = get_operand_size(insn);
	if (opsz != 2 && opsz != 4) {
		log_warnx("%s: invalid operand size %d", __func__, opsz);
		return (EINVAL);
	}

	if (!(exit->vee.vee_insn_info & VEE_GPA_VALID))
		fatalx("%s: exit information missing GPA", __func__);
	gpa = exit->vee.vee_gpa;

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	data = 0;
	ret = mmio_fn(vcpu_id, MMIO_DIR_READ, gpa, opsz, &data);
	if (ret != 0) {
		log_warnx("%s: mmio function indicated failure", __func__);
		return (ret);
	}

	/* PUSH uses the pre-decremented ESP and the SS segment */
	old_sp = vrs->vrs_gprs[VCPU_REGS_RSP];
	new_sp = (uint32_t)(old_sp - opsz);
	stack_gva = (uint32_t)(ss->vsi_base + new_sp);
	ret = translate_gva(exit, stack_gva, &stack_gpa, PROT_WRITE);
	if (ret != 0) {
		log_warnx("%s: error translating stack gva 0x%llx: %s",
		    __func__, stack_gva, strerror(ret));
		return (ret);
	}
	ret = write_mem(stack_gpa, &data, opsz);
	if (ret != 0) {
		log_warnx("%s: error writing stack gpa 0x%llx: %s",
		    __func__, stack_gpa, strerror(ret));
		return (ret);
	}

	vrs->vrs_gprs[VCPU_REGS_RSP] =
	    (old_sp & 0xffffffff00000000ULL) | new_sp;
	return (0);
}

static int
emulate_sub(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	uint64_t data, gpa, lhs, mask, result, rhs;
	mmio_dev_fn_t mmio_fn;
	int opsz, ret;

	if (insn->insn_opcode.op_encoding != OP_ENC_RM) {
		log_warnx("%s: unsupported encoding %s", __func__,
		    str_operand_enc(&insn->insn_opcode));
		return (EINVAL);
	}

	if (!(exit->vee.vee_insn_info & VEE_GPA_VALID))
		fatalx("%s: exit information missing GPA", __func__);
	gpa = exit->vee.vee_gpa;

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	opsz = get_operand_size(insn);
	data = 0;
	ret = mmio_fn(vcpu_id, MMIO_DIR_READ, gpa, opsz, &data);
	if (ret != 0) {
		log_warnx("%s: mmio function indicated failure", __func__);
		return (ret);
	}

	switch (opsz) {
	case 2:
		mask = 0xffff;
		break;
	case 4:
		mask = 0xffffffff;
		break;
	case 8:
		mask = UINT64_MAX;
		break;
	default:
		fatalx("invalid SUB operand size %d", opsz);
	}

	lhs = exit->vrs.vrs_gprs[insn->insn_reg] & mask;
	rhs = data & mask;
	result = (lhs - rhs) & mask;
	if (opsz == 2)
		exit->vrs.vrs_gprs[insn->insn_reg] =
		    (exit->vrs.vrs_gprs[insn->insn_reg] & ~mask) | result;
	else
		exit->vrs.vrs_gprs[insn->insn_reg] = result;

	emulate_sub_flags(exit, lhs, rhs, result, opsz);
	return (0);
}

/* Intel SDM Vol. 2: SUB and CMP set OF, SF, ZF, AF, PF and CF */
static void
emulate_sub_flags(struct vm_exit *exit, uint64_t lhs, uint64_t rhs,
    uint64_t result, int opsz)
{
	uint64_t rflags, sign;

	rflags = exit->vrs.vrs_gprs[VCPU_REGS_RFLAGS];
	rflags &= ~(PSL_C | PSL_PF | PSL_AF | PSL_Z | PSL_N | PSL_V);
	sign = 1ULL << (opsz * 8 - 1);
	if (lhs < rhs)
		rflags |= PSL_C;
	if (__builtin_parity((unsigned int)(result & 0xff)) == 0)
		rflags |= PSL_PF;
	if ((lhs ^ rhs ^ result) & 0x10)
		rflags |= PSL_AF;
	if (result == 0)
		rflags |= PSL_Z;
	if (result & sign)
		rflags |= PSL_N;
	if ((lhs ^ rhs) & (lhs ^ result) & sign)
		rflags |= PSL_V;
	exit->vrs.vrs_gprs[VCPU_REGS_RFLAGS] = rflags;
}

static int
emulate_mov(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	int reg, regshift = 0, ret, opsz;
	uint64_t gpa, data, mask, value_mask;
	mmio_dev_fn_t mmio_fn;

	DPRINTF("%s: entered", __func__);

	if (!(exit->vee.vee_insn_info & VEE_GPA_VALID))
		fatalx("%s: exit information missing GPA", __func__);
	gpa = exit->vee.vee_gpa;

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	switch (insn->insn_opcode.op_encoding) {
	case OP_ENC_FD:		/* Read: From displacement */
	case OP_ENC_RM:		/* Read: mem->reg */
		DPRINTF("%s: read from gva 0x%lx to %s", __func__,
		    insn->insn_gva, str_reg(insn->insn_reg));
		opsz = get_operand_size(insn);
		switch (opsz) {
		case 1:
			reg = insn->insn_reg;
			if (insn->insn_modrm_valid &&
			    insn->insn_prefix.pfx_rex == REX_NONE &&
			    MODRM_REGOP(insn->insn_modrm) >= 4) {
				reg = MODRM_REGOP(insn->insn_modrm) - 4;
				regshift = 8;
			}
			mask = ~(0xffULL << regshift);
			value_mask = 0xff;
			break;
		case 2:
			mask = 0xFFFFFFFFFFFF0000;
			value_mask = 0xFFFF;
			break;
		case 4:
			/* Writes to a 32-bit register zero its upper half */
			mask = 0;
			value_mask = 0xFFFFFFFF;
			break;
		case 8:
			mask = 0;
			value_mask = 0xFFFFFFFFFFFFFFFF;
			break;
		default:
			fatalx("invalid MOV operand size %d", opsz);
		}

		DPRINTF("%s: reading %d bytes to %s, prior value "
		    "0x%llx", __func__, opsz, str_reg(insn->insn_reg),
		    exit->vrs.vrs_gprs[insn->insn_reg]);

		ret = mmio_fn(vcpu_id, MMIO_DIR_READ, gpa, opsz, &data);
		if (!ret) {
			reg = opsz == 1 ? reg : insn->insn_reg;
			exit->vrs.vrs_gprs[reg] &= mask;
			exit->vrs.vrs_gprs[reg] |=
			    (data & value_mask) << regshift;
			DPRINTF("%s: set %s=0x%llx", __func__,
			    str_reg(insn->insn_reg),
			    exit->vrs.vrs_gprs[insn->insn_reg]);
		} else {
			log_warnx("%s: mmio function indicated failure",
			    __func__);
		}
		return (0);
	case OP_ENC_TD:		/* Write: To displacement */
	case OP_ENC_MR:		/* Write: reg->mem */
		DPRINTF("%s: write to gva 0x%lx to %s", __func__,
		    insn->insn_gva, str_reg(insn->insn_reg));
		opsz = get_operand_size(insn);
		reg = insn->insn_reg;
		if (opsz == 1 && insn->insn_modrm_valid &&
		    insn->insn_prefix.pfx_rex == REX_NONE &&
		    MODRM_REGOP(insn->insn_modrm) >= 4) {
			reg = MODRM_REGOP(insn->insn_modrm) - 4;
			regshift = 8;
		}
		data = exit->vrs.vrs_gprs[reg] >> regshift;
		DPRINTF("%s: write 0x%llx to mmio addr 0x%llx", __func__, data,
		    gpa);
		ret = mmio_fn(vcpu_id, MMIO_DIR_WRITE, gpa, opsz, &data);
		if (ret) {
			log_warnx("%s: mmio function indicated failure", __func__);
		}
		return (0);
	case OP_ENC_MI:		/* Write: immediate to mem */
		DPRINTF("%s: write immediate 0x%llx to gva 0x%lx", __func__,
		    insn->insn_immediate, insn->insn_gva);
		opsz = get_operand_size(insn);
		data = insn->insn_immediate;
		ret = mmio_fn(vcpu_id, MMIO_DIR_WRITE, gpa, opsz, &data);
		if (ret) {
			log_warnx("%s: mmio function indicated failure",
			    __func__);
		}
		return (0);
	default:
		log_warnx("%s: unsupported encoding %s", __func__,
		    str_operand_enc(&insn->insn_opcode));
	}

	return (0);
}

#define MMIO_STRING_BATCH	4096
#define EMULATE_RESTART		(-2)

static int
emulate_movs(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	struct vcpu_reg_state *vrs = &exit->vrs;
	struct vcpu_segment_info *srcseg, *dstseg;
	mmio_dev_fn_t srcfn, dstfn;
	uint64_t count, data, dstgpa, dstidx, mask, srcgpa, srcidx;
	size_t batch, i;
	int addrsize, delta, opsz, ret, sreg = VCPU_REGS_DS;

	if (insn->insn_opcode.op_encoding != OP_ENC_ZO)
		return (EINVAL);
	if (insn->insn_prefix.pfx_group2 == LEG_2_CS)
		sreg = VCPU_REGS_CS;
	else if (insn->insn_prefix.pfx_group2 == LEG_2_SS)
		sreg = VCPU_REGS_SS;
	else if (insn->insn_prefix.pfx_group2 == LEG_2_ES)
		sreg = VCPU_REGS_ES;
	else if (insn->insn_prefix.pfx_group2 == LEG_2_FS)
		sreg = VCPU_REGS_FS;
	else if (insn->insn_prefix.pfx_group2 == LEG_2_GS)
		sreg = VCPU_REGS_GS;
	srcseg = &vrs->vrs_sregs[sreg];
	dstseg = &vrs->vrs_sregs[VCPU_REGS_ES];

	if (insn->insn_cpu_mode == VMM_CPU_MODE_LONG)
		addrsize = insn->insn_prefix.pfx_group4 == LEG_4_ADDRSZ ? 4 : 8;
	else if (insn->insn_cpu_mode == VMM_CPU_MODE_PROT32)
		addrsize = insn->insn_prefix.pfx_group4 == LEG_4_ADDRSZ ? 2 : 4;
	else
		addrsize = insn->insn_prefix.pfx_group4 == LEG_4_ADDRSZ ? 4 : 2;
	mask = addrsize == 8 ? UINT64_MAX : (1ULL << (addrsize * 8)) - 1;
	opsz = get_operand_size(insn);
	delta = vrs->vrs_gprs[VCPU_REGS_RFLAGS] & EFLAGS_DF ? -opsz : opsz;
	srcidx = vrs->vrs_gprs[VCPU_REGS_RSI] & mask;
	dstidx = vrs->vrs_gprs[VCPU_REGS_RDI] & mask;
	count = (insn->insn_prefix.pfx_group1 == LEG_1_REP ||
	    insn->insn_prefix.pfx_group1 == LEG_1_REPNE) ?
	    vrs->vrs_gprs[VCPU_REGS_RCX] & mask : 1;
	if (count == 0)
		return (0);
	batch = MMIO_STRING_BATCH / opsz;
	if (batch > count)
		batch = count;

	for (i = 0; i < batch; i++) {
		ret = translate_gva(exit, srcseg->vsi_base + srcidx,
		    &srcgpa, PROT_READ);
		if (ret != 0)
			return (ret);
		ret = translate_gva(exit, dstseg->vsi_base + dstidx,
		    &dstgpa, PROT_WRITE);
		if (ret != 0)
			return (ret);
		srcfn = mmio_find_dev(srcgpa);
		dstfn = mmio_find_dev(dstgpa);
		if (srcfn == NULL && dstfn == NULL)
			fatalx("%s: no functions for src gpa=0x%llx, dst "
			    "gpa=0x%llx", __func__, srcgpa, dstgpa);
		data = 0;
		if (srcfn != NULL)
			ret = srcfn(vcpu_id, MMIO_DIR_READ, srcgpa, opsz,
			    &data);
		else
			ret = read_mem(srcgpa, &data, opsz);
		if (ret != 0)
			return (ret);
		if (dstfn != NULL)
			ret = dstfn(vcpu_id, MMIO_DIR_WRITE, dstgpa, opsz,
			    &data);
		else
			ret = write_mem(dstgpa, &data, opsz);
		if (ret != 0)
			return (ret);
		srcidx = (srcidx + delta) & mask;
		dstidx = (dstidx + delta) & mask;
		if (insn->insn_prefix.pfx_group1 == LEG_1_REP ||
		    insn->insn_prefix.pfx_group1 == LEG_1_REPNE)
			count--;
	}
	vrs->vrs_gprs[VCPU_REGS_RSI] =
	    (vrs->vrs_gprs[VCPU_REGS_RSI] & ~mask) | srcidx;
	vrs->vrs_gprs[VCPU_REGS_RDI] =
	    (vrs->vrs_gprs[VCPU_REGS_RDI] & ~mask) | dstidx;
	if (insn->insn_prefix.pfx_group1 == LEG_1_REP ||
	    insn->insn_prefix.pfx_group1 == LEG_1_REPNE) {
		vrs->vrs_gprs[VCPU_REGS_RCX] =
		    (vrs->vrs_gprs[VCPU_REGS_RCX] & ~mask) | count;
		if (count != 0)
			return (EMULATE_RESTART);
	}
	return (0);
}

static int
emulate_movzx(struct x86_insn *insn, struct vm_exit *exit, uint32_t vcpu_id)
{
	uint8_t byte, len, src = 1, dst = 2;
	uint64_t gpa, mask, value = 0;
	mmio_dev_fn_t mmio_fn;
	int ret;

	/* Only RM is valid for MOVZX. */
	if (insn->insn_opcode.op_encoding != OP_ENC_RM) {
		log_warnx("invalid op encoding for MOVZX: %d",
		    insn->insn_opcode.op_encoding);
		return (-1);
	}

	len = insn->insn_opcode.op_bytes_len;
	if (len < 1 || len > sizeof(insn->insn_opcode.op_bytes)) {
		log_warnx("invalid opcode byte length: %d", len);
		return (-1);
	}

	byte = insn->insn_opcode.op_bytes[len - 1];
	switch (byte) {
	case 0xB6:
		src = 1;
		dst = get_operand_size(insn);
		break;
	case 0xB7:
		src = 2;
		dst = get_operand_size(insn);
		break;
	default:
		log_warnx("invalid byte in MOVZX opcode: %x", byte);
		return (-1);
	}

	if (!(exit->vee.vee_insn_info & VEE_GPA_VALID))
		fatalx("%s: exit information missing GPA", __func__);
	gpa = exit->vee.vee_gpa;

	if ((mmio_fn = mmio_find_dev(gpa)) == NULL)
		mmio_fatal(insn, exit, gpa);

	if ((ret = mmio_fn(vcpu_id, MMIO_DIR_READ, gpa, src, &value)) != 0)
		return (ret);
	mask = src == 1 ? 0xff : 0xffff;
	value &= mask;
	if (dst == 2)
		exit->vrs.vrs_gprs[insn->insn_reg] =
		    (exit->vrs.vrs_gprs[insn->insn_reg] & ~0xffffULL) | value;
	else
		exit->vrs.vrs_gprs[insn->insn_reg] = value;

	return (0);
}

/*
 * insn_emulate
 *
 * Returns:
 *  0: success
 *  EINVAL: exception occurred
 *  EFAULT: page fault occurred, requires retry
 *  ENOTSUP: an unsupported instruction was provided
 */
int
insn_emulate(struct vm_exit *exit, struct x86_insn *insn, uint32_t vcpu_id)
{
	int res;

	switch (insn->insn_opcode.op_type) {
	case OP_ADD:
		res = emulate_add(insn, exit, vcpu_id);
		break;
	case OP_AND:
		res = emulate_and(insn, exit, vcpu_id);
		break;
	case OP_CMP:
		res = emulate_cmp(insn, exit, vcpu_id);
		break;
	case OP_MOV:
		res = emulate_mov(insn, exit, vcpu_id);
		break;
	case OP_MOVS:
		res = emulate_movs(insn, exit, vcpu_id);
		break;
	case OP_MOVZX:
		res = emulate_movzx(insn, exit, vcpu_id);
		break;
	case OP_POP:
		res = emulate_pop(insn, exit, vcpu_id);
		break;
	case OP_PUSH:
		res = emulate_push(insn, exit, vcpu_id);
		break;
	case OP_SUB:
		res = emulate_sub(insn, exit, vcpu_id);
		break;
	case OP_TEST:
		res = emulate_test(insn, exit, vcpu_id);
		break;
	default:
		log_warnx("%s: emulation not defined for %s", __func__,
		    str_opcode(&insn->insn_opcode));
		res = ENOTSUP;
	}

	if (res == EMULATE_RESTART)
		return (0);
	if (res == 0)
		exit->vrs.vrs_gprs[VCPU_REGS_RIP] += insn->insn_bytes_len;

	return (res);
}

void
mmio_init(void)
{
	SLIST_INIT(&mmio_devs);
}

int
mmio_dev_add(paddr_t start, paddr_t end, mmio_dev_fn_t fn)
{
	struct mmio_dev *dev;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return ENOMEM;

	dev->start = start;
	dev->end = end;
	dev->fn = fn;

	SLIST_INSERT_HEAD(&mmio_devs, dev, dev_next);
	log_debug("%s: added mmio handler for range [0x%lx - 0x%lx]",
	    __func__, start, end);

	return 0;
}

mmio_dev_fn_t
mmio_find_dev(paddr_t addr)
{
	struct mmio_dev *dev;

	SLIST_FOREACH(dev, &mmio_devs, dev_next) {
		if (addr >= dev->start &&
		    addr <= dev->end)
			return dev->fn;
	}

	return NULL;
}
