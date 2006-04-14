#!/bin/sh
# check library exported symbols
# $KTH: check-symbols.sh,v 1.10.2.1 2005/06/15 01:25:48 lha Exp $

LANG=C
export LANG

exit 0 #disable test on release branch

esym=
symbols=

# AIX has different default output format
nmargs=""
if [ "`uname`" = AIX ]; then
	nmargs="-B"
fi

while test $# != 0 ;do 
  case "$1" in
    -lib) lib="$2" ; shift;;
    -com_err)
       esym="${esym} com_right\$ free_error_table\$ initialize_error_table_r\$"
       esym="${esym} com_err error_message\$ error_table_name\$"
       esym="${esym} init_error_table\$ add_to_error_table\$"
       esym="${esym} reset_com_err_hook\$ set_com_err_hook\$ _et_list\$"
       esym="${esym} et_[A-Za-z0-9]*_error_table"
       esym="${esym} initialize_[A-Za-z0-9]*_error_table et_*_error_table" ;;
    -version)
       esym="${esym} print_version\$" ;;
    -asn1compile)
       esym="${esym} copy_ free_ length_ decode_ encode_ length_ "
       esym="${esym} *.2int\$ int2 asn1_[A-Za-z0-9]*_units\$" ;;
    -*) echo "unknown option $1" ; exit 1 ;;
    *) break ;;
  esac
  shift
done

for a in "$@" $esym; do
    symbols="\$3 !~ /^_?${a}/ ${symbols:+&&} ${symbols}"
done

# F filename, N debugsymbols, W weak symbols, U undefined

(nm $nmargs .libs/lib${lib}.a || nm $nmargs .libs/lib${lib}.so*)  |
awk "BEGIN { stat = 0 }
NF == 3 && \$2 ~ /[A-EG-MO-TVX-Z]/ && $symbols { printf \"%s should not be exported (type %s)\\n\", \$3, \$2; ++stat } END { exit stat }"

