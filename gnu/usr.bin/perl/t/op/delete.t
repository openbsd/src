#!./perl

# $RCSfile: delete.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:44 $

print "1..7\n";

$foo{1} = 'a';
$foo{2} = 'b';
$foo{3} = 'c';

$foo = delete $foo{2};

if ($foo eq 'b') {print "ok 1\n";} else {print "not ok 1 $foo\n";}
if ($foo{2} eq '') {print "ok 2\n";} else {print "not ok 2 $foo{2}\n";}
if ($foo{1} eq 'a') {print "ok 3\n";} else {print "not ok 3\n";}
if ($foo{3} eq 'c') {print "ok 4\n";} else {print "not ok 4\n";}

$foo = join('',values(foo));
if ($foo eq 'ac' || $foo eq 'ca') {print "ok 5\n";} else {print "not ok 5\n";}

foreach $key (keys foo) {
    delete $foo{$key};
}

$foo{'foo'} = 'x';
$foo{'bar'} = 'y';

$foo = join('',values(foo));
if ($foo eq 'xy' || $foo eq 'yx') {print "ok 6\n";} else {print "not ok 6\n";}

$refhash{"top"}->{"foo"} = "FOO";
$refhash{"top"}->{"bar"} = "BAR";

delete $refhash{"top"}->{"bar"};
@list = keys %{$refhash{"top"}};

print "@list" eq "foo" ? "ok 7\n" : "not ok 7 @list\n";
