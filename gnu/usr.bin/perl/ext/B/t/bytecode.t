#!./perl
my $keep_plc      = 0;	# set it to keep the bytecode files
my $keep_plc_fail = 1;	# set it to keep the bytecode files on failures

BEGIN {
    if ($^O eq 'VMS') {
       print "1..0 # skip - Bytecode/ByteLoader doesn't work on VMS\n";
       exit 0;
    }
    if ($ENV{PERL_CORE}){
	chdir('t') if -d 't';
	@INC = ('.', '../lib');
    } else {
	unshift @INC, 't';
	push @INC, "../../t";
    }
    use Config;
    if (($Config{'extensions'} !~ /\bB\b/) ){
        print "1..0 # Skip -- Perl configured without B module\n";
        exit 0;
    }
    if ($Config{ccflags} =~ /-DPERL_COPY_ON_WRITE/) {
	print "1..0 # skip - no COW for now\n";
	exit 0;
    }
    require 'test.pl'; # for run_perl()
}
use strict;

undef $/;
my @tests = split /\n###+\n/, <DATA>;

print "1..".($#tests+1)."\n";

my $cnt = 1;
my $test;

for (@tests) {
    my $got;
    my ($script, $expect) = split />>>+\n/;
    $expect =~ s/\n$//;
    $test = "bytecode$cnt.pl";
    open T, ">$test"; print T $script; close T;
    $got = run_perl(switches => [ "-MO=Bytecode,-H,-o${test}c" ],
		    verbose  => 0, # for debugging
		    stderr   => 1, # to capture the "bytecode.pl syntax ok"
		    progfile => $test);
    unless ($?) {
	$got = run_perl(progfile => "${test}c"); # run the .plc
	unless ($?) {
	    if ($got =~ /^$expect$/) {
		print "ok $cnt\n";
		next;
	    } else {
		$keep_plc = $keep_plc_fail unless $keep_plc;
		print <<"EOT"; next;
not ok $cnt
--------- SCRIPT
$script
--------- GOT
$got
--------- EXPECT
$expect
----------------

EOT
	    }
	}
    }
    print <<"EOT";
not ok $cnt
--------- SCRIPT
$script
--------- \$\? = $?
$got
EOT
} continue {
    1 while unlink($test, $keep_plc ? () : "${test}c");
    $cnt++;
}

__DATA__

print 'hi'
>>>>
hi
############################################################
for (1,2,3) { print if /\d/ }
>>>>
123
############################################################
$_ = "xyxyx"; %j=(1,2); s/x/$j{print('z')}/ge; print $_
>>>>
zzz2y2y2
############################################################
$_ = "xyxyx"; %j=(1,2); s/x/$j{print('z')}/g; print $_
>>>>
z2y2y2
############################################################
split /a/,"bananarama"; print @_
>>>>
bnnrm
############################################################
{ package P; sub x { print 'ya' } x }
>>>>
ya
############################################################
@z = split /:/,"b:r:n:f:g"; print @z
>>>>
brnfg
############################################################
sub AUTOLOAD { print 1 } &{"a"}()
>>>>
1
############################################################
my $l = 3; $x = sub { print $l }; &$x
>>>>
3
############################################################
my $i = 1;
my $foo = sub {$i = shift if @_};
&$foo(3);
print 'ok';
>>>>
ok
############################################################
$x="Cannot use"; print index $x, "Can"
>>>>
0
############################################################
my $i=6; eval "print \$i\n"
>>>>
6
############################################################
BEGIN { %h=(1=>2,3=>4) } print $h{3}
>>>>
4
############################################################
open our $T,"a";
print 'ok';
>>>>
ok
############################################################
print <DATA>
__DATA__
a
b
>>>>
a
b
############################################################
BEGIN { tie @a, __PACKAGE__; sub TIEARRAY { bless{} } sub FETCH { 1 } }
print $a[1]
>>>>
1
############################################################
my $i=3; print 1 .. $i
>>>>
123
############################################################
my $h = { a=>3, b=>1 }; print sort {$h->{$a} <=> $h->{$b}} keys %$h
>>>>
ba
############################################################
print sort { my $p; $b <=> $a } 1,4,3
>>>>
431
