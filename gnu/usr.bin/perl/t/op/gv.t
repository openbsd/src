#!./perl

#
# various typeglob tests
#

print "1..11\n";

# type coersion on assignment
$foo = 'foo';
$bar = *main::foo;
$bar = $foo;
print ref(\$bar) eq 'SCALAR' ? "ok 1\n" : "not ok 1\n";
$foo = *main::bar;

# type coersion (not) on misc ops

if ($foo) {
  print ref(\$foo) eq 'GLOB' ? "ok 2\n" : "not ok 2\n";
}

unless ($foo =~ /abcd/) {
  print ref(\$foo) eq 'GLOB' ? "ok 3\n" : "not ok 3\n";
}

if ($foo eq '*main::bar') {
  print ref(\$foo) eq 'GLOB' ? "ok 4\n" : "not ok 4\n";
}

# type coersion on substitutions that match
$a = *main::foo;
$b = $a;
$a =~ s/^X//;
print ref(\$a) eq 'GLOB' ? "ok 5\n" : "not ok 5\n";
$a =~ s/^\*//;
print $a eq 'main::foo' ? "ok 6\n" : "not ok 6\n";
print ref(\$b) eq 'GLOB' ? "ok 7\n" : "not ok 7\n";

# typeglobs as lvalues
substr($foo, 0, 1) = "XXX";
print ref(\$foo) eq 'SCALAR' ? "ok 8\n" : "not ok 8\n";
print $foo eq 'XXXmain::bar' ? "ok 9\n" : "not ok 9\n";

# returning glob values
sub foo {
  local($bar) = *main::foo;
  $foo = *main::bar;
  return ($foo, $bar);
}

($fuu, $baa) = foo();
if (defined $fuu) {
  print ref(\$fuu) eq 'GLOB' ? "ok 10\n" : "not ok 10\n";
}

if (defined $baa) {
  print ref(\$baa) eq 'GLOB' ? "ok 11\n" : "not ok 11\n";
}

