$! SUBCONFIGURE.COM - build a config.sh for VMS Perl.
$!
$! Note for folks from other platforms changing things in here:
$!   Fancy changes (based on compiler capabilities or VMS version or
$!   whatever) are tricky, so go ahead and punt on those.
$!
$!   Simple changes, though (say, always setting something to 1, or undef,
$!   or something like that) are straightforward. Adding a new item for the
$!   ultimately created config.sh requires adding two lines to this file.
$!
$!   First, a line in the format:
$!     $ perl_foo = "bar"
$!   after the line tagged ##ADD NEW CONSTANTS HERE##. Replace foo with the
$!   variable name as it appears in config.sh.
$!
$!   Second, add a line in the format:
$!     $ WC "foo='" + perl_foo + "'"
$!   after the line tagged ##WRITE NEW CONSTANTS HERE##. Careful of the
$!   quoting, as it can be tricky. 
$! 
$! This .COM file expects to be called by configure.com, and thus expects
$! a few symbols in the environment. Notably:
$!
$!  One of: Using_Dec_C, Using_Vax_C, Using_Gnu_C set to "YES"
$!  Dec_C_Version set to the Dec C version (defaults to 0 if not specified)
$!  Has_Socketshr set to "T" if using socketshr
$!  Has_Dec_C_Sockets set to "T" if using Dec C sockets
$!  Use_Threads set to "T" if they're using threads
$!  C_Compiler_Invoke is the command needed to invoke the C compiler
$!
$! Set Dec_C_Version to something
$ WRITE_RESULT := "WRITE SYS$OUTPUT ""%CONFIG-I-RESULT "" + "
$ Dec_C_Version := "''Dec_C_Version'"
$ Dec_C_Version = Dec_C_Version + 0
$ Vms_Ver := "''f$extract(1,3, f$getsyi(""version""))'"
$ perl_extensions := "''extensions'"
$ if f$length(Mcc) .eq. 0 then Mcc := "cc"
$ MCC = f$edit(mcc, "UPCASE")
$ IF Mcc.eqs."CC
$ THEN
$   C_Compiler_Replace := "CC="
$ ELSE
$   C_Compiler_Replace := "CC=CC=''Mcc'"
$ ENDIF
$ if "''Using_Dec_C'" .eqs. "Yes"
$ THEN
$   Checkcc := "''Mcc'/prefix=all"
$ ELSE
$   Checkcc := "''Mcc'"
$ ENDIF
$ cc_flags = ""
$! Some constant defaults.
$
$ hwname = f$getsyi("HW_NAME")
$ myname = myhostname
$ if "''myname'" .eqs. "" THEN myname = f$trnlnm("SYS$NODE")
$!
$! ##ADD NEW CONSTANTS HERE##
$ perl_i_sysmount="undef"
$ perl_d_fstatfs="undef"
$ perl_i_machcthreads="undef"
$ perl_i_pthread="define"
$ perl_d_fstatvfs="undef"
$ perl_d_statfsflags="undef"
$ perl_i_sysstatvfs="undef"
$ perl_i_mntent="undef"
$ perl_d_getmntent="undef"
$ perl_d_hasmntopt="undef"
$ perl_package="''package'"
$ perl_baserev = "''baserev'"
$ cc_defines=""
$ perl_CONFIG="true"
$ perl_i_netdb="undef"
$ perl_d_gnulibc="undef"
$ perl_cf_by="unknown"
$ perl_ccdlflags=""
$ perl_cccdlflags=""
$ perl_mab=""
$ perl_libpth="/sys$share /sys$library"
$ perl_ld="Link"
$ perl_lddlflags="/Share"
$ perl_ranlib=""
$ perl_ar=""
$ perl_eunicefix=":"
$ perl_hint="none"
$ perl_i_arpainet="undef"
$ perl_d_grpasswd="undef"
$ perl_d_setgrent="undef"
$ perl_d_getgrent="define"
$ perl_d_endgrent="define"
$ perl_d_pwpasswd="define"
$ perl_d_setpwent="define"
$ perl_d_getpwent="define"
$ perl_d_endpwent="define"
$ perl_ebcdic="undef"
$ perl_hintfile=""
$ perl_shrplib="define"
$ perl_usemymalloc=mymalloc
$ perl_usevfork="true"
$ perl_useposix="false"
$ perl_spitshell="write sys$output "
$ perl_dlsrc="dl_vms.c"
$ perl_man1ext="rno"
$ perl_man3ext="rno"
$ perl_prefix="perl_root"
$ perl_binexp="''perl_prefix':[000000]"
$ perl_builddir="''perl_prefix':[000000]"
$ perl_installbin="''perl_prefix':[000000]"
$ perl_installscript="''perl_prefix':[000000]"
$ perl_installman1dir="''perl_prefix':[man.man1]"
$ perl_installman3dir="''perl_prefix':[man.man3]"
$ perl_installprivlib="''perl_prefix':[lib]"
$ perl_installsitelib="''perl_prefix':[lib.site_perl]"
$ perl_path_sep="|"
$ perl_cc=Mcc
$ perl_d_sockpair="undef"
$ perl_i_neterrno="define"
$ perl_ldflags="/NoTrace/NoMap"
$ perl_d_lchown="undef"
$ perl_d_mknod="undef"
$ perl_d_union_semun="undef"
$ perl_d_semctl_semun="undef"
$ perl_d_semctl_semid_ds="undef"
$ IF (sharedperl.EQS."Y")
$ THEN
$ perl_obj_ext=".abj"
$ perl_so="axe"
$ perl_dlext="axe"
$ perl_exe_ext=".axe"
$ perl_lib_ext=".alb"
$ ELSE
$ perl_obj_ext=".obj"
$ perl_so="exe"
$ perl_dlext="exe"
$ perl_exe_ext=".exe"
$ perl_lib_ext=".olb"
$ENDIF
$ perl_dlobj="dl_vms''perl_obj_ext'"
$ perl_osname="VMS"
$ perl_d_archlib="define"
$ perl_d_bincompat3="undef"
$ perl_cppstdin="''Perl_CC'/noobj/preprocess=sys$output sys$input"
$ perl_cppminus=""
$ perl_d_castneg="define"
$ perl_castflags="0"
$ perl_d_chsize="undef"
$ perl_d_const="define"
$ perl_d_crypt="define"
$ perl_byteorder="1234"
$ perl_full_csh=""
$ perl_d_csh="undef"
$ perl_d_dup2="define"
$ perl_d_fchmod="undef"
$ perl_d_fchown="undef"
$ perl_d_fcntl="undef"
$ perl_d_fgetpos="define"
$ perl_d_flexfnam="define"
$ perl_d_flock="undef"
$ perl_d_fsetpos="define"
$ perl_d_getgrps="undef"
$ perl_d_setgrps="undef"
$ perl_d_getprior="undef"
$ perl_d_killpg="undef"
$ perl_d_link="undef"
$ perl_d_lstat="undef"
$ perl_d_lockf="undef"
$ perl_d_memcmp="define"
$ perl_d_memcpy="define"
$ perl_d_memmove="define"
$ perl_d_memset="define"
$ perl_d_mkdir="define"
$ perl_d_msg="undef"
$ perl_d_open3="define"
$ perl_d_poll="undef"
$ perl_d_readdir="define"
$ perl_d_seekdir="define"
$ perl_d_telldir="define"
$ perl_d_rewinddir="define"
$ perl_d_rename="define"
$ perl_d_rmdir="define"
$ perl_d_sem="undef"
$ perl_d_setegid="undef"
$ perl_d_seteuid="undef"
$ perl_d_setprior="undef"
$ perl_d_setregid="undef"
$ perl_d_setresgid="undef"
$ perl_d_setreuid="undef"
$ perl_d_setresuid="undef"
$ perl_d_setrgid="undef"
$ perl_d_setruid="undef"
$ perl_d_setsid="undef"
$ perl_d_shm="undef"
$ perl_d_shmatprototype="undef"
$ perl_d_statblks="undef"
$ perl_stdio_ptr="((*fp)->_ptr)"
$ perl_stdio_cnt="((*fp)->_cnt)"
$ perl_stdio_base="((*fp)->_base)"
$ perl_stdio_bufsiz="((*fp)->_cnt + (*fp)->_ptr - (*fp)->_base)"
$ perl_d_strctcpy="define"
$ perl_d_strerror="define"
$ perl_d_syserrlst="undef"
$ perl_d_strerrm="strerror((e),vaxc$errno)"
$ perl_d_symlink="undef"
$ perl_d_syscall="undef"
$ perl_d_system="define"
$ perl_timetype="time_t"
$ perl_d_vfork="define"
$ perl_signal_t="void"
$ perl_d_volatile="define"
$ perl_d_vprintf="define"
$ perl_d_charvspr="undef"
$ perl_d_waitpid="define"
$ perl_i_dirent="undef"
$ perl_d_dirnamlen="define"
$ perl_direntrytype="struct dirent"
$ perl_i_fcntl="undef"
$ perl_i_grp="undef"
$ perl_i_limits="define"
$ perl_i_memory="undef"
$ perl_i_ndbm="undef"
$ perl_i_stdarg="define"
$ perl_i_pwd="undef"
$ perl_d_pwquota="undef"
$ perl_d_pwage="undef"
$ perl_d_pwchange="undef"
$ perl_d_pwclass="undef"
$ perl_d_pwexpire="undef"
$ perl_d_pwcomment="define"
$ perl_i_stddef="define"
$ perl_i_stdlib="define"
$ perl_i_string="define"
$ perl_i_sysdir="undef"
$ perl_i_sysfile="undef"
$ perl_i_sysioctl="undef"
$ perl_i_sysndir="undef"
$ perl_i_sysresrc="undef"
$ perl_i_sysselct="undef"
$ perl_i_dbm="undef"
$ perl_i_rpcsvcdbm="undef"
$ perl_i_sfio="undef"
$ perl_i_sysstat="define"
$ perl_i_systimes="undef"
$ perl_i_systypes="define"
$ perl_i_sysun="undef"
$ perl_i_syswait="undef"
$ perl_i_termio="undef"
$ perl_i_sgtty="undef"
$ perl_i_termios="undef"
$ perl_i_time="define"
$ perl_i_systime="undef"
$ perl_i_systimek="undef"
$! perl_i_unistd="undef"
$ perl_i_utime="undef"
$ perl_i_varargs="undef"
$ perl_i_vfork="undef"
$ perl_prototype="define"
$ perl_randbits="31"
$ perl_stdchar="char"
$ perl_d_unlink_all_versions="undef"
$ perl_full_sed="_NLA0:"
$ perl_bin="/''perl_prefix'/000000"
$ perl_binexp="''perl_prefix':[000000]"
$ perl_d_alarm="define"
$ perl_d_casti32="define"
$ perl_d_chown="define"
$ perl_d_chroot="undef"
$ perl_d_cuserid="define"
$ perl_d_dbl_dig="define"
$ perl_d_difftime="define"
$ perl_d_fork="undef"
$ perl_d_getlogin="define"
$ perl_d_getppid="undef"
$ perl_d_nice="define"
$ perl_d_pause="define"
$ perl_d_pipe="define"
$ perl_d_readlink="undef"
$ perl_d_setlinebuf="undef"
$ perl_d_strchr="define"
$ perl_d_strtod="define"
$ perl_d_strtol="define"
$ perl_d_strtoul="define"
$ perl_d_tcgetpgrp="undef"
$ perl_d_tcsetpgrp="undef"
$ perl_d_times="define"
$ perl_d_tzname="undef"
$ perl_d_umask="define"
$ perl_fpostype="fpos_t"
$ perl_i_dlfcn="undef"
$ perl_i_float="define"
$ perl_i_math="define"
$ perl_lseektype="int"
$ perl_i_values="undef"
$ perl_malloctype="void *"
$ perl_freetype="void"
$ if "''mymalloc'".eqs."Y"
$ THEN
$ perl_d_mymalloc="define"
$ ELSE
$ perl_d_mymalloc="undef"
$ENDIF
$ perl_sh="MCR"
$ perl_modetype="unsigned int"
$ perl_ssizetype="int"
$ perl_o_nonblock=""
$ perl_eagain=""
$ perl_rd_nodata=""
$ perl_d_eofnblk="undef"
$ perl_d_oldarchlib="define"
$ perl_privlibexp="''perl_prefix':[lib]"
$ perl_privlib="''perl_prefix':[lib]"
$ perl_sitelibexp="''perl_prefix':[lib.site_perl]"
$ perl_sitelib="''perl_prefix':[lib.site_perl]"
$ perl_sizetype="size_t"
$ perl_i_sysparam="undef"
$ perl_d_void_closedir="define"
$ perl_d_dlerror="undef"
$ perl_d_dlsymun="undef"
$ perl_d_suidsafe="undef"
$ perl_d_dosuid="undef"
$ perl_d_inetaton="undef"
$ perl_d_isascii="define"
$ perl_d_mkfifo="undef"
$ perl_d_safebcpy="undef"
$ perl_d_safemcpy="define"
$ perl_d_sanemcmp="define"
$ perl_d_setpgrp="undef"
$ perl_d_bsdsetpgrp="undef"
$ perl_d_bsdpgrp="undef"
$ perl_d_setpgid="undef"
$ perl_d_setpgrp2="undef"
$ perl_d_Gconvert="my_gconvert(x,n,t,b)"
$ perl_d_getpgid="undef"
$ perl_d_getpgrp="undef"
$ perl_d_bsdgetpgrp="undef"
$ perl_d_getpgrp2="undef"
$ perl_d_sfio="undef"
$ perl_usedl="define"
$ perl_startperl="""$ perl 'f$env(\""procedure\"")' 'p1' 'p2' 'p3' 'p4' 'p5' 'p6' 'p7' 'p8'  !\n$ exit++ + ++$status != 0 and $exit = $status = undef;"""
$ perl_db_hashtype=""
$ perl_db_prefixtype=""
$ perl_useperlio="undef"
$ perl_defvoidused="15"
$ perl_voidflags="15"
$ perl_d_eunice="undef"
$ perl_d_pwgecos="define"
$ IF ("''Use_Threads'".eqs."T").and.("''VMS_VER'".LES."6.2")
$ THEN
$ perl_libs="SYS$SHARE:CMA$LIB_SHR.EXE/SHARE SYS$SHARE:CMA$RTL.EXE/SHARE SYS$SHARE:CMA$OPEN_LIB_SHR.exe/SHARE SYS$SHARE:CMA$OPEN_RTL.exe/SHARE"
$ ELSE
$ perl_libs=""
$ ENDIF
$ IF ("''Using_Dec_C'".eqs."Yes")
$ THEN
$ perl_libc="(DECCRTL)"
$ ELSE
$ perl_libc=""
$ ENDIF
$ perl_PATCHLEVEL="''patchlevel'"
$ perl_SUBVERSION="''subversion'"
$ perl_pager="most"
$!
$!
$! Now some that we build up
$!
$ LocalTime = f$time()
$ perl_cf_time= f$extract(0, 3, f$cvtime(LocalTime,, "WEEKDAY")) + " " + - 
                f$edit(f$cvtime(LocalTime, "ABSOLUTE", "MONTH"), "LOWERCASE") + -
                " " + f$cvtime(LocalTime,, "DAY") + " " + f$cvtime(LocalTime,, "TIME") + -
                " " + f$cvtime(LocalTime,, "YEAR")
