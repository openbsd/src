require 5.004;

# $OpenBSD: Netmask.pm,v 1.1 2003/03/02 19:17:53 beck Exp $

# From version 1.9002, CPAN, Feb 2003.
# Copyright (C) 1998, 2001 David Muir Sharnoff. License hereby granted
# for anyone to use, modify or redistribute this module at their own
# risk. Please feed useful changes back to muir@idiom.com.

package Net::Netmask;

use vars qw($VERSION);
$VERSION = 1.9002;

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(findNetblock findOuterNetblock findAllNetblock
	cidrs2contiglists range2cidrlist sort_by_ip_address);
@EXPORT_OK = qw(int2quad quad2int %quadmask2bits imask);

my $remembered = {};
my %quadmask2bits;
my %imask2bits;
my %size2bits;

use vars qw($error $debug);
$debug = 1;

use strict;
use Carp;

sub new
{
	my ($package, $net, $mask) = @_;

	$mask = '' unless defined $mask;

	my $base;
	my $bits;
	my $ibase;
	undef $error;

	if ($net =~ m,^(\d+\.\d+\.\d+\.\d+)/(\d+)$,) {
		($base, $bits) = ($1, $2);
	} elsif ($net =~ m,^(\d+\.\d+\.\d+\.\d+)[:/](\d+\.\d+\.\d+\.\d+)$,) {
		$base = $1;
		my $quadmask = $2;
		if (exists $quadmask2bits{$quadmask}) {
			$bits = $quadmask2bits{$quadmask};
		} else {
			$error = "illegal netmask: $quadmask";
		}
	} elsif (($net =~ m,^\d+\.\d+\.\d+\.\d+$,)
		&& ($mask =~ m,\d+\.\d+\.\d+\.\d+$,)) 
	{
		$base = $net;
		if (exists $quadmask2bits{$mask}) {
			$bits = $quadmask2bits{$mask};
		} else {
			$error = "illegal netmask: $mask";
		}
	} elsif (($net =~ m,^\d+\.\d+\.\d+\.\d+$,) &&
		($mask =~ m,0x[a-z0-9]+,i)) 
	{
		$base = $net;
		my $imask = hex($mask);
		if (exists $imask2bits{$imask}) {
			$bits = $imask2bits{$imask};
		} else {
			$error = "illegal netmask: $mask ($imask)";
		}
	} elsif ($net =~ /^\d+\.\d+\.\d+\.\d+$/ && ! $mask) {
		($base, $bits) = ($net, 32);
	} elsif ($net =~ /^\d+\.\d+\.\d+$/ && ! $mask) {
		($base, $bits) = ("$net.0", 24);
	} elsif ($net =~ /^\d+\.\d+$/ && ! $mask) {
		($base, $bits) = ("$net.0.0", 16);
	} elsif ($net =~ /^\d+$/ && ! $mask) {
		($base, $bits) = ("$net.0.0.0", 8);
	} elsif ($net =~ m,^(\d+\.\d+\.\d+)/(\d+)$,) {
		($base, $bits) = ("$1.0", $2);
	} elsif ($net =~ m,^(\d+\.\d+)/(\d+)$,) {
		($base, $bits) = ("$1.0.0", $2);
	} elsif ($net eq 'default') {
		($base, $bits) = ("0.0.0.0", 0);
	} elsif ($net =~ m,^(\d+\.\d+\.\d+\.\d+)\s*-\s*(\d+\.\d+\.\d+\.\d+)$,) {
		# whois format
		$ibase = quad2int($1);
		my $end = quad2int($2);
		$error = "illegal dotted quad: $net" 
			unless defined($ibase) && defined($end);
		my $diff = ($end || 0) - ($ibase || 0) + 1;
		$bits = $size2bits{$diff};
		$error = "could not find exact fit for $net"
			if ! defined($bits) && ! defined($error);
	} else {
		$error = "could not parse $net";
		$error .= " $mask" if $mask;
	}

	carp $error if $error && $debug;

	$ibase = quad2int($base || 0) unless $ibase;
	unless (defined($ibase) || defined($error)) {
		$error = "could not parse $net";
		$error .= " $mask" if $mask;
	}
	$ibase &= imask($bits)
		if defined $ibase && defined $bits;

	return bless { 
		'IBASE' => $ibase,
		'BITS' => $bits, 
		( $error ? ( 'ERROR' => $error ) : () ),
	};
}

