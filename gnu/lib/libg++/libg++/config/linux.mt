# Needs some work.

# Linux puts iostream in libc.a.
STREAM_OBJS =
REGEX_OBJ=
# IO_DIR=no-stream might be the right thing for Linux, but you need
# to re-run gendepend in libg++/utils first.  There is also the problem
# that -nostdinc++ won't pick up the iostream include files ...
# IO_DIR=no-stream
G_CONFIG_ARGS = CONFIG_NM="nm -d"
# Don't include iostream files in libg++.a.
IO_OBJECTS_TO_GET = 
