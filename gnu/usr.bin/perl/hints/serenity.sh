libs='-ldl -lm -lcrypt -lcore'

# Use OS's malloc() by default.
case "$usemymalloc" in
'') usemymalloc='n' ;;
esac

# uses gcc.
cc='gcc'
ld='gcc'

# as of the latest some symbols are confusing the nm lookup
case "$usenm" in
'') usenm='undef' ;;
esac

# disable nanosleep
d_nanosleep='undef'

# don't try to test min/max of gmtime/localtime
sGMTIME_max=2147483647
sGMTIME_min=-2147481748
sLOCALTIME_max=2147483647
sLOCALTIME_min=-2147481748