$ if f$getsyi("HW_MODEL").ge.1024
$ THEN
$ perl_arch="VMS_AXP"
$ perl_archname="VMS_AXP"
$ perl_alignbytes="8"
$ ELSE
$ perl_arch="VMS_VAX"
$ perl_archname="VMS_VAX"
$ perl_alignbytes="8"
$ ENDIF
$ if ("''Use_Threads'".eqs."T")
$ THEN
$ perl_arch = "''perl_arch'-thread"
$ perl_archname = "''perl_archname'-thread"
$ ENDIF
$ perl_osvers=f$edit(osvers, "TRIM")
$ if (perl_subversion + 0).eq.0
$ THEN
$ LocalPerlVer = "5_" + Perl_PATCHLEVEL
$ ELSE
$ LocalPerlVer = "5_" + Perl_PATCHLEVEL + perl_subversion
$ ENDIF
$!
$! Some that we need to invoke the compiler for
$ OS := "open/write SOURCECHAN []temp.c"
$ WS := "write SOURCECHAN"
$ CS := "close SOURCECHAN"
$ DS := "delete/nolog []temp.*;*"
$ Needs_Opt := "No"
$ if ("''using_vax_c'".eqs."Yes").or.("''using_gnu_c'".eqs."Yes")
$ THEN
$   open/write OPTCHAN []temp.opt
$   IF ("''using_gnu_c'".eqs."Yes")
$   THEN
$     write OPTCHAN "Gnu_CC:[000000]gcclib.olb/library"
$   endif
$   write OPTCHAN "Sys$Share:VAXCRTL/Share"
$   Close OPTCHAN
$   Needs_Opt := "Yes"
$ ENDIF
$!
$! Check for __STDC__
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "#ifdef __STDC__
$ WS "printf(""42\n"");
$ WS "#else
$ WS "printf(""1\n"");
$ WS "#endif
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   ON ERROR THEN CONTINUE
$   ON WARNING THEN CONTINUE
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$   DEFINE SYS$ERROR TEMPOUT
$   DEFINE SYS$OUTPUT TEMPOUT
$   mcr []temp
$   CLOSE TEMPOUT
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   OPEN/READ TEMPOUT [-.uu]tempout.lis
$   READ TEMPOUT line
$   CLOSE TEMPOUT
$ 
$ perl_cpp_stuff=line
$ WRITE_RESULT "cpp_stuff is ''perl_cpp_stuff'"
$!
$! Check for double size
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "int foo;
$ WS "foo = sizeof(double);
$ WS "printf(""%d\n"", foo);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   ON ERROR THEN CONTINUE
$   ON WARNING THEN CONTINUE
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$!   link temp.obj
$   OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   DEFINE SYS$ERROR TEMPOUT
$   DEFINE SYS$OUTPUT TEMPOUT
$   mcr []temp
$   CLOSE TEMPOUT
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   OPEN/READ TEMPOUT [-.uu]tempout.lis
$   READ TEMPOUT line
$   CLOSE TEMPOUT
$ 
$ perl_doublesize=line
$ WRITE_RESULT "doublesize is ''perl_doublesize'"
$!
$! Check for long double size
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "printf(""%d\n"", sizeof(long double));
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   ON ERROR THEN CONTINUE
$   ON WARNING THEN CONTINUE
$   'Checkcc' temp.c
$   teststatus = f$extract(9,1,$status)
$   if (teststatus.nes."1")
$   THEN
$     perl_longdblsize="0"
$     perl_d_longdbl="undef"
$   ELSE
$     ON ERROR THEN CONTINUE
$     ON WARNING THEN CONTINUE
$     If (Needs_Opt.eqs."Yes")
$     THEN
$     link temp.obj,temp.opt/opt
$     else
$       link temp.obj
$     endif
$     teststatus = f$extract(9,1,$status)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_longdblsize="0"
$       perl_d_longdbl="undef"
$     ELSE
$       OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$       DEFINE SYS$ERROR TEMPOUT
$       DEFINE SYS$OUTPUT TEMPOUT
$       mcr []temp
$       CLOSE TEMPOUT
$       DEASSIGN SYS$OUTPUT
$       DEASSIGN SYS$ERROR
$       OPEN/READ TEMPOUT [-.uu]tempout.lis
$       READ TEMPOUT line
$       CLOSE TEMPOUT
$ 
$       perl_longdblsize=line
$       perl_d_longdbl="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "longdblsize is ''perl_longdblsize'"
$ WRITE_RESULT "d_longdbl is ''perl_d_longdbl'"
$!
$! Check for long long existance and size
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "printf(""%d\n"", sizeof(long long));
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_longlongsize="0"
$     perl_d_longlong="undef"
$   ELSE
$     OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$     DEFINE SYS$ERROR TEMPOUT
$     DEFINE SYS$OUTPUT TEMPOUT
$     mcr []temp
$     CLOSE TEMPOUT
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     OPEN/READ TEMPOUT [-.uu]tempout.lis
$     READ TEMPOUT line
$     CLOSE TEMPOUT
$ 
$     perl_longlongsize=line
$     perl_d_longlong="define"
$   ENDIF
$ WRITE_RESULT "longlongsize is ''perl_longlongsize'"
$ WRITE_RESULT "d_longlong is ''perl_d_longlong'"
$!
$! Check for int size
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "printf(""%d\n"", sizeof(int));
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   DEFINE SYS$ERROR TEMPOUT
$   DEFINE SYS$OUTPUT TEMPOUT
$   mcr []temp
$   CLOSE TEMPOUT
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   OPEN/READ TEMPOUT [-.uu]tempout.lis
$   READ TEMPOUT line
$   CLOSE TEMPOUT
$ 
$   perl_intsize=line
$ WRITE_RESULT "intsize is ''perl_intsize'"
$!
$! Check for short size
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "printf(""%d\n"", sizeof(short));
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   DEFINE SYS$ERROR TEMPOUT
$   DEFINE SYS$OUTPUT TEMPOUT
$   mcr []temp
$   CLOSE TEMPOUT
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   OPEN/READ TEMPOUT [-.uu]tempout.lis
$   READ TEMPOUT line
$   CLOSE TEMPOUT
$ 
$   perl_shortsize=line
$ WRITE_RESULT "shortsize is ''perl_shortsize'"
$!
$! Check for long size
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "int foo;
$ WS "foo = sizeof(long);
$ WS "printf(""%d\n"", foo);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   DEFINE SYS$ERROR TEMPOUT
$   DEFINE SYS$OUTPUT TEMPOUT
$   mcr []temp
$   CLOSE TEMPOUT
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   OPEN/READ TEMPOUT [-.uu]tempout.lis
$   READ TEMPOUT line
$   CLOSE TEMPOUT
$ 
$   perl_longsize=line
$ WRITE_RESULT "longsize is ''perl_longsize'"
$!
$! Check the prototype for getgid
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#include <unistd.h>
$ WS "int main()
$ WS "{"
$ WS "gid_t foo;
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$!   Okay, gid_t failed. Must be unsigned int
$     perl_gidtype = "unsigned int"
$   ELSE
$     perl_gidtype = "gid_t"
$   ENDIF
$ WRITE_RESULT "Gid_t is ''perl_gidtype'"
$!
$! Check to see if we've got dev_t
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#include <unistd.h>
$ WS "int main()
$ WS "{"
$ WS "dev_t foo;
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$!   Okay, dev_t failed. Must be unsigned int
$     perl_devtype = "unsigned int"
$   ELSE
$     perl_devtype = "dev_t"
$   ENDIF
$ WRITE_RESULT "Dev_t is ''perl_devtype'"
$!
$! Check to see if we've got unistd.h (which we ought to, but you never know)
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <unistd.h>
$ WS "int main()
$ WS "{"
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$!   Okay, failed. Must not have it
$     perl_i_unistd = "undef"
$   ELSE
$     perl_i_unistd = "define"

