/* Generated automatically by the program `genflags'
from the machine description file `md'.  */

#define HAVE_cmpsi 1
#define HAVE_cmpsf (TARGET_FPU)
#define HAVE_cmpdf (TARGET_FPU)
#define HAVE_cmptf (TARGET_FPU)
#define HAVE_seq_special 1
#define HAVE_sne_special 1
#define HAVE_seq 1
#define HAVE_sne 1
#define HAVE_sgt 1
#define HAVE_slt 1
#define HAVE_sge 1
#define HAVE_sle 1
#define HAVE_sgtu 1
#define HAVE_sltu 1
#define HAVE_sgeu 1
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
#define HAVE_movsi 1
#define HAVE_reload_insi 1
#define HAVE_movhi 1
#define HAVE_movqi 1
#define HAVE_movtf 1
#define HAVE_movdf 1
#define HAVE_movdi 1
#define HAVE_movsf 1
#define HAVE_zero_extendhisi2 1
#define HAVE_zero_extendqihi2 1
#define HAVE_zero_extendqisi2 1
#define HAVE_extendhisi2 1
#define HAVE_extendqihi2 1
#define HAVE_extendqisi2 1
#define HAVE_extendsfdf2 (TARGET_FPU)
#define HAVE_extendsftf2 (TARGET_FPU)
#define HAVE_extenddftf2 (TARGET_FPU)
#define HAVE_truncdfsf2 (TARGET_FPU)
#define HAVE_trunctfsf2 (TARGET_FPU)
#define HAVE_trunctfdf2 (TARGET_FPU)
#define HAVE_floatsisf2 (TARGET_FPU)
#define HAVE_floatsidf2 (TARGET_FPU)
#define HAVE_floatsitf2 (TARGET_FPU)
#define HAVE_fix_truncsfsi2 (TARGET_FPU)
#define HAVE_fix_truncdfsi2 (TARGET_FPU)
#define HAVE_fix_trunctfsi2 (TARGET_FPU)
#define HAVE_adddi3 1
#define HAVE_addsi3 1
#define HAVE_subdi3 1
#define HAVE_subsi3 1
#define HAVE_mulsi3 (TARGET_V8 || TARGET_SPARCLITE)
#define HAVE_mulsidi3 (TARGET_V8 || TARGET_SPARCLITE)
#define HAVE_const_mulsidi3 (TARGET_V8 || TARGET_SPARCLITE)
#define HAVE_umulsidi3 (TARGET_V8 || TARGET_SPARCLITE)
#define HAVE_const_umulsidi3 (TARGET_V8 || TARGET_SPARCLITE)
#define HAVE_divsi3 (TARGET_V8)
#define HAVE_udivsi3 (TARGET_V8)
#define HAVE_anddi3 1
#define HAVE_andsi3 1
#define HAVE_iordi3 1
#define HAVE_iorsi3 1
#define HAVE_xordi3 1
#define HAVE_xorsi3 1
#define HAVE_negdi2 1
#define HAVE_negsi2 1
#define HAVE_one_cmpldi2 1
#define HAVE_one_cmplsi2 1
#define HAVE_addtf3 (TARGET_FPU)
#define HAVE_adddf3 (TARGET_FPU)
#define HAVE_addsf3 (TARGET_FPU)
#define HAVE_subtf3 (TARGET_FPU)
#define HAVE_subdf3 (TARGET_FPU)
#define HAVE_subsf3 (TARGET_FPU)
#define HAVE_multf3 (TARGET_FPU)
#define HAVE_muldf3 (TARGET_FPU)
#define HAVE_mulsf3 (TARGET_FPU)
#define HAVE_divtf3 (TARGET_FPU)
#define HAVE_divdf3 (TARGET_FPU)
#define HAVE_divsf3 (TARGET_FPU)
#define HAVE_negtf2 (TARGET_FPU)
#define HAVE_negdf2 (TARGET_FPU)
#define HAVE_negsf2 (TARGET_FPU)
#define HAVE_abstf2 (TARGET_FPU)
#define HAVE_absdf2 (TARGET_FPU)
#define HAVE_abssf2 (TARGET_FPU)
#define HAVE_sqrttf2 (TARGET_FPU)
#define HAVE_sqrtdf2 (TARGET_FPU)
#define HAVE_sqrtsf2 (TARGET_FPU)
#define HAVE_ashldi3 1
#define HAVE_ashlsi3 1
#define HAVE_lshldi3 1
#define HAVE_ashrsi3 1
#define HAVE_lshrsi3 1
#define HAVE_lshrdi3 1
#define HAVE_jump 1
#define HAVE_tablejump 1
#define HAVE_pic_tablejump 1
#define HAVE_call 1
#define HAVE_call_value 1
#define HAVE_untyped_call 1
#define HAVE_untyped_return 1
#define HAVE_update_return 1
#define HAVE_return (! TARGET_EPILOGUE)
#define HAVE_nop 1
#define HAVE_indirect_jump 1
#define HAVE_nonlocal_goto 1
#define HAVE_flush_register_windows 1
#define HAVE_goto_handler_and_restore 1
#define HAVE_ffssi2 (TARGET_SPARCLITE)

