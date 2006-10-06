/*	$OpenBSD: sh_opcode.h,v 1.1.1.1 2006/10/06 21:02:55 miod Exp $	*/
/*	$NetBSD: sh_opcode.h,v 1.3 2002/04/28 17:10:36 uch Exp $ */

typedef union {
	unsigned word;

#if _BYTE_ORDER == BIG_ENDIAN
	struct {
		unsigned op: 16;
	} oType;

	struct {
		unsigned op1: 4;
		unsigned n: 4;
		unsigned op2: 8;
	} nType;

	struct {
		unsigned op1: 4;
		unsigned m: 4;
		unsigned op2: 8;
	} mType;

	struct {
		unsigned op1: 4;
		unsigned n: 4;
		unsigned m: 4;
		unsigned op2: 4;
	} nmType;

	struct {
		unsigned op: 8;
		unsigned m: 4;
		unsigned d: 4;
	} mdType;

	struct {
		unsigned op: 8;
		unsigned n: 4;
		unsigned d: 4;
	} nd4Type;

	struct {
		unsigned op: 4;
		unsigned n: 4;
		unsigned m: 4;
		unsigned d: 4;
	} nmdType;

	struct {
		unsigned op: 8;
		unsigned d: 8;
	} dType;

	struct {
		unsigned op: 4;
		unsigned d: 12;
	} d12Type;

	struct {
		unsigned op: 4;
		unsigned n: 4;
		unsigned d: 8;
	} nd8Type;

	struct {
		unsigned op: 8;
		unsigned i: 8;
	} iType;

	struct {
		unsigned op: 4;
		unsigned n: 4;
		unsigned i: 8;
	} niType;
#endif
#if _BYTE_ORDER == LITTLE_ENDIAN
struct {
		unsigned op: 16;
	} oType;

	struct {
		unsigned op2: 8;
		unsigned n: 4;
		unsigned op1: 4;
	} nType;

	struct {
		unsigned op2: 8;
		unsigned m: 4;
		unsigned op1: 4;
	} mType;

	struct {
		unsigned op2: 4;
		unsigned m: 4;
		unsigned n: 4;
		unsigned op1: 4;
	} nmType;

	struct {
		unsigned d: 4;
		unsigned m: 4;
		unsigned op: 8;
	} mdType;

	struct {
		unsigned d: 4;
		unsigned n: 4;
		unsigned op: 8;
	} nd4Type;

	struct {
		unsigned d: 4;
		unsigned m: 4;
		unsigned n: 4;
		unsigned op: 4;
	} nmdType;

	struct {
		unsigned d: 8;
		unsigned op: 8;
	} dType;

	struct {
		unsigned d: 12;
		unsigned op: 4;
	} d12Type;

	struct {
		unsigned d: 8;
		unsigned n: 4;
		unsigned op: 4;
	} nd8Type;

	struct {
		unsigned i: 8;
		unsigned op: 8;
	} iType;

	struct {
		unsigned i: 8;
		unsigned n: 4;
		unsigned op: 4;
	} niType;
#endif
} InstFmt;

#define	OP_BF	0x8b
#define	OP_BFS	0x8f
#define	OP_BT	0x89
#define	OP_BTS	0x8d
#define	OP_BRA	0xa
#define	OP_BSR	0xb
#define	OP1_BRAF	0x0
#define	OP2_BRAF	0x23
#define	OP1_BSRF	0x0
#define	OP2_BSRF	0x03
#define	OP1_JMP	0x4
#define	OP2_JMP	0x2b
#define	OP1_JSR	0x4
#define	OP2_JSR	0x0b
#define	OP_RTS	0xffff
