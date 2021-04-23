/*
 * Copyright (c) 2016 Dale Rahn <drahn@dalerahn.com>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_access.h>

#define RV64_MASK0 0xFC00707F	/* 11111100000000000111000001111111 */
#define RV64_MASK1 0x0600007F	/* 00000110000000000000000001111111 */
#define RV64_MASK2 0xFFF0707F	/* 11111111111100000111000001111111 */
#define RV64_MASK3 0xFFF0007F	/* 11111111111100000000000001111111 */
#define RV64_MASK4 0xFE00007F	/* 11111110000000000000000001111111 */
#define RV64_MASK5 0x0C00007F	/* 00001100000000000000000001111111 */
#define RV64_MASK6 0xF800707F	/* 11111000000000000111000001111111 */
#define RV64_MASK7 0xF9F0707F	/* 11111001111100000111000001111111 */
#define RV64_MASK8 0xFE007FFF	/* 11111110000000000111111111111111 */
#define RV64_MASK9 0xFFFFFFFF	/* 11111111111111111111111111111111 */
#define RV64_MASK10 0xF01FFFFF	/* 11110000000111111111111111111111 */
#define RV64_MASK11 0xFE00707F	/* 11111110000000000111000001111111 */
#define RV64_MASK12 0x0000707F	/* 00000000000000000111000001111111 */
#define RV64_MASK13 0x0000007F	/* 00000000000000000000000001111111 */

