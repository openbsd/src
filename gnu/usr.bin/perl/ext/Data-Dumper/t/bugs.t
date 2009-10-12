#!perl
#
# regression tests for old bugs that don't fit other categories

BEGIN {
    if ($ENV{PERL_CORE}){
	chdir 't' if -d 't';
	unshift @INC, '../lib';
	require Config; import Config;
	no warnings 'once';
	if ($Config{'extensions'} !~ /\bData\/Dumper\b/) {
	    print "1..0 # Skip: Data::Dumper was not built\n";
	    exit 0;
	}
    }
}

use strict;
use Test::More tests => 5;
use Data::Dumper;

{
    sub iterate_hash {
	my ($h) = @_;
	my $count = 0;
	$count++ while each %$h;
	return $count;
    }

    my $dumper = Data::Dumper->new( [\%ENV], ['ENV'] )->Sortkeys(1);
    my $orig_count = iterate_hash(\%ENV);
    $dumper->Dump;
    my $new_count = iterate_hash(\%ENV);
    is($new_count, $orig_count, 'correctly resets hash iterators');
}

# [perl #38612] Data::Dumper core dump in 5.8.6, fixed by 5.8.7
sub foo {
     my $s = shift;
     local $Data::Dumper::Terse = 1;
     my $c = eval Dumper($s);
     sub bar::quote { }
     bless $c, 'bar';
     my $d = Data::Dumper->new([$c]);
     $d->Freezer('quote');
     return $d->Dump;
}
foo({});
ok(1, "[perl #38612]"); # Still no core dump? We are fine.

{
    my %h = (1,2,3,4);
    each %h;

    my $d = Data::Dumper->new([\%h]);
    $d->Useqq(1);
    my $txt = $d->Dump();
    my $VAR1;
    eval $txt;
    is_deeply($VAR1, \%h, '[perl #40668] Reset hash iterator'); 
}

# [perl #64744] Data::Dumper each() bad interaction
{
    local $Data::Dumper::Useqq = 1;
    my $a = {foo => 1, bar => 1};
    each %$a;
    $a = {x => $a};

    my $d = Data::Dumper->new([$a]);
    $d->Useqq(1);
    my $txt = $d->Dump();
    my $VAR1;
    eval $txt;
    is_deeply($VAR1, $a, '[perl #64744] Reset hash iterator'); 
}

# [perl #56766] Segfaults on bad syntax - fixed with version 2.121_17
sub doh
{
    # 2nd arg is supposed to be an arrayref
    my $doh = Data::Dumper->Dump([\@_],'@_');
}
doh('fixed');
ok(1, "[perl #56766]"); # Still no core dump? We are fine.

# EOF
