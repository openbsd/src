#!perl -w

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = ('../lib', 'lib');
    }
    else {
        unshift @INC, 't/lib';
    }
}

# Can't use Test.pm, that's a 5.005 thing.
print "1..4\n";

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

use TieOut;
use Test::Builder;
my $Test = Test::Builder->new();

my $result;
my $out = $Test->output('foo');

ok( defined $out );

print $out "hi!\n";
close *$out;

undef $out;
open(IN, 'foo') or die $!;
chomp(my $line = <IN>);
close IN;

ok($line eq 'hi!');

open(FOO, ">>foo") or die $!;
$out = $Test->output(\*FOO);
$old = select *$out;
print "Hello!\n";
close *$out;
undef $out;
select $old;
open(IN, 'foo') or die $!;
my @lines = <IN>;
close IN;

ok($lines[1] =~ /Hello!/);

unlink('foo');


# Ensure stray newline in name escaping works.
$out = tie *FAKEOUT, 'TieOut';
$Test->output(\*FAKEOUT);
$Test->exported_to(__PACKAGE__);
$Test->no_ending(1);
$Test->plan(tests => 5);

$Test->ok(1, "ok");
$Test->ok(1, "ok\n");
$Test->ok(1, "ok, like\nok");
$Test->skip("wibble\nmoof");
$Test->todo_skip("todo\nskip\n");

my $output = $out->read;
ok( $output eq <<OUTPUT ) || print STDERR $output;
1..5
ok 1 - ok
ok 2 - ok
# 
ok 3 - ok, like
# ok
ok 4 # skip wibble
# moof
not ok 5 # TODO & SKIP todo
# skip
# 
OUTPUT
