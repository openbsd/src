# $OpenBSD: Exec.pm,v 1.6 2023/07/06 08:29:26 espie Exp $

# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
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

use v5.36;

package LT::Exec;
use LT::Trace;
use LT::Util;

# OO singleton
my $dry = 0;
my $verbose = 0;
my $performed = 0;

sub performed($)
{
	return $performed;
}

sub dry_run($)
{
	$dry = 1;
}

sub verbose_run($)
{
	$verbose = 1;
}

sub silent_run($)
{
	$verbose = 0;
}

sub new($class)
{
	bless {}, $class;
}

sub chdir($self, $dir)
{
	my $class = ref($self) || $self;
	bless {dir => $dir}, $class;
}

sub compile($self, @l)
{
	$self->command("compile", @l);	
}

sub execute($self, @l)
{
	$self->command("execute", @l);
}

sub install($self, @l)
{
	$self->command("install", @l);
}

sub link($self, @l)
{
	$self->command("link", @l);
}

sub command_run($self, @l)
{
	if ($self->{dir}) {
		tprint {"cd $self->{dir} && "};
	}
	tsay { "@l" };
	my $pid = fork();
	if (!defined $pid) {
		die "Couldn't fork while running @l\n";
	}
	if ($pid == 0) {
		if ($self->{dir}) {
			CORE::chdir($self->{dir}) or die "Can't chdir to $self->{dir}\n";
		}
		exec(@l);
		die "Exec failed @l\n";
	} else {
		my $kid = waitpid($pid, 0);
		if ($? != 0) {
			shortdie "Error while executing @l\n";
		}
	}
}

sub shell($self, @cmds)
{
	# create an object "on the run"
	if (!ref($self)) {
		$self = $self->new;
	}
	for my $c (@cmds) {
		say $c if $verbose || $dry;
		if (!$dry) {
			$self->command_run($c);
	        }
	}
	$performed++;
}

sub command($self, $mode, @l)
{
	# create an object "on the run"
	if (!ref($self)) {
		$self = $self->new;
	}
	if ($mode eq "compile"){
		say "@l" if $verbose || $dry;
	} else {
		say "libtool: $mode: @l" if $verbose || $dry;	
	}
	if (!$dry) {
		$self->command_run(@l);
	}
	$performed++;
}

1;
