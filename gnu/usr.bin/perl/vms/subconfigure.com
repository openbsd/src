$! SUBCONFIGURE.COM
$!  - build a config.sh for VMS Perl.
$!  - use built config.sh to take config_h.SH -> config.h
$!  - also take vms/descrip_mms.template -> descrip.mms (VMS Makefile)
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
$!  One of: Using_Dec_C, Using_Gnu_C set to "YES"
$!  Dec_C_Version set to the Dec C version (defaults to 0 if not specified)
$!  Has_Socketshr set to "T" if using socketshr
$!  Has_Dec_C_Sockets set to "T" if using Dec C sockets
$!  Use_Threads set to "T" if they're using threads
$!  C_Compiler_Invoke is the command needed to invoke the C compiler
$!
$! We'll be playing with Sys$Output; don't clutter it
$ Set NoVerify
$!
$! Set Dec_C_Version to something
$ WRITE_RESULT := "WRITE SYS$OUTPUT ""%CONFIG-I-RESULT "" + "
$ echo = "Write Sys$Output "
$ Dec_C_Version := "''Dec_C_Version'"
$ Dec_C_Version = Dec_C_Version + 0
$ Vms_Ver := "''f$extract(1,3, f$getsyi(""version""))'"
$ perl_extensions := "''extensions'"
$ IF F$LENGTH(Mcc) .EQ. 0 THEN Mcc := "cc"
$ MCC = f$edit(mcc, "UPCASE")
$ C_Compiler_Replace := "CC=CC=''Mcc'''CC_flags'"
$ IF Using_Dec_C
$ THEN
$   Checkcc := "''Mcc'/prefix=all"
$ ELSE
$   Checkcc := "''Mcc'"
$ ENDIF
$ cc_flags = cc_flags + extra_flags
$ IF be_case_sensitive
$ THEN
$   d_vms_be_case_sensitive = "define"
$ ELSE
$   d_vms_be_case_sensitive = "undef"
$ ENDIF
$ IF use_multiplicity
$ THEN
$   perl_usemultiplicity = "define"
$ ELSE
$   perl_usemultiplicity = "undef"
$ ENDIF
$! Some constant defaults.
$ hwname = f$getsyi("HW_NAME")
$ myname = myhostname
$ IF myname .EQS. "" THEN myname = F$TRNLNM("SYS$NODE")
$!
$! ##ADD NEW CONSTANTS HERE##
$ perl_sizesize = "4"
$ perl_shmattype = ""
$ perl_mmaptype = ""
$ perl_gidformat = "lu"
$ perl_gidsize = "4"
$ perl_gidsign = "1"
$ perl_groupstype = "Gid_t"
$ perl_stdio_stream_array = ""
$ perl_uidformat = "lu"
$ perl_uidsize = "4"
$ perl_uidsign = "1"
$ perl_d_getcwd = "undef"
$ perl_d_nv_preserves_uv = "define"
$ perl_d_fs_data_s = "undef"
$ perl_d_getmnt = "undef"
$ perl_d_sqrtl = "define"
$ perl_d_statfs_f_flags = "undef"
$ perl_d_statfs_s = "undef"
$ perl_d_ustat = "undef"
$ perl_i_ieeefp = "undef"
$ perl_i_sunmath = "undef"
$ perl_i_sysstatfs = "undef"
$ perl_i_sysvfs = "undef"
$ perl_i_ustat = "undef"
$ perl_d_llseek="undef"
$ perl_d_iconv="undef"
$ perl_d_madvise="undef"
$ perl_selectminbits="32"
$ perl_d_vendorarch="undef"
$ perl_vendorarchexp=""
$ perl_d_msync="undef"
$ perl_d_mprotect="undef"
$ perl_d_munmap="undef"
$ perl_crosscompile="undef"
$ perl_multiarch="undef"
$ perl_d_mmap="undef"
$ perl_i_sysmman="undef"
$ perl_d_telldirproto="define"
$ perl_i_sysmount="undef"
$ perl_d_bincompat="undef"
$ perl_d_endspent="undef
$ perl_d_getspent="undef
$ perl_d_getspnam="undef
$ perl_d_setspent="undef
$ perl_d_fstatfs="undef"
$ perl_d_getfsstat="undef"
$ perl_i_machcthreads="undef"
$ perl_i_pthread="define"
$ perl_d_fstatvfs="undef"
$ perl_usesocks="undef"
$ perl_d_vendorlib="undef"
$ perl_vendorlibexp=""
$ perl_vendorlib_stem=""
$ perl_d_statfsflags="undef"
$ perl_i_sysstatvfs="undef"
$ perl_i_mntent="undef"
$ perl_d_getmntent="undef"
$ perl_d_hasmntopt="undef"
$ perl_package="''package'"
$ perl_baserev = "''baserev'"
$ cc_defines=""
$ perl_installusrbinperl="undef"
$ perl_CONFIG="true"
$ perl_d_fseeko="undef"
$ perl_d_ftello="undef"
$ perl_d_qgcvt="undef"
$ perl_d_readv="undef"
$ perl_d_writev="undef"
$ perl_i_machcthr="undef"
$ perl_i_netdb="undef"
$ perl_d_gnulibc="undef"
$ perl_ccdlflags=""
$ perl_cccdlflags=""
$ perl_mab=""
$ perl_drand01 = "drand48()"
$ perl_randseedtype = "long int"
$ perl_seedfunc = "srand48"
$ perl_d_msg_ctrunc = "undef"
$ perl_d_msg_dontroute = "undef"
$ perl_d_msg_oob = "undef"
$ perl_d_msg_peek = "undef"
$ perl_d_msg_proxy = "undef"
$ perl_d_scm_rights = "undef"
$ perl_d_sendmsg = "undef"
$ perl_d_recvmsg = "undef"
$ perl_d_msghdr_s = "undef"
$ perl_d_cmsghdr_s = "undef"
$ IF (use64bitint)
$ THEN
$   perl_use64bitint = "define"
$   perl_uselargefiles = "define"
$   perl_uselongdouble = "define"
$   perl_usemorebits = "define"
$ ELSE
$   perl_use64bitint = "undef"
$   perl_uselargefiles = "undef"
$   perl_uselongdouble = "undef"
$   perl_usemorebits = "undef"
$ ENDIF
$ IF (use64bitall)
$ THEN
$   perl_use64bitall = "define"
$ ELSE
$   perl_use64bitall = "undef"
$ ENDIF
$ perl_d_drand48proto = "define"
$ perl_d_lseekproto = "define"
$ perl_libpth="/sys$share /sys$library"
$ perl_ld="Link"
$ perl_lddlflags="/Share"
$ perl_ranlib=""
$ perl_ar=""
$ perl_full_ar=""
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
$ perl_d_phostname="undef"
$ perl_d_accessx="undef"
$ perl_d_eaccess="undef"
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
$ perl_installscript="''perl_prefix':[utils]"
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
$ IF (sharedperl .AND. F$GETSYI("HW_MODEL") .GE. 1024)
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
$ perl_cppminus=" "
$ perl_cpprun="''Perl_CC'/noobj/preprocess=sys$output sys$input"
$ perl_cpplast=" "
$ perl_aphostname=""
$ perl_d_castneg="define"
$ perl_castflags="0"
$ perl_d_chsize="undef"
$ perl_d_const="define"
$ perl_d_crypt="define"
$ perl_byteorder="1234"
$ perl_full_csh=" "
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
$ perl_d_ldbl_dig="define"
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
$ IF mymalloc
$ THEN
$ perl_d_mymalloc="define"
$ ELSE
$ perl_d_mymalloc="undef"
$ ENDIF
$ perl_sh="MCR"
$ perl_modetype="unsigned int"
$ perl_ssizetype="int"
$ perl_o_nonblock=" "
$ perl_eagain=" "
$ perl_rd_nodata=" "
$ perl_d_eofnblk="undef"
$ perl_d_oldarchlib="define"
$ perl_privlibexp="''perl_prefix':[lib]"
$ perl_privlib="''perl_prefix':[lib]"
$ perl_sitelibexp="''perl_prefix':[lib.site_perl]"
$ perl_sitelib="''perl_prefix':[lib.site_perl]"
$ perl_sitelib_stem="''perl_prefix':[lib.site_perl]"
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
$ perl_db_hashtype=" "
$ perl_db_prefixtype=" "
$ perl_useperlio="undef"
$ perl_defvoidused="15"
$ perl_voidflags="15"
$ perl_d_eunice="undef"
$ perl_d_pwgecos="define"
$ IF ((Use_Threads) .AND. (VMS_VER .LES. "6.2"))
$ THEN
$ perl_libs="SYS$SHARE:CMA$LIB_SHR.EXE/SHARE SYS$SHARE:CMA$RTL.EXE/SHARE SYS$SHARE:CMA$OPEN_LIB_SHR.exe/SHARE SYS$SHARE:CMA$OPEN_RTL.exe/SHARE"
$ ELSE
$ perl_libs=" "
$ ENDIF
$ IF Using_Dec_C
$ THEN
$   perl_libc="(DECCRTL)"
$ ELSE
$   perl_libc=" "
$ ENDIF
$ perl_pager="most"
$!
$! Are we 64 bit?
$!
$ IF (use64bitint)
$ THEN
$   perl_d_PRIfldbl = "define"
$   perl_d_PRIgldbl = "define"
$   perl_d_PRId64 = "define"
$   perl_d_PRIu64 = "define"
$   perl_d_PRIo64 = "define"
$   perl_d_PRIx64 = "define"
$   perl_sPRIfldbl = """Lf"""
$   perl_sPRIgldbl = """Lg"""
$   perl_sPRId64 = """Ld"""
$   perl_sPRIu64 = """Lu"""
$   perl_sPRIo64 = """Lo"""
$   perl_sPRIx64 = """Lx"""
$   perl_d_quad = "define"
$   perl_quadtype = "long long"
$   perl_uquadtype = "unsigned long long"
$   perl_quadkind  = "QUAD_IS_LONG_LONG"
$ ELSE
$   perl_d_PRIfldbl = "undef"
$   perl_d_PRIgldbl = "undef"
$   perl_d_PRId64 = "undef"
$   perl_d_PRIu64 = "undef"
$   perl_d_PRIo64 = "undef"
$   perl_d_PRIx64 = "undef"
$   perl_sPRIfldbl = ""
$   perl_sPRIgldbl = ""
$   perl_sPRId64 = ""
$   perl_sPRIu64 = ""
$   perl_sPRIo64 = ""
$   perl_sPRIx64 = ""
$   perl_d_quad = "undef"
$   perl_quadtype = "long"
$   perl_uquadtype = "unsigned long"
$   perl_quadkind  = "QUAD_IS_LONG"
$ ENDIF
$!
$! Now some that we build up
$!
$ IF Use_Threads
$ THEN
$   if use_5005_threads
$   THEN
$     arch = "''arch'-thread"
$     archname = "''archname'-thread"
$     perl_d_old_pthread_create_joinable = "undef"
$     perl_old_pthread_create_joinable = " "
$     perl_use5005threads = "define"
$     perl_useithreads = "undef"
$   ELSE
$     arch = "''arch'-ithread"
$     archname = "''archname'-ithread"
$     perl_d_old_pthread_create_joinable = "undef"
$     perl_old_pthread_create_joinable = " "
$     perl_use5005threads = "undef"
$     perl_useithreads = "define"
$   ENDIF
$ ELSE
$   perl_d_old_pthread_create_joinable = "undef"
$   perl_old_pthread_create_joinable = " "
$   perl_use5005threads = "undef"
$   perl_useithreads = "undef"
$ ENDIF
$!
$! Some that we need to invoke the compiler for
$ OS := "open/write SOURCECHAN []temp.c"
$ WS := "write SOURCECHAN"
$ CS := "close SOURCECHAN"
$ DS := "delete/nolog []temp.*;*"
$ Needs_Opt := N
$ IF using_gnu_c
$ THEN
$   open/write OPTCHAN []temp.opt
$   write OPTCHAN "Gnu_CC:[000000]gcclib.olb/library"
$   write OPTCHAN "Sys$Share:VAXCRTL/Share"
$   Close OPTCHAN
$   Needs_Opt := Y
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
$   If Needs_Opt
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
$   DELETE/NOLOG [-.uu]tempout.lis;
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
$   If Needs_Opt
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
$ DELETE/NOLOG [-.uu]tempout.lis;
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
$     IF Needs_Opt
$     THEN
$       link temp.obj,temp.opt/opt
$     ELSE
$       link temp.obj
$     ENDIF
$     teststatus = f$extract(9,1,$status)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     IF (teststatus.nes."1")
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
$       DELETE/NOLOG [-.uu]tempout.lis;
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
$   IF Needs_Opt
$   THEN
$     link temp.obj,temp.opt/opt
$   ELSE
$     link temp.obj
$   ENDIF
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
$     DELETE/NOLOG [-.uu]tempout.lis;
$     perl_longlongsize=line
$     perl_d_longlong="define"
$   ENDIF
$ WRITE_RESULT "longlongsize is ''perl_longlongsize'"
$ WRITE_RESULT "d_longlong is ''perl_d_longlong'"
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
$! Check to see if we've got shadow.h (probably not, but...)
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <shadow.h>
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
$     perl_i_shadow = "undef"
$   ELSE
$     perl_i_shadow = "define"
$   ENDIF
$ WRITE_RESULT "i_shadow is ''perl_i_shadow'"
$!
$! Check to see if we've got socks.h (probably not, but...)
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <socks.h>
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
$     perl_i_socks = "undef"
$   ELSE
$     perl_i_socks = "define"
$   ENDIF
$ WRITE_RESULT "i_socks is ''perl_i_socks'"
$!
$! Check the prototype for select
$!
$ IF Has_Dec_C_Sockets .OR. Has_Socketshr
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#include <unistd.h>
$ IF Has_Socketshr
$ THEN
$   WS "#include <socketshr.h>"
$ ELSE
$   WS "#include <time.h>
$   WS "#include <socket.h>
$ ENDIF
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
$! Check to see if fd_set exists
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#include <unistd.h>
$ IF Has_Socketshr
$ THEN
$   WS "#include <socketshr.h>"
$ ENDIF
$ IF Has_Dec_C_Sockets
$ THEN
$   WS "#include <time.h>
$   WS "#include <socket.h>
$ ENDIF
$ WS "int main()
$ WS "{"
$ WS "fd_set *foo;
$ WS "int bar;
$ WS "exit(0);
$ WS "}"
$ CS
$ DEFINE SYS$ERROR _NLA0:
$ DEFINE SYS$OUTPUT _NLA0:
$ on error then continue
$ on warning then continue
$ 'Checkcc' temp.c
$ teststatus = f$extract(9,1,$status)
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ if (teststatus.nes."1")
$ THEN
$!  Okay, fd_set failed. Must not exist
$   perl_d_fd_set = "undef"
$ ELSE
$   perl_d_fd_set="define"
$ ENDIF
$ WRITE_RESULT "d_fd_set is ''perl_d_fd_set'"
$!
$! Check for inttypes.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <inttypes.h>
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
$     perl_i_inttypes="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     IF Needs_Opt
$     THEN
$       link temp.obj,temp.opt/opt
$     ELSE
$       link temp.obj
$     ENDIF
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_i_inttypes="undef"
$     ELSE
$       perl_i_inttypes="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_inttypes is ''perl_i_inttypes'"
$!
$! Check for h_errno
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <netdb.h>
$ WS "int main()
$ WS "{"
$ WS "h_errno = 3;
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
$     perl_d_herrno="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     IF Needs_Opt
$     THEN
$       link temp.obj,temp.opt/opt
$     ELSE
$       link temp.obj
$     ENDIF
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_d_herrno="undef"
$     ELSE
$       perl_d_herrno="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_herrno is ''perl_d_herrno'"
$!
$! Check to see if int64_t exists
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#''perl_i_inttypes IIH
$ WS "#ifdef IIH
$ WS "#include <inttypes.h>
$ WS "#endif
$ WS "#include <unistd.h>
$ WS "int main()
$ WS "{"
$ WS "int64_t bar;
$ WS "exit(0);
$ WS "}"
$ CS
$ DEFINE SYS$ERROR _NLA0:
$ DEFINE SYS$OUTPUT _NLA0:
$ on error then continue
$ on warning then continue
$ 'Checkcc' temp.c
$ teststatus = f$extract(9,1,$status)
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ if (teststatus.nes."1")
$ THEN
$!  Okay, int64_t failed. Must not exist
$   perl_d_int64_t = "undef"
$ ELSE
$   perl_d_int64_t="define"
$ ENDIF
$ WRITE_RESULT "d_int64_t is ''perl_d_int64_t'"
$!
$! Check to see if off64_t exists
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#''perl_i_inttypes IIH
$ WS "#ifdef IIH
$ WS "#include <inttypes.h>
$ WS "#endif
$ WS "#include <unistd.h>
$ WS "int main()
$ WS "{"
$ WS "off64_t bar;
$ WS "exit(0);
$ WS "}"
$ CS
$ DEFINE SYS$ERROR _NLA0:
$ DEFINE SYS$OUTPUT _NLA0:
$ on error then continue
$ on warning then continue
$ 'Checkcc' temp.c
$ teststatus = f$extract(9,1,$status)
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ if (teststatus.nes."1")
$ THEN
$!  Okay, off64_t failed. Must not exist
$   perl_d_off64_t = "undef"
$ ELSE
$   perl_d_off64_t="define"
$ ENDIF
$ WRITE_RESULT "d_off64_t is ''perl_d_off64_t'"
$!
$! Check to see if fpos64_t exists
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#''perl_i_inttypes IIH
$ WS "#ifdef IIH
$ WS "#include <inttypes.h>
$ WS "#endif
$ WS "#include <unistd.h>
$ WS "int main()
$ WS "{"
$ WS "fpos64_t bar;
$ WS "exit(0);
$ WS "}"
$ CS
$ DEFINE SYS$ERROR _NLA0:
$ DEFINE SYS$OUTPUT _NLA0:
$ on error then continue
$ on warning then continue
$ 'Checkcc' temp.c
$ teststatus = f$extract(9,1,$status)
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ if (teststatus.nes."1")
$ THEN
$!  Okay, fpos64_t failed. Must not exist
$   perl_d_fpos64_t = "undef"
$ ELSE
$   perl_d_fpos64_t="define"
$ ENDIF
$ WRITE_RESULT "d_fpos64_t is ''perl_d_fpos64_t'"
$!
$! Check to see if gethostname exists
$!
$ IF (Has_Dec_C_Sockets .OR. Has_Socketshr)
$ THEN
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <types.h>
$ WS "#include <unistd.h>
$ IF Has_Socketshr
$ THEN
$   WS "#include <socketshr.h>"
$ ELSE
$   WS "#include <time.h>
$   WS "#include <socket.h>
$ ENDIF
$ WS "int main()
$ WS "{"
$ WS "char name[100];
$ WS "int bar, baz;
$ WS "bar = 100;
$ WS "baz = gethostname(name, bar);
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
$!   Okay, compile failed. Must not have it
$     perl_d_gethname = "undef"
$   ELSE
$     IF Needs_Opt
$     THEN
$       link temp.obj,temp.opt/opt
$     ELSE
$       link temp.obj
$     ENDIF
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     if (teststatus.nes."1")
$     THEN
$       perl_d_gethname="undef"
$     ELSE
$       perl_d_gethname="define"
$     ENDIF
$   ENDIF
$ ELSE
$   ! No sockets, so no gethname
$   perl_d_gethname = "undef"
$ ENDIF
$ WRITE_RESULT "d_gethname is ''perl_d_gethname'"
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
$     IF Needs_Opt
$     THEN
$       link temp.obj,temp.opt/opt
$     ELSE
$       link temp.obj
$     ENDIF
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
$! Check for sys/utsname.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <sys/utsname.h>
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
$     perl_i_sysutsname="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     IF Needs_Opt
$     THEN
$       link temp.obj,temp.opt/opt
$     ELSE
$       link temp.obj
$     ENDIF
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_i_sysutsname="undef"
$     ELSE
$       perl_i_sysutsname="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_sysutsname is ''perl_i_sysutsname'"
$!
$! Check for syslog.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <syslog.h>
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
$     perl_i_syslog="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     IF Needs_Opt
$     THEN
$       link temp.obj,temp.opt/opt
$     ELSE
$       link temp.obj
$     ENDIF
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_i_syslog="undef"
$     ELSE
$       perl_i_syslog="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_syslog is ''perl_i_syslog'"
$!
$! Check for poll.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <poll.h>
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
$     perl_i_poll="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     IF Needs_Opt
$     THEN
$       link temp.obj,temp.opt/opt
$     ELSE
$       link temp.obj
$     ENDIF
$     savedstatus = $status
$     teststatus = f$extract(9,1,savedstatus)
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$     if (teststatus.nes."1")
$     THEN
$       perl_i_poll="undef"
$     ELSE
$       perl_i_poll="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_poll is ''perl_i_poll'"
$!
$! Check for sys/uio.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <sys/uio.h>
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
$     perl_i_sysuio="undef"
$     DEASSIGN SYS$OUTPUT
$     DEASSIGN SYS$ERROR
$   ELSE
$     IF Needs_Opt
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
$       perl_i_sysuio="undef"
$     ELSE
$       perl_i_sysuio="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_sysuio is ''perl_i_sysuio'"
$!
$! Check for sys/mode.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <sys/mode.h>
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
$     perl_i_sysmode="undef"
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
$       perl_i_sysmode="undef"
$     ELSE
$       perl_i_sysmode="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_sysmode is ''perl_i_sysmode'"
$!
$! Check for sys/access.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <sys/access.h>
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
$     perl_i_sysaccess="undef"
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
$       perl_i_sysaccess="undef"
$     ELSE
$       perl_i_sysaccess="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_sysaccess is ''perl_i_sysaccess'"
$!
$! Check for sys/security.h
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "#include <sys/security.h>
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
$     perl_i_syssecrt="undef"
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
$       perl_i_syssecrt="undef"
$     ELSE
$       perl_i_syssecrt="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "i_syssecrt is ''perl_i_syssecrt'"
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
$! Check for memchr
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <string.h>
$ WS "int main()
$ WS "{"
$ WS "char * place;
$ WS "place = memchr(""foo"", 47, 3)
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
$     perl_d_memchr="undef"
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
$       perl_d_memchr="undef"
$     ELSE
$       perl_d_memchr="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_memchr is ''perl_d_memchr'"
$!
$! Check for strtoull
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <string.h>
$ WS "int main()
$ WS "{"
$ WS "unsigned __int64 result;
$ WS "result = strtoull(""123123"", NULL, 10);
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
$     perl_d_strtoull="undef"
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
$       perl_d_strtoull="undef"
$     ELSE
$       perl_d_strtoull="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_strtoull is ''perl_d_strtoull'"
$!
$! Check for strtouq
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <string.h>
$ WS "int main()
$ WS "{"
$ WS "unsigned __int64 result;
$ WS "result = strtouq(""123123"", NULL, 10);
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
$     perl_d_strtouq="undef"
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
$       perl_d_strtouq="undef"
$     ELSE
$       perl_d_strtouq="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_strtouq is ''perl_d_strtouq'"
$!
$! Check for strtoll
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <string.h>
$ WS "int main()
$ WS "{"
$ WS "__int64 result;
$ WS "result = strtoll(""123123"", NULL, 10);
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
$     perl_d_strtoll="undef"
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
$       perl_d_strtoll="undef"
$     ELSE
$       perl_d_strtoll="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_strtoll is ''perl_d_strtoll'"
$!
$! Check for strtold
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <string.h>
$ WS "int main()
$ WS "{"
$ WS "long double result;
$ WS "result = strtold(""123123"", NULL, 10);
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
$     perl_d_strtold="undef"
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
$       perl_d_strtold="undef"
$     ELSE
$       perl_d_strtold="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_strtold is ''perl_d_strtold'"
$!
$! Check for atoll
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <string.h>
$ WS "int main()
$ WS "{"
$ WS " __int64 result;
$ WS "result = atoll(""123123"");
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
$     perl_d_atoll="undef"
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
$       perl_d_atoll="undef"
$     ELSE
$       perl_d_atoll="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_atoll is ''perl_d_atoll'"
$!
$! Check for atoll
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <string.h>
$ WS "int main()
$ WS "{"
$ WS "long double
$ WS "result = atolf(""123123"");
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
$     perl_d_atolf="undef"
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
$       perl_d_atolf="undef"
$     ELSE
$       perl_d_atolf="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_atolf is ''perl_d_atolf'"
$!
$! Check for access
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <unistd.h>
$ WS "int main()
$ WS "{"
$ WS "access("foo", F_OK);
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
$     perl_d_access="undef"
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
$       perl_d_access="undef"
$     ELSE
$       perl_d_access="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_access is ''perl_d_access'"
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
$! Check for mkstemps
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "mkstemps(""foo"", 1);
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
$     perl_d_mkstemps="undef"
$   ELSE
$     perl_d_mkstemps="define"
$   ENDIF
$ WRITE_RESULT "d_mkstemps is ''perl_d_mkstemps'"
$!
$! Check for iconv
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "#include <iconv.h>
$ WS "int main()
$ WS "{"
$ WS "  iconv_t cd = (iconv_t)0;"
$ WS "  char *inbuf, *outbuf;"
$ WS "  size_t inleft, outleft;"
$ WS "  iconv(cd, &inbuf, &inleft, &outbuf, &outleft);"
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
$     perl_d_iconv="undef"
$     perl_i_iconv="undef"
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
$  THEN
$       perl_d_iconv="undef"
$       perl_i_iconv="undef"
$     ELSE
$       perl_d_iconv="define"
$       perl_i_iconv="define"
$     ENDIF
$   ENDIF
$ WRITE_RESULT "d_iconv is ''perl_d_iconv'"
$ WRITE_RESULT "i_iconv is ''perl_i_iconv'"
$!
$! Check for mkdtemp
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "mkdtemp(""foo"");
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
$     perl_d_mkdtemp="undef"
$   ELSE
$     perl_d_mkdtemp="define"
$   ENDIF
$ WRITE_RESULT "d_mkdtemp is ''perl_d_mkdtemp'"
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
$! Check for setenv
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "setenv(""FOO"", ""BAR"", 0);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   on error then continue
$   on warning then continue
$   'Checkcc' temp
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp,temp/opt
$   else
$     link temp
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_d_setenv="undef"
$   ELSE
$     perl_d_setenv="define"
$   ENDIF
$ WRITE_RESULT "d_setenv is ''perl_d_setenv'"
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
$! Check for <netinet/tcp.h>
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
$ WS "#include <netinet/tcp.h>"
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
$     perl_i_netinettcp="undef"
$   ELSE
$     perl_i_netinettcp="define"
$   ENDIF
$ ELSE
$   perl_i_netinettcp="undef"
$ ENDIF
$ WRITE_RESULT "i_netinettcp is ''perl_i_netinettcp'"
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
$! Check for socklen_t
$!
$ if ("''Has_Dec_C_Sockets'".eqs."T").or.("''Has_Socketshr'".eqs."T")
$ THEN
$   OS
$   WS "#ifdef __DECC
$   WS "#include <stdlib.h>
$   WS "#endif
$   WS "#include <stdio.h>
$   IF ("''Has_Socketshr'".eqs."T")
$   THEN
$     WS "#include <socketshr.h>"
$   ELSE
$     WS "#include <netdb.h>
$   ENDIF
$   WS "int main()
$   WS "{"
$   WS "socklen_t x = 16;
$   WS "exit(0);
$   WS "}"
$   CS
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
$     perl_d_socklen_t="undef"
$   ELSE
$     perl_d_socklen_t="define"
$   ENDIF
$ ELSE
$   perl_d_socklen_t="undef"
$ ENDIF
$ WRITE_RESULT "d_socklen_t is ''perl_d_socklen_t'"
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
$     perl_sched_yield = " "
$   ELSE
$     perl_d_sched_yield="define"
$     perl_sched_yield = "sched_yield"
$   ENDIF
$ ELSE
$   perl_d_sched_yield="undef"
$   perl_sched_yield = " "
$ ENDIF
$ WRITE_RESULT "d_sched_yield is ''perl_d_sched_yield'"
$ WRITE_RESULT "sched_yield is ''perl_sched_yield'"
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
$ DEFINE SYS$ERROR _NLA0:
$ DEFINE SYS$OUTPUT _NLA0:
$ ON ERROR THEN CONTINUE
$ ON WARNING THEN CONTINUE
$ 'Checkcc' temp.c
$ If (Needs_Opt.eqs."Yes")
$ THEN
$   link temp.obj,temp.opt/opt
$ ELSE
$   link temp.obj
$ ENDIF
$ OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ DEFINE SYS$ERROR TEMPOUT
$ DEFINE SYS$OUTPUT TEMPOUT
$ mcr []temp.exe
$ CLOSE TEMPOUT
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ OPEN/READ TEMPOUT [-.uu]tempout.lis
$ READ TEMPOUT line
$ CLOSE TEMPOUT
$ DELETE/NOLOG [-.uu]tempout.lis;
$ 
$ perl_ptrsize=line
$ WRITE_RESULT "ptrsize is ''perl_ptrsize'"
$!
$! Check for size_t size
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "int foo;
$ WS "foo = sizeof(size_t);
$ WS "printf(""%d\n"", foo);
$ WS "exit(0);
$ WS "}"
$ CS
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   ON ERROR THEN CONTINUE
$   ON WARNING THEN CONTINUE
$   'Checkcc' temp.c
$   If Needs_Opt
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
$ DELETE/NOLOG [-.uu]tempout.lis;
$ 
$ perl_sizesize=line
$ WRITE_RESULT "sizesize is ''perl_sizesize'"
$!
$! Check rand48 and its ilk
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "srand48(12L);"
$ WS "exit(0);
$ WS "}"
$ CS
$!
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   ON ERROR THEN CONTINUE
$   ON WARNING THEN CONTINUE
$   'Checkcc' temp
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp,temp.opt/opt
$   else
$     link temp
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_drand01="random()"
$     perl_randseedtype = "unsigned"
$     perl_seedfunc = "srandom"
$   ENDIF
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "srandom(12);"
$ WS "exit(0);
$ WS "}"
$ CS
$! copy temp.c sys$output
$!
$   DEFINE SYS$ERROR _NLA0:
$   DEFINE SYS$OUTPUT _NLA0:
$   ON ERROR THEN CONTINUE
$   ON WARNING THEN CONTINUE
$   'Checkcc' temp
$   If (Needs_Opt.eqs."Yes")
$   THEN
$     link temp,temp.opt/opt
$   else
$     link temp
$   endif
$   teststatus = f$extract(9,1,$status)
$   DEASSIGN SYS$OUTPUT
$   DEASSIGN SYS$ERROR
$   if (teststatus.nes."1")
$   THEN
$     perl_drand01="(((float)rand())/((float)RAND_MAX))"
$     perl_randseedtype = "unsigned"
$     perl_seedfunc = "srand"
$   ENDIF
$ WRITE_RESULT "drand01 is ''perl_drand01'"
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
$   perl_vms_cc_type="cc"
$   perl_d_attribut="undef"
$ ENDIF
$
$! Dec C >= 5.2 and VMS ver >= 7.0
$ IF (Using_Dec_C).AND.(F$INTEGER(Dec_C_Version).GE.50200000).AND.(VMS_VER .GES. "7.0")
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
$ perl_sig_num="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 6 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 64"","0"
$ perl_sig_num_init="0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,6,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,64,0"
$ perl_sig_num_with_commas=perl_sig_num_init
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
$ perl_sig_num="0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 6 16 17"",0"
$ perl_sig_num_init="0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,6,16,17,0"
$ perl_sig_num_with_commas=perl_sig_num_init
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
$ IF ("''Using_Dec_C'".EQS."Yes")
$ THEN
$ perl_socksizetype="unsigned int"
$ ELSE
$ perl_socksizetype="int *"
$ ENDIF
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
$ perl_socksizetype="undef"
$ ENDIF
$! Threads
$ IF use_threads
$ THEN
$   perl_usethreads="define"
$   perl_d_pthreads_created_joinable="define"
$   if (VMS_VER .GES. "7.0")
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
$! new (5.005_62++) typedefs for primitives
$! 
$ perl_ivtype="long"
$ perl_uvtype="unsigned long"
$ perl_i8type="char"
$ perl_u8type="unsigned char"
$ perl_i16type="short"
$ perl_u16type="unsigned short"
$ perl_i32type="int"
$ perl_u32type="unsigned int"
$ perl_i64type="long long"
$ perl_u64type="unsigned long long"
$ perl_nvtype="double"
$!
$ GOTO beyond_type_size_check
$!
$type_size_check: 
$!
$! Check for type sizes 
$!
$ OS
$ WS "#ifdef __DECC
$ WS "#include <stdlib.h>
$ WS "#endif
$ WS "#include <stdio.h>
$ WS "int main()
$ WS "{"
$ WS "printf(""%d\n"", sizeof(''type'));"
$ WS "exit(0);
$ WS "}"
$ CS
$ DEFINE SYS$ERROR _NLA0:
$ DEFINE SYS$OUTPUT _NLA0:
$ ON ERROR THEN CONTINUE
$ ON WARNING THEN CONTINUE
$ 'Checkcc' temp.c
$ If (Needs_Opt.eqs."Yes")
$ THEN
$   link temp.obj,temp.opt/opt
$ ELSE
$   link temp.obj
$ ENDIF
$ OPEN/WRITE TEMPOUT [-.uu]tempout.lis
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ DEFINE SYS$ERROR TEMPOUT
$ DEFINE SYS$OUTPUT TEMPOUT
$ mcr []temp.exe
$ CLOSE TEMPOUT
$ DEASSIGN SYS$OUTPUT
$ DEASSIGN SYS$ERROR
$ OPEN/READ TEMPOUT [-.uu]tempout.lis
$ READ TEMPOUT line
$ CLOSE TEMPOUT
$ DELETE/NOLOG [-.uu]tempout.lis;
$ WRITE_RESULT "''size_name' is ''line'"
$ DS
$ RETURN
$!
$beyond_type_size_check:
$!
$ line = ""
$ type = "''perl_ivtype'"
$ size_name = "ivsize"
$ gosub type_size_check
$ perl_ivsize="''line'"
$ IF type .eqs. "long"
$ THEN perl_longsize = "''line'"
$ ELSE
$   type = "long"
$   size_name = "longsize"
$   gosub type_size_check
$   perl_longsize="''line'"
$ ENDIF
$
$ type = "''perl_uvtype'"
$ size_name = "uvsize"
$ gosub type_size_check
$ perl_uvsize="''line'"
$
$ type = "''perl_i8type'"
$ size_name = "i8size"
$ gosub type_size_check
$ perl_i8size="''line'"
$
$ type = "''perl_u8type'"
$ size_name = "u8size"
$ gosub type_size_check
$ perl_u8size="''line'"
$
$ type = "''perl_i16type'"
$ size_name = "i16size"
$ gosub type_size_check
$ perl_i16size="''line'"
$ IF type .eqs. "short"
$ THEN perl_shortsize="''line'"
$ ELSE
$   type = "''perl_i16type'"
$   size_name = "shortsize"
$   gosub type_size_check
$   perl_shortsize="''line'"
$ ENDIF
$
$ type = "''perl_u16type'"
$ size_name = "u16size"
$ gosub type_size_check
$ perl_u16size="''line'"
$
$ type = "''perl_i32type'"
$ size_name = "i32size"
$ gosub type_size_check
$ perl_i32size="''line'"
$ IF type .eqs. "int"
$ THEN perl_intsize="''perl_i32size'"
$ ELSE
$   type = "int"
$   size_name = "intsize"
$   gosub type_size_check
$   perl_intsize="''line'"
$ ENDIF
$
$ type = "''perl_u32type'"
$ size_name = "u32size"
$ gosub type_size_check
$ perl_u32size="''line'"
$
$ If use64bitint
$ Then
$   type = "''perl_i64type'"
$   size_name = "i64size"
$   gosub type_size_check
$   perl_i64size="''line'"
$   perl_ivtype="''perl_i64type'"
$
$   type = "''perl_u64type'"
$   size_name = "u64size"
$   gosub type_size_check
$   perl_u64size="''line'"
$   perl_uvtype="''perl_u64type'"
$   perl_nvtype="long double"
$ Else
$   perl_i64size="undef"
$   perl_u64size="undef"
$ EndIf
$!
$ perl_ivdformat="""ld"""
$ perl_uvuformat="""lu"""
$ perl_uvoformat="""lo"""
$ perl_uvxformat="""lx"""
$! 
$! Finally the composite ones. All config
$ perl_installarchlib="''perl_prefix':[lib.''archname'.''version']"
$ perl_installsitearch="''perl_prefix':[lib.site_perl.''archname']"
$ perl_myhostname="''myhostname'"
$ perl_mydomain="''mydomain'"
$ perl_perladmin="''perladmin'"
$ perl_myuname:="''osname' ''myname' ''osvers' ''f$edit(hwname, "TRIM")'"
$ perl_archlibexp="''perl_prefix':[lib.''archname'.''version']"
$ perl_archlib="''perl_prefix':[lib.''archname'.''version']"
$ perl_oldarchlibexp="''perl_prefix':[lib.''archname']"
$ perl_oldarchlib="''perl_prefix':[lib.''archname']"
$ perl_sitearchexp="''perl_prefix':[lib.site_perl.''archname']"
$ perl_sitearch="''perl_prefix':[lib.site_perl.''archname']"
$ IF Using_Dec_C
$ THEN
$ perl_ccflags="/Include=[]/Standard=Relaxed_ANSI/Prefix=All/Obj=''perl_obj_ext'/NoList''cc_flags'"
$ ENDIF
$ if use_vmsdebug_perl .eqs. "Y"
$ then
$     perl_optimize="/Debug/NoOpt"
$     perl_dbgprefix = "DBG"
$ else
$     perl_optimize= ""
$     perl_dbgprefix = ""
$ endif
$!
$! Okay, we've got everything configured. Now go write out a config.sh.
$ echo4 "Creating config.sh..."
$ open/write CONFIG [-]config.sh
$ WC := "write CONFIG"
$!
$ WC "# This file generated by Configure.COM on a VMS system."
$ WC "# Time: " + cf_time
$ WC ""
$ WC "CONFIGDOTSH=true"
$ WC "package='" + perl_package + "'"
$ WC "config_args='" + config_args + "'"
$ WC "d_nv_preserves_uv='" + perl_d_nv_preserves_uv + "'"
$ WC "use5005threads='" + perl_use5005threads + "'"
$ WC "useithreads='" + perl_useithreads + "'"
$ WC "CONFIG='" + perl_config + "'"
$ WC "cf_time='" + cf_time + "'"
$ WC "cf_by='" + cf_by + "'"
$ WC "cpp_stuff='" + perl_cpp_stuff + "'"
$ WC "ccdlflags='" + perl_ccdlflags + "'"
$ WC "cccdlflags='" + perl_cccdlflags + "'"
$ WC "mab='" + perl_mab + "'"
$ WC "libpth='" + perl_libpth + "'"
$ WC "ld='" + perl_ld + "'"
$ WC "lddlflags='" + perl_lddlflags + "'"
$ WC "ranlib='" + perl_ranlib + "'"
$ WC "ar='" + perl_ar + "'"
$ WC "full_ar='" + perl_full_ar + "'"
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
$ WC "archname='" + archname + "'"
$ WC "osvers='" + osvers + "'"
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
$ WC "optimize='" + perl_optimize + "'"
$ WC "dbgprefix='" + perl_dbgprefix + "'"
$ WC "d_vms_do_sockets='" + perl_d_vms_do_sockets + "'"
$ WC "d_socket='" + perl_d_socket + "'"
$ WC "d_sockpair='" + perl_d_sockpair + "'"
$ WC "d_gethent='" + perl_d_gethent + "'"
$ WC "d_getsent='" + perl_d_getsent + "'"
$ WC "d_socklen_t='" + perl_d_socklen_t + "'"
$ WC "d_select='" + perl_d_select + "'"
$ WC "i_niin='" + perl_i_niin + "'"
$ WC "i_netinettcp='" + perl_i_netinettcp + "'"
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
$ WC "cf_email='" + cf_email + "'"
$ WC "myuname='" + perl_myuname + "'"
$ WC "alignbytes='" + alignbytes + "'"
$ WC "osname='" + perl_osname + "'"
$ WC "d_archlib='" + perl_d_archlib + "'"
$ WC "archlibexp='" + perl_archlibexp + "'"
$ WC "archlib='" + perl_archlib + "'"
$ WC "archname='" + archname + "'"
$ WC "d_bincompat3='" + perl_d_bincompat3 + "'"
$ WC "cppstdin='" + perl_cppstdin + "'"
$ WC "cppminus='" + perl_cppminus + "'"
$ WC "cpprun='" + perl_cpprun + "'"
$ WC "cpplast='" + perl_cpplast + "'"
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
$ WC "groupstype='" + perl_groupstype + "'"
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
$ WC "shmattype='" + perl_shmattype + "'"
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
$ WC "selectminbits='" + perl_selectminbits + "'"
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
$ WC "d_ldbl_dig='" + perl_d_ldbl_dig + "'"
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
$ WC "lseeksize='4'"
$ WC "i_values='" + perl_i_values + "'"
$ WC "malloctype='" + perl_malloctype + "'"
$ WC "freetype='" + perl_freetype + "'"
$ WC "d_mymalloc='" + perl_d_mymalloc + "'"
$ WC "sh='" + perl_sh + "'"
$ WC "sig_name='" + perl_sig_name + "'"
$ WC "sig_num='" + perl_sig_num + "'"
$ tempsym = "sig_name_init='" + perl_sig_name_with_commas + "'"
$ WC/symbol tempsym
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
$ WC "sitelib_stem='" + perl_sitelib_stem + "'"
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
$ WC "d_int64_t='" + perl_d_int64_t + "'"
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
$ WC "xs_apiversion='" + version + "'"
$ WC "pm_apiversion='" + version + "'"
$ WC "PERL_VERSION='" + patchlevel + "'"
$ WC "PERL_SUBVERSION='" + subversion + "'"
$ WC "pager='" + perl_pager + "'"
$ WC "uidtype='" + perl_uidtype + "'"
$ WC "uidformat='" + perl_uidformat + "'"
$ WC "uidsize='" + perl_uidsize + "'"
$ WC "uidsign='" + perl_uidsign + "'"
$ WC "gidtype='" + perl_gidtype + "'"
$ WC "gidformat='" + perl_gidformat + "'"
$ WC "gidsize='" + perl_gidsize + "'"
$ WC "gidsign='" + perl_gidsign + "'"
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
$ WC "socksizetype='" + perl_socksizetype + "'"
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
$ WC "d_mkstemps='" + perl_d_mkstemps + "'"
$ WC "d_mkdtemp='" + perl_d_mkdtemp + "'"
$ WC "d_setvbuf='" + perl_d_setvbuf + "'"
$ WC "d_setenv='" + perl_d_setenv + "'"
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
$ WC "known_extensions='" + perl_known_extensions + "'"
$ WC "static_ext='" + "'"
$ WC "dynamic_ext='" + perl_extensions + "'"
$ WC "d_mknod='" + perl_d_mknod + "'"
$ WC "devtype='" + perl_devtype + "'"
$ WC "d_gethname='" + perl_d_gethname + "'"
$ WC "d_phostname='" + perl_d_phostname + "'"
$ WC "aphostname='" + perl_aphostname + "'"
$ WC "d_accessx='" + perl_d_accessx + "'"
$ WC "d_eaccess='" + perl_d_eaccess + "'"
$ WC "i_ieeefp='" + perl_i_ieeefp + "'"
$ WC "i_sunmath='" + perl_i_sunmath + "'"
$ WC "i_sysaccess='" + perl_i_sysaccess + "'"
$ WC "i_syssecrt='" + perl_i_syssecrt + "'"
$ WC "d_fd_set='" + perl_d_fd_set + "'"
$ WC "d_access='" + perl_d_access + "'"
$ WC "d_msg_ctrunc='" + perl_d_msg_ctrunc + "'"
$ WC "d_msg_dontroute='" + perl_d_msg_dontroute + "'"
$ WC "d_msg_oob='" + perl_d_msg_oob + "'"
$ WC "d_msg_peek='" + perl_d_msg_peek + "'"
$ WC "d_msg_proxy='" + perl_d_msg_proxy + "'"
$ WC "d_scm_rights='" + perl_d_scm_rights + "'"
$ WC "d_sendmsg='" + perl_d_sendmsg + "'"
$ WC "d_recvmsg='" + perl_d_recvmsg + "'"
$ WC "d_msghdr_s='" + perl_d_msghdr_s + "'"
$ WC "d_cmsghdr_s='" + perl_d_cmsghdr_s + "'"
$ WC "i_sysuio='" + perl_i_sysuio + "'"
$ WC "d_fseeko='" + perl_d_fseeko + "'"
$ WC "d_ftello='" + perl_d_ftello + "'"
$ WC "d_qgcvt='" + perl_d_qgcvt + "'"
$ WC "d_readv='" + perl_d_readv + "'"
$ WC "d_writev='" + perl_d_writev + "'"
$ WC "i_machcthr='" + perl_i_machcthr + "'"
$ WC "usemultiplicity='" + perl_usemultiplicity + "'"
$ WC "i_poll='" + perl_i_poll + "'"
$ WC "i_inttypes='" + perl_i_inttypes + "'"
$ WC "d_off64_t='" + perl_d_off64_t + "'"
$ WC "d_fpos64_t='" + perl_d_fpos64_t + "'"
$ WC "use64bitall='" + perl_use64bitall + "'"
$ WC "use64bitint='" + perl_use64bitint + "'"
$ WC "d_drand48proto='" + perl_d_drand48proto + "'"
$ WC "d_lseekproto='" + perl_d_drand48proto + "'"
$ WC "d_old_pthread_create_joinable='" + perl_d_old_pthread_create_joinable + "'"
$ WC "old_pthread_create_joinable='" + perl_old_pthread_create_joinable + "'"
$ WC "drand01='" + perl_drand01 + "'"
$ WC "randseedtype='" + perl_randseedtype + "'"
$ WC "seedfunc='" + perl_seedfunc + "'"
$ WC "sig_num_init='" + perl_sig_num_with_commas + "'"
$ WC "i_sysmount='" + perl_i_sysmount + "'"
$ WC "d_fstatfs='" + perl_d_fstatfs + "'"
$ WC "d_getfsstat='" + perl_d_getfsstat + "'"
$ WC "d_memchr='" + perl_d_memchr + "'"
$ WC "d_statfsflags='" + perl_d_statfsflags + "'"
$ WC "fflushNULL='define'"
$ WC "fflushall='undef'"
$ WC "d_stdio_stream_array='undef'"
$ WC "stdio_stream_array='" + perl_stdio_stream_array + "'"
$ WC "i_sysstatvfs='" + perl_i_sysstatvfs + "'"
$ WC "i_syslog='" + perl_i_syslog + "'"
$ WC "i_sysmode='" + perl_i_sysmode + "'"
$ WC "i_sysutsname='" + perl_i_sysutsname + "'"
$ WC "i_machcthreads='" + perl_i_machcthreads + "'"
$ WC "i_pthread='" + perl_i_pthread + "'"
$ WC "d_fstatvfs='" + perl_d_fstatvfs + "'"
$ WC "i_mntent='" + perl_i_mntent + "'"
$ WC "d_getmntent='" + perl_d_getmntent + "'"
$ WC "d_hasmntopt='" + perl_d_hasmntopt + "'"
$ WC "d_telldirproto='" + perl_d_telldirproto + "'"
$ WC "d_madvise='" + perl_d_madvise + "'"
$ WC "d_msync='" + perl_d_msync + "'"
$ WC "d_mprotect='" + perl_d_mprotect + "'"
$ WC "d_munmap='" + perl_d_munmap + "'"
$ WC "d_mmap='" + perl_d_mmap + "'"
$ WC "mmaptype='" + perl_mmaptype + "'"
$ WC "i_sysmman='" + perl_i_sysmman + "'"
$ WC "installusrbinperl='" + perl_installusrbinperl + "'"
$! WC "selectminbits='" + perl_selectminbits + "'"
$ WC "crosscompile='" + perl_crosscompile + "'"
$ WC "multiarch='" + perl_multiarch + "'"
$ WC "sched_yield='" + perl_sched_yield + "'"
$ WC "d_strtoull='" + perl_d_strtoull + "'"
$ WC "d_strtouq='" + perl_d_strtouq + "'"
$ WC "d_strtoll='" + perl_d_strtoll + "'"
$ WC "d_strtold='" + perl_d_strtold + "'"
$ WC "usesocks='" + perl_usesocks + "'"
$ WC "d_vendorlib='" + perl_d_vendorlib + "'"
$ WC "vendorlibexp='" + perl_vendorlibexp + "'"
$ WC "vendorlib_stem='" + perl_vendorlib_stem + "'"
$ WC "d_atolf='" + perl_d_atolf + "'"
$ WC "d_atoll='" + perl_d_atoll + "'"
$ WC "d_bincompat5005='" + perl_d_bincompat + "'"
$ WC "d_endspent='" + perl_d_endspent + "'"
$ WC "d_getspent='" + perl_d_getspent + "'"
$ WC "d_getspnam='" + perl_d_getspnam + "'"
$ WC "d_setspent='" + perl_d_setspent + "'"
$ WC "i_shadow='" + perl_i_shadow + "'"
$ WC "i_socks='" + perl_i_socks + "'"
$ WC "d_PRIfldbl='" + perl_d_PRIfldbl + "'"
$ WC "d_PRIgldbl='" + perl_d_PRIgldbl + "'"
$ WC "d_PRId64='" + perl_d_PRId64 + "'"
$ WC "d_PRIu64='" + perl_d_PRIu64 + "'"
$ WC "d_PRIo64='" + perl_d_PRIo64 + "'"
$ WC "d_PRIx64='" + perl_d_PRIx64 + "'"
$ WC "sPRIfldbl='" + perl_sPRIfldbl + "'"
$ WC "sPRIgldbl='" + perl_sPRIgldbl + "'"
$ WC "sPRId64='" + perl_sPRId64 + "'"
$ WC "sPRIu64='" + perl_sPRIu64 + "'"
$ WC "sPRIo64='" + perl_sPRIo64 + "'"
$ WC "sPRIx64='" + perl_sPRIx64 + "'"
$ WC "d_llseek='" + perl_d_llseek + "'"
$ WC "d_iconv='" + perl_d_iconv +"'"
$ WC "i_iconv='" + perl_i_iconv +"'"
$ WC "inc_version_list='0'"
$ WC "inc_version_list_init='0'"
$ WC "uselargefiles='" + perl_uselargefiles + "'"
$ WC "uselongdouble='" + perl_uselongdouble + "'"
$ WC "usemorebits='" + perl_usemorebits + "'"
$ WC "d_quad='" + perl_d_quad + "'"
$ WC "quadtype='" + perl_quadtype + "'" 
$ WC "uquadtype='" + perl_uquadtype + "'" 
$ WC "quadkind='" + perl_quadkind + "'"
$ WC "d_fs_data_s='" + perl_d_fs_data_s + "'" 
$ WC "d_getcwd='" + perl_d_getcwd + "'"
$ WC "d_getmnt='" + perl_d_getmnt + "'"
$ WC "d_sqrtl='" + perl_d_sqrtl + "'"
$ WC "d_statfs_f_flags='" + perl_d_statfs_f_flags + "'"
$ WC "d_statfs_s='" + perl_d_statfs_s + "'"
$ WC "d_ustat='" + perl_d_ustat + "'"
$ WC "d_vendorarch='" + perl_d_vendorarch + "'"
$ WC "vendorarchexp='" + perl_vendorarchexp + "'"
$ WC "i_sysstatfs='" + perl_i_sysstatfs + "'"
$ WC "i_sysvfs='" + perl_i_sysvfs + "'"
$ WC "i_ustat='" + perl_i_ustat + "'"
$ WC "ivtype='" + perl_ivtype + "'"
$ WC "uvtype='" + perl_uvtype + "'"
$ WC "i8type='" + perl_i8type + "'"
$ WC "i16type='" + perl_i16type + "'"
$ WC "u8type='" + perl_u8type + "'"
$ WC "u16type='" + perl_u16type + "'"
$ WC "i32type='" + perl_i32type + "'"
$ WC "u32type='" + perl_u32type + "'"
$ WC "i64type='" + perl_i64type + "'"
$ WC "u64type='" + perl_u64type + "'"
$ WC "nvtype='" + perl_nvtype + "'"
$ WC "ivsize='" + perl_ivsize + "'"
$ WC "uvsize='" + perl_uvsize + "'"
$ WC "i8size='" + perl_i8size + "'"
$ WC "u8size='" + perl_u8size + "'"
$ WC "i16size='" + perl_i16size + "'"
$ WC "u16size='" + perl_u16size + "'"
$ WC "i32size='" + perl_i32size + "'"
$ WC "u32size='" + perl_u32size + "'"
$ WC "i64size='" + perl_i64size + "'"
$ WC "u64size='" + perl_u64size + "'"
$ WC "ivdformat='" + perl_ivdformat + "'"
$ WC "uvuformat='" + perl_uvuformat + "'"
$ WC "uvoformat='" + perl_uvoformat + "'"
$ WC "uvxformat='" + perl_uvxformat + "'"
$ WC "d_vms_case_sensitive_symbols='" + d_vms_be_case_sensitive + "'"
$ WC "sizesize='" + perl_sizesize + "'"
$!
$! ##WRITE NEW CONSTANTS HERE##
$!
$ Close CONFIG
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
$ echo ""
$ echo "Doing variable substitutions on .SH files..."
$ echo "Extracting config.h (with variable substitutions)"
$ !
$ ! we need an fdl file
$ CREATE [-]CONFIG.FDL
RECORD
  FORMAT STREAM_LF
