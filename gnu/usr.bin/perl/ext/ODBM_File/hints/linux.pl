# uses GDBM dbm compatibility feature - at least on SuSE 8.0
$self->{LIBS} = ['-lgdbm'];

# Debian/Ubuntu have /usr/lib/libgdbm_compat.so.3* but not this file,
# so linking may fail
if (-e '/usr/lib/libgdbm_compat.so' or -e '/usr/lib64/libgdbm_compat.so') {
    $self->{LIBS}->[0] .= ' -lgdbm_compat';
}
