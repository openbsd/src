#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

my $test_num = 1;
# Utility testing functions.
sub ok ($;$) {
    my($test, $name) = @_;
    my $ok = '';
    $ok .= "not " unless $test;
    $ok .= "ok $test_num";
    $ok .= " - $name" if defined $name;
    $ok .= "\n";
    print $ok;
    $test_num++;

    return $test;
}


use Test::Builder;
my $Test = Test::Builder->new;

print "1..2\n";

eval { $Test->plan(7); };
ok( $@ =~ /^plan\(\) doesn't understand 7/, 'bad plan()' ) ||
    print STDERR "# $@";

eval { $Test->plan(wibble => 7); };
ok( $@ =~ /^plan\(\) doesn't understand wibble 7/, 'bad plan()' ) ||
    print STDERR "# $@";

