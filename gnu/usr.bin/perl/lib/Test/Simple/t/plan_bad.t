#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}


# Can't use Test.pm, that's a 5.005 thing.
package My::Test;

print "1..7\n";

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


sub is ($$;$) {
    my($this, $that, $name) = @_;
    my $test = $this eq $that;
    my $ok = '';
    $ok .= "not " unless $test;
    $ok .= "ok $test_num";
    $ok .= " - $name" if defined $name;
    $ok .= "\n";
    print $ok;

    unless( $test ) {
        print "# got      \n$this";
        print "# expected \n$that";
    }
    $test_num++;

    return $test;
}


use Test::More import => ['plan'];

ok !eval { plan tests => 'no_plan'; };
is $@, "Number of tests must be a postive integer.  You gave it 'no_plan'.\n";

my $foo = [];
my @foo = ($foo, 2, 3);
ok !eval { plan tests => @foo };
is $@, "Number of tests must be a postive integer.  You gave it '$foo'.\n";

ok !eval { plan tests => 0 };
ok !eval { plan tests => -1 };
ok !eval { plan tests => '' };
