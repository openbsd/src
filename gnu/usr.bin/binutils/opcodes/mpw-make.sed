# Sed commands to finish translating the opcodes Makefile.in into MPW syntax.

# Empty HDEFINES.
/HDEFINES/s/@HDEFINES@//

/INCDIR=/s/"{srcdir}":/"{topsrcdir}"/
/^CSEARCH = .*$/s/$/ -i "{INCDIR}":mpw: -i ::extra-include:/
/BFD_MACHINES/s/@BFD_MACHINES@/{BFD_MACHINES}/
/archdefs/s/@archdefs@/{ARCHDEFS}/