$   ENDIF
$ WRITE_RESULT "i_unistd is ''perl_i_unistd'"
$!
$! Check the prototype for select
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#include <unistd.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <time.h>
$  WS "#include <socket.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "fd_set *foo;
$ WS "int bar;
$ WS "foo = NULL;
$ WS "bar = select(2, foo, foo, foo, NULL);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$!   Okay, fd_set failed. Must be an int
$     perl_selecttype = "int *"
$   ELSE
$     perl_selecttype="fd_set *"
$   ENDIF
$ ELSE
$   ! No sockets, so stick in an int *
$   perl_selecttype = "int *"
$ ENDIF
$ WRITE_RESULT "selectype is ''perl_selecttype'"
$!
$! Check for sys/file.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <sys/file.h>
$ WS "int main()
$ WS "{"
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   savedstatus = $status
$   teststatus = f$extract(9,1,savedstatus)
$   if (teststatus.nes."1")
$   THEN
$     perl_i_sysfile="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     If (Needs_Opt.eqs."Yes")
$     THEN
$       link temp.obj,temp.opt/opt
$     else
$       link temp.obj
$     endif
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_i_sysfile="undef"
$     ELSE
$       perl_i_sysfile="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_sysfile is ''perl_i_sysfile'"
$!
$! Check for fcntl.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <fcntl.h>
$ WS "int main()
$ WS "{"
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   savedstatus = $status
$   teststatus = f$extract(9,1,savedstatus)
$   if (teststatus.nes."1")
$   THEN
$     perl_i_fcntl="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     If (Needs_Opt.eqs."Yes")
$     THEN
$       link temp.obj,temp.opt/opt
$     else
$       link temp.obj
$     endif
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_i_fcntl="undef"
$     ELSE
$       perl_i_fcntl="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_fcntl is ''perl_i_fcntl'"
$!
$! Check for fcntl
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <fcntl.h>
$ WS "int main()
$ WS "{"
$ WS "fcntl(1,2,3);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   savedstatus = $status
$   teststatus = f$extract(9,1,savedstatus)
$   if (teststatus.nes."1")
$   THEN
$     perl_d_fcntl="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     If (Needs_Opt.eqs."Yes")
$     THEN
$       link temp.obj,temp.opt/opt
$     else
$       link temp.obj
$     endif
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_d_fcntl="undef"
$     ELSE
$       perl_d_fcntl="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_fcntl is ''perl_d_fcntl'"
$!
$! Check for bzero
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <strings.h>
$ WS "int main()
$ WS "{"
$ WS "char foo[10];
$ WS "bzero(foo, 10);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   savedstatus = $status
$   teststatus = f$extract(9,1,savedstatus)
$   if (teststatus.nes."1")
$   THEN
$     perl_d_bzero="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     If (Needs_Opt.eqs."Yes")
$     THEN
$       link temp.obj,temp.opt/opt
$     else
$       link temp.obj
$     endif
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_d_bzero="undef"
$     ELSE
$       perl_d_bzero="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_bzero is ''perl_d_bzero'"
$!
$! Check for bcopy
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <strings.h>
$ WS "int main()
$ WS "{"
$ WS "char foo[10], bar[10];
$ WS "bcopy(""foo"", bar, 3);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   savedstatus = $status
$   teststatus = f$extract(9,1,savedstatus)
$   if (teststatus.nes."1")
$   THEN
$     perl_d_bcopy="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     If (Needs_Opt.eqs."Yes")
$     THEN
$       link temp.obj,temp.opt/opt
$     else
$       link temp.obj
$     endif
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_d_bcopy="undef"
$     ELSE
$       perl_d_bcopy="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_bcopy is ''perl_d_bcopy'"
$!
$! Check for mkstemp
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "mkstemp(""foo"");
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   savedstatus = $status
$   teststatus = f$extract(9,1,savedstatus)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_mkstemp="undef"
$   ELSE
$     perl_d_mkstemp="define"
$   ENDIF
$ WRITE_RESULT "d_mkstemp is ''perl_d_mkstemp'"
$!
$! Check for setvbuf
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "FILE *foo;
$ WS "char Buffer[99];
$ WS "foo = fopen(""foo"", ""r"");
$ WS "setvbuf(foo, Buffer, 0, 0);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_setvbuf="undef"
$   ELSE
$     perl_d_setvbuf="define"
$   ENDIF
$ WRITE_RESULT "d_setvbuf is ''perl_d_setvbuf'"
$!
$! Check for <netinet/in.h>
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "#include <netinet/in.h>"
$ WS "int main()
$ WS "{"
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_i_niin="undef"
$   ELSE
$     perl_i_niin="define"
$   ENDIF
$ ELSE
$   perl_i_niin="undef"
$ ENDIF
$ WRITE_RESULT "i_niin is ''perl_i_niin'"
$!
$! Check for endhostent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "endhostent();
$ WS "exit(0);

