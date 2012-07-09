# $OpenBSD: Getopt.pm,v 1.7 2012/07/09 17:53:15 espie Exp $

# Copyright (c) 2012 Marc Espie <espie@openbsd.org>
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
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

use strict;
use warnings;

package Option;
sub factory
{
	my ($class, $_) = @_;
	if (m/^(.)$/) {
		return Option::Short->new($1);
	} elsif (m/^(.)\:$/) {
		return Option::ShortArg->new($1);
	} elsif (m/^(\-?.*)\=$/) {
		return Option::LongArg->new($1);
	} elsif (m/^(\-?.*)$/) {
		return Option::Long->new($1);
	}
}

sub new
{
	my ($class, $v) = @_;
	bless \$v, $class;
}

sub setup
{
	my ($self, $opts, $isarray) = @_;
	$opts->add_option_accessor($$self, $isarray);
	return $self;
}

package Option::Short;
our @ISA = qw(Option);

sub match
{
	my ($self, $_, $opts, $canonical, $code) = @_;
	if (m/^\-\Q$$self\E$/) {
		&$code($opts, $canonical, 1);
		return 1;
	}
	if (m/^\-\Q$$self\E(.*)$/) {
		unshift(@main::ARGV, "-$1");
		&$code($opts, $canonical, 1);
		return 1;
	}
	return 0;
}

package Option::ShortArg;
our @ISA = qw(Option::Short);

sub match
{
	my ($self, $_, $opts, $canonical, $code) = @_;
	if (m/^\-\Q$$self\E$/) {
		&$code($opts, $canonical, (shift @main::ARGV));
		return 1;
	}
	if (m/^\-\Q$$self\E(.*)$/) {
		&$code($opts, $canonical, $1);
		return 1;
	}
	return 0;
}

package Option::Long;
our @ISA = qw(Option);

sub match
{
	my ($self, $_, $opts, $canonical, $code) = @_;
	if (m/^\-\Q$$self\E$/) {
		&$code($opts, $canonical, 1);
		return 1;
	}
	return 0;
}

package Option::LongArg;
our @ISA = qw(Option::Long);

sub match
{
	my ($self, $_, $opts, $canonical, $code) = @_;
	if (m/^\-\Q$$self\E$/) {
		if (@main::ARGV > 0) {
			&$code($opts, $canonical, (shift @main::ARGV));
			return 1;
		} else {
			die "Missing argument  for option -$$self\n";
		}
	}
	if (m/^-\Q$$self\E\=(.*)$/) {
		&$code($opts, $canonical, $1);
		return 1;
	}
	return 0;
}

package Options;

sub new
{
	my ($class, $string, $code) = @_;

	my @alternates = split(/\|/, $string);

	bless {alt => [map { Option->factory($_); } @alternates], code => $code}, $class;
}

sub setup
{
	my ($self, $allopts, $isarray) = @_;
	$self->{alt}[0]->setup($allopts, $isarray);
	return $self;
}

sub match
{
	my ($self, $arg, $opts) = @_;

	my $canonical = ${$self->{alt}[0]};
	for my $s (@{$self->{alt}}) {
		if ($s->match($arg, $opts, $canonical, $self->{code})) {
			return 1;
		}
	}
	return 0;
}

# seems I spend my life rewriting option handlers, not surprisingly...
package LT::Getopt;
use LT::Util;


# parsing an option 'all-static' will automatically add an
# accessor $self->all_static   that maps to the option.

sub add_option_accessor
{
	my ($self, $option, $isarray) = @_;
	my $access = $option;
	$access =~ s/^\-//;
	$access =~ s/-/_/g;
	my $actual = $isarray ? 
		sub {
		    my $self = shift;
		    $self->{opt}{$option} //= [];
		    if (wantarray) {
			    return @{$self->{opt}{$option}};
		    } else {
			    return scalar @{$self->{opt}{$option}};
		    }
		} : sub {
		    my $self = shift;
		    return $self->{opt}{$option};
		};
	my $callpkg = ref($self);
	unless ($self->can($access)) {
		no strict 'refs';
		*{$callpkg."::$access"} = $actual;
	}
}

sub create_options
{
	my ($self, @l) = @_;
	my @options = ();
	# first pass creates accessors
	while (my $opt = shift @l) {
		my $isarray = ($opt =~ s/\@$//);
		# default code or not
		my $code;
		if (@l > 0 && ref($l[0]) eq 'CODE') {
			$code = shift @l;
		} else {
			if ($isarray) {
				$code = sub {
				    my ($object, $canonical, $value) = @_;
				    push(@{$object->{opt}{$canonical}}, $value);
				};
			} else {
				$code = sub {
				    my ($object, $canonical, $value) = @_;
				    $object->{opt}{$canonical} = $value;
				};
			}
		}
		push(@options, Options->new($opt, $code)->setup($self, $isarray));
	}
	return @options;
}

sub handle_options
{
	my ($self, @l) = @_;

	my @options = $self->create_options(@l);

MAINLOOP:
	while (@main::ARGV > 0) {
		my $_ = shift @main::ARGV;
		if (m/^\-\-$/) {
			last;
		}
		if (m/^\-/) {
			for my $opt (@options) {
				if ($opt->match($_, $self)) {
					next MAINLOOP;
				}
			}
			shortdie "Unknown option $_\n";
		} else {
			unshift(@main::ARGV, $_);
			last;
		}
	}
}

sub handle_permuted_options
{
	my ($self, @l) = @_;

	my @options = $self->create_options(@l);

	my @kept = ();
MAINLOOP2:
	while (@main::ARGV > 0) {
		my $_ = shift @main::ARGV;
		if (m/^\-\-$/) {
			next;   # XXX ?
		}
		if (m/^\-/) {
			for my $opt (@options) {
				if ($opt->match($_, $self)) {
					next MAINLOOP2;
				}
			}
		}
		push(@kept, $_);
	}
	@main::ARGV = @kept;
}

sub new
{
	my $class = shift;
	bless {}, $class;
}

1;