sub new2
{
	local($debug) = 0;
	my $net = new(@_);
	return undef if $error;
	return $net;
}

sub errstr { return $error; }
sub debug  { my $this = shift; return (@_ ? $debug = shift : $debug) }

sub base { my ($this) = @_; return int2quad($this->{'IBASE'}); }
sub bits { my ($this) = @_; return $this->{'BITS'}; }
sub size { my ($this) = @_; return 2**(32- $this->{'BITS'}); }
sub next { my ($this) = @_; int2quad($this->{'IBASE'} + $this->size()); }

sub broadcast 
{
	my($this) = @_;
	int2quad($this->{'IBASE'} + $this->size() - 1);
}

sub desc 
{ 
	my ($this) = @_; 
	return int2quad($this->{'IBASE'}).'/'.$this->{'BITS'};
}

sub imask 
{
	return (2**32 -(2** (32- $_[0])));
}

sub mask 
{
	my ($this) = @_;

	return int2quad ( imask ($this->{'BITS'}));
}

sub hostmask
{
	my ($this) = @_;

	return int2quad ( ~ imask ($this->{'BITS'}));
}

sub nth
{
	my ($this, $index, $bitstep) = @_;
	my $size = $this->size();
	my $ibase = $this->{'IBASE'};
	$bitstep = 32 unless $bitstep;
	my $increment = 2**(32-$bitstep);
	$index *= $increment;
	$index += $size if $index < 0;
	return undef if $index < 0;
	return undef if $index >= $size;
	return int2quad($ibase+$index);
}

sub enumerate
{
	my ($this, $bitstep) = @_;
	$bitstep = 32 unless $bitstep;
	my $size = $this->size();
	my $increment = 2**(32-$bitstep);
	my @ary;
	my $ibase = $this->{'IBASE'};
	for (my $i = 0; $i < $size; $i += $increment) {
		push(@ary, int2quad($ibase+$i));
	}
	return @ary;
}

sub inaddr
{
	my ($this) = @_;
	my $ibase = $this->{'IBASE'};
	my $blocks = int($this->size()/256);
	return (join('.',unpack('xC3', pack('V', $ibase))).".in-addr.arpa",
		$ibase%256, $ibase%256+$this->size()-1) if $blocks == 0;
	my @ary;
	for (my $i = 0; $i < $blocks; $i++) {
		push(@ary, join('.',unpack('xC3', pack('V', $ibase+$i*256)))
			.".in-addr.arpa", 0, 255);
	}
	return @ary;
}

sub quad2int
{
	my @bytes = split(/\./,$_[0]);

	return undef unless @bytes == 4 && ! grep {!(/\d+$/ && $_<256)} @bytes;

	return unpack("N",pack("C4",@bytes));
}

sub int2quad
{
	return join('.',unpack('C4', pack("N", $_[0])));
}

sub storeNetblock
{
	my ($this, $t) = @_;
	$t = $remembered unless $t;

	my $base = $this->{'IBASE'};

	$t->{$base} = [] unless exists $t->{$base};

	my $mb = maxblock($this);
	my $b = $this->{'BITS'};
	my $i = $b - $mb;

	$t->{$base}->[$i] = $this;
}

sub deleteNetblock
{
	my ($this, $t) = @_;
	$t = $remembered unless $t;

	my $base = $this->{'IBASE'};

	my $mb = maxblock($this);
	my $b = $this->{'BITS'};
	my $i = $b - $mb;

	return unless defined $t->{$base};

	undef $t->{$base}->[$i];

	for my $x (@{$t->{$base}}) {
		return if $x;
	}
	delete $t->{$base};
}