$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_endhent="undef"
$   ELSE
$     perl_d_endhent="define"
$   ENDIF
$ ELSE
$ perl_d_endhent="undef"
$ ENDIF
$ WRITE_RESULT "d_endhent is ''perl_d_endhent'"
$!
$! Check for endnetent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "endnetent();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_endnent="undef"
$   ELSE
$     perl_d_endnent="define"
$   ENDIF
$ ELSE
$ perl_d_endnent="undef"
$ ENDIF
$ WRITE_RESULT "d_endnent is ''perl_d_endnent'"
$!
$! Check for endprotoent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "endprotoent();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_endpent="undef"
$   ELSE
$     perl_d_endpent="define"
$   ENDIF
$ ELSE
$ perl_d_endpent="undef"
$ ENDIF
$ WRITE_RESULT "d_endpent is ''perl_d_endpent'"
$!
$! Check for endservent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "endservent();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_endsent="undef"
$   ELSE
$     perl_d_endsent="define"
$   ENDIF
$ ELSE
$ perl_d_endsent="undef"
$ ENDIF
$ WRITE_RESULT "d_endsent is ''perl_d_endsent'"
$!
$! Check for sethostent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "sethostent(1);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_sethent="undef"
$   ELSE
$     perl_d_sethent="define"
$   ENDIF
$ ELSE
$ perl_d_sethent="undef"
$ ENDIF
$ WRITE_RESULT "d_sethent is ''perl_d_sethent'"
$!
$! Check for setnetent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "setnetent(1);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_setnent="undef"
$   ELSE
$     perl_d_setnent="define"
$   ENDIF
$ ELSE
$ perl_d_setnent="undef"
$ ENDIF
$ WRITE_RESULT "d_setnent is ''perl_d_setnent'"
$!
$! Check for setprotoent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "setprotoent(1);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_setpent="undef"
$   ELSE
$     perl_d_setpent="define"
$   ENDIF
$ ELSE
$ perl_d_setpent="undef"
$ ENDIF
$ WRITE_RESULT "d_setpent is ''perl_d_setpent'"
$!
$! Check for setservent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "setservent(1);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_setsent="undef"
$   ELSE
$     perl_d_setsent="define"
$   ENDIF
$ ELSE
$ perl_d_setsent="undef"
$ ENDIF
$ WRITE_RESULT "d_setsent is ''perl_d_setsent'"
$!
$! Check for gethostent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "gethostent();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_gethent="undef"
$   ELSE
$     perl_d_gethent="define"
$   ENDIF
$ ELSE
$ perl_d_gethent="undef"
$ ENDIF
$ WRITE_RESULT "d_gethent is ''perl_d_gethent'"
$!
$! Check for getnetent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "getnetent();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_getnent="undef"
$   ELSE
$     perl_d_getnent="define"
$   ENDIF
$ ELSE
$ perl_d_getnent="undef"
$ ENDIF
$ WRITE_RESULT "d_getnent is ''perl_d_getnent'"
$!
$! Check for getprotoent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "getprotoent();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_getpent="undef"
$   ELSE
$     perl_d_getpent="define"
$   ENDIF
$ ELSE
$ perl_d_getpent="undef"
$ ENDIF
$ WRITE_RESULT "d_getpent is ''perl_d_getpent'"
$!
$! Check for getservent
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ if ("''Has_Socketshr'".eqs."T")
$ THEN
$  WS "#include <socketshr.h>"
$ else
$  WS "#include <netdb.h>
$ endif
$ WS "int main()
$ WS "{"
$ WS "getservent();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_getsent="undef"
$   ELSE
$     perl_d_getsent="define"
$   ENDIF
$ ELSE
$ perl_d_getsent="undef"
$ ENDIF
$ WRITE_RESULT "d_getsent is ''perl_d_getsent'"
$!
$! Check for pthread_yield
$!
$ if ("''use_threads'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <pthread.h>
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "pthread_yield();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_pthread_yield="undef"
$   ELSE
$     perl_d_pthread_yield="define"
$   ENDIF
$ ELSE
$   perl_d_pthread_yield="undef"
$ ENDIF
$ WRITE_RESULT "d_pthread_yield is ''perl_d_pthread_yield'"
$!
$! Check for sched_yield
$!
$ if ("''use_threads'".eqs."T")
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <pthread.h>
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "sched_yield();
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp.c
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_sched_yield="undef"
$   ELSE
$     perl_d_sched_yield="define"
$   ENDIF
$ ELSE
$   perl_d_sched_yield="undef"
$ ENDIF
$ WRITE_RESULT "d_sched_yield is ''perl_d_sched_yield'"
$!
$! Check for generic pointer size
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "int foo;
$ WS "foo = sizeof(char *);
$ WS "printf(""%d\n"", foo);
$ WS "exit(0);
$ WS "}"
$ CS
$! copy temp.c sys$output
$!
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   ON ERROR THEN CONTINUE
$   ON WARNING THEN CONTINUE
$   'Checkcc' temp.c
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp.obj,temp.opt/opt
$   else
$     link temp.obj
$   endif
$   OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   DEFINE SYS$ERROR TEMPOUT
$   DEFINE SYS$OUTPUT TEMPOUT
$   mcr []temp
$   CLOSE TEMPOUT
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   OPEN/READ TEMPOUT [-.uu]tempout.lis
$   READ TEMPOUT line
$   CLOSE TEMPOUT
$ 
$ perl_ptrsize=line
$ WRITE_RESULT "ptrsize is ''perl_ptrsize'"
$!
$ set nover
$! Done with compiler checks. Clean up.
$ if f$search("temp.c").nes."" then DELETE/NOLOG temp.c;*
$ if f$search("temp.obj").nes."" then DELETE/NOLOG temp.obj;*
$ if f$search("temp.exe").nes."" then DELETE/NOLOG temp.exe;*
$ if f$search("temp.opt").nes."" then DELETE/NOLOG Temp.opt;*
$!
$!
$! Some that are compiler or VMS version sensitive
$!
$! Gnu C stuff
$ IF "''Using_Gnu_C'".EQS."Yes"
$ THEN
$   perl_d_attribut="define"
$   perl_vms_cc_type="gcc"
$ ELSE
$   perl_d_attribut="undef"
$ ENDIF
$
$! Dec C >= 5.2 and VMS ver >= 7.0
$ IF ("''Using_Dec_C'".EQS."Yes").AND.(F$INTEGER(Dec_C_Version).GE.50200000).AND.("''VMS_VER'".GES."7.0")
$ THEN
$ perl_d_bcmp="define"
$ perl_d_gettimeod="define"
$ perl_d_uname="define"
$ perl_d_sigaction="define"
$ perl_d_truncate="define"
$ perl_d_wait4="define"
$ perl_d_index="define"
$ perl_pidtype="pid_t"
$ perl_sig_name="ZERO HUP INT QUIT ILL TRAP IOT EMT FPE KILL BUS SEGV SYS PIPE ALRM TERM ABRT USR1 USR2 SPARE18 SPARE19 CHLD CONT STOP TSTP TTIN TTOU DEBUG SPARE27 SPARE28 SPARE29 SPARE30 SPARE31 SPARE32 RTMIN RTMAX"",0"
$ psnwc1="""ZERO"",""HUP"",""INT"",""QUIT"",""ILL"",""TRAP"",""IOT"",""EMT"",""FPE"",""KILL"",""BUS"",""SEGV"",""SYS"","
$ psnwc2="""PIPE"",""ALRM"",""TERM"",""ABRT"",""USR1"",""USR2"",""SPARE18"",""SPARE19"",""CHLD"",""CONT"",""STOP"",""TSTP"","
$ psnwc3="""TTIN"",""TTOU"",""DEBUG"",""SPARE27"",""SPARE28"",""SPARE29"",""SPARE30"",""SPARE31"",""SPARE32"",""RTMIN"",""RTMAX"",0"
$perl_sig_name_with_commas = psnwc1 + psnwc2 + psnwc3
$ perl_sig_num="0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,6,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,64,0"
$ perl_sig_num_with_commas=perl_sig_num
$ perl_uidtype="uid_t"
$ perl_d_pathconf="define"
$ perl_d_fpathconf="define"
$ perl_d_sysconf="define"
$ perl_d_sigsetjmp="define"
$ ELSE
$ perl_pidtype="unsigned int"
$ perl_d_gettimeod="undef"
$ perl_d_bcmp="undef"
$ perl_d_uname="undef"
$ perl_d_sigaction="undef"
$ perl_d_truncate="undef"
$ perl_d_wait4="undef"
$ perl_d_index="undef"
$ perl_sig_name="ZERO HUP INT QUIT ILL TRAP IOT EMT FPE KILL BUS SEGV SYS PIPE ALRM TERM ABRT USR1 USR2"",0"
$ psnwc1="""ZERO"",""HUP"",""INT"",""QUIT"",""ILL"",""TRAP"",""IOT"",""EMT"",""FPE"",""KILL"",""BUS"",""SEGV"",""SYS"","
$ psnwc2="""PIPE"",""ALRM"",""TERM"",""ABRT"",""USR1"",""USR2"",0"
$ perl_sig_name_with_commas = psnwc1 + psnwc2
$ perl_sig_num="0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,6,16,17,0"
$ perl_sig_num_with_commas=perl_sig_num
$ perl_uidtype="unsigned int"
$ perl_d_pathconf="undef"
$ perl_d_fpathconf="undef"
$ perl_d_sysconf="undef"
$ perl_d_sigsetjmp="undef"
$ ENDIF
$!
$! Dec C alone
$ IF ("''Using_Dec_C'".EQS."Yes")
$ THEN
$ perl_d_mbstowcs="define"
$ perl_d_mbtowc="define"
$ perl_d_stdiobase="define"
$ perl_d_stdio_ptr_lval="define"
$ perl_d_stdio_cnt_lval="define"
$ perl_d_stdstdio="define"
$ perl_d_wcstombs="define"
$ perl_d_mblen="define"
$ perl_d_mktime="define"
$ perl_d_strcoll="define"
$ perl_d_strxfrm="define"
$ perl_d_wctomb="define"
$ perl_i_locale="define"
$ perl_d_locconv="define"
$ perl_d_setlocale="define"
$ perl_vms_cc_type="decc"
$ ELSE
$ perl_d_mbstowcs="undef"
$ perl_d_mbtowc="undef"
$ perl_d_stdiobase="undef"
$ perl_d_stdio_ptr_lval="undef"
$ perl_d_stdio_cnt_lval="undef"
$ perl_d_stdstdio="undef"
$ perl_d_wcstombs="undef"
$ perl_d_mblen="undef"
$ perl_d_mktime="undef"
$ perl_d_strcoll="undef"
$ perl_d_strxfrm="undef"
$ perl_d_wctomb="undef"
$ perl_i_locale="undef"
$ perl_d_locconv="undef"
$ perl_d_setlocale="undef"
$ ENDIF
$!
$! Vax C stuff
$ if ("''Using_Vax_C'".EQS."Yes")
$ THEN
$ perl_vms_cc_type="vaxc"
$ ENDIF
$!
$!
$! Sockets?
$ if ("''Has_Socketshr'".EQS."T").OR.("''Has_Dec_C_Sockets'".EQS."T")
$ THEN
$ perl_d_vms_do_sockets="define"
$ perl_d_htonl="define"
$ perl_d_socket="define"
$ perl_d_select="define"
$ perl_netdb_host_type="char *"
$ perl_netdb_hlen_type="int"
$ perl_netdb_name_type="char *"
$ perl_netdb_net_type="long"
$ perl_d_gethbyaddr="define"
$ perl_d_gethbyname="define"
$ perl_d_getnbyaddr="define"
$ perl_d_getnbyname="define"
$ perl_d_getpbynumber="define"
$ perl_d_getpbyname="define"
$ perl_d_getsbyport="define"
$ perl_d_getsbyname="define"
$ perl_d_gethostprotos="define"
$ perl_d_getnetprotos="define"
$ perl_d_getprotoprotos="define"
$ perl_d_getservprotos="define"
$ ELSE
$ perl_d_vms_do_sockets="undef"
$ perl_d_htonl="undef"
$ perl_d_socket="undef"
$ perl_d_select="undef"
$ perl_netdb_host_type="char *"
$ perl_netdb_hlen_type="int"
$ perl_netdb_name_type="char *"
$ perl_netdb_net_type="long"
$ perl_d_gethbyaddr="undef"
$ perl_d_gethbyname="undef"
$ perl_d_getnbyaddr="undef"
$ perl_d_getnbyname="undef"
$ perl_d_getpbynumber="undef"
$ perl_d_getpbyname="undef"
$ perl_d_getsbyport="undef"
$ perl_d_getsbyname="undef"
$ perl_d_gethostprotos="undef"
$ perl_d_getnetprotos="undef"
$ perl_d_getprotoprotos="undef"
$ perl_d_getservprotos="undef"
$ ENDIF
$! Threads
$ if ("''use_threads'".eqs."T")
$ THEN
$   perl_usethreads="define"
$   perl_d_pthreads_created_joinable="define"
$   if ("''VMS_VER'".ges."7.0")
$   THEN
$     perl_d_oldpthreads="undef"
$   ELSE
$     perl_d_oldpthreads="define"
$   ENDIF
$ ELSE
$   perl_d_oldpthreads="undef"
$   perl_usethreads="undef"
$   
$   perl_d_pthreads_created_joinable="undef"
$ ENDIF
$! 
$! 
$! Finally the composite ones. All config
$ perl_installarchlib="''perl_prefix':[lib.''perl_arch'.''localperlver']"
$ perl_installsitearch="''perl_prefix':[lib.site_perl.''perl_arch']"
$ perl_myhostname="''myhostname'"
$ perl_mydomain="''mydomain'"
$ perl_perladmin="''perladmin'"
$ perl_cf_email="''cf_email'"
$ perl_myuname:="VMS ''myname' ''f$edit(perl_osvers, "TRIM")' ''f$edit(hwname, "TRIM")'"
$ perl_archlibexp="''perl_prefix':[lib.''perl_arch'.''localperlver']"
$ perl_archlib="''perl_prefix':[lib.''perl_arch'.''lovalperlver']"
$ perl_oldarchlibexp="''perl_prefix':[lib.''perl_arch']"
$ perl_oldarchlib="''perl_prefix':[lib.''perl_arch']"
$ perl_sitearchexp="''perl_prefix':[lib.site_perl.''perl_arch']"
$ perl_sitearch="''perl_prefix':[lib.site_perl.''perl_arch']"
$ if "''Using_Dec_C'" .eqs. "Yes"
$ THEN
$ perl_ccflags="/Include=[]/Standard=Relaxed_ANSI/Prefix=All/Obj=''perl_obj_ext'/NoList''cc_flags'"
$ ELSE
$   IF "''Using_Vax_C'" .eqs. "Yes"
$   THEN
$     perl_ccflags="/Include=[]/Obj=''perl_obj_ext'/NoList''cc_flags'"
$   ENDIF
$ ENDIF
$!
$! Finally clean off any leading zeros from the patchlevel or subversion
$ perl_patchlevel = perl_patchlevel + 0
$ perl_subversion = perl_subversion + 0
$!
$! Okay, we've got everything configured. Now go write out a config.sh.
$ open/write CONFIGSH [-]config.sh
$ WC := "write CONFIGSH"
$!
$ WC "# This file generated by Configure.COM on a VMS system."
$ WC "# Time: " + perl_cf_time
$ WC ""
$ WC "package='" + perl_package + "'"
$ WC "CONFIG='" + perl_config + "'"
$ WC "cf_time='" + perl_cf_time + "'"
$ WC "cf_by='" + perl_cf_by+ "'"
$ WC "cpp_stuff='" + perl_cpp_stuff + "'"
$ WC "ccdlflags='" + perl_ccdlflags + "'"
$ WC "cccdlflags='" + perl_cccdlflags + "'"
$ WC "mab='" + perl_mab + "'"
$ WC "libpth='" + perl_libpth + "'"
$ WC "ld='" + perl_ld + "'"
$ WC "lddlflags='" + perl_lddlflags + "'"
$ WC "ranlib='" + perl_ranlib + "'"
$ WC "ar='" + perl_ar + "'"
$ WC "eunicefix='" + perl_eunicefix + "'"
$ WC "hint='" + perl_hint +"'"
$ WC "hintfile='" + perl_hintfile + "'"
$ WC "shrplib='" + perl_shrplib + "'"
$ WC "usemymalloc='" + perl_usemymalloc + "'"
$ WC "usevfork='" + perl_usevfork + "'"
$ WC "useposix='false'"
$ WC "spitshell='write sys$output '"
$ WC "dlsrc='dl_vms.c'"
$ WC "binexp='" + perl_binexp + "'"
$ WC "man1ext='" + perl_man1ext + "'"
$ WC "man3ext='" + perl_man3ext + "'"
$ WC "arch='" + perl_arch + "'"
$ WC "archname='" + perl_archname + "'"
$ WC "osvers='" + perl_osvers + "'"
$ WC "prefix='" + perl_prefix + "'"
$ WC "builddir='" + perl_builddir + "'"
$ WC "installbin='" + perl_installbin + "'"
$ WC "installscript='" + perl_installscript + "'"
$ WC "installman1dir='" + perl_installman1dir + "'"
$ WC "installman3dir='" + perl_installman3dir + "'"
$ WC "installprivlib='" + perl_installprivlib + "'"
$ WC "installarchlib='" + perl_installarchlib + "'"
$ WC "installsitelib='" + perl_installsitelib + "'"
$ WC "installsitearch='" + perl_installsitearch + "'"
$ WC "path_sep='" + perl_path_sep + "'"
$ WC "vms_cc_type='" + perl_vms_cc_type + "'"
$ WC "d_attribut='" + perl_d_attribut + "'"
$ WC "cc='" + perl_cc + "'"
$ WC "ccflags='" + perl_ccflags + "'"
$ WC "d_vms_do_sockets='" + perl_d_vms_do_sockets + "'"
$ WC "d_socket='" + perl_d_socket + "'"
$ WC "d_sockpair='" + perl_d_sockpair + "'"
$ WC "d_gethent='" + perl_d_gethent + "'"
$ WC "d_getsent='" + perl_d_getsent + "'"
$ WC "d_select='" + perl_d_select + "'"
$ WC "i_niin='" + perl_i_niin + "'"
$ WC "i_neterrno='" + perl_i_neterrno + "'"
$ WC "d_stdstdio='" + perl_d_stdstdio + "'"
$ WC "d_stdio_ptr_lval='" + perl_d_stdio_ptr_lval + "'"
$ WC "d_stdio_cnt_lval='" + perl_d_stdio_cnt_lval + "'"
$ WC "d_stdiobase='" + perl_d_stdiobase + "'"
$ WC "d_locconv='" + perl_d_locconv + "'"
$ WC "d_setlocale='" + perl_d_setlocale + "'"
$ WC "i_locale='" + perl_i_locale + "'"
$ WC "d_mbstowcs='" + perl_d_mbstowcs + "'"
$ WC "d_mbtowc='" + perl_d_mbtowc + "'"
$ WC "d_wcstombs='" + perl_d_wcstombs + "'"
$ WC "d_wctomb='" + perl_d_wctomb + "'"
$ WC "d_mblen='" + perl_d_mblen + "'"
$ WC "d_mktime='" + perl_d_mktime + "'"
$ WC "d_strcoll='" + perl_d_strcoll + "'"
$ WC "d_strxfrm='" + perl_d_strxfrm  + "'"
$ WC "ldflags='" + perl_ldflags + "'"
$ WC "dlobj='" + perl_dlobj + "'"
$ WC "obj_ext='" + perl_obj_ext + "'"
$ WC "so='" + perl_so + "'"
$ WC "dlext='" + perl_dlext + "'"
$ WC "exe_ext='" + perl_exe_ext + "'"
$ WC "lib_ext='" + perl_lib_ext + "'"
$ WC "myhostname='" + perl_myhostname + "'"
$ WC "mydomain='" + perl_mydomain + "'"
$ WC "perladmin='" + perl_perladmin + "'"
$ WC "cf_email='" + perl_cf_email + "'"
$ WC "myuname='" + perl_myuname + "'"
$ WC "alignbytes='" + perl_alignbytes + "'"
$ WC "osname='" + perl_osname + "'"
$ WC "d_archlib='" + perl_d_archlib + "'"
$ WC "archlibexp='" + perl_archlibexp + "'"
$ WC "archlib='" + perl_archlib + "'"
$ WC "archname='" + perl_archname + "'"
$ WC "d_bincompat3='" + perl_d_bincompat3 + "'"
$ WC "cppstdin='" + perl_cppstdin + "'"
$ WC "cppminus='" + perl_cppminus + "'"
$ WC "d_bcmp='" + perl_d_bcmp + "'"
$ WC "d_bcopy='" + perl_d_bcopy + "'"
$ WC "d_bzero='" + perl_d_bzero + "'"
$ WC "d_castneg='" + perl_d_castneg + "'"
$ WC "castflags='" + perl_castflags + "'"
$ WC "d_chsize='" + perl_d_chsize + "'"
$ WC "d_const='" + perl_d_const + "'"
$ WC "d_crypt='" + perl_d_crypt + "'"
$ WC "byteorder='" + perl_byteorder + "'"
$ WC "full_csh='" + perl_full_csh + "'"
$ WC "d_csh='" + perl_d_csh + "'"
$ WC "d_dup2='" + perl_d_dup2 + "'"
$ WC "d_fchmod='" + perl_d_fchmod + "'"
$ WC "d_fchown='" + perl_d_fchown + "'"
$ WC "d_fcntl='" + perl_d_fcntl + "'"
$ WC "d_fgetpos='" + perl_d_fgetpos + "'"
$ WC "d_flexfnam='" + perl_d_flexfnam + "'"
$ WC "d_flock='" + perl_d_flock + "'"
$ WC "d_fsetpos='" + perl_d_fsetpos + "'"
$ WC "d_gettimeod='" + perl_d_gettimeod + "'"
$ WC "d_getgrps='" + perl_d_getgrps + "'"
$ WC "d_setgrps='" + perl_d_setgrps + "'"
$ WC "d_uname='" + perl_d_uname + "'"
$ WC "d_getprior='" + perl_d_getprior + "'"
$ WC "d_killpg='" + perl_d_killpg + "'"
$ WC "d_link='" + perl_d_link + "'"
$ WC "d_lstat='" + perl_d_lstat + "'"
$ WC "d_lockf='" + perl_d_lockf + "'"
$ WC "d_memcmp='" + perl_d_memcmp + "'"
$ WC "d_memcpy='" + perl_d_memcpy + "'"
$ WC "d_memmove='" + perl_d_memmove + "'"
$ WC "d_memset='" + perl_d_memset + "'"
$ WC "d_mkdir='" + perl_d_mkdir + "'"
$ WC "d_msg='" + perl_d_msg + "'"
$ WC "d_open3='" + perl_d_open3 + "'"
$ WC "d_poll='" + perl_d_poll + "'"
$ WC "d_readdir='" + perl_d_readdir + "'"
$ WC "d_seekdir='" + perl_d_seekdir + "'"
$ WC "d_telldir='" + perl_d_telldir + "'"
$ WC "d_rewinddir='" + perl_d_rewinddir + "'"
$ WC "d_rename='" + perl_d_rename + "'"
$ WC "d_rmdir='" + perl_d_rmdir + "'"
$ WC "d_sem='" + perl_d_sem + "'"
$ WC "d_setegid='" + perl_d_setegid + "'"
$ WC "d_seteuid='" + perl_d_seteuid + "'"
$ WC "d_setprior='" + perl_d_setprior + "'"
$ WC "d_setregid='" + perl_d_setregid + "'"
$ WC "d_setresgid='" + perl_d_setresgid + "'"
$ WC "d_setreuid='" + perl_d_setreuid + "'"
$ WC "d_setresuid='" + perl_d_setresuid + "'"
$ WC "d_setrgid='" + perl_d_setrgid + "'"
$ WC "d_setruid='" + perl_d_setruid + "'"
$ WC "d_setsid='" + perl_d_setsid + "'"
$ WC "d_shm='" + perl_d_shm + "'"
$ WC "d_shmatprototype='" + perl_d_shmatprototype + "'"
$ WC "d_sigaction='" + perl_d_sigaction + "'"
$ WC "d_statblks='" + perl_d_statblks + "'"
$ WC "stdio_ptr='" + perl_stdio_ptr + "'"
$ WC "stdio_cnt='" + perl_stdio_cnt + "'"
$ WC "stdio_base='" + perl_stdio_base + "'"
$ WC "stdio_bufsiz='" + perl_stdio_bufsiz + "'"
$ WC "d_strctcpy='" + perl_d_strctcpy + "'"
$ WC "d_strerror='" + perl_d_strerror + "'"
$ WC "d_syserrlst='" + perl_d_syserrlst + "'"
$ WC "d_strerrm='" + perl_d_strerrm + "'"
$ WC "d_symlink='" + perl_d_symlink + "'"
$ WC "d_syscall='" + perl_d_syscall + "'"
$ WC "d_system='" + perl_d_system + "'"
$ WC "timetype='" + perl_timetype + "'"
$ WC "d_truncate='" + perl_d_truncate + "'"
$ WC "d_vfork='" + perl_d_vfork + "'"
$ WC "signal_t='" + perl_signal_t + "'"
$ WC "d_volatile='" + perl_d_volatile + "'"
$ WC "d_vprintf='" + perl_d_vprintf + "'"
$ WC "d_charvspr='" + perl_d_charvspr + "'"
$ WC "d_wait4='" + perl_d_wait4 + "'"
$ WC "d_waitpid='" + perl_d_waitpid + "'"
$ WC "i_dirent='" + perl_i_dirent + "'"
$ WC "d_dirnamlen='" + perl_d_dirnamlen + "'"
$ WC "direntrytype='" + perl_direntrytype + "'"
$ WC "i_fcntl='" + perl_i_fcntl + "'"
$ WC "i_grp='" + perl_i_grp + "'"
$ WC "i_limits='" + perl_i_limits + "'"
$ WC "i_memory='" + perl_i_memory + "'"
$ WC "i_ndbm='" + perl_i_ndbm + "'"
$ WC "i_stdarg='" + perl_i_stdarg + "'"
$ WC "i_pwd='" + perl_i_pwd + "'"
$ WC "d_pwquota='" + perl_d_pwquota + "'"
$ WC "d_pwage='" + perl_d_pwage + "'"
$ WC "d_pwchange='" + perl_d_pwchange + "'"
$ WC "d_pwclass='" + perl_d_pwclass + "'"
$ WC "d_pwexpire='" + perl_d_pwexpire + "'"
$ WC "d_pwcomment='" + perl_d_pwcomment + "'"
$ WC "i_stddef='" + perl_i_stddef + "'"
$ WC "i_stdlib='" + perl_i_stdlib + "'"
$ WC "i_string='" + perl_i_string + "'"
$ WC "i_sysdir='" + perl_i_sysdir + "'"
$ WC "i_sysfile='" + perl_i_sysfile + "'"
$ WC "i_sysioctl='" + perl_i_sysioctl + "'"
$ WC "i_sysndir='" + perl_i_sysndir + "'"
$ WC "i_sysresrc='" + perl_i_sysresrc + "'"
$ WC "i_sysselct='" + perl_i_sysselct + "'"
$ WC "i_dbm='" + perl_i_dbm + "'"
$ WC "i_rpcsvcdbm='" + perl_i_rpcsvcdbm + "'"
$ WC "i_sfio='" + perl_i_sfio + "'"
$ WC "i_sysstat='" + perl_i_sysstat + "'"
$ WC "i_systimes='" + perl_i_systimes + "'"
$ WC "i_systypes='" + perl_i_systypes + "'"
$ WC "i_sysun='" + perl_i_sysun + "'"
$ WC "i_syswait='" + perl_i_syswait + "'"
$ WC "i_termio='" + perl_i_termio + "'"
$ WC "i_sgtty='" + perl_i_sgtty + "'"
$ WC "i_termios='" + perl_i_termios + "'"
$ WC "i_time='" + perl_i_time + "'"
$ WC "i_systime='" + perl_i_systime + "'"
$ WC "i_systimek='" + perl_i_systimek + "'"
$ WC "i_unistd='" + perl_i_unistd + "'"
$ WC "i_utime='" + perl_i_utime + "'"
$ WC "i_varargs='" + perl_i_varargs + "'"
$ WC "i_vfork='" + perl_i_vfork + "'"
$ WC "prototype='" + perl_prototype + "'"
$ WC "randbits='" + perl_randbits +"'"
$ WC "selecttype='" + perl_selecttype + "'"
$ WC "stdchar='" + perl_stdchar + "'"
$ WC "d_unlink_all_versions='" + perl_d_unlink_all_versions + "'"
$ WC "full_sed='" + perl_full_sed + "'"
$ WC "bin='" + perl_bin + "'"
$ WC "binexp='" + perl_binexp + "'"
$ WC "d_alarm='" + perl_d_alarm + "'"
$ WC "d_casti32='" + perl_d_casti32 + "'"
$ WC "d_chown='" + perl_d_chown + "'"
$ WC "d_chroot='" + perl_d_chroot + "'"
$ WC "d_cuserid='" + perl_d_cuserid + "'"
$ WC "d_dbl_dig='" + perl_d_dbl_dig + "'"
$ WC "d_difftime='" + perl_d_difftime + "'"
$ WC "d_fork='" + perl_d_fork + "'"
$ WC "d_getlogin='" + perl_d_getlogin + "'"
$ WC "d_getppid='" + perl_d_getppid + "'"
$ WC "d_htonl='" + perl_d_htonl + "'"
$ WC "d_nice='" + perl_d_nice + "'"
$ WC "d_pause='" + perl_d_pause + "'"
$ WC "d_pipe='" + perl_d_pipe + "'"
$ WC "d_readlink='" + perl_d_readlink + "'"
$ WC "d_setlinebuf='" + perl_d_setlinebuf + "'"
$ WC "d_strchr='" + perl_d_strchr + "'"
$ WC "d_index='" + perl_d_index + "'"
$ WC "d_strtod='" + perl_d_strtod + "'"
$ WC "d_strtol='" + perl_d_strtol + "'"
$ WC "d_strtoul='" + perl_d_strtoul + "'"
$ WC "d_tcgetpgrp='" + perl_d_tcgetpgrp + "'"
$ WC "d_tcsetpgrp='" + perl_d_tcsetpgrp + "'"
$ WC "d_times='" + perl_d_times + "'"
$ WC "d_tzname='" + perl_d_tzname + "'"
$ WC "d_umask='" + perl_d_umask + "'"
$ WC "fpostype='" + perl_fpostype + "'"
$ WC "i_dlfcn='" + perl_i_dlfcn + "'"
$ WC "i_float='" + perl_i_float + "'"
$ WC "i_math='" + perl_i_math + "'"
$ WC "intsize='" + perl_intsize + "'"
$ WC "longsize='" + perl_longsize + "'"
$ WC "shortsize='" + perl_shortsize + "'"
$ WC "lseektype='" + perl_lseektype + "'"
$ WC "i_values='" + perl_i_values + "'"
$ WC "malloctype='" + perl_malloctype + "'"
$ WC "freetype='" + perl_freetype + "'"
$ WC "d_mymalloc='" + perl_d_mymalloc + "'"
$ WC "sh='" + perl_sh + "'"
$ WC "sig_name='" + perl_sig_name + "'"
$ WC "sig_num='" + perl_sig_num + "'"
$ tempsym = "sig_name_init='" + perl_sig_name_with_commas + "'"
$ WC/symbol tempsym
$ WC "sig_num_init='" + perl_sig_num_with_commas + "'"
$ WC "modetype='" + perl_modetype + "'"
$ WC "ssizetype='" + perl_ssizetype + "'"
$ WC "o_nonblock='" + perl_o_nonblock + "'"
$ WC "eagain='" + perl_eagain + "'"
$ WC "rd_nodata='" + perl_rd_nodata + "'"
$ WC "d_eofnblk='" + perl_d_eofnblk + "'"
$ WC "d_oldarchlib='" + perl_d_oldarchlib + "'"
$ WC "oldarchlibexp='" + perl_oldarchlibexp + "'"
$ WC "oldarchlib='" + perl_oldarchlib + "'"
$ WC "privlibexp='" + perl_privlibexp + "'"
$ WC "privlib='" + perl_privlib + "'"
$ WC "sitelibexp='" + perl_sitelibexp + "'"
$ WC "sitelib='" + perl_sitelib + "'"
$ WC "sitearchexp='" + perl_sitearchexp + "'"
$ WC "sitearch='" + perl_sitearch + "'"
$ WC "sizetype='" + perl_sizetype + "'"
$ WC "i_sysparam='" + perl_i_sysparam + "'"
$ WC "d_void_closedir='" + perl_d_void_closedir + "'"
$ WC "d_dlerror='" + perl_d_dlerror + "'"
$ WC "d_dlsymun='" + perl_d_dlsymun + "'"
$ WC "d_suidsafe='" + perl_d_suidsafe + "'"
$ WC "d_dosuid='" + perl_d_dosuid + "'"
$ WC "d_inetaton='" + perl_d_inetaton + "'"
$ WC "d_isascii='" + perl_d_isascii + "'"
$ WC "d_mkfifo='" + perl_d_mkfifo + "'"
$ WC "d_pathconf='" + perl_d_pathconf + "'"
$ WC "d_fpathconf='" + perl_d_fpathconf + "'"
$ WC "d_safebcpy='" + perl_d_safebcpy + "'"
$ WC "d_safemcpy='" + perl_d_safemcpy + "'"
$ WC "d_sanemcmp='" + perl_d_sanemcmp + "'"
$ WC "d_setpgrp='" + perl_d_setpgrp + "'"
$ WC "d_bsdsetpgrp='" + perl_d_bsdsetpgrp + "'"
$ WC "d_bsdpgrp='" + perl_d_bsdpgrp + "'"
$ WC "d_setpgid='" + perl_d_setpgid + "'"
$ WC "d_setpgrp2='" + perl_d_setpgrp2 + "'"
$ WC "d_sysconf='" + perl_d_sysconf + "'"
$ WC "d_Gconvert='" + perl_d_Gconvert + "'"
$ WC "d_getpgid='" + perl_d_getpgid + "'"
$ WC "d_getpgrp='" + perl_d_getpgrp + "'"
$ WC "d_bsdgetpgrp='" + perl_d_bsdgetpgrp + "'"
$ WC "d_getpgrp2='" + perl_d_getpgrp2 + "'"
$ WC "d_sfio='" + perl_d_sfio + "'"
$ WC "d_sigsetjmp='" + perl_d_sigsetjmp + "'"
$ WC "usedl='" + perl_usedl + "'"
$ WC "startperl=" + perl_startperl ! This one's special--no enclosing single quotes
$ WC "db_hashtype='" + perl_db_hashtype + "'"
$ WC "db_prefixtype='" + perl_db_prefixtype + "'"
$ WC "useperlio='" + perl_useperlio + "'"
$ WC "defvoidused='" + perl_defvoidused + "'"
$ WC "voidflags='" + perl_voidflags + "'"
$ WC "d_eunice='" + perl_d_eunice + "'"
$ WC "libs='" + perl_libs + "'"
$ WC "libc='" + perl_libc + "'"
$ tempstring = "PATCHLEVEL='" + "''perl_patchlevel'" + "'"
$ WC tempstring
$ tempstring = "SUBVERSION='" + "''perl_SUBVERSION'" + "'"
$ WC tempstring
$ WC "pager='" + perl_pager + "'"
$ WC "uidtype='" + perl_uidtype + "'"
$ WC "gidtype='" + perl_gidtype + "'"
$ WC "usethreads='" + perl_usethreads + "'"
$ WC "d_pthread_yield='" + perl_d_pthread_yield + "'"
$ WC "d_pthreads_created_joinable='" + perl_d_pthreads_created_joinable + "'"
$ WC "d_gnulibc='" + perl_d_gnulibc + "'"
$ WC "i_netdb='" + perl_i_netdb + "'"
$ WC "pidtype='" + perl_pidtype + "'"
$ WC "netdb_host_type='" + perl_netdb_host_type + "'"
$ WC "netdb_hlen_type='" + perl_netdb_hlen_type + "'"
$ WC "netdb_name_type='" + perl_netdb_name_type + "'"
$ WC "netdb_net_type='" + perl_netdb_net_type + "'"
$ WC "baserev='" + perl_baserev + "'"
$ WC "doublesize='" + perl_doublesize + "'"
$ WC "ptrsize='" + perl_ptrsize + "'"
$ WC "d_gethbyaddr='" + perl_d_gethbyaddr + "'"
$ WC "d_gethbyname='" + perl_d_gethbyname + "'"
$ WC "d_getnbyaddr='" + perl_d_getnbyaddr + "'"
$ WC "d_getnbyname='" + perl_d_getnbyname + "'"
$ WC "d_getpbynumber='" + perl_d_getpbynumber + "'"
$ WC "d_getpbyname='" + perl_d_getpbyname + "'"
$ WC "d_getsbyport='" + perl_d_getsbyport + "'"
$ WC "d_getsbyname='" + perl_d_getsbyname + "'"
$ WC "d_sethent='" + perl_d_sethent + "'"
$ WC "d_oldpthreads='" + perl_d_oldpthreads + "'"
$ WC "d_longdbl='" + perl_d_longdbl + "'"
$ WC "longdblsize='" + perl_longdblsize + "'"
$ WC "d_longlong='" + perl_d_longlong + "'"
$ WC "longlongsize='" + perl_longlongsize + "'"
$ WC "d_mkstemp='" + perl_d_mkstemp + "'"
$ WC "d_setvbuf='" + perl_d_setvbuf + "'"
$ WC "d_endhent='" + perl_d_endhent + "'"
$ WC "d_endnent='" + perl_d_endsent + "'"
$ WC "d_endpent='" + perl_d_endpent + "'"
$ WC "d_endsent='" + perl_d_endsent + "'"
$ WC "d_gethent='" + perl_d_gethent + "'"
$ WC "d_getnent='" + perl_d_getsent + "'"
$ WC "d_getpent='" + perl_d_getpent + "'"
$ WC "d_getsent='" + perl_d_getsent + "'"
$ WC "d_sethent='" + perl_d_sethent + "'"
$ WC "d_setnent='" + perl_d_setsent + "'"
$ WC "d_setpent='" + perl_d_setpent + "'"
$ WC "ebcdic='" + perl_ebcdic + "'"
$ WC "d_setsent='" + perl_d_setsent + "'"
$ WC "d_gethostprotos='" + perl_d_gethostprotos + "'"
$ WC "d_getnetprotos='" + perl_d_getnetprotos + "'"
$ WC "d_getprotoprotos='" + perl_d_getprotoprotos + "'"
$ WC "d_getservprotos='" + perl_d_getservprotos + "'"
$ WC "d_pwgecos='" + perl_d_pwgecos + "'"
$ WC "d_sched_yield='" + perl_d_sched_yield + "'"
$ WC "d_lchown='" + perl_d_lchown + "'"
$ WC "d_union_semun='" + perl_d_union_semun + "'"
$ WC "i_arpainet='" + perl_i_arpainet + "'"
$ WC "d_grpasswd='" + perl_d_grpasswd + "'"
$ WC "d_setgrent='" + perl_d_setgrent + "'"
$ WC "d_getgrent='" + perl_d_getgrent + "'"
$ WC "d_endgrent='" + perl_d_endgrent + "'"
$ WC "d_pwpasswd='" + perl_d_pwpasswd + "'"
$ WC "d_setpwent='" + perl_d_setpwent + "'"
$ WC "d_getpwent='" + perl_d_getpwent + "'"
$ WC "d_endpwent='" + perl_d_endpwent + "'"
$ WC "d_semctl_semun='" + perl_d_semctl_semun + "'"
$ WC "d_semctl_semid_ds='" + perl_d_semctl_semid_ds + "'"
$ WC "extensions='" + perl_extensions + "'"
$ WC "d_mknod='" + perl_d_mknod + "'"
$ WC "devtype='" + perl_devtype + "'"
$ WC "i_sysmount='" + perl_i_sysmount + "'"
$ WC "d_fstatfs='" + perl_d_fstatfs + "'"
$ WC "d_statfsflags='" + perl_d_statfsflags + "'"
$ WC "i_sysstatvfs='" + perl_i_sysstatvfs + "'"
$ WC "i_machcthreads='" + perl_i_machcthreads + "'"
$ WC "i_pthread='" + perl_i_pthread + "'"
$ WC "d_fstatvfs='" + perl_d_fstatvfs + "'"
$ WC "i_mntent='" + perl_i_mntent + "'"
$ WC "d_getmntent='" + perl_d_getmntent + "'"
$ WC "d_hasmntopt='" + perl_d_hasmntopt + "'"
$!
$! ##WRITE NEW CONSTANTS HERE##
$!
$ Close CONFIGSH
$
$! Okay, we've gotten here. Build munchconfig and run it
$ 'Perl_CC' munchconfig.c
$ If (Needs_Opt.eqs."Yes")
$ THEN
$   open/write OPTCHAN []munchconfig.opt
$   IF ("''using_gnu_c'".eqs."Yes")
$   THEN
$     write OPTCHAN "Gnu_CC:[000000]gcclib.olb/library"
$   endif
$   write OPTCHAN "Sys$Share:VAXCRTL/Share"
$   Close OPTCHAN
$   link munchconfig.obj,munchconfig.opt/opt
$   delete munchconfig.opt;*
$ else
$   link munchconfig.obj
$ endif
$ WRITE_RESULT "Writing config.h"
$ !
$ ! we need an fdl file
$ CREATE [-]CONFIG.FDL
RECORD
  FORMAT STREAM_LF
$ CREATE /FDL=[-]CONFIG.FDL [-]CONFIG.LOCAL
$ ! First spit out the header info with the local defines (to get
$ ! around the 255 character command line limit)
$ OPEN/APPEND CONFIG [-]config.local
$ if use_debugging_perl.eqs."Y"
$ THEN
$   WRITE CONFIG "#define DEBUGGING"
$ ENDIF
$ if preload_env.eqs."Y"
$ THEN
$    WRITE CONFIG "#define PRIME_ENV_AT_STARTUP"
$ ENDIF
$ if use_two_pot_malloc.eqs."Y"
$ THEN
$    WRITE CONFIG "#define TWO_POT_OPTIMIZE"
$ endif
$ if mymalloc.eqs."Y"
$ THEN
$    WRITE CONFIG "#define EMBEDMYMALLOC"
$ ENDIF
$ if use_pack_malloc.eqs."Y"
$ THEN
$    WRITE CONFIG "#define PACK_MALLOC"
$ endif
$ if use_debugmalloc.eqs."Y"
$ THEN
$    write config "#define DEBUGGING_MSTATS"
$ ENDIF
$ if "''Using_Gnu_C'" .eqs."Yes"
$ THEN
$   WRITE CONFIG "#define GNUC_ATTRIBUTE_CHECK"
$ ENDIF
$ if "''Has_Dec_C_Sockets'".eqs."T"
$ THEN
$    WRITE CONFIG "#define VMS_DO_SOCKETS"
$    WRITE CONFIG "#define DECCRTL_SOCKETS"
$ ENDIF
$ if "''Has_Socketshr'".eqs."T"
$ THEN
$    WRITE CONFIG "#define VMS_DO_SOCKETS"
$ ENDIF
$ CLOSE CONFIG
$!
$! Now build the normal config.h
$ define/user sys$output [-]config.main
$ mcr []munchconfig [-]config.sh [-]config_h.sh
$ ! Concatenate them together
$ copy [-]config.local,[-]config.main [-]config.h
$! Clean up
$ DELETE/NOLOG [-]CONFIG.MAIN;*
$ DELETE/NOLOG [-]CONFIG.LOCAL;*
$ DELETE/NOLOG [-]CONFIG.FDL;*
$!
$ if "''Using_Dec_C'" .eqs."Yes"
$ THEN
$ DECC_REPLACE = "DECC=decc=1"
$ ELSE
$ DECC_REPLACE = "DECC=" 
$ ENDIF
$ if "''Using_Gnu_C'" .eqs."Yes"
$ THEN
$ GNUC_REPLACE = "GNUC=gnuc=1"
$ ELSE
$ GNUC_REPLACE = "GNUC=" 
$ ENDIF
$ if "''Has_Dec_C_Sockets'" .eqs."T"
$ THEN
$   SOCKET_REPLACE = "SOCKET=DECC_SOCKETS=1"
$ ELSE
$   if "''Has_Socketshr'" .eqs."T"
$   THEN
$     SOCKET_REPLACE = "SOCKET=SOCKETSHR_SOCKETS=1"
$   ELSE
$     SOCKET_REPLACE = "SOCKET="
$   ENDIF
$ ENDIF
$ IF ("''Use_Threads'".eqs."T")
$ THEN
$   if ("''VMS_VER'".LES."6.2")
$   THEN
$     THREAD_REPLACE = "THREAD=OLDTHREADED=1"
$   ELSE
$     THREAD_REPLACE = "THREAD=THREADED=1"
$   ENDIF
$ ELSE
$   THREAD_REPLACE = "THREAD="
$ ENDIF
$ if mymalloc.eqs."Y"
$ THEN
$   MALLOC_REPLACE = "MALLOC=MALLOC=1"
$ ELSE
$   MALLOC_REPLACE = "MALLOC="
$ ENDIF
$ if f$getsyi("HW_MODEL").ge.1024
$ THEN
$ ARCH_TYPE = "ARCH-TYPE=__AXP__"
$ ELSE
$ ARCH_TYPE = "ARCH-TYPE=__VAX__"
$ ENDIF
$ WRITE_RESULT "Writing DESCRIP.MMS"
$!set ver
$ define/user sys$output [-]descrip.mms
$ mcr []munchconfig [-]config.sh descrip_mms.template "''DECC_REPLACE'" "''ARCH_TYPE'" "''GNUC_REPLACE'" "''SOCKET_REPLACE'" "''THREAD_REPLACE'" "''C_Compiler_Replace'" "''MALLOC_REPLACE'" "''Thread_Live_Dangerously'" "PV=''LocalPerlVer'"
$! set nover
$!
$! Clean up after ourselves
$ delete/nolog munchconfig.exe;*
$ delete/nolog munchconfig.obj;*
