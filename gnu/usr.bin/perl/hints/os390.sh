# hints/os390.sh
# OS/390 OpenEdition Release 3 Mon Sep 22 1997 thanks to:
#     
#     John Pfuntner <pfuntner@vnet.ibm.com>
#     Len Johnson <lenjay@ibm.net>
#     Bud Huff  <BAHUFF@us.oracle.com>
#     Peter Prymmer <pvhp@forte.com>
#     Andy Dougherty  <doughera@lafcol.lafayette.edu>
#     Tim Bunce  <Tim.Bunce@ig.co.uk>
#
#  as well as the authors of the aix.sh file
#

cc='c89'
ccflags='-DMAXSIG=38 -DOEMVS -D_OE_SOCKETS -D_XOPEN_SOURCE_EXTENDED -D_ALL_SOURCE'
optimize='none'
alignbytes=8
usemymalloc='y'
so='a'
dlext='none'
d_shmatprototype='define'
usenm='false'
i_time='define'
i_systime='define'
d_select='undef'

# (from aix.sh)
# uname -m output is too specific and not appropriate here
#
case "$archname" in
'') archname="$osname" ;;
esac

