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
BEGIN { print "1..11\n" };
use threads;
use threads::shared;
ok(1,1,"loaded");

my $sv;
share($sv);
$sv = "hi";
my @av;
share(@av);
push @av, $sv;
ok(2, $av[0] eq "hi");
push @av, "foo";
ok(3, $av[1] eq 'foo');
my $av = threads->create(sub {	
    my $av;	
    my @av2;
    share($av);
    share(@av2);
    $av = \@av2;
    push @$av, "bar", \@av;
    return $av;
})->join();
ok(4,$av->[0] eq "bar");
ok(5,$av->[1]->[0] eq 'hi');
threads->create(sub { $av[0] = "hihi" })->join();
ok(6,$av->[1]->[0] eq 'hihi');
ok(7, pop(@{$av->[1]}) eq "foo");
ok(8, scalar(@{$av->[1]}) == 1);
threads->create(sub { @$av = () })->join();
threads->create(sub { ok(9, scalar @$av == 0)})->join();
threads->create(sub { unshift(@$av, threads->create(sub { my @array; share(@array); return \@array})->join())})->join();
ok(10, ref($av->[0]) eq 'ARRAY');
threads->create(sub { push @{$av->[0]}, \@av })->join();
threads->create(sub { $av[0] = 'testtest'})->join();
threads->create(sub { ok(11, $av->[0]->[0]->[0] eq 'testtest')})->join();






