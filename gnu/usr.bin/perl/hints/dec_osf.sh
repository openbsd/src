# hints/dec_osf.sh
case "$optimize" in
'')
    case "$cc" in 
    *gcc*) ;;
    *)	optimize='-O2 -Olimit 2900' ;;
    esac
    ;;
esac
ccflags="$ccflags -DSTANDARD_C"
lddlflags='-shared -expect_unresolved "*" -s -hidden'
