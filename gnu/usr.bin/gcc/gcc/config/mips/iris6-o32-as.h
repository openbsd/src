/* Definitions of target machine for GNU compiler, for MIPS running IRIX 6
   (O32 ABI) using the SGI assembler.  */

/* Override mips.h default: the IRIX 6 O32 assembler warns about -O3:

   as: Warning: -O3 is not supported for assembly compiles for ucode
   compilers; changing to -O2.
   
   So avoid passing it in the first place.  */
#undef SUBTARGET_ASM_OPTIMIZING_SPEC
#define SUBTARGET_ASM_OPTIMIZING_SPEC "\
%{noasmopt:-O0} \
%{!noasmopt:%{O|O1|O2|O3:-O2}}"

/* Enforce use of O32 linker, irrespective of SGI_ABI environment variable
   and machine type (e.g., R8000 systems default to -64).  Copied from
   iris5.h, only adding -32.  The default options -call_shared -no_unresolved
   are only passed if not invoked with -r.  */
#undef LINK_SPEC
#define LINK_SPEC "\
%{G*} %{EB} %{EL} %{mips1} %{mips2} %{mips3} \
%{bestGnum} %{shared} %{non_shared} \
%{call_shared} %{no_archive} %{exact_version} \
%{static: -non_shared} \
%{!static: \
  %{!shared:%{!non_shared:%{!call_shared:%{!r: -call_shared -no_unresolved}}}}} \
%{rpath} \
-_SYSTYPE_SVR4 \
-32"
