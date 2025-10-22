# Wrapper around hppaelf to disable multi-space operation
. ${srcdir}/emultempl/hppaelf.em
unset PARSE_AND_LIST_PROLOGUE
unset PARSE_AND_LIST_LONGOPTS
unset PARSE_AND_LIST_OPTIONS
unset PARSE_AND_LIST_ARGS_CASES
