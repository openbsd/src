/* Generated automatically by the program `genflags'
from the machine description file `md'.  */

#define HAVE_tstsi 1
#define HAVE_tsthi 1
#define HAVE_tstqi 1
#define HAVE_tstsf (TARGET_68881 || TARGET_FPA)
#define HAVE_tstsf_fpa (TARGET_FPA)
#define HAVE_tstdf (TARGET_68881 || TARGET_FPA)
#define HAVE_tstdf_fpa (TARGET_FPA)
#define HAVE_cmpsi 1
#define HAVE_cmphi 1
#define HAVE_cmpqi 1
#define HAVE_cmpdf (TARGET_68881 || TARGET_FPA)
#define HAVE_cmpdf_fpa (TARGET_FPA)
#define HAVE_cmpsf (TARGET_68881 || TARGET_FPA)
#define HAVE_cmpsf_fpa (TARGET_FPA)
#define HAVE_movsi 1
#define HAVE_movhi 1
#define HAVE_movstricthi 1
#define HAVE_movqi 1
#define HAVE_movstrictqi 1
#define HAVE_movsf 1
#define HAVE_movdf 1
#define HAVE_movxf 1
#define HAVE_movdi 1
#define HAVE_pushasi 1
#define HAVE_truncsiqi2 1
#define HAVE_trunchiqi2 1
#define HAVE_truncsihi2 1
#define HAVE_zero_extendhisi2 1
#define HAVE_zero_extendqihi2 1
#define HAVE_zero_extendqisi2 1
#define HAVE_extendhisi2 1
#define HAVE_extendqihi2 1
#define HAVE_extendqisi2 (TARGET_68020)
#define HAVE_extendsfdf2 (TARGET_68881 || TARGET_FPA)
#define HAVE_truncdfsf2 (TARGET_68881 || TARGET_FPA)
#define HAVE_floatsisf2 (TARGET_68881 || TARGET_FPA)
#define HAVE_floatsidf2 (TARGET_68881 || TARGET_FPA)
#define HAVE_floathisf2 (TARGET_68881)
#define HAVE_floathidf2 (TARGET_68881)
#define HAVE_floatqisf2 (TARGET_68881)
#define HAVE_floatqidf2 (TARGET_68881)
#define HAVE_fix_truncdfsi2 (TARGET_68040)
#define HAVE_fix_truncdfhi2 (TARGET_68040)
#define HAVE_fix_truncdfqi2 (TARGET_68040)
#define HAVE_ftruncdf2 (TARGET_68881 && !TARGET_68040)
#define HAVE_ftruncsf2 (TARGET_68881 && !TARGET_68040)
#define HAVE_fixsfqi2 (TARGET_68881)
#define HAVE_fixsfhi2 (TARGET_68881)
#define HAVE_fixsfsi2 (TARGET_68881)
#define HAVE_fixdfqi2 (TARGET_68881)
#define HAVE_fixdfhi2 (TARGET_68881)
#define HAVE_fixdfsi2 (TARGET_68881)
#define HAVE_addsi3 1
#define HAVE_addhi3 1
#define HAVE_addqi3 1
#define HAVE_adddf3 (TARGET_68881 || TARGET_FPA)
#define HAVE_addsf3 (TARGET_68881 || TARGET_FPA)
#define HAVE_subsi3 1
#define HAVE_subhi3 1
#define HAVE_subqi3 1
#define HAVE_subdf3 (TARGET_68881 || TARGET_FPA)
#define HAVE_subsf3 (TARGET_68881 || TARGET_FPA)
#define HAVE_mulhi3 1
#define HAVE_mulhisi3 1
#define HAVE_mulsi3 (TARGET_68020)
#define HAVE_umulhisi3 1
#define HAVE_umulsidi3 (TARGET_68020)
#define HAVE_mulsidi3 (TARGET_68020)
#define HAVE_muldf3 (TARGET_68881 || TARGET_FPA)
#define HAVE_mulsf3 (TARGET_68881 || TARGET_FPA)
#define HAVE_divhi3 1
#define HAVE_divhisi3 1
#define HAVE_udivhi3 1
#define HAVE_udivhisi3 1
#define HAVE_divdf3 (TARGET_68881 || TARGET_FPA)
#define HAVE_divsf3 (TARGET_68881 || TARGET_FPA)
#define HAVE_modhi3 1
#define HAVE_modhisi3 1
#define HAVE_umodhi3 1
#define HAVE_umodhisi3 1
#define HAVE_divmodsi4 (TARGET_68020)
#define HAVE_udivmodsi4 (TARGET_68020)
#define HAVE_andsi3 1
#define HAVE_andhi3 1
#define HAVE_andqi3 1
#define HAVE_iorsi3 1
#define HAVE_iorhi3 1
#define HAVE_iorqi3 1
#define HAVE_xorsi3 1
#define HAVE_xorhi3 1
#define HAVE_xorqi3 1
#define HAVE_negsi2 1
#define HAVE_neghi2 1
#define HAVE_negqi2 1
#define HAVE_negsf2 (TARGET_68881 || TARGET_FPA)
#define HAVE_negdf2 (TARGET_68881 || TARGET_FPA)
#define HAVE_sqrtdf2 (TARGET_68881)
#define HAVE_abssf2 (TARGET_68881 || TARGET_FPA)
#define HAVE_absdf2 (TARGET_68881 || TARGET_FPA)
#define HAVE_one_cmplsi2 1
#define HAVE_one_cmplhi2 1
#define HAVE_one_cmplqi2 1
#define HAVE_ashlsi3 1
#define HAVE_ashlhi3 1
#define HAVE_ashlqi3 1
#define HAVE_ashrsi3 1
#define HAVE_ashrhi3 1
#define HAVE_ashrqi3 1
#define HAVE_lshlsi3 1
#define HAVE_lshlhi3 1
#define HAVE_lshlqi3 1
#define HAVE_lshrsi3 1
#define HAVE_lshrhi3 1
#define HAVE_lshrqi3 1
#define HAVE_rotlsi3 1
#define HAVE_rotlhi3 1
#define HAVE_rotlqi3 1
#define HAVE_rotrsi3 1
#define HAVE_rotrhi3 1
#define HAVE_rotrqi3 1
#define HAVE_extv (TARGET_68020 && TARGET_BITFIELD)
#define HAVE_extzv (TARGET_68020 && TARGET_BITFIELD)
#define HAVE_insv (TARGET_68020 && TARGET_BITFIELD)
#define HAVE_seq 1
#define HAVE_sne 1
#define HAVE_sgt 1
#define HAVE_sgtu 1
#define HAVE_slt 1
#define HAVE_sltu 1
#define HAVE_sge 1
#define HAVE_sgeu 1
#define HAVE_sle 1
#define HAVE_sleu 1
#define HAVE_beq 1
#define HAVE_bne 1
#define HAVE_bgt 1
#define HAVE_bgtu 1
#define HAVE_blt 1
#define HAVE_bltu 1
#define HAVE_bge 1
#define HAVE_bgeu 1
#define HAVE_ble 1
#define HAVE_bleu 1
#define HAVE_jump 1
#define HAVE_tablejump 1
#define HAVE_decrement_and_branch_until_zero (find_reg_note (insn, REG_NONNEG, 0))
#define HAVE_call 1
#define HAVE_call_value 1
#define HAVE_untyped_call (NEEDS_UNTYPED_CALL)
#define HAVE_blockage 1
#define HAVE_nop 1
#define HAVE_probe (NEED_PROBE)
#define HAVE_return (USE_RETURN_INSN)
#define HAVE_indirect_jump 1
#define HAVE_tstxf (TARGET_68881)
#define HAVE_cmpxf (TARGET_68881)
#define HAVE_extendsfxf2 (TARGET_68881)
#define HAVE_extenddfxf2 (TARGET_68881)
#define HAVE_truncxfdf2 (TARGET_68881)
#define HAVE_truncxfsf2 (TARGET_68881)
#define HAVE_floatsixf2 (TARGET_68881)
#define HAVE_floathixf2 (TARGET_68881)
#define HAVE_floatqixf2 (TARGET_68881)
#define HAVE_ftruncxf2 (TARGET_68881)
#define HAVE_fixxfqi2 (TARGET_68881)
#define HAVE_fixxfhi2 (TARGET_68881)
#define HAVE_fixxfsi2 (TARGET_68881)
#define HAVE_addxf3 (TARGET_68881)
#define HAVE_subxf3 (TARGET_68881)
#define HAVE_mulxf3 (TARGET_68881)
#define HAVE_divxf3 (TARGET_68881)
#define HAVE_negxf2 (TARGET_68881)
#define HAVE_absxf2 (TARGET_68881)
#define HAVE_sqrtxf2 (TARGET_68881)