#ifndef NO_MD_PROTOTYPES
extern rtx gen_cmpsi                    PROTO((rtx, rtx));
extern rtx gen_cmpsf                    PROTO((rtx, rtx));
extern rtx gen_cmpdf                    PROTO((rtx, rtx));
extern rtx gen_cmptf                    PROTO((rtx, rtx));
extern rtx gen_seq_special              PROTO((rtx, rtx, rtx));
extern rtx gen_sne_special              PROTO((rtx, rtx, rtx));
extern rtx gen_seq                      PROTO((rtx));
extern rtx gen_sne                      PROTO((rtx));
extern rtx gen_sgt                      PROTO((rtx));
extern rtx gen_slt                      PROTO((rtx));
extern rtx gen_sge                      PROTO((rtx));
extern rtx gen_sle                      PROTO((rtx));
extern rtx gen_sgtu                     PROTO((rtx));
extern rtx gen_sltu                     PROTO((rtx));
extern rtx gen_sgeu                     PROTO((rtx));
extern rtx gen_sleu                     PROTO((rtx));
extern rtx gen_beq                      PROTO((rtx));
extern rtx gen_bne                      PROTO((rtx));
extern rtx gen_bgt                      PROTO((rtx));
extern rtx gen_bgtu                     PROTO((rtx));
extern rtx gen_blt                      PROTO((rtx));
extern rtx gen_bltu                     PROTO((rtx));
extern rtx gen_bge                      PROTO((rtx));
extern rtx gen_bgeu                     PROTO((rtx));
extern rtx gen_ble                      PROTO((rtx));
extern rtx gen_bleu                     PROTO((rtx));
extern rtx gen_movsi                    PROTO((rtx, rtx));
extern rtx gen_reload_insi              PROTO((rtx, rtx, rtx));
extern rtx gen_movhi                    PROTO((rtx, rtx));
extern rtx gen_movqi                    PROTO((rtx, rtx));
extern rtx gen_movtf                    PROTO((rtx, rtx));
extern rtx gen_movdf                    PROTO((rtx, rtx));
extern rtx gen_movdi                    PROTO((rtx, rtx));
extern rtx gen_movsf                    PROTO((rtx, rtx));
extern rtx gen_zero_extendhisi2         PROTO((rtx, rtx));
extern rtx gen_zero_extendqihi2         PROTO((rtx, rtx));
extern rtx gen_zero_extendqisi2         PROTO((rtx, rtx));
extern rtx gen_extendhisi2              PROTO((rtx, rtx));
extern rtx gen_extendqihi2              PROTO((rtx, rtx));
extern rtx gen_extendqisi2              PROTO((rtx, rtx));
extern rtx gen_extendsfdf2              PROTO((rtx, rtx));
extern rtx gen_extendsftf2              PROTO((rtx, rtx));
extern rtx gen_extenddftf2              PROTO((rtx, rtx));
extern rtx gen_truncdfsf2               PROTO((rtx, rtx));
extern rtx gen_trunctfsf2               PROTO((rtx, rtx));
extern rtx gen_trunctfdf2               PROTO((rtx, rtx));
extern rtx gen_floatsisf2               PROTO((rtx, rtx));
extern rtx gen_floatsidf2               PROTO((rtx, rtx));
extern rtx gen_floatsitf2               PROTO((rtx, rtx));
extern rtx gen_fix_truncsfsi2           PROTO((rtx, rtx));
extern rtx gen_fix_truncdfsi2           PROTO((rtx, rtx));
extern rtx gen_fix_trunctfsi2           PROTO((rtx, rtx));
extern rtx gen_adddi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_addsi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_subdi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_subsi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_mulsi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_mulsidi3                 PROTO((rtx, rtx, rtx));
extern rtx gen_const_mulsidi3           PROTO((rtx, rtx, rtx));
extern rtx gen_umulsidi3                PROTO((rtx, rtx, rtx));
extern rtx gen_const_umulsidi3          PROTO((rtx, rtx, rtx));
extern rtx gen_divsi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_udivsi3                  PROTO((rtx, rtx, rtx));
extern rtx gen_anddi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_andsi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_iordi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_iorsi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_xordi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_xorsi3                   PROTO((rtx, rtx, rtx));
extern rtx gen_negdi2                   PROTO((rtx, rtx));
extern rtx gen_negsi2                   PROTO((rtx, rtx));
extern rtx gen_one_cmpldi2              PROTO((rtx, rtx));
extern rtx gen_one_cmplsi2              PROTO((rtx, rtx));
extern rtx gen_addtf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_adddf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_addsf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_subtf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_subdf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_subsf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_multf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_muldf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_mulsf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_divtf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_divdf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_divsf3                   PROTO((rtx, rtx, rtx));
extern rtx gen_negtf2                   PROTO((rtx, rtx));
extern rtx gen_negdf2                   PROTO((rtx, rtx));
extern rtx gen_negsf2                   PROTO((rtx, rtx));
extern rtx gen_abstf2                   PROTO((rtx, rtx));
extern rtx gen_absdf2                   PROTO((rtx, rtx));
extern rtx gen_abssf2                   PROTO((rtx, rtx));
extern rtx gen_sqrttf2                  PROTO((rtx, rtx));
extern rtx gen_sqrtdf2                  PROTO((rtx, rtx));
extern rtx gen_sqrtsf2                  PROTO((rtx, rtx));
extern rtx gen_ashldi3                  PROTO((rtx, rtx, rtx));
extern rtx gen_ashlsi3                  PROTO((rtx, rtx, rtx));
extern rtx gen_lshldi3                  PROTO((rtx, rtx, rtx));
extern rtx gen_ashrsi3                  PROTO((rtx, rtx, rtx));
extern rtx gen_lshrsi3                  PROTO((rtx, rtx, rtx));
extern rtx gen_lshrdi3                  PROTO((rtx, rtx, rtx));
extern rtx gen_jump                     PROTO((rtx));
extern rtx gen_tablejump                PROTO((rtx, rtx));
extern rtx gen_pic_tablejump            PROTO((rtx, rtx));
extern rtx gen_untyped_call             PROTO((rtx, rtx, rtx));
extern rtx gen_untyped_return           PROTO((rtx, rtx));
extern rtx gen_update_return            PROTO((rtx, rtx));
extern rtx gen_return                   PROTO((void));
extern rtx gen_nop                      PROTO((void));
extern rtx gen_indirect_jump            PROTO((rtx));
extern rtx gen_nonlocal_goto            PROTO((rtx, rtx, rtx, rtx));
extern rtx gen_flush_register_windows   PROTO((void));
extern rtx gen_goto_handler_and_restore PROTO((void));
extern rtx gen_ffssi2                   PROTO((rtx, rtx));