sub findNetblock
{
	my ($ipquad, $t) = @_;
	$t = $remembered unless $t;

	my $ip = quad2int($ipquad);

	for (my $b = 32; $b >= 0; $b--) {
		my $im = imask($b);
		my $nb = $ip & $im;
		next unless exists $t->{$nb};
		my $mb = imaxblock($nb, 32);
		my $i = $b - $mb;
		confess "$mb, $b, $ipquad, $nb" if $i < 0;
		confess "$mb, $b, $ipquad, $nb" if $i > 32;
		while ($i >= 0) {
			return $t->{$nb}->[$i]
				if defined $t->{$nb}->[$i];
			$i--;
		}
	}
}

sub findOuterNetblock
{
	my ($ipquad, $t) = @_;
	$t = $remembered unless $t;

	my $ip = quad2int($ipquad);

	for (my $b = 0; $b <= 32; $b++) {
		my $im = imask($b);
		my $nb = $ip & $im;
		next unless exists $t->{$nb};
		my $mb = imaxblock($nb, 32);
		my $i = $b - $mb;
		confess "$mb, $b, $ipquad, $nb" if $i < 0;
		confess "$mb, $b, $ipquad, $nb" if $i > 32;
		while ($i >= 0) {
			return $t->{$nb}->[$i]
				if defined $t->{$nb}->[$i];
			$i--;
		}
	}
}

sub findAllNetblock
{
	my ($ipquad, $t) = @_;
	$t = $remembered unless $t;
	my @ary ;
	my $ip = quad2int($ipquad);

	for (my $b = 32; $b >= 0; $b--) {
		my $im = imask($b);
		my $nb = $ip & $im;
		next unless exists $t->{$nb};
		my $mb = imaxblock($nb, 32);
		my $i = $b - $mb;
		confess "$mb, $b, $ipquad, $nb" if $i < 0;
		confess "$mb, $b, $ipquad, $nb" if $i > 32;
		while ($i >= 0) {
			push(@ary,  $t->{$nb}->[$i])
				if defined $t->{$nb}->[$i];
			$i--;
		}
	}
	return @ary;
}

sub match
{
	my ($this, $ip) = @_;
	my $i = quad2int($ip);
	my $imask = imask($this->{BITS});
	if (($i & $imask) == $this->{IBASE}) {
		return (($i & ~ $imask) || "0 ");
	} else {
		return 0;
	}
}

sub maxblock 
{ 
	my ($this) = @_;
	return imaxblock($this->{'IBASE'}, $this->{'BITS'});
}

sub imaxblock
{
	my ($ibase, $tbit) = @_;
	confess unless defined $ibase;
	while ($tbit > 0) {
		my $im = imask($tbit-1);
		last if (($ibase & $im) != $ibase);
		$tbit--;
	}
	return $tbit;
}

sub range2cidrlist
{
	my ($startip, $endip) = @_;

	my $start = quad2int($startip);
	my $end = quad2int($endip);

	($start, $end) = ($end, $start)
		if $start > $end;

	my @result;
	while ($end >= $start) {
		my $maxsize = imaxblock($start, 32);
		my $maxdiff = 32 - int(log($end - $start + 1)/log(2));
		$maxsize = $maxdiff if $maxsize < $maxdiff;
		push (@result, bless {
			'IBASE' => $start,
			'BITS' => $maxsize
		});
		$start += 2**(32-$maxsize);
	}
	return @result;
}

sub cidrs2contiglists
{
	my (@cidrs) = sort by_net_netmask_block @_;
	my @result;
	while (@cidrs) {
		my (@r) = shift(@cidrs);
		push(@r, shift(@cidrs))
			while $cidrs[0] && $r[$#r]->{'IBASE'} + $r[$#r]->size 
				== $cidrs[0]->{'IBASE'};
		push(@result, [@r]);
	}
	return @result;
}

sub by_net_netmask_block
{
	$a->{'IBASE'} <=> $b->{'IBASE'}
		|| $a->{'BITS'} <=> $b->{'BITS'};
}

sub sort_by_ip_address
{
	return sort { pack("C4",split(/\./,$a)) cmp pack("C4",split(/\./,$b)) } @_
}


BEGIN {
	for (my $i = 0; $i <= 32; $i++) {
		$imask2bits{imask($i)} = $i;
		$quadmask2bits{int2quad(imask($i))} = $i;
		$size2bits{ 2**(32-$i) } = $i;
	}
}
1;