#ifndef NO_MD_PROTOTYPES
extern rtx gen_tstsi                           PROTO((rtx));
extern rtx gen_tsthi                           PROTO((rtx));
extern rtx gen_tstqi                           PROTO((rtx));
extern rtx gen_tstsf                           PROTO((rtx));
extern rtx gen_tstsf_fpa                       PROTO((rtx));
extern rtx gen_tstdf                           PROTO((rtx));
extern rtx gen_tstdf_fpa                       PROTO((rtx));
extern rtx gen_cmpsi                           PROTO((rtx, rtx));
extern rtx gen_cmphi                           PROTO((rtx, rtx));
extern rtx gen_cmpqi                           PROTO((rtx, rtx));
extern rtx gen_cmpdf                           PROTO((rtx, rtx));
extern rtx gen_cmpdf_fpa                       PROTO((rtx, rtx));
extern rtx gen_cmpsf                           PROTO((rtx, rtx));
extern rtx gen_cmpsf_fpa                       PROTO((rtx, rtx));
extern rtx gen_movsi                           PROTO((rtx, rtx));
extern rtx gen_movhi                           PROTO((rtx, rtx));
extern rtx gen_movstricthi                     PROTO((rtx, rtx));
extern rtx gen_movqi                           PROTO((rtx, rtx));
extern rtx gen_movstrictqi                     PROTO((rtx, rtx));
extern rtx gen_movsf                           PROTO((rtx, rtx));
extern rtx gen_movdf                           PROTO((rtx, rtx));
extern rtx gen_movxf                           PROTO((rtx, rtx));
extern rtx gen_movdi                           PROTO((rtx, rtx));
extern rtx gen_pushasi                         PROTO((rtx, rtx));
extern rtx gen_truncsiqi2                      PROTO((rtx, rtx));
extern rtx gen_trunchiqi2                      PROTO((rtx, rtx));
extern rtx gen_truncsihi2                      PROTO((rtx, rtx));
extern rtx gen_zero_extendhisi2                PROTO((rtx, rtx));
extern rtx gen_zero_extendqihi2                PROTO((rtx, rtx));
extern rtx gen_zero_extendqisi2                PROTO((rtx, rtx));
extern rtx gen_extendhisi2                     PROTO((rtx, rtx));
extern rtx gen_extendqihi2                     PROTO((rtx, rtx));
extern rtx gen_extendqisi2                     PROTO((rtx, rtx));
extern rtx gen_extendsfdf2                     PROTO((rtx, rtx));
extern rtx gen_truncdfsf2                      PROTO((rtx, rtx));
extern rtx gen_floatsisf2                      PROTO((rtx, rtx));
extern rtx gen_floatsidf2                      PROTO((rtx, rtx));
extern rtx gen_floathisf2                      PROTO((rtx, rtx));
extern rtx gen_floathidf2                      PROTO((rtx, rtx));
extern rtx gen_floatqisf2                      PROTO((rtx, rtx));
extern rtx gen_floatqidf2                      PROTO((rtx, rtx));
extern rtx gen_fix_truncdfsi2                  PROTO((rtx, rtx));
extern rtx gen_fix_truncdfhi2                  PROTO((rtx, rtx));
extern rtx gen_fix_truncdfqi2                  PROTO((rtx, rtx));
extern rtx gen_ftruncdf2                       PROTO((rtx, rtx));
extern rtx gen_ftruncsf2                       PROTO((rtx, rtx));
extern rtx gen_fixsfqi2                        PROTO((rtx, rtx));
extern rtx gen_fixsfhi2                        PROTO((rtx, rtx));
extern rtx gen_fixsfsi2                        PROTO((rtx, rtx));
extern rtx gen_fixdfqi2                        PROTO((rtx, rtx));
extern rtx gen_fixdfhi2                        PROTO((rtx, rtx));
extern rtx gen_fixdfsi2                        PROTO((rtx, rtx));
extern rtx gen_addsi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_addhi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_addqi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_adddf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_addsf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_subsi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_subhi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_subqi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_subdf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_subsf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_mulhi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_mulhisi3                        PROTO((rtx, rtx, rtx));
extern rtx gen_mulsi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_umulhisi3                       PROTO((rtx, rtx, rtx));
extern rtx gen_umulsidi3                       PROTO((rtx, rtx, rtx));
extern rtx gen_mulsidi3                        PROTO((rtx, rtx, rtx));
extern rtx gen_muldf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_mulsf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_divhi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_divhisi3                        PROTO((rtx, rtx, rtx));
extern rtx gen_udivhi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_udivhisi3                       PROTO((rtx, rtx, rtx));
extern rtx gen_divdf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_divsf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_modhi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_modhisi3                        PROTO((rtx, rtx, rtx));
extern rtx gen_umodhi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_umodhisi3                       PROTO((rtx, rtx, rtx));
extern rtx gen_divmodsi4                       PROTO((rtx, rtx, rtx, rtx));
extern rtx gen_udivmodsi4                      PROTO((rtx, rtx, rtx, rtx));
extern rtx gen_andsi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_andhi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_andqi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_iorsi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_iorhi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_iorqi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_xorsi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_xorhi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_xorqi3                          PROTO((rtx, rtx, rtx));
extern rtx gen_negsi2                          PROTO((rtx, rtx));
extern rtx gen_neghi2                          PROTO((rtx, rtx));
extern rtx gen_negqi2                          PROTO((rtx, rtx));
extern rtx gen_negsf2                          PROTO((rtx, rtx));
extern rtx gen_negdf2                          PROTO((rtx, rtx));
extern rtx gen_sqrtdf2                         PROTO((rtx, rtx));
extern rtx gen_abssf2                          PROTO((rtx, rtx));
extern rtx gen_absdf2                          PROTO((rtx, rtx));
extern rtx gen_one_cmplsi2                     PROTO((rtx, rtx));
extern rtx gen_one_cmplhi2                     PROTO((rtx, rtx));
extern rtx gen_one_cmplqi2                     PROTO((rtx, rtx));
extern rtx gen_ashlsi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_ashlhi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_ashlqi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_ashrsi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_ashrhi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_ashrqi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_lshlsi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_lshlhi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_lshlqi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_lshrsi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_lshrhi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_lshrqi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_rotlsi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_rotlhi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_rotlqi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_rotrsi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_rotrhi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_rotrqi3                         PROTO((rtx, rtx, rtx));
extern rtx gen_extv                            PROTO((rtx, rtx, rtx, rtx));
extern rtx gen_extzv                           PROTO((rtx, rtx, rtx, rtx));
extern rtx gen_insv                            PROTO((rtx, rtx, rtx, rtx));
extern rtx gen_seq                             PROTO((rtx));
extern rtx gen_sne                             PROTO((rtx));
extern rtx gen_sgt                             PROTO((rtx));
extern rtx gen_sgtu                            PROTO((rtx));
extern rtx gen_slt                             PROTO((rtx));
extern rtx gen_sltu                            PROTO((rtx));
extern rtx gen_sge                             PROTO((rtx));
extern rtx gen_sgeu                            PROTO((rtx));
extern rtx gen_sle                             PROTO((rtx));
extern rtx gen_sleu                            PROTO((rtx));
extern rtx gen_beq                             PROTO((rtx));
extern rtx gen_bne                             PROTO((rtx));
extern rtx gen_bgt                             PROTO((rtx));
extern rtx gen_bgtu                            PROTO((rtx));
extern rtx gen_blt                             PROTO((rtx));
extern rtx gen_bltu                            PROTO((rtx));
extern rtx gen_bge                             PROTO((rtx));
extern rtx gen_bgeu                            PROTO((rtx));
extern rtx gen_ble                             PROTO((rtx));
extern rtx gen_bleu                            PROTO((rtx));
extern rtx gen_jump                            PROTO((rtx));
extern rtx gen_tablejump                       PROTO((rtx, rtx));
extern rtx gen_decrement_and_branch_until_zero PROTO((rtx, rtx));
extern rtx gen_untyped_call                    PROTO((rtx, rtx, rtx));
extern rtx gen_blockage                        PROTO((void));
extern rtx gen_nop                             PROTO((void));
extern rtx gen_probe                           PROTO((void));
extern rtx gen_return                          PROTO((void));
extern rtx gen_indirect_jump                   PROTO((rtx));
extern rtx gen_tstxf                           PROTO((rtx));
extern rtx gen_cmpxf                           PROTO((rtx, rtx));
extern rtx gen_extendsfxf2                     PROTO((rtx, rtx));
extern rtx gen_extenddfxf2                     PROTO((rtx, rtx));
extern rtx gen_truncxfdf2                      PROTO((rtx, rtx));
extern rtx gen_truncxfsf2                      PROTO((rtx, rtx));
extern rtx gen_floatsixf2                      PROTO((rtx, rtx));
extern rtx gen_floathixf2                      PROTO((rtx, rtx));
extern rtx gen_floatqixf2                      PROTO((rtx, rtx));
extern rtx gen_ftruncxf2                       PROTO((rtx, rtx));
extern rtx gen_fixxfqi2                        PROTO((rtx, rtx));
extern rtx gen_fixxfhi2                        PROTO((rtx, rtx));
extern rtx gen_fixxfsi2                        PROTO((rtx, rtx));
extern rtx gen_addxf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_subxf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_mulxf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_divxf3                          PROTO((rtx, rtx, rtx));
extern rtx gen_negxf2                          PROTO((rtx, rtx));
extern rtx gen_absxf2                          PROTO((rtx, rtx));
extern rtx gen_sqrtxf2                         PROTO((rtx, rtx));

