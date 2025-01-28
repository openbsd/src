/* need to replace pregcomp et al, so enable that */
#ifndef PERL_IN_XSUB_RE
#  define PERL_IN_XSUB_RE
#endif
/* need access to debugger hooks */
#if defined(PERL_EXT_RE_DEBUG) && !defined(DEBUGGING)
#  define DEBUGGING
#  define DEBUGGING_RE_ONLY
#endif

/* We *really* need to overwrite these symbols: */
#define Perl_regexec_flags      my_regexec
#define Perl_regdump            my_regdump
#define Perl_regprop            my_regprop
#define Perl_re_intuit_start    my_re_intuit_start
#define Perl_re_compile         my_re_compile
#define Perl_re_op_compile      my_re_op_compile
#define Perl_regfree_internal   my_regfree
#define Perl_re_intuit_string   my_re_intuit_string
#define Perl_regdupe_internal   my_regdupe
#define Perl_reg_numbered_buff_fetch  my_reg_numbered_buff_fetch
#define Perl_reg_numbered_buff_store  my_reg_numbered_buff_store
#define Perl_reg_numbered_buff_length  my_reg_numbered_buff_length
#define Perl_reg_named_buff      my_reg_named_buff
#define Perl_reg_named_buff_iter my_reg_named_buff_iter
#define Perl_reg_named_buff_fetch    my_reg_named_buff_fetch    
#define Perl_reg_named_buff_exists   my_reg_named_buff_exists  
#define Perl_reg_named_buff_firstkey my_reg_named_buff_firstkey
#define Perl_reg_named_buff_nextkey  my_reg_named_buff_nextkey 
#define Perl_reg_named_buff_scalar   my_reg_named_buff_scalar  
#define Perl_reg_named_buff_all      my_reg_named_buff_all     
#define Perl_reg_qr_package        my_reg_qr_package

/* We override these names because currently under static builds
 * we end up with confusion between the normal regex engine and
 * the debugging one. Ideally this problem should be solved in
 * another way, but for now this should prevent debugging mode
 * code being called from non-debugging codepaths. I suspect that
 * this being needed is a symptom of something else deeper being
 * wrong, but for now this seems to resolve the problem.
 *
 * Without these defines at least one pattern in t/op/split.t will
 * fail when perl is built with -Uusedl. */

#define Perl_study_chunk                        my_study_chunk
#define Perl_scan_commit                        my_scan_commit
#define Perl_ssc_init                           my_ssc_init
#define Perl_join_exact                         my_join_exact
#define Perl_make_trie                          my_make_trie
#define Perl_construct_ahocorasick_from_trie    my_construct_ahocorasick_from_trie
#define Perl_make_trie                          my_make_trie

#define PERL_NO_GET_CONTEXT

/*
 * ex: set ts=8 sts=4 sw=4 et:
 */
