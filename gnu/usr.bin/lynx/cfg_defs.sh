#!/bin/sh
# Translate the lynx_cfg.h and config.cache data into a table, useful for
# display at runtime.

TOP="${1-.}"
OUT=cfg_defs.h

cat >$OUT <<EOF
#ifndef CFG_DEFS_H
#define CFG_DEFS_H 1

static CONST struct {
	CONST char *name;
	CONST char *value;
} config_cache[] = {
EOF

sed \
	-e '/^#/d'     \
	-e 's/^.[^=]*_cv_//' \
	-e 's/=\${.*=/=/'  \
	-e 's/}$//'          \
	config.cache | /bin/sh $TOP/cfg_edit.sh >>$OUT

cat >>$OUT <<EOF
};

static CONST struct {
	CONST char *name;
	CONST char *value;
} config_defines[] = {
EOF
fgrep	'#define' lynx_cfg.h |
sed	-e 's@	@ @g' \
	-e 's@  @ @g' \
	-e 's@^[ 	]*#define[ 	]*@@' \
	-e 's@[ ]*/\*.*\*/@@' \
	-e 's@[ 	][ 	]*@=@' \
    | /bin/sh $TOP/cfg_edit.sh >>$OUT

cat >>$OUT <<EOF
};

#endif /* CFG_DEFS_H */
EOF
