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

sub skip {
    my ($id, $ok, $name) = @_;
    print "ok $id # skip _thrcnt - $name \n";
}

use ExtUtils::testlib;
use strict;
BEGIN { print "1..36\n" };
use threads;
use threads::shared;

my ($hobj, $aobj, $sobj) : shared;

$hobj = &share({});
$aobj = &share([]);
my $sref = \do{ my $x };
share($sref);
$sobj = $sref;

threads->new(sub {
                # Bless objects
                bless $hobj, 'foo';
                bless $aobj, 'bar';
                bless $sobj, 'baz';

                # Add data to objects
                $$aobj[0] = bless(&share({}), 'yin');
                $$aobj[1] = bless(&share([]), 'yang');
                $$aobj[2] = $sobj;

                $$hobj{'hash'}   = bless(&share({}), 'yin');
                $$hobj{'array'}  = bless(&share([]), 'yang');
                $$hobj{'scalar'} = $sobj;

                $$sobj = 3;

                # Test objects in child thread
                ok(1, ref($hobj) eq 'foo', "hash blessing does work");
                ok(2, ref($aobj) eq 'bar', "array blessing does work");
                ok(3, ref($sobj) eq 'baz', "scalar blessing does work");
                ok(4, $$sobj eq '3', "scalar contents okay");

                ok(5, ref($$aobj[0]) eq 'yin', "blessed hash in array");
                ok(6, ref($$aobj[1]) eq 'yang', "blessed array in array");
                ok(7, ref($$aobj[2]) eq 'baz', "blessed scalar in array");
                ok(8, ${$$aobj[2]} eq '3', "blessed scalar in array contents");

                ok(9, ref($$hobj{'hash'}) eq 'yin', "blessed hash in hash");
                ok(10, ref($$hobj{'array'}) eq 'yang', "blessed array in hash");
                ok(11, ref($$hobj{'scalar'}) eq 'baz', "blessed scalar in hash");
                ok(12, ${$$hobj{'scalar'}} eq '3', "blessed scalar in hash contents");

             })->join;

# Test objects in parent thread
ok(13, ref($hobj) eq 'foo', "hash blessing does work");
ok(14, ref($aobj) eq 'bar', "array blessing does work");
ok(15, ref($sobj) eq 'baz', "scalar blessing does work");
ok(16, $$sobj eq '3', "scalar contents okay");

ok(17, ref($$aobj[0]) eq 'yin', "blessed hash in array");
ok(18, ref($$aobj[1]) eq 'yang', "blessed array in array");
ok(19, ref($$aobj[2]) eq 'baz', "blessed scalar in array");
ok(20, ${$$aobj[2]} eq '3', "blessed scalar in array contents");

ok(21, ref($$hobj{'hash'}) eq 'yin', "blessed hash in hash");
ok(22, ref($$hobj{'array'}) eq 'yang', "blessed array in hash");
ok(23, ref($$hobj{'scalar'}) eq 'baz', "blessed scalar in hash");
ok(24, ${$$hobj{'scalar'}} eq '3', "blessed scalar in hash contents");

threads->new(sub {
                # Rebless objects
                bless $hobj, 'oof';
                bless $aobj, 'rab';
                bless $sobj, 'zab';

                my $data = $$aobj[0];
                bless $data, 'niy';
                $$aobj[0] = $data;
                $data = $$aobj[1];
                bless $data, 'gnay';
                $$aobj[1] = $data;

                $data = $$hobj{'hash'};
                bless $data, 'niy';
                $$hobj{'hash'} = $data;
                $data = $$hobj{'array'};
                bless $data, 'gnay';
                $$hobj{'array'} = $data;

                $$sobj = 'test';
             })->join;

# Test reblessing
ok(25, ref($hobj) eq 'oof', "hash reblessing does work");
ok(26, ref($aobj) eq 'rab', "array reblessing does work");
ok(27, ref($sobj) eq 'zab', "scalar reblessing does work");
ok(28, $$sobj eq 'test', "scalar contents okay");

ok(29, ref($$aobj[0]) eq 'niy', "reblessed hash in array");
ok(30, ref($$aobj[1]) eq 'gnay', "reblessed array in array");
ok(31, ref($$aobj[2]) eq 'zab', "reblessed scalar in array");
ok(32, ${$$aobj[2]} eq 'test', "reblessed scalar in array contents");

ok(33, ref($$hobj{'hash'}) eq 'niy', "reblessed hash in hash");
ok(34, ref($$hobj{'array'}) eq 'gnay', "reblessed array in hash");
ok(35, ref($$hobj{'scalar'}) eq 'zab', "reblessed scalar in hash");
ok(36, ${$$hobj{'scalar'}} eq 'test', "reblessed scalar in hash contents");

