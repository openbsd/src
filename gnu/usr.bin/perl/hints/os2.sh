# hints/os2.sh
# This file reflects the tireless work of
# Ilya Zakharevich <ilya@math.ohio-state.edu>
#
# Trimmed and comments added by 
#     Andy Dougherty  <doughera@lafcol.lafayette.edu>
#     Exactly what is required beyond a standard OS/2 installation?
#     There are notes about "patched pdksh" I don't understand.

# Note that symbol extraction code gives wrong answers (sometimes?) on
# gethostent and setsid.

# Note that during the .obj compile you need to move the perl.dll file
# to LIBPATH :-(

#osname="OS/2"
sysman=`../UU/loc . /man/man1 c:/man/man1 c:/usr/man/man1 d:/man/man1 d:/usr/man/man1 e:/man/man1 e:/usr/man/man1 f:/man/man1 f:/usr/man/man1 g:/man/man1 g:/usr/man/man1 /usr/man/man1`
cc='gcc'
usrinc='/emx/include'
libemx="`../UU/loc . X c:/emx/lib d:/emx/lib e:/emx/lib f:/emx/lib g:/emx/lib h:/emx/lib /emx/lib`"

if test "$libemx" = "X"; then echo "Cannot find C library!"; fi

libpth="$libemx/st $libemx"

so='dll'

# Additional definitions:

firstmakefile='GNUmakefile'
exe_ext='.exe'

if [ "$emxaout" != "" ]; then
    d_shrplib='undef'
    obj_ext='.o'
    lib_ext='.a'
    ar='ar'
    plibext='.a'
    d_fork='define'
    lddlflags='-Zdll'
    ldflags='-Zexe'
    ccflags='-DDOSISH -DNO_SYS_ALLOC -DOS2=2 -DEMBED -I. -DPACK_MALLOC'
    use_clib='c'
else
    d_shrplib='define'
    obj_ext='.obj'
    lib_ext='.lib'
    ar='emxomfar'
    plibext='.lib'
    d_fork='undef'
    lddlflags='-Zdll -Zomf -Zcrtdll'
    # Recursive regmatch may eat 2.5M of stack alone.
    ldflags='-Zexe -Zomf -Zcrtdll -Zstack 32000'
    ccflags='-Zomf -DDOSISH -DOS2=2 -DEMBED -I. -DPACK_MALLOC'
    use_clib='c_import'
fi

# To get into config.sh (should start at the beginning of line)
# or you can put it into config.over.
plibext="$plibext"

#libc="/emx/lib/st/c_import$lib_ext"
libc="$libemx/st/$use_clib$lib_ext"

if test -r "$libemx/c_alias$lib_ext"; then 
    libnames="$libemx/c_alias$lib_ext"
fi
# otherwise puts -lc ???

# [Maybe we should just remove c from $libswanted ?]

libs='-lsocket -lm'
archobjs="os2$obj_ext"

# Run files without extension with sh - feature of patched ksh
# [???]
NOHASHBANG=sh
# Same with newer ksh
EXECSHELL=sh

cccdlflags='-Zdll'
dlsrc='dl_os2.xs'
ld='gcc'
usedl='define'

#cppflags='-DDOSISH -DOS2=2 -DEMBED -I.'

# for speedup: (some patches to ungetc are also needed):
# Note that without this guy tests 8 and 10 of io/tell.t fail, with it 11 fails

stdstdunder=`echo "#include <stdio.h>" | cpp | egrep -c "char +\* +_ptr"`
d_stdstdio='define'
d_stdiobase='define'
d_stdio_ptr_lval='define'
d_stdio_cnt_lval='define'

if test "$stdstdunder" = 0; then
  stdio_ptr='((fp)->ptr)'
  stdio_cnt='((fp)->rcount)'
  stdio_base='((fp)->buffer)'
  stdio_bufsiz='((fp)->rcount + (fp)->ptr - (fp)->buffer)'
  ccflags="$ccflags -DMYTTYNAME"
  myttyname='define'
else
  stdio_ptr='((fp)->_ptr)'
  stdio_cnt='((fp)->_rcount)'
  stdio_base='((fp)->_buffer)'
  stdio_bufsiz='((fp)->_rcount + (fp)->_ptr - (fp)->_buffer)'
fi

# to put into config.sh
myttyname="$myttyname"

# To have manpages installed
nroff='nroff.cmd'
# above will be overwritten otherwise, indented to avoid config.sh
  _nroff='nroff.cmd'

ln='cp'
# Will be rewritten otherwise, indented to not put in config.sh
  _ln='cp'
lns='cp'

nm_opt='-p'

####### All the rest is commented

# I do not have these:
#dynamic_ext='Fcntl GDBM_File SDBM_File POSIX Socket UPM REXXCALL'
#dynamic_ext='Fcntl POSIX Socket SDBM_File Devel/DProf'
#extensions='Fcntl GDBM_File SDBM_File POSIX Socket UPM REXXCALL'
#extensions='Fcntl SDBM_File POSIX Socket Devel/DProf'

# The next two are commented. pdksh handles #!
# sharpbang='extproc '
# shsharp='false'

# Commented:
#startsh='extproc ksh\\n#! sh'