#ifdef MD_CALL_PROTOTYPES
extern rtx gen_call                            PROTO((rtx, rtx));
extern rtx gen_call_value                      PROTO((rtx, rtx, rtx));

#else /* !MD_CALL_PROTOTYPES */
extern rtx gen_call ();
extern rtx gen_call_value ();
#endif /* !MD_CALL_PROTOTYPES */

#else  /* NO_MD_PROTOTYPES */
extern rtx gen_tstsi ();
extern rtx gen_tsthi ();
extern rtx gen_tstqi ();
extern rtx gen_tstsf ();
extern rtx gen_tstsf_fpa ();
extern rtx gen_tstdf ();
extern rtx gen_tstdf_fpa ();
extern rtx gen_cmpsi ();
extern rtx gen_cmphi ();
extern rtx gen_cmpqi ();
extern rtx gen_cmpdf ();
extern rtx gen_cmpdf_fpa ();
extern rtx gen_cmpsf ();
extern rtx gen_cmpsf_fpa ();
extern rtx gen_movsi ();
extern rtx gen_movhi ();
extern rtx gen_movstricthi ();
extern rtx gen_movqi ();
extern rtx gen_movstrictqi ();
extern rtx gen_movsf ();
extern rtx gen_movdf ();
extern rtx gen_movxf ();
extern rtx gen_movdi ();
extern rtx gen_pushasi ();
extern rtx gen_truncsiqi2 ();
extern rtx gen_trunchiqi2 ();
extern rtx gen_truncsihi2 ();
extern rtx gen_zero_extendhisi2 ();
extern rtx gen_zero_extendqihi2 ();
extern rtx gen_zero_extendqisi2 ();
extern rtx gen_extendhisi2 ();
extern rtx gen_extendqihi2 ();
extern rtx gen_extendqisi2 ();
extern rtx gen_extendsfdf2 ();
extern rtx gen_truncdfsf2 ();
extern rtx gen_floatsisf2 ();
extern rtx gen_floatsidf2 ();
extern rtx gen_floathisf2 ();
extern rtx gen_floathidf2 ();
extern rtx gen_floatqisf2 ();
extern rtx gen_floatqidf2 ();
extern rtx gen_fix_truncdfsi2 ();
extern rtx gen_fix_truncdfhi2 ();
extern rtx gen_fix_truncdfqi2 ();
extern rtx gen_ftruncdf2 ();
extern rtx gen_ftruncsf2 ();
extern rtx gen_fixsfqi2 ();
extern rtx gen_fixsfhi2 ();
extern rtx gen_fixsfsi2 ();
extern rtx gen_fixdfqi2 ();
extern rtx gen_fixdfhi2 ();
extern rtx gen_fixdfsi2 ();
extern rtx gen_addsi3 ();
extern rtx gen_addhi3 ();
extern rtx gen_addqi3 ();
extern rtx gen_adddf3 ();
extern rtx gen_addsf3 ();
extern rtx gen_subsi3 ();
extern rtx gen_subhi3 ();
extern rtx gen_subqi3 ();
extern rtx gen_subdf3 ();
extern rtx gen_subsf3 ();
extern rtx gen_mulhi3 ();
extern rtx gen_mulhisi3 ();
extern rtx gen_mulsi3 ();
extern rtx gen_umulhisi3 ();
extern rtx gen_umulsidi3 ();
extern rtx gen_mulsidi3 ();
extern rtx gen_muldf3 ();
extern rtx gen_mulsf3 ();
extern rtx gen_divhi3 ();
extern rtx gen_divhisi3 ();
extern rtx gen_udivhi3 ();
extern rtx gen_udivhisi3 ();
extern rtx gen_divdf3 ();
extern rtx gen_divsf3 ();
extern rtx gen_modhi3 ();
extern rtx gen_modhisi3 ();
extern rtx gen_umodhi3 ();
extern rtx gen_umodhisi3 ();
extern rtx gen_divmodsi4 ();
extern rtx gen_udivmodsi4 ();
extern rtx gen_andsi3 ();
extern rtx gen_andhi3 ();
extern rtx gen_andqi3 ();
extern rtx gen_iorsi3 ();
extern rtx gen_iorhi3 ();
extern rtx gen_iorqi3 ();
extern rtx gen_xorsi3 ();
extern rtx gen_xorhi3 ();
extern rtx gen_xorqi3 ();
extern rtx gen_negsi2 ();
extern rtx gen_neghi2 ();
extern rtx gen_negqi2 ();
extern rtx gen_negsf2 ();
extern rtx gen_negdf2 ();
extern rtx gen_sqrtdf2 ();
extern rtx gen_abssf2 ();
extern rtx gen_absdf2 ();
extern rtx gen_one_cmplsi2 ();
extern rtx gen_one_cmplhi2 ();
extern rtx gen_one_cmplqi2 ();
extern rtx gen_ashlsi3 ();
extern rtx gen_ashlhi3 ();
extern rtx gen_ashlqi3 ();
extern rtx gen_ashrsi3 ();
extern rtx gen_ashrhi3 ();
extern rtx gen_ashrqi3 ();
extern rtx gen_lshlsi3 ();
extern rtx gen_lshlhi3 ();
extern rtx gen_lshlqi3 ();
extern rtx gen_lshrsi3 ();
extern rtx gen_lshrhi3 ();
extern rtx gen_lshrqi3 ();
extern rtx gen_rotlsi3 ();
extern rtx gen_rotlhi3 ();
extern rtx gen_rotlqi3 ();
extern rtx gen_rotrsi3 ();
extern rtx gen_rotrhi3 ();
extern rtx gen_rotrqi3 ();
extern rtx gen_extv ();
extern rtx gen_extzv ();
extern rtx gen_insv ();
extern rtx gen_seq ();
extern rtx gen_sne ();
extern rtx gen_sgt ();
extern rtx gen_sgtu ();
extern rtx gen_slt ();
extern rtx gen_sltu ();
extern rtx gen_sge ();
extern rtx gen_sgeu ();
extern rtx gen_sle ();
extern rtx gen_sleu ();
extern rtx gen_beq ();
extern rtx gen_bne ();
extern rtx gen_bgt ();
extern rtx gen_bgtu ();
extern rtx gen_blt ();
extern rtx gen_bltu ();
extern rtx gen_bge ();
extern rtx gen_bgeu ();
extern rtx gen_ble ();
extern rtx gen_bleu ();
extern rtx gen_jump ();
extern rtx gen_tablejump ();
extern rtx gen_decrement_and_branch_until_zero ();
extern rtx gen_untyped_call ();
extern rtx gen_blockage ();
extern rtx gen_nop ();
extern rtx gen_probe ();
extern rtx gen_return ();
extern rtx gen_indirect_jump ();
extern rtx gen_tstxf ();
extern rtx gen_cmpxf ();
extern rtx gen_extendsfxf2 ();
extern rtx gen_extenddfxf2 ();
extern rtx gen_truncxfdf2 ();
extern rtx gen_truncxfsf2 ();
extern rtx gen_floatsixf2 ();
extern rtx gen_floathixf2 ();
extern rtx gen_floatqixf2 ();
extern rtx gen_ftruncxf2 ();
extern rtx gen_fixxfqi2 ();
extern rtx gen_fixxfhi2 ();
extern rtx gen_fixxfsi2 ();
extern rtx gen_addxf3 ();
extern rtx gen_subxf3 ();
extern rtx gen_mulxf3 ();
extern rtx gen_divxf3 ();
extern rtx gen_negxf2 ();
extern rtx gen_absxf2 ();
extern rtx gen_sqrtxf2 ();
extern rtx gen_call ();
extern rtx gen_call_value ();
#endif  /* NO_MD_PROTOTYPES */
