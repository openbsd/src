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
use Test::More tests => 1;
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
