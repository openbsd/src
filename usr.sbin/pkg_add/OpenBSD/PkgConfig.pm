# ex:ts=8 sw=4:
# $OpenBSD: PkgConfig.pm,v 1.4 2006/11/30 00:07:50 espie Exp $
#
# Copyright (c) 2006 Marc Espie <espie@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#
use strict;
use warnings;

# this is a 'special' package, interface to the *.pc file format of pkg-config.
package OpenBSD::PkgConfig;

sub new
{
	my $class = shift;

	return bless { 
		variables => {},  
		vlist => [], 
		properties => {},
		proplist => []
	}, $class;
}

sub add_variable
{
	my ($self, $name, $value) = @_;
	if (defined $self->{variables}->{$name}) {
		die "Duplicate variable $name";
	}
	push(@{$self->{vlist}}, $name);
	$self->{variables}->{$name} = $value;
}

sub add_property
{
	my ($self, $name, $value) = @_;
	if (defined $self->{properties}->{$name}) {
		die "Duplicate property $name";
	}
	push(@{$self->{proplist}}, $name);
	if (defined $value) {
		$self->{properties}->{$name} = [split /\s+/, $value] ;
	} else {
		$self->{properties}->{$name} = [];
	}
}

sub read_fh
{
	my ($class, $fh, $name) = @_;
	my $cfg = $class->new;
	local $_;

	$name = '' if !defined $name;
	while (<$fh>) {
		chomp;
		next if m/^\s*$/;
		next if m/^\#/;
		if (m/^(.*?)\=(.*)$/) {
			$cfg->add_variable($1, $2);
		} elsif (m/^(.*?)\:\s+(.*)$/) {
			$cfg->add_property($1, $2);
		} elsif (m/^(.*?)\:\s*$/) {
			$cfg->add_property($1);
		} else {
			die "Incorrect cfg file $name";
		}
	}
	return $cfg;
}

sub read_file
{
	my ($class, $filename) = @_;

	open my $fh, '<:crlf', $filename or die "Can't open $filename: $!";
	return $class->read_fh($fh, $filename);
}

sub write_fh
{
	my ($self, $fh) = @_;

	foreach my $variable (@{$self->{vlist}}) {
		print $fh "$variable=", $self->{variables}->{$variable}, "\n";
	}
	print $fh "\n\n";
	foreach my $property (@{$self->{proplist}}) {
		print $fh "$property:", 
			(map { " $_" } @{$self->{properties}->{$property}}), 
			"\n";
	}
}

sub write_file
{
	my ($cfg, $filename) = @_;
	open my $fh, '>', $filename or die "Can't open $filename: $!";
	$cfg->write_fh($fh);
}

sub compress_list
{
	my ($class, $l, $keep) = @_;
	my $h = {};
	my $r = [];
	foreach my $i (@$l) {
		next if defined $h->{$i};
		next if defined $keep && !&$keep($i);
		push(@$r, $i);
		$h->{$i} = 1;
	}
	return $r;
}

sub compress
{
	my ($class, $l, $keep) = @_;
	return join(' ', @{$class->compress_list($l, $keep)});
}

sub expanded
{
	my ($self, $v, $extra) = @_;

	$extra = {} if !defined $extra;
	my $get_value = 
		sub {
			my $var = shift;
			if (defined $extra->{$var}) {
				return $extra->{$var};
			} elsif (defined $self->{variables}->{$var}) {
				return $self->{variables}->{$var};
			} else {
				return '';
			}
	};

	while ($v =~ s/\$\{(.*?)\}/&$get_value($1)/ge) {
	}
	return $v;
}

sub get_property
{
	my ($self, $k, $extra) = @_;

	my $l = $self->{properties}->{$k};
	if (!defined $l) {
		return undef;
	}
	my $r = [];
	for my $v (@$l) {
		push(@$r, $self->expanded($v, $extra));
	}
	return $r;
}

sub get_variable
{
	my ($self, $k, $extra) = @_;

	my $v = $self->{variables}->{$k};
	if (defined $v) {
		return $self->expanded($v, $extra);
	} else {
		return undef;
	}
}

# to be used to make sure a config does not depend on absolute path names,
# e.g., $cfg->add_bases(X11R6 => '/usr/X11R6');

sub add_bases
{
	my ($self, $extra) = @_;

	while (my ($k, $v) = each %$extra) {
		for my $name (keys %{$self->{variables}}) {
			$self->{variables}->{$name} =~ s/\Q$v\E\b/\$\{\Q$k\E\}/g;
		}
		for my $name (keys %{$self->{properties}}) {
			for my $e (@{$self->{properties}->{$name}}) {
				$e =~ s/\Q$v\E\b/\$\{\Q$k\E\}/g;
			}
		}
		$self->{variables}->{$k} = $v;
		unshift(@{$self->{vlist}}, $k);
	}
}

1;
