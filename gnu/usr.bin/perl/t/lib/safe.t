#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
    if ($Config{'extensions'} !~ /\bSafe\b/ && $^O ne 'VMS') {
        print "1..0\n";
        exit 0;
    }
}

use Safe qw(opname opcode ops_to_mask mask_to_ops);

print "1..23\n";

# Set up a package namespace of things to be visible to the unsafe code
$Root::foo = "visible";

# Stop perl from moaning about identifies which are apparently only used once
$Root::foo .= "";
$bar .= "";

$bar = "invisible";
$cpt = new Safe "Root";
$cpt->reval(q{
    system("echo not ok 1");
});
if ($@ =~ /^system trapped by operation mask/) {
    print "ok 1\n";
} else {
    print "not ok 1\n";
}

$cpt->reval(q{
    print $foo eq 'visible' ? "ok 2\n" : "not ok 2\n";
    print $main::foo  eq 'visible' ? "ok 3\n" : "not ok 3\n";
    print defined($bar) ? "not ok 4\n" : "ok 4\n";
    print defined($::bar) ? "not ok 5\n" : "ok 5\n";
    print defined($main::bar) ? "not ok 6\n" : "ok 6\n";
});
print $@ ? "not ok 7\n" : "ok 7\n";

$foo = "ok 8\n";
%bar = (key => "ok 9\n");
@baz = "o";
push(@baz, "10"); # Two steps to prevent "Identifier used only once..."
$glob = "ok 11\n";
@glob = qw(not ok 16);

$" = 'k ';

sub sayok12 { print "ok 12\n" }

$cpt->share(qw($foo %bar @baz *glob &sayok12 $"));

$cpt->reval(q{
    print $foo ? $foo : "not ok 8\n";
    print $bar{key} ? $bar{key} : "not ok 9\n";
    if (@baz) {
	print "@baz\n";
    } else {
	print "not ok 10\n";
    }
    print $glob;
    sayok12();
    $foo =~ s/8/14/;
    $bar{new} = "ok 15\n";
    @glob = qw(ok 16);
});
print $@ ? "not ok 13\n#$@" : "ok 13\n";
$" = ' ';
print $foo, $bar{new}, "@glob\n";

$Root::foo = "not ok 17";
@{$cpt->varglob('bar')} = qw(not ok 18);
${$cpt->varglob('foo')} = "ok 17";
@Root::bar = "ok";
push(@Root::bar, "18"); # Two steps to prevent "Identifier used only once..."

print "$Root::foo\n";
print "@{$cpt->varglob('bar')}\n";

print opname(23) eq "bless" ? "ok 19\n" : "not ok 19\n";
print opcode("bless") == 23 ? "ok 20\n" : "not ok 20\n";

$m1 = $cpt->mask();
$cpt->trap("negate");
$m2 = $cpt->mask();
@masked = mask_to_ops($m1);
print $m2 eq ops_to_mask("negate", @masked) ? "ok 21\n" : "not ok 21\n";
$cpt->untrap(187);
substr($m2, 187, 1) = "\0";
print $m2 eq $cpt->mask() ? "ok 22\n" : "not ok 22\n";

print $cpt->reval("2 + 2") == 4 ? "ok 23\n" : "not ok 23\n";
