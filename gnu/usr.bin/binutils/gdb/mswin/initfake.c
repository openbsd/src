
#include "windefs.h"
#include "ansidecl.h"

void initialize_all_files () 
{
  {extern void _initialize_parallel_win32 (); _initialize_parallel_win32 ();}
  {extern void _initialize_blockframe (); _initialize_blockframe ();}
  {extern void _initialize_breakpoint (); _initialize_breakpoint ();}
  {extern void _initialize_stack (); _initialize_stack ();}
  {extern void _initialize_thread (); _initialize_thread ();}
  {extern void _initialize_source (); _initialize_source ();}
  {extern void _initialize_values (); _initialize_values ();}
  {extern void _initialize_valarith (); _initialize_valarith ();}
  {extern void _initialize_valprint (); _initialize_valprint ();}
  {extern void _initialize_dcache (); _initialize_dcache ();}
  {extern void _initialize_printcmd (); _initialize_printcmd ();}
  {extern void _initialize_symtab (); _initialize_symtab ();}
  {extern void _initialize_symfile (); _initialize_symfile ();}
  {extern void _initialize_symmisc (); _initialize_symmisc ();}
  {extern void _initialize_infcmd (); _initialize_infcmd ();}
  {extern void _initialize_infrun (); _initialize_infrun ();}
  {extern void _initialize_command (); _initialize_command ();}
  {extern void _initialize_gdbtypes (); _initialize_gdbtypes ();}
  {extern void _initialize_copying (); _initialize_copying ();}
  {extern void _initialize_exec (); _initialize_exec ();}
  {extern void _initialize_annotate (); _initialize_annotate ();}

  {extern void _initialize_remote (); _initialize_remote ();}
  {extern void _initialize_sr_support (); _initialize_sr_support ();}
  {extern void _initialize_parse (); _initialize_parse ();}
  {extern void _initialize_language (); _initialize_language ();}
  {extern void _initialize_buildsym (); _initialize_buildsym ();}
  {extern void _initialize_exec (); _initialize_exec ();}
  {extern void _initialize_maint_cmds (); _initialize_maint_cmds ();}
  {extern void _initialize_demangler (); _initialize_demangler ();}

  {extern void _initialize_coffread (); _initialize_coffread ();}

  {extern void _initialize_core (); _initialize_core ();}
  {extern void _initialize_c_language (); _initialize_c_language ();}

  {extern void _initialize_dbxread (); _initialize_dbxread ();}
  {extern void _initialize_mipsread (); _initialize_mipsread ();}
  {extern void _initialize_elfread (); _initialize_elfread ();}
  {extern void _initialize_stabsread (); _initialize_stabsread ();}
  {extern void _initialize_chill_language (); _initialize_chill_language ();}
  {extern void _initialize_f_language (); _initialize_f_language ();}
  {extern void _initialize_mdebugread (); _initialize_mdebugread ();}
  {extern void _initialize_m2_language (); _initialize_m2_language ();}
  {extern void _initialize_cp_valprint (); _initialize_cp_valprint ();}
  {extern void _initialize_f_valprint (); _initialize_f_valprint ();}
  {extern void _initialize_nlmread (); _initialize_nlmread ();}
  {extern void _initialize_os9kread (); _initialize_os9kread ();}
  {extern void _initialize_complaints (); _initialize_complaints ();}
  {extern void _initialize_typeprint (); _initialize_typeprint ();}
  {extern void _initialize_serial (); _initialize_serial ();}
  {extern void _initialize_inflow (); _initialize_inflow ();}
  {extern void _initialize_gdbwin (); _initialize_gdbwin ();}
  {extern void _initialize_remote_monitors (); _initialize_remote_monitors ();}
  {extern void _initialize_ser_win32s(); _initialize_ser_win32s();}

#if defined(TARGET_SH)
  {extern void _initialize_sh_tdep (); _initialize_sh_tdep ();}
  {extern void _initialize_remote_sim (); _initialize_remote_sim ();}
  {extern void _initialize_sh3_rom (); _initialize_sh3_rom ();}
  {extern void _initialize_remote_e7000 (); _initialize_remote_e7000 ();}
  {extern void _initialize_ser_e7000pc (); _initialize_ser_e7000pc ();}
#elif defined(TARGET_H8300)
  {extern void _initialize_h8300_tdep (); _initialize_h8300_tdep ();}
  {extern void _initialize_h8300m (); _initialize_h8300m ();}
  {extern void _initialize_remote_sim (); _initialize_remote_sim ();}
  {extern void _initialize_remote_hms (); _initialize_remote_hms ();}
  {extern void _initialize_remote_e7000 (); _initialize_remote_e7000 ();}
  {extern void _initialize_ser_e7000pc (); _initialize_ser_e7000pc ();}
#elif defined(TARGET_M68K)
  {extern void _initialize_m68k_tdep(); _initialize_m68k_tdep();}
  {extern void _initialize_est(); _initialize_est();}
  {extern void _initialize_rom68k(); _initialize_rom68k();}
  {extern void _initialize_cpu32bug_rom (); _initialize_cpu32bug_rom ();}
#elif defined(TARGET_SPARCLITE)
  {extern void _initialize_sparc_tdep(); _initialize_sparc_tdep();}
  {extern void _initialize_sparcl_tdep(); _initialize_sparcl_tdep();}
#elif defined(TARGET_SPARCLET)
  {extern void _initialize_sparc_tdep(); _initialize_sparc_tdep();}
  {extern void _initialize_sparclet(); _initialize_sparclet();}
#elif defined(TARGET_MIPS)
  {extern void _initialize_mips_tdep(); _initialize_mips_tdep();}
  {extern void _initialize_remote_mips(); _initialize_remote_mips();}
#elif defined(TARGET_I386)
  {extern void _initialize_i386_tdep(); _initialize_i386_tdep();}
#elif defined(TARGET_A29K)
  {extern void _initialize_a29k_tdep(); _initialize_a29k_tdep();}
#else
#error  HEY! no target defined!
#endif


}
