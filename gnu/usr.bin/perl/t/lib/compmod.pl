#!./perl

BEGIN {
    chdir 't';
    @INC = '../lib';
}

my $module = shift;

# 'require open' confuses Perl, so we use instead.
eval "use $module ();";
if( $@ ) {
    print "not ";
    $@ =~ s/\n/\n# /g;
    warn "# require failed with '$@'\n";
}
print "ok - $module\n";


