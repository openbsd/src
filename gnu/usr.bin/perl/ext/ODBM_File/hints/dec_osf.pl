# The -hidden option causes compilation to fail on Digital Unix.
#   Andy Dougherty  <doughera@lafcol.lafayette.edu>
#   Sat Jan 13 16:29:52 EST 1996
$self->{LDDLFLAGS} = $Config{lddlflags};
$self->{LDDLFLAGS} =~ s/-hidden//;
