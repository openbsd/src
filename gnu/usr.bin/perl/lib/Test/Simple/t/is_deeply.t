#!/usr/bin/perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

use strict;

use Test::Builder;
require Test::Simple::Catch;
my($out, $err) = Test::Simple::Catch::caught();
Test::Builder->new->no_header(1);
Test::Builder->new->no_ending(1);

# Can't use Test.pm, that's a 5.005 thing.
package main;

print "1..22\n";

my $test_num = 1;
# Utility testing functions.
sub is ($$;$) {
    my($this, $that, $name) = @_;
    my $test = $$this eq $that;
    my $ok = '';
    $ok .= "not " unless $test;
    $ok .= "ok $test_num";
    $ok .= " - $name" if defined $name;
    $ok .= "\n";
    print $ok;

    unless( $test ) {
        print "# got      \n$$this";
        print "# expected \n$that";
    }
    $test_num++;

    $$this = '';

    return $test;
}

sub like ($$;$) {
    my($this, $regex, $name) = @_;

    my $test = $$this =~ /$regex/;

    my $ok = '';
    $ok .= "not " unless $test;
    $ok .= "ok $test_num";
    $ok .= " - $name" if defined $name;
    $ok .= "\n";
    print $ok;

    unless( $test ) {
        print "# got      \n$$this";
        print "# expected \n$regex";
    }
    $test_num++;

    $$this = '';


    return $test;
}


require Test::More;
Test::More->import(tests => 11, import => ['is_deeply']);

my $Filename = quotemeta $0;

#line 68
is_deeply('foo', 'bar', 'plain strings');
is( $out, "not ok 1 - plain strings\n",     'plain strings' );
is( $err, <<ERR,                            '    right diagnostic' );
#     Failed test ($0 at line 68)
#          got: 'foo'
#     expected: 'bar'
ERR


#line 78
is_deeply({}, [], 'different types');
is( $out, "not ok 2 - different types\n",   'different types' );
like( $err, <<ERR,                          '   right diagnostic' );
#     Failed test \\($Filename at line 78\\)
#     Structures begin differing at:
#          \\\$got = 'HASH\\(0x[0-9a-f]+\\)'
#     \\\$expected = 'ARRAY\\(0x[0-9a-f]+\\)'
ERR

#line 88
is_deeply({ this => 42 }, { this => 43 }, 'hashes with different values');
is( $out, "not ok 3 - hashes with different values\n", 
                                        'hashes with different values' );
is( $err, <<ERR,                        '   right diagnostic' );
#     Failed test ($0 at line 88)
#     Structures begin differing at:
#          \$got->{this} = '42'
#     \$expected->{this} = '43'
ERR

#line 99
is_deeply({ that => 42 }, { this => 42 }, 'hashes with different keys');
is( $out, "not ok 4 - hashes with different keys\n",
                                        'hashes with different keys' );
is( $err, <<ERR,                        '    right diagnostic' );
#     Failed test ($0 at line 99)
#     Structures begin differing at:
#          \$got->{this} = Does not exist
#     \$expected->{this} = '42'
ERR

#line 110
is_deeply([1..9], [1..10],    'arrays of different length');
is( $out, "not ok 5 - arrays of different length\n",
                                        'arrays of different length' );
is( $err, <<ERR,                        '    right diagnostic' );
#     Failed test ($0 at line 110)
#     Structures begin differing at:
#          \$got->[9] = Does not exist
#     \$expected->[9] = '10'
ERR

#line 121
is_deeply([undef, undef], [undef], 'arrays of undefs' );
is( $out, "not ok 6 - arrays of undefs\n",  'arrays of undefs' );
is( $err, <<ERR,                            '    right diagnostic' );
#     Failed test ($0 at line 121)
#     Structures begin differing at:
#          \$got->[1] = undef
#     \$expected->[1] = Does not exist
ERR

#line 131
is_deeply({ foo => undef }, {},    'hashes of undefs',    'hashes of undefs' );
is( $out, "not ok 7 - hashes of undefs\n",  'hashes of undefs' );
is( $err, <<ERR,                            '    right diagnostic' );
#     Failed test ($0 at line 131)
#     Structures begin differing at:
#          \$got->{foo} = undef
#     \$expected->{foo} = Does not exist
ERR

#line 141
is_deeply(\42, \23,   'scalar refs');
is( $out, "not ok 8 - scalar refs\n",   'scalar refs' );
is( $err, <<ERR,                        '    right diagnostic' );
#     Failed test ($0 at line 141)
#     Structures begin differing at:
#     \${     \$got} = '42'
#     \${\$expected} = '23'
ERR

#line 151
is_deeply([], \23,    'mixed scalar and array refs');
is( $out, "not ok 9 - mixed scalar and array refs\n",
                                        'mixed scalar and array refs' );
like( $err, <<ERR,                      '    right diagnostic' );
#     Failed test \\($Filename at line 151\\)
#     Structures begin differing at:
#          \\\$got = 'ARRAY\\(0x[0-9a-f]+\\)'
#     \\\$expected = 'SCALAR\\(0x[0-9a-f]+\\)'
ERR


my($a1, $a2, $a3);
$a1 = \$a2;  $a2 = \$a3;
$a3 = 42;

my($b1, $b2, $b3);
$b1 = \$b2;  $b2 = \$b3;
$b3 = 23;

#line 173
is_deeply($a1, $b1, 'deep scalar refs');
is( $out, "not ok 10 - deep scalar refs\n",     'deep scalar refs' );
is( $err, <<ERR,                              '    right diagnostic' );
#     Failed test ($0 at line 173)
#     Structures begin differing at:
#     \${\${     \$got}} = '42'
#     \${\${\$expected}} = '23'
ERR

# I don't know how to properly display this structure.
# $a2 = { foo => \$a3 };
# $b2 = { foo => \$b3 };
# is_deeply([$a1], [$b1], 'deep mixed scalar refs');

my $foo = {
           this => [1..10],
           that => { up => "down", left => "right" },
          };

my $bar = {
           this => [1..10],
           that => { up => "down", left => "right", foo => 42 },
          };

#line 198
is_deeply( $foo, $bar, 'deep structures' );
is( $out, "not ok 11 - deep structures\n",  'deep structures' );
is( $err, <<ERR,                            '    right diagnostic' );
#     Failed test ($0 at line 198)
#     Structures begin differing at:
#          \$got->{that}{foo} = Does not exist
#     \$expected->{that}{foo} = '42'
ERR
