s|po2tbl\.sed\.in|po2tblsed.in|g
s|gdb\.c++|gdb.cxx|g
/ac_rel_source/s|ln -s|cp -p|
s|\.gdbinit|gdb.ini|g

/^ac_given_INSTALL=/,/^CEOF/ {
  /^s%@prefix@%/a\
  s,\\([yp*]\\)\\.tab,\\1_tab,g\
  /^	@rm -f/s,\\$@-\\[0-9\\]\\[0-9\\],& *.i[1-9] *.i[1-9][0-9],\
  s,standards\\.info\\*,standard*.inf*,\
  s,configure\\.info\\*,configur*.inf*,\
  s,\\.info\\*,.inf* *.i[1-9] *.i[1-9][0-9],\
  s,\\.gdbinit,gdb.ini,g\
  /TEXINPUTS=/s,:,';',g\
  /VPATH *=/s,\\([^A-z]\\):,\\1;,g\
  /\\$\\$file-\\[0-9\\]/s,echo,& *.i[1-9] *.i[1-9][0-9],\
  /\\$\\$file-\\[0-9\\]/s,rm -f \\$\\$file,& \\${PACKAGE}.i[1-9] \\${PACKAGE}.i[1-9][0-9],\
  s,config\\.h\\.in,config.h-in,g\
  s,po2tbl\\.sed\\.in,po2tblsed.in,g

}

/^ac_given_srcdir=/,/^CEOF/ {
  /^s%@TOPLEVEL_CONFIGURE_ARGUMENTS@%/a\
  /@test ! -f /s,\\(.\\)\$, export am_cv_exeext=.exe; export lt_cv_sys_max_cmd_len=12288; \\1,

}

/^CONFIG_FILES=/,/^EOF/ {
  s|po/Makefile.in\([^-:a-z]\)|po/Makefile.in:po/Makefile.in-in\1|
}

/^ *# *Handling of arguments/,/^done/ {
  s| config.h"| config.h:config.h-in"|
  s|config.h\([^-:"a-z]\)|config.h:config.h-in\1|
}

/^[ 	]*\/\*)/s,/\*,/*|[A-z]:/*,
/\$]\*) INSTALL=/s,\[/\$\]\*,&|[A-z]:/*,
/\$]\*) ac_rel_source=/s,\[/\$\]\*,&|[A-z]:/*,
/ac_file_inputs=/s,\( -e "s%\^%\$ac_given_srcdir/%"\)\( -e "s%:% $ac_given_srcdir/%g"\),\2\1,
/^[ 	]*if test "x`echo /s,sed 's@/,sed -e 's@^[A-z]:@@' -e 's@/,
/^ *ac_config_headers=/s, config.h", config.h:config.h-in",
