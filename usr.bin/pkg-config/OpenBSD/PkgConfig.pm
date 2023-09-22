# ex:ts=8 sw=4:
# $OpenBSD: PkgConfig.pm,v 1.11 2023/09/22 07:29:14 espie Exp $
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

use v5.36;


# interface to the *.pc file format of pkg-config.
package OpenBSD::PkgConfig;

# specific properties may have specific needs.

my $parse = {
	Requires => sub($req) {
	    my @l = split(/[,\s]+/, $req);
	    my @r = ();
	    while (@l > 0) {
		    my $n = shift @l;
		    if ($n =~ m/[<=>]+$/) {
			    if (@l > 0) {
				    $n .= shift @l;
			    }
		    }
		    if ($n =~ m/^[<=>]+/) {
			    if (@r > 0) {
				    $n = (pop @r).$n;
			    }
		    }
		    push(@r, $n);
	    }
	    return \@r;
	},
};


my $write = {
	Libs => sub($arg) { " ".__PACKAGE__->compress($arg) }
};

$parse->{'Requires.private'} = $parse->{Requires};
$write->{'Libs.private'} = $write->{Libs};

sub new($class)
{
	return bless {
		variables => {},
		vlist => [],
		properties => {},
		proplist => []
	}, $class;
}

sub add_variable($self, $name, $value)
{
	if (defined $self->{variables}{$name}) {
		die "Duplicate variable $name";
	}
	push(@{$self->{vlist}}, $name);
	$self->{variables}{$name} = ($value =~ s/^\"|\"$//rg);
}

sub parse_value($self, $name, $value)
{
	my $class = "OpenBSD::PkgConfig::NoExpand";
	if ($value =~ m/\$\{.*\}/) {
		$class = "OpenBSD::PkgConfig::ToExpand";
	}
	if (defined $parse->{$name}) {
		return bless $parse->{$name}($value), $class;
	} else {
		return bless [split /(?<!\\)\s+/o, $value], $class;
	}
}

sub add_property($self, $name, $value)
{
	if (defined $self->{properties}{$name}) {
		die "Duplicate property $name";
	}
	push(@{$self->{proplist}}, $name);
	my $v;
	if (defined $value) {
		$v = $self->parse_value($name, $value);
	} else {
		$v = bless [], "OpenBSD::PkgConfig::NoExpand";
	}
	$self->{properties}{$name} = $v;
}

sub read_fh($class, $fh, $name = '')
{
	my $cfg = $class->new;

	while (<$fh>) {
		chomp;
		# continuation lines
		while (m/(?<!\\)\\$/) {
			s/\\$//;
			$_.=<$fh>;
			chomp;
		}
		next if m/^\s*$/;
		next if m/^\#/;
		# zap comments
		s/(?<!\\)\#.*//;
		if (m/^([\w.]*)\s*\=\s*(.*)$/) {
			$cfg->add_variable($1, $2);
		} elsif (m/^([\w.]*)\:\s*(.*)$/) {
			$cfg->add_property($1, $2);
		} elsif (m/^([\w.]*)\:\s*$/) {
			$cfg->add_property($1);
		} else {
			die "Incorrect cfg file $name";
		}
	}
	if (defined $cfg->{properties}{Libs}) {
		$cfg->{properties}{Libs} = bless
		    $cfg->compress_list($cfg->{properties}{Libs}),
		    ref($cfg->{properties}{Libs});
	}
	return $cfg;
}

sub read_file($class, $filename)
{
	open my $fh, '<:crlf', $filename or die "Can't open $filename: $!";
	return $class->read_fh($fh, $filename);
}

sub write_fh($self, $fh)
{
	foreach my $variable (@{$self->{vlist}}) {
		say $fh "$variable=", $self->{variables}{$variable};
	}
	print $fh "\n\n";
	foreach my $property (@{$self->{proplist}}) {
		my $p = $self->{properties}{$property};
		print $fh "$property:";
		if (defined $write->{$property}) {
			print $fh $write->{$property}($p);
		} else {
			print $fh (map { " $_" } @$p);
		}
	    	print $fh "\n";
	}
}

sub write_file($cfg, $filename)
{
	open my $fh, '>', $filename or die "Can't open $filename: $!";
	$cfg->write_fh($fh);
}

sub compress_list($class, $l, $keep = undef)
{
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

sub compress($class, $l, $keep = undef)
{
	return join(' ', @{$class->compress_list($l, $keep)});
}

sub rcompress($class, $l, $keep = undef)
{
	my @l2 = reverse @$l;
	return join(' ', reverse @{$class->compress_list(\@l2, $keep)});
}

sub expanded($self, $v, $extra = {})
{
	my $get_value =
	    sub($var) {
		if (defined $extra->{$var}) {
			if ($extra->{$var} =~ m/\$\{.*\}/ ) {
				return undef;
			} else {
				return $extra->{$var};
			}
		} elsif (defined $self->{variables}{$var}) {
			return $self->{variables}{$var};
		} else {
			return '';
		}
	    };

	# Expand all variables, unless the returned value is defined as an
	# as an unexpandable variable (such as with --defined-variable).
	while ($v =~ m/\$\{(.*?)\}/) {
		# Limit the expanded variable size if 64K to prevent a
		# malicious .pc file from consuming too much memory.
		die "Variable expansion overflow" if length($v) > 64 * 1024;

		unless (defined &$get_value($1)) {
			$v =~ s/\$\{(.*?)\}/$extra->{$1}/g;
			last;
		}
		$v =~ s/\$\{(.*?)\}/&$get_value($1)/ge;
	}
	return $v;
}

sub get_property($self, $k, $extra = {})
{
	my $l = $self->{properties}{$k};
	if (!defined $l) {
		return undef;
	}
	if ($l->noexpand) {
		return [@$l];
	}
	my $r = [];
	for my $v (@$l) {
		my $w = $self->expanded($v, $extra);
		# Optimization: don't bother reparsing if value didn't change
		if ($w ne $v) {
			my $l = $self->parse_value($k, $w);
			push(@$r, @$l);
		} else {
			push(@$r, $w);
		}
	}
	return $r;
}

sub get_variable($self, $k, $extra = {})
{
	my $v = $self->{variables}{$k};
	if (defined $v) {
		return $self->expanded($v, $extra);
	} else {
		return undef;
	}
}

# to be used to make sure a config does not depend on absolute path names,
# e.g., $cfg->add_bases(X11R6 => '/usr/X11R6');

sub add_bases($self, $extra)
{
	while (my ($k, $v) = each %$extra) {
		for my $name (keys %{$self->{variables}}) {
			$self->{variables}{$name} =~ s/\Q$v\E\b/\$\{\Q$k\E\}/g;
		}
		for my $name (keys %{$self->{properties}}) {
			for my $e (@{$self->{properties}{$name}}) {
				$e =~ s/\Q$v\E\b/\$\{\Q$k\E\}/g;
			}
		}
		$self->{variables}{$k} = $v;
		unshift(@{$self->{vlist}}, $k);
	}
}

package OpenBSD::PkgConfig::NoExpand;
our @ISA = qw(OpenBSD::PkgConfig);
sub noexpand($)
{
	1
}

package OpenBSD::PkgConfig::ToExpand;
our @ISA = qw(OpenBSD::PkgConfig);
sub noexpand($)
{
	0
}
1;
