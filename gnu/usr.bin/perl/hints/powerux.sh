# Hints for the PowerUX operating system running on Harris NightHawk
# machines.  Written by Tom.Horsley@mail.hcsc.com
#
# This config uses dynamic linking and the Harris C compiler.  It has been
# tested on a Harris 6800 running PowerUX.

# Internally at Harris, we use a source management tool which winds up
# giving us read-only copies of source trees that are mostly symbolic links.
# That upsets the perl build process when it tries to edit opcode.h and
# embed.h or touch perly.c or perly.h, so turn those files into "real" files
# when Configure runs. (If you already have "real" source files, this won't
# do anything).
#
if [ -x /usr/local/mkreal ]
then
   for i in '.' '..'
   do
      for j in embed.h opcode.h perly.h perly.c
      do
         if [ -h $i/$j ]
         then
            ( cd $i ; /usr/local/mkreal $j ; chmod 666 $j )
         fi
      done
   done
fi

# We DO NOT want -lmalloc or -lPW, we DO need -lgen to follow -lnsl, so
# fixup libswanted to reflect that desire.
#
libswanted=`echo ' '$libswanted' ' | sed -e 's/ malloc / /' -e 's/ PW / /' -e 's/ nsl / nsl gen /'`

# We DO NOT want /usr/ucblib in glibpth
#
glibpth=`echo ' '$glibpth' ' | sed -e 's@ /usr/ucblib @ @'`

# Yes, csh exists, but doesn't work worth beans, if perl tries to use it,
# the glob test fails, so just pretend it isn't there...
#
d_csh='undef'

# Need to use Harris cc for most of these options to be meaningful (if you
# want to get this to work with gcc, you're on your own :-). Passing
# -Bexport to the linker when linking perl is important because it leaves
# the interpreter internal symbols visible to the shared libs that will be
# loaded on demand (and will try to reference those symbols).
#
cc='/bin/cc'
cccdlflags='-Zpic'
ccdlflags='-Zlink=dynamic -Wl,-Bexport'
lddlflags='-Zlink=so'

# Configure sometime finds what it believes to be ndbm header files on the
# system and imagines that we have the NDBM library, but we really don't.
# There is something there that once resembled ndbm, but it is purely
# for internal use in some tool and has been hacked beyond recognition
# (or even function :-)
#
i_ndbm='undef'

# Misc other flags that might be able to change, but I know these work right.
#
d_suidsafe='define'
d_isascii='define'
d_mymalloc='undef'
usemymalloc='n'
ssizetype='ssize_t'
usevfork='false'
