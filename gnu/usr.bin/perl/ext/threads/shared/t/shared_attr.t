use warnings;

BEGIN {
#    chdir 't' if -d 't';
#    push @INC ,'../lib';
    require Config; import Config;
    unless ($Config{'useithreads'}) {
        print "1..0 # Skip: no useithreads\n";
        exit 0;
    }
}


sub ok {
    my ($id, $ok, $name) = @_;

    $name = '' unless defined $name;
    # You have to do it this way or VMS will get confused.
    print $ok ? "ok $id - $name\n" : "not ok $id - $name\n";

    printf "# Failed test at line %d\n", (caller)[2] unless $ok;

    return $ok;
}


use ExtUtils::testlib;
use strict;
BEGIN { print "1..81\n" };
use threads;
use threads::shared;
ok(1,1,"loaded");

my $test_count;
share($test_count);
$test_count = 2;

for(1..10) {
    my $foo : shared = "foo";
    ok($test_count++, $foo eq "foo");
    threads->create(sub { $foo = "bar" })->join();
    ok($test_count++, $foo eq "bar");
    my @foo : shared = ("foo","bar");
    ok($test_count++, $foo[1] eq "bar");
    threads->create(sub { ok($test_count++, shift(@foo) eq "foo")})->join();
    ok($test_count++, $foo[0] eq "bar");
    my %foo : shared = ( foo => "bar" );
    ok($test_count++, $foo{foo} eq "bar");
    threads->create(sub { $foo{bar} = "foo" })->join();
    ok($test_count++, $foo{bar} eq "foo");
    
    threads->create(sub { $foo{array} = \@foo})->join();
    threads->create(sub { push @{$foo{array}}, "baz"})->join();
    ok($test_count++, $foo[-1] eq "baz");
}
