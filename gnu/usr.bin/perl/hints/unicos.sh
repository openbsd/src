case `uname -r` in
6.1*) shellflags="-m+65536" ;;
esac
optimize="-O1"
d_setregid='undef'
d_setreuid='undef'