#define RV64I_LUI_OPCODE    	0x00000037	/* lui */
#define RV64I_AUIPC_OPCODE  	0x00000017	/* auipc */
#define RV64I_JAL_OPCODE    	0x0000006F	/* jal */
#define RV64I_JALR_OPCODE   	0x00000067	/* jalr */
#define RV64I_BEQ_OPCODE    	0x00000063	/* beq */
#define RV64I_BNE_OPCODE    	0x00001063	/* bne */
#define RV64I_BLT_OPCODE    	0x00004063	/* blt */
#define RV64I_BGE_OPCODE    	0x00005063	/* bge */
#define RV64I_BLTU_OPCODE   	0x00006063	/* bltu */
#define RV64I_BGEU_OPCODE   	0x00007063	/* bgeu */
#define RV64I_LB_OPCODE     	0x00000003	/* lb */
#define RV64I_LH_OPCODE     	0x00001003	/* lh */
#define RV64I_LHU_OPCODE    	0x00005003	/* lhu */
#define RV64I_LW_OPCODE     	0x00002003	/* lw */
#define RV64I_LBU_OPCODE    	0x00004003	/* lbu */
#define RV64I_SB_OPCODE     	0x00000023	/* sb */
#define RV64I_SH_OPCODE     	0x00001023	/* sh */
#define RV64I_SW_OPCODE     	0x00002023	/* sw */
#define RV64I_ADDI_OPCODE   	0x00000013	/* addi */
#define RV64I_SLTI_OPCODE   	0x00002013	/* slti */
#define RV64I_SLTIU_OPCODE  	0x00003013	/* sltiu */
#define RV64I_XORI_OPCODE   	0x00004013	/* xori */
#define RV64I_ORI_OPCODE    	0x00006013	/* ori */
#define RV64I_ANDI_OPCODE   	0x00007013	/* andi */
#define RV64I_ADD_OPCODE    	0x00000033	/* add */
#define RV64I_SUB_OPCODE    	0x40000033	/* sub */
#define RV64I_SLL_OPCODE    	0x00001033	/* sll */
#define RV64I_SLT_OPCODE    	0x00002033	/* slt */
#define RV64I_SLTU_OPCODE   	0x00003033	/* sltu */
#define RV64I_XOR_OPCODE    	0x00004033	/* xor */
#define RV64I_SRL_OPCODE    	0x00005033	/* srl */
#define RV64I_SRA_OPCODE    	0x40005033	/* sra */
#define RV64I_OR_OPCODE     	0x00006033	/* or */
#define RV64I_AND_OPCODE    	0x00007033	/* and */
#define RV64I_FENCE_OPCODE  	0x0000000F	/* fence */
#define RV64I_FENCE_I_OPCODE	0x0000100F	/* fence.i */
#define RV64I_WFI_OPCODE    	0x10500073	/* wfi */
#define RV64I_SFENCE_VMA_OPCODE	0x120000E7	/* sfence.vma */
#define RV64I_ECALL_OPCODE  	0x00000073	/* ecall */
#define RV64I_EBREAK_OPCODE 	0x00100073	/* ebreak */
#define RV64I_CSRRW_OPCODE  	0x00001073	/* csrrw */
#define RV64I_CSRRS_OPCODE  	0x00002073	/* csrrs */
#define RV64I_CSRRC_OPCODE  	0x00003073	/* csrrc */
#define RV64I_CSRRWI_OPCODE 	0x00005073	/* csrrwi */
#define RV64I_CSRRSI_OPCODE 	0x00006073	/* csrrsi */
#define RV64I_CSRRCI_OPCODE 	0x00007073	/* csrrci */
#define RV64I_SRET_OPCODE   	0x10200073	/* sret */
#define RV64I_MRET_OPCODE   	0x30200073	/* mret */
#define RV64M_MUL_OPCODE    	0x02000033	/* mul */
#define RV64M_MULH_OPCODE   	0x02001033	/* mulh */
#define RV64M_MULHSU_OPCODE 	0x02002033	/* mulhsu */
#define RV64M_MULHU_OPCODE  	0x02003033	/* mulhu */
#define RV64M_DIV_OPCODE    	0x02004033	/* div */
#define RV64M_DIVU_OPCODE   	0x02005033	/* divu */
#define RV64M_REM_OPCODE    	0x02006033	/* rem */
#define RV64M_REMU_OPCODE   	0x02007033	/* remu */
#define RV64A_LR_W_OPCODE   	0x1000202F	/* lr.w */
#define RV64A_SC_W_OPCODE   	0x1800202F	/* sc.w */
#define RV64A_AMOSWAP_W_OPCODE	0x0800202F	/* amoswap.w */
#define RV64A_AMOADD_W_OPCODE	0x0000202F	/* amoadd.w */
#define RV64A_AMOXOR_W_OPCODE	0x2000202F	/* amoxor.w */
#define RV64A_AMOAND_W_OPCODE	0x6000202F	/* amoand.w */
#define RV64A_AMOOR_W_OPCODE	0x4000202F	/* amoor.w */
#define RV64A_AMOMIN_W_OPCODE	0x8000202F	/* amomin.w */
#define RV64A_AMOMAX_W_OPCODE	0xA000202F	/* amomax.w */
#define RV64A_AMOMINU_W_OPCODE	0xC000202F	/* amominu.w */
#define RV64A_AMOMAXU_W_OPCODE	0xE000202F	/* amomaxu.w */
#define RV64F_FLW_OPCODE    	0x00002007	/* flw */
#define RV64F_FSW_OPCODE    	0x00002027	/* fsw */
#define RV64F_FMADD_S_OPCODE	0x00000043	/* fmadd.s */
#define RV64F_FMSUB_S_OPCODE	0x00000047	/* fmsub.s */
#define RV64F_FNMSUB_S_OPCODE	0x0000004B	/* fnmsub.s */
#define RV64F_FNMADD_S_OPCODE	0x0000004F	/* fnmadd.s */
#define RV64F_FADD_S_OPCODE 	0x00000053	/* fadd.s */
#define RV64F_FSUB_S_OPCODE 	0x08000053	/* fsub.s */
#define RV64F_FMUL_S_OPCODE 	0x10000053	/* fmul.s */
#define RV64F_FDIV_S_OPCODE 	0x18000053	/* fdiv.s */
#define RV64F_FSQRT_S_OPCODE	0x58000053	/* fsqrt.s */
#define RV64F_FSGNJ_S_OPCODE	0x20000053	/* fsgnj.s */
#define RV64F_FSGNJN_S_OPCODE	0x20001053	/* fsgnjn.s */
#define RV64F_FSGNJX_S_OPCODE	0x20002053	/* fsgnjx.s */
#define RV64F_FMIN_S_OPCODE 	0x28000053	/* fmin.s */
#define RV64F_FMAX_S_OPCODE 	0x28001053	/* fmax.s */
#define RV64F_FMAX_S_OPCODE 	0x28001053	/* fmax.s */
#define RV64F_FCVT_W_S_OPCODE	0xC0000053	/* fcvt.w.s */
#define RV64F_FCVT_WU_S_OPCODE	0xC0100053	/* fcvt.wu.s */
#define RV64F_FMV_X_W_OPCODE	0xE0000053	/* fmv.x.w */
#define RV64F_FEQ_S_OPCODE  	0xA0002053	/* feq.s */
#define RV64F_FLT_S_OPCODE  	0xA0001053	/* flt.s */
#define RV64F_FLE_S_OPCODE  	0xA0000053	/* fle.s */
#define RV64F_FCLASS_S_OPCODE	0xE0001053	/* fclass.s */
#define RV64F_FCVT_S_W_OPCODE	0xD0000053	/* fcvt.s.w */
#define RV64F_FCVT_S_WU_OPCODE	0xD0100053	/* fcvt.s.wu */
#define RV64F_FMV_W_X_OPCODE	0xF0000053	/* fmv.w.x */
#define RV64D_FLD_OPCODE    	0x00003007	/* fld */
#define RV64D_FSD_OPCODE    	0x00003027	/* fsd */
#define RV64D_FMADD_D_OPCODE	0x00000043	/* fmadd.d */
#define RV64D_FMSUB_D_OPCODE	0x00000047	/* fmsub.d */
#define RV64D_FNMSUB_D_OPCODE	0x0000004B	/* fnmsub.d */
#define RV64D_FNMADD_D_OPCODE	0x0000004F	/* fnmadd.d */
#define RV64D_FADD_D_OPCODE 	0x02000053	/* fadd.d */
#define RV64D_FSUB_D_OPCODE 	0x0A000053	/* fsub.d */
#define RV64D_FMUL_D_OPCODE 	0x12000053	/* fmul.d */
#define RV64D_FDIV_D_OPCODE 	0x1A000053	/* fdiv.d */
#define RV64D_FSQRT_D_OPCODE	0x5A000053	/* fsqrt.d */
#define RV64D_FSGNJ_D_OPCODE	0x22000053	/* fsgnj.d */
#define RV64D_FSGNJN_D_OPCODE	0x22001053	/* fsgnjn.d */
#define RV64D_FSGNJX_D_OPCODE	0x22002053	/* fsgnjx.d */
#define RV64D_FMIN_D_OPCODE 	0x2A000053	/* fmin.d */
#define RV64D_FMAX_D_OPCODE 	0x2A001053	/* fmax.d */
#define RV64D_FCVT_S_D_OPCODE	0x40100053	/* fcvt.s.d */
#define RV64D_FCVT_D_S_OPCODE	0x42000053	/* fcvt.d.s */
#define RV64D_FEQ_D_OPCODE  	0xA2002053	/* feq.d */
#define RV64D_FLT_D_OPCODE  	0xA2001053	/* flt.d */
#define RV64D_FLE_D_OPCODE  	0xA2000053	/* fle.d */
#define RV64D_FCLASS_D_OPCODE	0xE2001053	/* fclass.d */
#define RV64D_FCVT_W_D_OPCODE	0xC2000053	/* fcvt.w.d */
#define RV64D_FCVT_WU_D_OPCODE	0xC2100053	/* fcvt.wu.d */
#define RV64D_FCVT_D_W_OPCODE	0xD2000053	/* fcvt.d.w */
#define RV64D_FCVT_D_WU_OPCODE	0xD2100053	/* fcvt.d.wu */
#define RV64I_LWU_OPCODE    	0x00006003	/* lwu */
#define RV64I_LD_OPCODE     	0x00003003	/* ld */
#define RV64I_SD_OPCODE     	0x00003023	/* sd */
#define RV64I_SLLI_OPCODE   	0x00001013	/* slli */
#define RV64I_SRLI_OPCODE   	0x00005013	/* srli */
#define RV64I_SRAI_OPCODE   	0x40005013	/* srai */
#define RV64I_ADDIW_OPCODE  	0x0000001B	/* addiw */
#define RV64I_SLLIW_OPCODE  	0x0000101B	/* slliw */
#define RV64I_SRLIW_OPCODE  	0x0000501B	/* srliw */
#define RV64I_SRAIW_OPCODE  	0x4000501B	/* sraiw */
#define RV64I_ADDW_OPCODE   	0x0000003B	/* addw */
#define RV64I_SUBW_OPCODE   	0x4000003B	/* subw */
#define RV64I_SLLW_OPCODE   	0x0000103B	/* sllw */
#define RV64I_SRLW_OPCODE   	0x0000503B	/* srlw */
#define RV64I_SRAW_OPCODE   	0x4000503B	/* sraw */
#define RV64M_MULW_OPCODE   	0x0200003B	/* mulw */
#define RV64M_DIVW_OPCODE   	0x0200403B	/* divw */
#define RV64M_DIVUW_OPCODE  	0x0200503B	/* divuw */
#define RV64M_REMW_OPCODE   	0x0200603B	/* remw */
#define RV64M_REMUW_OPCODE  	0x0200703B	/* remuw */
#define RV64A_LR_D_OPCODE   	0x1000302F	/* lr.d */
#define RV64A_SC_D_OPCODE   	0x1800302F	/* sc.d */
#define RV64A_AMOSWAP_D_OPCODE	0x0800302F	/* amoswap.d */
#define RV64A_AMOADD_D_OPCODE	0x0000302F	/* amoadd.d */
#define RV64A_AMOXOR_D_OPCODE	0x2000302F	/* amoxor.d */
#define RV64A_AMOAND_D_OPCODE	0x6000302F	/* amoand.d */
#define RV64A_AMOOR_D_OPCODE	0x4000302F	/* amoor.d */
#define RV64A_AMOMIN_D_OPCODE	0x8000302F	/* amomin.d */
#define RV64A_AMOMAX_D_OPCODE	0xA000302F	/* amomax.d */
#define RV64A_AMOMAX_D_OPCODE	0xA000302F	/* amomax.d */
#define RV64A_AMOMINU_D_OPCODE	0xC000302F	/* amominu.d */
#define RV64A_AMOMAXU_D_OPCODE	0xE000302F	/* amomaxu.d */
#define RV64F_FCVT_L_S_OPCODE	0xC0200053	/* fcvt.l.s */
#define RV64F_FCVT_LU_S_OPCODE	0xC0300053	/* fcvt.lu.s */
#define RV64F_FCVT_S_L_OPCODE	0xD0200053	/* fcvt.s.l */
#define RV64F_FCVT_S_LU_OPCODE	0xD0300053	/* fcvt.s.lu */
#define RV64D_FCVT_L_D_OPCODE	0xC2200053	/* fcvt.l.d */
#define RV64D_FCVT_LU_D_OPCODE	0xC2300053	/* fcvt.lu.d */
#define RV64D_FMV_X_D_OPCODE	0xE2000053	/* fmv.x.d */
#define RV64D_FCVT_D_L_OPCODE	0xD2200053	/* fcvt.d.l */
#define RV64D_FCVT_D_LU_OPCODE	0xD2300053	/* fcvt.d.lu */
#define RV64D_FMV_D_X_OPCODE	0xF2000053	/* fmv.d.x */
#define RV64Q_URET_OPCODE   	0x00200073	/* uret */
#define RV64Q_DRET_OPCODE   	0x7B200073	/* dret */
#define RV64Q_FADD_Q_OPCODE 	0x06000053	/* fadd.q */
#define RV64Q_FSUB_Q_OPCODE 	0x0E000053	/* fsub.q */
#define RV64Q_FMUL_Q_OPCODE 	0x16000053	/* fmul.q */
#define RV64Q_FDIV_Q_OPCODE 	0x1E000053	/* fdiv.q */
#define RV64Q_FSGNJ_Q_OPCODE	0x26000053	/* fsgnj.q */
#define RV64Q_FSGNJN_Q_OPCODE	0x26001053	/* fsgnjn.q */
#define RV64Q_FSGNJX_Q_OPCODE	0x26002053	/* fsgnjx.q */
#define RV64Q_FMIN_Q_OPCODE 	0x2E000053	/* fmin.q */
#define RV64Q_FMAX_Q_OPCODE 	0x2E001053	/* fmax.q */
#define RV64Q_FCVT_S_Q_OPCODE	0x40300053	/* fcvt.s.q */
#define RV64Q_FCVT_Q_S_OPCODE	0x46000053	/* fcvt.q.s */
#define RV64Q_FCVT_D_Q_OPCODE	0x42300053	/* fcvt.d.q */
#define RV64Q_FCVT_Q_D_OPCODE	0x46100053	/* fcvt.q.d */
#define RV64Q_FSQRT_Q_OPCODE	0x5E000053	/* fsqrt.q */
#define RV64Q_FLE_Q_OPCODE  	0xA6000053	/* fle.q */
#define RV64Q_FLT_Q_OPCODE  	0xA6001053	/* flt.q */
#define RV64Q_FEQ_Q_OPCODE  	0xA6002053	/* feq.q */
#define RV64Q_FCVT_W_Q_OPCODE	0xC6000053	/* fcvt.w.q */
#define RV64Q_FCVT_WU_Q_OPCODE	0xC6100053	/* fcvt.wu.q */
#define RV64Q_FCVT_L_Q_OPCODE	0xC6200053	/* fcvt.l.q */
#define RV64Q_FCVT_LU_Q_OPCODE	0xC6300053	/* fcvt.lu.q */
#define RV64Q_FMV_X_Q_OPCODE	0xE6000053	/* fmv.x.q */
#define RV64Q_FCLASS_Q_OPCODE	0xE6001053	/* fclass.q */
#define RV64Q_FCVT_Q_W_OPCODE	0xD6000053	/* fcvt.q.w */
#define RV64Q_FCVT_Q_WU_OPCODE	0xD6100053	/* fcvt.q.wu */
#define RV64Q_FCVT_Q_L_OPCODE	0xD6200053	/* fcvt.q.l */
#define RV64Q_FCVT_Q_LU_OPCODE	0xD6300053	/* fcvt.q.lu */
#define RV64Q_FMV_Q_X_OPCODE	0xF6000053	/* fmv.q.x */
#define RV64Q_FLQ_OPCODE    	0x00004007	/* flq */
#define RV64Q_FSQ_OPCODE    	0x00004027	/* fsq */
#define RV64Q_FMADD_Q_OPCODE	0x06000043	/* fmadd.q */
#define RV64Q_FMSUB_Q_OPCODE	0x06000047	/* fmsub.q */
#define RV64Q_FNMSUB_Q_OPCODE	0x0600004B	/* fnmsub.q */
#define RV64Q_FNMADD_Q_OPCODE	0x0600004F	/* fnmadd.q */