#ifdef MD_CALL_PROTOTYPES
extern rtx gen_call                     PROTO((rtx, rtx));
extern rtx gen_call_value               PROTO((rtx, rtx, rtx));

#else /* !MD_CALL_PROTOTYPES */
extern rtx gen_call ();
extern rtx gen_call_value ();
#endif /* !MD_CALL_PROTOTYPES */

#else  /* NO_MD_PROTOTYPES */
extern rtx gen_cmpsi ();
extern rtx gen_cmpsf ();
extern rtx gen_cmpdf ();
extern rtx gen_cmptf ();
extern rtx gen_seq_special ();
extern rtx gen_sne_special ();
extern rtx gen_seq ();
extern rtx gen_sne ();
extern rtx gen_sgt ();
extern rtx gen_slt ();
extern rtx gen_sge ();
extern rtx gen_sle ();
extern rtx gen_sgtu ();
extern rtx gen_sltu ();
extern rtx gen_sgeu ();
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
extern rtx gen_movsi ();
extern rtx gen_reload_insi ();
extern rtx gen_movhi ();
extern rtx gen_movqi ();
extern rtx gen_movtf ();
extern rtx gen_movdf ();
extern rtx gen_movdi ();
extern rtx gen_movsf ();
extern rtx gen_zero_extendhisi2 ();
extern rtx gen_zero_extendqihi2 ();
extern rtx gen_zero_extendqisi2 ();
extern rtx gen_extendhisi2 ();
extern rtx gen_extendqihi2 ();
extern rtx gen_extendqisi2 ();
extern rtx gen_extendsfdf2 ();
extern rtx gen_extendsftf2 ();
extern rtx gen_extenddftf2 ();
extern rtx gen_truncdfsf2 ();
extern rtx gen_trunctfsf2 ();
extern rtx gen_trunctfdf2 ();
extern rtx gen_floatsisf2 ();
extern rtx gen_floatsidf2 ();
extern rtx gen_floatsitf2 ();
extern rtx gen_fix_truncsfsi2 ();
extern rtx gen_fix_truncdfsi2 ();
extern rtx gen_fix_trunctfsi2 ();
extern rtx gen_adddi3 ();
extern rtx gen_addsi3 ();
extern rtx gen_subdi3 ();
extern rtx gen_subsi3 ();
extern rtx gen_mulsi3 ();
extern rtx gen_mulsidi3 ();
extern rtx gen_const_mulsidi3 ();
extern rtx gen_umulsidi3 ();
extern rtx gen_const_umulsidi3 ();
extern rtx gen_divsi3 ();
extern rtx gen_udivsi3 ();
extern rtx gen_anddi3 ();
extern rtx gen_andsi3 ();
extern rtx gen_iordi3 ();
extern rtx gen_iorsi3 ();
extern rtx gen_xordi3 ();
extern rtx gen_xorsi3 ();
extern rtx gen_negdi2 ();
extern rtx gen_negsi2 ();
extern rtx gen_one_cmpldi2 ();
extern rtx gen_one_cmplsi2 ();
extern rtx gen_addtf3 ();
extern rtx gen_adddf3 ();
extern rtx gen_addsf3 ();
extern rtx gen_subtf3 ();
extern rtx gen_subdf3 ();
extern rtx gen_subsf3 ();
extern rtx gen_multf3 ();
extern rtx gen_muldf3 ();
extern rtx gen_mulsf3 ();
extern rtx gen_divtf3 ();
extern rtx gen_divdf3 ();
extern rtx gen_divsf3 ();
extern rtx gen_negtf2 ();
extern rtx gen_negdf2 ();
extern rtx gen_negsf2 ();
extern rtx gen_abstf2 ();
extern rtx gen_absdf2 ();
extern rtx gen_abssf2 ();
extern rtx gen_sqrttf2 ();
extern rtx gen_sqrtdf2 ();
extern rtx gen_sqrtsf2 ();
extern rtx gen_ashldi3 ();
extern rtx gen_ashlsi3 ();
extern rtx gen_lshldi3 ();
extern rtx gen_ashrsi3 ();
extern rtx gen_lshrsi3 ();
extern rtx gen_lshrdi3 ();
extern rtx gen_jump ();
extern rtx gen_tablejump ();
extern rtx gen_pic_tablejump ();
extern rtx gen_untyped_call ();
extern rtx gen_untyped_return ();
extern rtx gen_update_return ();
extern rtx gen_return ();
extern rtx gen_nop ();
extern rtx gen_indirect_jump ();
extern rtx gen_nonlocal_goto ();
extern rtx gen_flush_register_windows ();
extern rtx gen_goto_handler_and_restore ();
extern rtx gen_ffssi2 ();
extern rtx gen_call ();
extern rtx gen_call_value ();
#endif  /* NO_MD_PROTOTYPES */