$ CREATE /FDL=[-]CONFIG.FDL [-]CONFIG.LOCAL
$ ! First spit out the header info with the local defines (to get
$ ! around the 255 character command line limit)
$ OPEN/APPEND CONFIG [-]config.local
$ IF use_debugging_perl THEN WC "#define DEBUGGING"
$ IF use_two_pot_malloc THEN WC "#define TWO_POT_OPTIMIZE"
$ IF mymalloc THEN WC "#define EMBEDMYMALLOC"
$ IF use_pack_malloc THEN WC "#define PACK_MALLOC"
$ IF use_debugmalloc THEN WC "#define DEBUGGING_MSTATS"
$ IF Using_Gnu_C THEN WC "#define GNUC_ATTRIBUTE_CHECK"
$ IF (Has_Dec_C_Sockets)
$ THEN
$    WC "#define VMS_DO_SOCKETS"
$    WC "#define DECCRTL_SOCKETS"
$ ELSE
$    IF Has_Socketshr THEN WC "#define VMS_DO_SOCKETS"
$ ENDIF
$! This is VMS-specific for now
$ WC "#''perl_d_setenv' HAS_SETENV"
$ IF d_secintgenv THEN WC "#define SECURE_INTERNAL_GETENV"
$ if d_alwdeftype THEN WC "#define ALWAYS_DEFTYPES"
$ IF (use64bitint)
$ THEN
$    WC "#define USE_64_BIT_INT"
$    WC "#define USE_LONG_DOUBLE"
$ ENDIF
$ IF use64bitall THEN WC "#define USE_64_BIT_ALL"
$ IF be_case_sensitive THEN WC "#define VMS_WE_ARE_CASE_SENSITIVE"
$ if perl_d_herrno .eqs. "undef"
$ THEN
$    WC "#define NEED_AN_H_ERRNO"
$ ENDIF
$ WC "#define HAS_ENVGETENV"
$ WC "#define PERL_EXTERNAL_GLOB"
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
$ IF Using_Dec_C
$ THEN
$   DECC_REPLACE = "DECC=decc=1"
$ ELSE
$   DECC_REPLACE = "DECC=" 
$ ENDIF
$ IF Using_Gnu_C
$ THEN
$   GNUC_REPLACE = "GNUC=gnuc=1"
$ ELSE
$   GNUC_REPLACE = "GNUC=" 
$ ENDIF
$ IF Has_Dec_C_Sockets
$ THEN
$   SOCKET_REPLACE = "SOCKET=DECC_SOCKETS=1"
$ ELSE
$   IF Has_Socketshr
$   THEN
$     SOCKET_REPLACE = "SOCKET=SOCKETSHR_SOCKETS=1"
$   ELSE
$     SOCKET_REPLACE = "SOCKET="
$   ENDIF
$ ENDIF
$ IF (Use_Threads)
$ THEN
$   IF (VMS_VER .LES. "6.2")
$   THEN
$     THREAD_REPLACE = "THREAD=OLDTHREADED=1"
$   ELSE
$     THREAD_REPLACE = "THREAD=THREADED=1"
$   ENDIF
$ ELSE
$   THREAD_REPLACE = "THREAD="
$ ENDIF
$ IF mymalloc
$ THEN
$   MALLOC_REPLACE = "MALLOC=MALLOC=1"
$ ELSE
$   MALLOC_REPLACE = "MALLOC="
$ ENDIF
$ echo "Extracting ''defmakefile' (with variable substitutions)"
$!set ver
$ define/user sys$output 'UUmakefile 
$ mcr []munchconfig [-]config.sh descrip_mms.template "''DECC_REPLACE'" "''ARCH_TYPE'" "''GNUC_REPLACE'" "''SOCKET_REPLACE'" "''THREAD_REPLACE'" -
"''C_Compiler_Replace'" "''MALLOC_REPLACE'" "''Thread_Live_Dangerously'" "PV=''version'" "FLAGS=FLAGS=''extra_flags'"
$ echo "Extracting Build_Ext.Com (without variable substitutions)"
$ Create Sys$Disk:[-]Build_Ext.Com
$ Deck/Dollar="$EndOfTpl$"
$!++ Build_Ext.Com
$!   NOTE: This file is extracted as part of the VMS configuration process.
$!   Any changes made to it directly will be lost.  If you need to make any
$!   changes, please edit the template in [.vms]SubConfigure.Com instead.
$    def = F$Environment("Default")
$    exts1 = F$Edit(p1,"Compress")
$    p2 = F$Edit(p2,"Upcase,Compress,Trim")
$    If F$Locate("MCR ",p2).eq.0 Then p2 = F$Extract(3,255,p2)
$    miniperl = "$" + F$Search(F$Parse(p2,".Exe"))
$    makeutil = p3
$    if f$type('p3') .nes. "" then makeutil = 'p3'
$    targ = F$Edit(p4,"Lowercase")
$    i = 0
$ next_ext:
$    ext = F$Element(i," ",p1)
$    If ext .eqs. " " Then Goto done
$    Define/User Perl_Env_Tables CLISYM_LOCAL
$    miniperl
     ($extdir = $ENV{'ext'}) =~ s/::/./g;
     $extdir =~ s#/#.#g;
     if ($extdir =~ /^vms/i) { $extdir =~ s/vms/.vms.ext/i; }
     else                    { $extdir = ".ext.$extdir";   }
     ($ENV{'extdir'} = "[$extdir]");
     ($ENV{'up'} = ('-') x ($extdir =~ tr/././));
$    Set Default &extdir
$    redesc = 0
$    If F$Locate("clean",targ) .eqs. F$Length(targ)
$    Then
$      Write Sys$Output "Building ''ext' . . ."
$      On Error Then Goto done
$      If F$Search("Descrip.MMS") .eqs. ""
$      Then
$        redesc = 1
$      Else
$        If F$CvTime(F$File("Descrip.MMS","rdt")) .lts. -
            F$CvTime(F$File("Makefile.PL","rdt")) Then redesc = 1
$      EndIf
$    Else
$      Write Sys$Output "''targ'ing ''ext' . . ."
$      On Error Then Continue
$    EndIf
$    If redesc Then -
       miniperl "-I[''up'.lib]" Makefile.PL "INST_LIB=[''up'.lib]" "INST_ARCHLIB=[''up'.lib]"
$    makeutil 'targ'
$    i = i + 1
$    Set Def &def
$    Goto next_ext
$ done:
$    sts = $Status
$    Set Def &def
$    Exit sts
$!-- Build_Ext.Com
$EndOfTpl$
$
$! set nover
$!
$! Clean up after ourselves
$ DELETE/NOLOG/NOCONFIRM munchconfig.exe;
$ DELETE/NOLOG/NOCONFIRM munchconfig.obj;