struct rv64_op {
	char *opcode;
	uint32_t num_op;
	uint32_t num_mask;
} rv64_opcodes[] = {
	{ "lui", RV64I_LUI_OPCODE, RV64_MASK13 },
	{ "auipc", RV64I_AUIPC_OPCODE, RV64_MASK13 },
	{ "jal", RV64I_JAL_OPCODE, RV64_MASK13 },
	{ "jalr", RV64I_JALR_OPCODE, RV64_MASK12 },
	{ "beq", RV64I_BEQ_OPCODE, RV64_MASK12 },
	{ "bne", RV64I_BNE_OPCODE, RV64_MASK12 },
	{ "blt", RV64I_BLT_OPCODE, RV64_MASK12 },
	{ "bge", RV64I_BGE_OPCODE, RV64_MASK12 },
	{ "bltu", RV64I_BLTU_OPCODE, RV64_MASK12 },
	{ "bgeu", RV64I_BGEU_OPCODE, RV64_MASK12 },
	{ "lb", RV64I_LB_OPCODE, RV64_MASK12 },
	{ "lh", RV64I_LH_OPCODE, RV64_MASK12 },
	{ "lhu", RV64I_LHU_OPCODE, RV64_MASK12 },
	{ "lw", RV64I_LW_OPCODE, RV64_MASK12 },
	{ "lbu", RV64I_LBU_OPCODE, RV64_MASK12 },
	{ "sb", RV64I_SB_OPCODE, RV64_MASK12 },
	{ "sh", RV64I_SH_OPCODE, RV64_MASK12 },
	{ "sw", RV64I_SW_OPCODE, RV64_MASK12 },
	{ "addi", RV64I_ADDI_OPCODE, RV64_MASK12 },
	{ "slti", RV64I_SLTI_OPCODE, RV64_MASK12 },
	{ "sltiu", RV64I_SLTIU_OPCODE, RV64_MASK12 },
	{ "xori", RV64I_XORI_OPCODE, RV64_MASK12 },
	{ "ori", RV64I_ORI_OPCODE, RV64_MASK12 },
	{ "andi", RV64I_ANDI_OPCODE, RV64_MASK12 },
	{ "add", RV64I_ADD_OPCODE, RV64_MASK11 },
	{ "sub", RV64I_SUB_OPCODE, RV64_MASK11 },
	{ "sll", RV64I_SLL_OPCODE, RV64_MASK11 },
	{ "slt", RV64I_SLT_OPCODE, RV64_MASK11 },
	{ "sltu", RV64I_SLTU_OPCODE, RV64_MASK11 },
	{ "xor", RV64I_XOR_OPCODE, RV64_MASK11 },
	{ "srl", RV64I_SRL_OPCODE, RV64_MASK11 },
	{ "sra", RV64I_SRA_OPCODE, RV64_MASK11 },
	{ "or", RV64I_OR_OPCODE, RV64_MASK11 },
	{ "and", RV64I_AND_OPCODE, RV64_MASK11 },
	{ "fence", RV64I_FENCE_OPCODE, RV64_MASK10 },
	{ "fence.i", RV64I_FENCE_I_OPCODE, RV64_MASK9 },
	{ "wfi", RV64I_WFI_OPCODE, RV64_MASK9 },
	{ "sfence.vma", RV64I_SFENCE_VMA_OPCODE, RV64_MASK8 },
	{ "ecall", RV64I_ECALL_OPCODE, RV64_MASK9 },
	{ "ebreak", RV64I_EBREAK_OPCODE, RV64_MASK9 },
	{ "csrrw", RV64I_CSRRW_OPCODE, RV64_MASK12 },
	{ "csrrs", RV64I_CSRRS_OPCODE, RV64_MASK12 },
	{ "csrrc", RV64I_CSRRC_OPCODE, RV64_MASK12 },
	{ "csrrwi", RV64I_CSRRWI_OPCODE, RV64_MASK12 },
	{ "csrrsi", RV64I_CSRRSI_OPCODE, RV64_MASK12 },
	{ "csrrci", RV64I_CSRRCI_OPCODE, RV64_MASK12 },
	{ "sret", RV64I_SRET_OPCODE, RV64_MASK9 },
	{ "mret", RV64I_MRET_OPCODE, RV64_MASK9 },
	{ "mul", RV64M_MUL_OPCODE, RV64_MASK11 },
	{ "mulh", RV64M_MULH_OPCODE, RV64_MASK11 },
	{ "mulhsu", RV64M_MULHSU_OPCODE, RV64_MASK11 },
	{ "mulhu", RV64M_MULHU_OPCODE, RV64_MASK11 },
	{ "div", RV64M_DIV_OPCODE, RV64_MASK11 },
	{ "divu", RV64M_DIVU_OPCODE, RV64_MASK11 },
	{ "rem", RV64M_REM_OPCODE, RV64_MASK11 },
	{ "remu", RV64M_REMU_OPCODE, RV64_MASK11 },
	{ "lr.w", RV64A_LR_W_OPCODE, RV64_MASK7 },
	{ "sc.w", RV64A_SC_W_OPCODE, RV64_MASK6 },
	{ "amoswap.w", RV64A_AMOSWAP_W_OPCODE, RV64_MASK6 },
	{ "amoadd.w", RV64A_AMOADD_W_OPCODE, RV64_MASK6 },
	{ "amoxor.w", RV64A_AMOXOR_W_OPCODE, RV64_MASK6 },
	{ "amoand.w", RV64A_AMOAND_W_OPCODE, RV64_MASK6 },
	{ "amoor.w", RV64A_AMOOR_W_OPCODE, RV64_MASK6 },
	{ "amomin.w", RV64A_AMOMIN_W_OPCODE, RV64_MASK6 },
	{ "amomax.w", RV64A_AMOMAX_W_OPCODE, RV64_MASK6 },
	{ "amominu.w", RV64A_AMOMINU_W_OPCODE, RV64_MASK6 },
	{ "amomaxu.w", RV64A_AMOMAXU_W_OPCODE, RV64_MASK6 },
	{ "flw", RV64F_FLW_OPCODE, RV64_MASK12 },
	{ "fsw", RV64F_FSW_OPCODE, RV64_MASK12 },
	{ "fmadd.s", RV64F_FMADD_S_OPCODE, RV64_MASK5 },
	{ "fmsub.s", RV64F_FMSUB_S_OPCODE, RV64_MASK5 },
	{ "fnmsub.s", RV64F_FNMSUB_S_OPCODE, RV64_MASK5 },
	{ "fnmadd.s", RV64F_FNMADD_S_OPCODE, RV64_MASK5 },
	{ "fadd.s", RV64F_FADD_S_OPCODE, RV64_MASK4 },
	{ "fsub.s", RV64F_FSUB_S_OPCODE, RV64_MASK4 },
	{ "fmul.s", RV64F_FMUL_S_OPCODE, RV64_MASK4 },
	{ "fdiv.s", RV64F_FDIV_S_OPCODE, RV64_MASK4 },
	{ "fsqrt.s", RV64F_FSQRT_S_OPCODE, RV64_MASK3 },
	{ "fsgnj.s", RV64F_FSGNJ_S_OPCODE, RV64_MASK11 },
	{ "fsgnjn.s", RV64F_FSGNJN_S_OPCODE, RV64_MASK11 },
	{ "fsgnjx.s", RV64F_FSGNJX_S_OPCODE, RV64_MASK11 },
	{ "fmin.s", RV64F_FMIN_S_OPCODE, RV64_MASK11 },
	{ "fmax.s", RV64F_FMAX_S_OPCODE, RV64_MASK11 },
	{ "fmax.s", RV64F_FMAX_S_OPCODE, RV64_MASK11 },
	{ "fcvt.w.s", RV64F_FCVT_W_S_OPCODE, RV64_MASK3 },
	{ "fcvt.wu.s", RV64F_FCVT_WU_S_OPCODE, RV64_MASK3 },
	{ "fmv.x.w", RV64F_FMV_X_W_OPCODE, RV64_MASK2 },
	{ "feq.s", RV64F_FEQ_S_OPCODE, RV64_MASK11 },
	{ "flt.s", RV64F_FLT_S_OPCODE, RV64_MASK11 },
	{ "fle.s", RV64F_FLE_S_OPCODE, RV64_MASK11 },
	{ "fclass.s", RV64F_FCLASS_S_OPCODE, RV64_MASK2 },
	{ "fcvt.s.w", RV64F_FCVT_S_W_OPCODE, RV64_MASK3 },
	{ "fcvt.s.wu", RV64F_FCVT_S_WU_OPCODE, RV64_MASK3 },
	{ "fmv.w.x", RV64F_FMV_W_X_OPCODE, RV64_MASK2 },
	{ "fld", RV64D_FLD_OPCODE, RV64_MASK12 },
	{ "fsd", RV64D_FSD_OPCODE, RV64_MASK12 },
	{ "fmadd.d", RV64D_FMADD_D_OPCODE, RV64_MASK1 },
	{ "fmsub.d", RV64D_FMSUB_D_OPCODE, RV64_MASK1 },
	{ "fnmsub.d", RV64D_FNMSUB_D_OPCODE, RV64_MASK1 },
	{ "fnmadd.d", RV64D_FNMADD_D_OPCODE, RV64_MASK1 },
	{ "fadd.d", RV64D_FADD_D_OPCODE, RV64_MASK4 },
	{ "fsub.d", RV64D_FSUB_D_OPCODE, RV64_MASK4 },
	{ "fmul.d", RV64D_FMUL_D_OPCODE, RV64_MASK4 },
	{ "fdiv.d", RV64D_FDIV_D_OPCODE, RV64_MASK4 },
	{ "fsqrt.d", RV64D_FSQRT_D_OPCODE, RV64_MASK3 },
	{ "fsgnj.d", RV64D_FSGNJ_D_OPCODE, RV64_MASK11 },
	{ "fsgnjn.d", RV64D_FSGNJN_D_OPCODE, RV64_MASK11 },
	{ "fsgnjx.d", RV64D_FSGNJX_D_OPCODE, RV64_MASK11 },
	{ "fmin.d", RV64D_FMIN_D_OPCODE, RV64_MASK11 },
	{ "fmax.d", RV64D_FMAX_D_OPCODE, RV64_MASK11 },
	{ "fcvt.s.d", RV64D_FCVT_S_D_OPCODE, RV64_MASK3 },
	{ "fcvt.d.s", RV64D_FCVT_D_S_OPCODE, RV64_MASK3 },
	{ "feq.d", RV64D_FEQ_D_OPCODE, RV64_MASK11 },
	{ "flt.d", RV64D_FLT_D_OPCODE, RV64_MASK11 },
	{ "fle.d", RV64D_FLE_D_OPCODE, RV64_MASK11 },
	{ "fclass.d", RV64D_FCLASS_D_OPCODE, RV64_MASK2 },
	{ "fcvt.w.d", RV64D_FCVT_W_D_OPCODE, RV64_MASK3 },
	{ "fcvt.wu.d", RV64D_FCVT_WU_D_OPCODE, RV64_MASK3 },
	{ "fcvt.d.w", RV64D_FCVT_D_W_OPCODE, RV64_MASK3 },
	{ "fcvt.d.wu", RV64D_FCVT_D_WU_OPCODE, RV64_MASK3 },
	{ "lwu", RV64I_LWU_OPCODE, RV64_MASK12 },
	{ "ld", RV64I_LD_OPCODE, RV64_MASK12 },
	{ "sd", RV64I_SD_OPCODE, RV64_MASK12 },
	{ "slli", RV64I_SLLI_OPCODE, RV64_MASK0 },
	{ "srli", RV64I_SRLI_OPCODE, RV64_MASK0 },
	{ "srai", RV64I_SRAI_OPCODE, RV64_MASK0 },
	{ "addiw", RV64I_ADDIW_OPCODE, RV64_MASK12 },
	{ "slliw", RV64I_SLLIW_OPCODE, RV64_MASK11 },
	{ "srliw", RV64I_SRLIW_OPCODE, RV64_MASK11 },
	{ "sraiw", RV64I_SRAIW_OPCODE, RV64_MASK11 },
	{ "addw", RV64I_ADDW_OPCODE, RV64_MASK11 },
	{ "subw", RV64I_SUBW_OPCODE, RV64_MASK11 },
	{ "sllw", RV64I_SLLW_OPCODE, RV64_MASK11 },
	{ "srlw", RV64I_SRLW_OPCODE, RV64_MASK11 },
	{ "sraw", RV64I_SRAW_OPCODE, RV64_MASK11 },
	{ "mulw", RV64M_MULW_OPCODE, RV64_MASK11 },
	{ "divw", RV64M_DIVW_OPCODE, RV64_MASK11 },
	{ "divuw", RV64M_DIVUW_OPCODE, RV64_MASK11 },
	{ "remw", RV64M_REMW_OPCODE, RV64_MASK11 },
	{ "remuw", RV64M_REMUW_OPCODE, RV64_MASK11 },
	{ "lr.d", RV64A_LR_D_OPCODE, RV64_MASK7 },
	{ "sc.d", RV64A_SC_D_OPCODE, RV64_MASK6 },
	{ "amoswap.d", RV64A_AMOSWAP_D_OPCODE, RV64_MASK6 },
	{ "amoadd.d", RV64A_AMOADD_D_OPCODE, RV64_MASK6 },
	{ "amoxor.d", RV64A_AMOXOR_D_OPCODE, RV64_MASK6 },
	{ "amoand.d", RV64A_AMOAND_D_OPCODE, RV64_MASK6 },
	{ "amoor.d", RV64A_AMOOR_D_OPCODE, RV64_MASK6 },
	{ "amomin.d", RV64A_AMOMIN_D_OPCODE, RV64_MASK6 },
	{ "amomax.d", RV64A_AMOMAX_D_OPCODE, RV64_MASK6 },
	{ "amomax.d", RV64A_AMOMAX_D_OPCODE, RV64_MASK6 },
	{ "amominu.d", RV64A_AMOMINU_D_OPCODE, RV64_MASK6 },
	{ "amomaxu.d", RV64A_AMOMAXU_D_OPCODE, RV64_MASK6 },
	{ "fcvt.l.s", RV64F_FCVT_L_S_OPCODE, RV64_MASK3 },
	{ "fcvt.lu.s", RV64F_FCVT_LU_S_OPCODE, RV64_MASK3 },
	{ "fcvt.s.l", RV64F_FCVT_S_L_OPCODE, RV64_MASK3 },
	{ "fcvt.s.lu", RV64F_FCVT_S_LU_OPCODE, RV64_MASK3 },
	{ "fcvt.l.d", RV64D_FCVT_L_D_OPCODE, RV64_MASK3 },
	{ "fcvt.lu.d", RV64D_FCVT_LU_D_OPCODE, RV64_MASK3 },
	{ "fmv.x.d", RV64D_FMV_X_D_OPCODE, RV64_MASK2 },
	{ "fcvt.d.l", RV64D_FCVT_D_L_OPCODE, RV64_MASK3 },
	{ "fcvt.d.lu", RV64D_FCVT_D_LU_OPCODE, RV64_MASK3 },
	{ "fmv.d.x", RV64D_FMV_D_X_OPCODE, RV64_MASK2 },
	{ "uret", RV64Q_URET_OPCODE, RV64_MASK9 },
	{ "dret", RV64Q_DRET_OPCODE, RV64_MASK9 },
	{ "fadd.q", RV64Q_FADD_Q_OPCODE, RV64_MASK4 },
	{ "fsub.q", RV64Q_FSUB_Q_OPCODE, RV64_MASK4 },
	{ "fmul.q", RV64Q_FMUL_Q_OPCODE, RV64_MASK4 },
	{ "fdiv.q", RV64Q_FDIV_Q_OPCODE, RV64_MASK4 },
	{ "fsgnj.q", RV64Q_FSGNJ_Q_OPCODE, RV64_MASK11 },
	{ "fsgnjn.q", RV64Q_FSGNJN_Q_OPCODE, RV64_MASK11 },
	{ "fsgnjx.q", RV64Q_FSGNJX_Q_OPCODE, RV64_MASK11 },
	{ "fmin.q", RV64Q_FMIN_Q_OPCODE, RV64_MASK11 },
	{ "fmax.q", RV64Q_FMAX_Q_OPCODE, RV64_MASK11 },
	{ "fcvt.s.q", RV64Q_FCVT_S_Q_OPCODE, RV64_MASK3 },
	{ "fcvt.q.s", RV64Q_FCVT_Q_S_OPCODE, RV64_MASK3 },
	{ "fcvt.d.q", RV64Q_FCVT_D_Q_OPCODE, RV64_MASK3 },
	{ "fcvt.q.d", RV64Q_FCVT_Q_D_OPCODE, RV64_MASK3 },
	{ "fsqrt.q", RV64Q_FSQRT_Q_OPCODE, RV64_MASK3 },
	{ "fle.q", RV64Q_FLE_Q_OPCODE, RV64_MASK11 },
	{ "flt.q", RV64Q_FLT_Q_OPCODE, RV64_MASK11 },
	{ "feq.q", RV64Q_FEQ_Q_OPCODE, RV64_MASK11 },
	{ "fcvt.w.q", RV64Q_FCVT_W_Q_OPCODE, RV64_MASK3 },
	{ "fcvt.wu.q", RV64Q_FCVT_WU_Q_OPCODE, RV64_MASK3 },
	{ "fcvt.l.q", RV64Q_FCVT_L_Q_OPCODE, RV64_MASK3 },
	{ "fcvt.lu.q", RV64Q_FCVT_LU_Q_OPCODE, RV64_MASK3 },
	{ "fmv.x.q", RV64Q_FMV_X_Q_OPCODE, RV64_MASK2 },
	{ "fclass.q", RV64Q_FCLASS_Q_OPCODE, RV64_MASK2 },
	{ "fcvt.q.w", RV64Q_FCVT_Q_W_OPCODE, RV64_MASK3 },
	{ "fcvt.q.wu", RV64Q_FCVT_Q_WU_OPCODE, RV64_MASK3 },
	{ "fcvt.q.l", RV64Q_FCVT_Q_L_OPCODE, RV64_MASK3 },
	{ "fcvt.q.lu", RV64Q_FCVT_Q_LU_OPCODE, RV64_MASK3 },
	{ "fmv.q.x", RV64Q_FMV_Q_X_OPCODE, RV64_MASK2 },
	{ "flq", RV64Q_FLQ_OPCODE, RV64_MASK12 },
	{ "fsq", RV64Q_FSQ_OPCODE, RV64_MASK12 },
	{ "fmadd.q", RV64Q_FMADD_Q_OPCODE, RV64_MASK1 },
	{ "fmsub.q", RV64Q_FMSUB_Q_OPCODE, RV64_MASK1 },
	{ "fnmsub.q", RV64Q_FNMSUB_Q_OPCODE, RV64_MASK1 },
	{ "fnmadd.q", RV64Q_FNMADD_Q_OPCODE, RV64_MASK1 },
	{ NULL, 0, 0 }
};

vaddr_t
db_disasm(vaddr_t loc, int altfmt)
{
	struct rv64_op *rvo;
	uint32_t instruction;

	db_read_bytes(loc, sizeof(instruction), (char *)&instruction);

	for (rvo = &rv64_opcodes[0]; rvo->opcode != NULL; rvo++) {
		if ((instruction & rvo->num_mask) == rvo->num_op) {
			db_printf("%s\n", rvo->opcode);
			return loc + 4;
		}
	}

	/*
	 * we went through the last instruction and didn't find it in our
	 * list, pretend it's a compressed instruction then (for now)
	 */

	db_printf("[not displaying compressed instruction]\n");
	return loc + 2;
}
