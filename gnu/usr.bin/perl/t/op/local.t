#!./perl

# $RCSfile: local.t,v $$Revision: 4.1 $$Date: 92/08/07 18:28:04 $

print "1..23\n";

sub foo {
    local($a, $b) = @_;
    local($c, $d);
    $c = "ok 3\n";
    $d = "ok 4\n";
    { local($a,$c) = ("ok 9\n", "ok 10\n"); ($x, $y) = ($a, $c); }
    print $a, $b;
    $c . $d;
}

$a = "ok 5\n";
$b = "ok 6\n";
$c = "ok 7\n";
$d = "ok 8\n";

print &foo("ok 1\n","ok 2\n");

print $a,$b,$c,$d,$x,$y;

# same thing, only with arrays and associative arrays

sub foo2 {
    local($a, @b) = @_;
    local(@c, %d);
    @c = "ok 13\n";
    $d{''} = "ok 14\n";
    { local($a,@c) = ("ok 19\n", "ok 20\n"); ($x, $y) = ($a, @c); }
    print $a, @b;
    $c[0] . $d{''};
}

$a = "ok 15\n";
@b = "ok 16\n";
@c = "ok 17\n";
$d{''} = "ok 18\n";

print &foo2("ok 11\n","ok 12\n");

print $a,@b,@c,%d,$x,$y;

eval 'local($$e)';
print +($@ =~ /Can't localize through a reference/) ? "" : "not ", "ok 21\n";

eval 'local(@$e)';
print +($@ =~ /Can't localize through a reference/) ? "" : "not ", "ok 22\n";

eval 'local(%$e)';
print +($@ =~ /Can't localize through a reference/) ? "" : "not ", "ok 23\n";
