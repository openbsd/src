#!./perl -w

#
# test auto defined() test insertion
#

BEGIN {
    chdir 't' if -d 't';
    @INC = qw(. ../lib);
    $SIG{__WARN__} = sub { $warns++; warn $_[0] };
}
require 'test.pl';
plan( tests => 19 );

my $unix_mode = 1;

if ($^O eq 'VMS') {
    # We have to know if VMS is in UNIX mode.  In UNIX mode, trailing dots
    # should not be present.  There are actually two settings that control this.

    $unix_mode = 0;
    my $unix_rpt = 0;
    my $drop_dot = 0;
    if (eval 'require VMS::Feature') {
        $unix_rpt = VMS::Feature::current('filename_unix_report');
        $drop_dot = VMS::Feature::current('readdir_dropdotnotype');
    } else {
        my $unix_report = $ENV{'DECC$FILENAME_UNIX_REPORT'} || '';
        $unix_rpt = $unix_report =~ /^[ET1]/i; 
        my $drop_dot_notype = $ENV{'DECC$READDIR_DROPDOTNOTYPE'} || '';
        $drop_dot = $drop_dot_notype =~ /^[ET1]/i;
    }
    $unix_mode = 1 if $drop_dot && unix_rpt;
}

$wanted_filename = $unix_mode ? '0' : '0.';
$saved_filename = './0';

cmp_ok($warns,'==',0,'no warns at start');

open(FILE,">$saved_filename");
ok(defined(FILE),'created work file');
print FILE "1\n";
print FILE "0";
close(FILE);

open(FILE,"<$saved_filename");
ok(defined(FILE),'opened work file');
my $seen = 0;
my $dummy;
while (my $name = <FILE>)
 {
  $seen++ if $name eq '0';
 }
cmp_ok($seen,'==',1,'seen in while()');

seek(FILE,0,0);
$seen = 0;
my $line = '';
do
 {
  $seen++ if $line eq '0';
 } while ($line = <FILE>);
cmp_ok($seen,'==',1,'seen in do/while');

seek(FILE,0,0);
$seen = 0;
while (($seen ? $dummy : $name) = <FILE> )
 {
  $seen++ if $name eq '0';
 }
cmp_ok($seen,'==',1,'seen in while() ternary');

seek(FILE,0,0);
$seen = 0;
my %where;
while ($where{$seen} = <FILE>)
 {
  $seen++ if $where{$seen} eq '0';
 }
cmp_ok($seen,'==',1,'seen in hash while()');
close FILE;

opendir(DIR,'.');
ok(defined(DIR),'opened current directory');
$seen = 0;
while (my $name = readdir(DIR))
 {
  $seen++ if $name eq $wanted_filename;
 }
cmp_ok($seen,'==',1,'saw work file once');

rewinddir(DIR);
$seen = 0;
$dummy = '';
while (($seen ? $dummy : $name) = readdir(DIR))
 {
  $seen++ if $name eq $wanted_filename;
 }
cmp_ok($seen,'>',0,'saw file in while() ternary');

rewinddir(DIR);
$seen = 0;
while ($where{$seen} = readdir(DIR))
 {
  $seen++ if $where{$seen} eq $wanted_filename;
 }
cmp_ok($seen,'==',1,'saw file in hash while()');

$seen = 0;
while (my $name = glob('*'))
 {
  $seen++ if $name eq $wanted_filename;
 }
cmp_ok($seen,'==',1,'saw file in glob while()');

$seen = 0;
$dummy = '';
while (($seen ? $dummy : $name) = glob('*'))
 {
  $seen++ if $name eq $wanted_filename;
 }
cmp_ok($seen,'>',0,'saw file in glob hash while() ternary');

$seen = 0;
while ($where{$seen} = glob('*'))
 {
  $seen++ if $where{$seen} eq $wanted_filename;
 }
cmp_ok($seen,'==',1,'seen in glob hash while()');

unlink($saved_filename);
ok(!(-f $saved_filename),'work file unlinked');

my %hash = (0 => 1, 1 => 2);

$seen = 0;
while (my $name = each %hash)
 {
  $seen++ if $name eq '0';
 }
cmp_ok($seen,'==',1,'seen in each');

$seen = 0;
$dummy = '';
while (($seen ? $dummy : $name) = each %hash)
 {
  $seen++ if $name eq '0';
 }
cmp_ok($seen,'==',1,'seen in each ternary');

$seen = 0;
while ($where{$seen} = each %hash)
 {
  $seen++ if $where{$seen} eq '0';
 }
cmp_ok($seen,'==',1,'seen in each hash');

cmp_ok($warns,'==',0,'no warns at finish');
