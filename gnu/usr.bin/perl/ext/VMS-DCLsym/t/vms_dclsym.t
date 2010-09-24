print "1..15\n";

require VMS::DCLsym or die "failed 1\n";
print "ok 1\n";

tie %syms, VMS::DCLsym or die "failed 2\n";
print "ok 2\n";

$name = 'FOO_'.time();
$syms{$name} = 'Perl_test';
print +($! ? "(\$! = $!) not " : ''),"ok 3\n";

print +($syms{$name} eq 'Perl_test' ? '' : 'not '),"ok 4\n";

($val) = `Show Symbol $name` =~ /(\w+)"$/;
print +($val eq 'Perl_test' ? '' : 'not '),"ok 5\n";

while (($sym,$val) = each %syms) {
  last if $sym eq $name && $val eq 'Perl_test';
}
print +($sym ? '' : 'not '),"ok 6\n";

delete $syms{$name};
print +($! ? "(\$! = $!) not " : ''),"ok 7\n";

print +(defined($syms{$name}) ? 'not ' : ''),"ok 8\n";
undef %syms;

$obj = new VMS::DCLsym 'GLOBAL';
print +($obj ? '' : 'not '),"ok 9\n";

print +($obj->clearcache(0) ? '' : 'not '),"ok 10\n";
print +($obj->clearcache(1) ? 'not ' : ''),"ok 11\n";

print +($obj->setsym($name,'Another_test') ? '' : 'not '),"ok 12\n";

($val,$tab) = $obj->getsym($name);
print +($val eq 'Another_test' && $tab eq 'GLOBAL' ? '' : 'not '),"ok 13\n";

print +($obj->delsym($name,'LOCAL') ? 'not ' : ''),"ok 14\n";
print +($obj->delsym($name,'GLOBAL') ? '' : 'not '),"ok 15\n";
