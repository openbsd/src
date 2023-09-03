#!/usr/bin/perl
#	$OpenBSD: gen_syscall_emulator.pl,v 1.1 2023/09/03 01:43:09 afresh1 Exp $	#
use v5.36;
use autodie;

# Copyright (c) 2023 Andrew Hewus Fresh <afresh1@openbsd.org>
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

my $includes = '/usr/include';

# Because perl uses a long for every syscall argument,
# if we are building a syscall_emulator for use by perl,
# taking that into account make things work more consistently
# across different OpenBSD architectures.
# Unfortunately there doesn't appear to be an easy way
# to make everything work "the way it was".
use constant PERL_LONG_ARGS => 1;

# See also /usr/src/sys/kern/syscalls.master
my %syscalls = parse_syscalls(
    "$includes/sys/syscall.h",
    "$includes/sys/syscallargs.h",
)->%*;
delete $syscalls{MAXSYSCALL}; # not an actual function

# The ordered list of all the headers we need
my @headers = qw<
	sys/syscall.h
	stdarg.h
	errno.h

	sys/socket.h
	sys/event.h
	sys/futex.h
	sys/ioctl.h
	sys/ktrace.h
	sys/mman.h
	sys/mount.h
	sys/msg.h
	sys/poll.h
	sys/ptrace.h
	sys/resource.h
	sys/select.h
	sys/sem.h
	sys/shm.h
	sys/stat.h
	sys/sysctl.h
	sys/time.h
	sys/uio.h
	sys/wait.h

	dirent.h
	fcntl.h
	sched.h
	signal.h
	stdlib.h
	stdio.h
	syslog.h
	tib.h
	time.h
	unistd.h
>;

foreach my $header (@headers) {
	my $filename = "$includes/$header";
	open my $fh, '<', $filename;
	my $content = do { local $/; readline $fh };
	close $fh;

	foreach my $name (sort keys %syscalls) {
		my $s = $syscalls{$name};
		my $func_sig = find_func_sig($content, $name, $s);

		if (ref $func_sig) {
			die "Multiple defs for $name <$header> <$s->{header}>"
			    if $s->{header};
			$s->{func} = $func_sig;
			$s->{header} = $header;
		} elsif ($func_sig) {
			$s->{mismatched_sig} = "$func_sig <$header>";
		}
	}
}

say "/*\n * Generated from gen_syscall_emulator.pl\n */";
say "#include <$_>" for @headers;
print <<"EOL";
#include "syscall_emulator.h"

long
syscall_emulator(int syscall, ...)
{
	long ret = 0;
	va_list args;
	va_start(args, syscall);

	switch(syscall) {
EOL

foreach my $name (
	sort { $syscalls{$a}{id} <=> $syscalls{$b}{id} } keys %syscalls
    ) {
	my %s = %{ $syscalls{$name} };

	# Some syscalls we can't emulate, so we comment those out.
	$s{skip} //= "Indirect syscalls not supported"
	    if !$s{argtypes} && ($s{args}[-1] || '') eq '...';
	$s{skip} //= "Mismatched func: $s{mismatched_sig}"
	    if $s{mismatched_sig} and not $s{func};
	$s{skip} //= "No signature found in headers"
	    unless $s{header};

	my $ret = $s{ret} eq 'void' ? '' : 'ret = ';
	$ret .= '(long)' if $s{ret} eq 'void *';

	my (@args, @defines);
	my $argname = '';
	if ($s{argtypes}) {
		if (@{ $s{argtypes} } > 1) {
			@defines = map {
				my $t = $_->{type};
				my $n = $_->{name};
				$n = "_$n" if $n eq $name; # link :-/
				push @args, $n;
				PERL_LONG_ARGS
				    ? "$t $n = ($t)va_arg(args, long);"
				    : "$t $n = va_arg(args, $t);"
			    } @{ $s{argtypes} };
		} else {
			if (@{ $s{argtypes} }) {
				$argname = " // " . join ', ',
				    map { $_->{name} }
				    @{ $s{argtypes} };
			}
			@args = map { "va_arg(args, $_->{type})" }
			    @{ $s{argtypes} };
		}
	} else {
		@args = @{ $s{args} };

		# If we didn't find args in syscallargs.h but have args
		# we don't know how to write our function.
		$s{skip} //= "Not found in sys/syscallargs.h"
		    if @args;
	}

	#my $header = $s{header} ? " <$s{header}>" : '';

	my $indent = "\t";
	say "$indent/* $s{skip}" if $s{skip};

	$indent .= ' *' if $s{skip};
	say "${indent}                  $s{signature} <sys/syscall.h>"
	    if $s{skip} && $s{skip} =~ /Mismatch/;

	my $brace = @defines ? " {" : "";
	say "${indent}case $s{define}:$brace"; # // $s{id}";
	say "${indent}\t$_" for @defines;
	#say "${indent}\t// $s{signature}$header";
	say "${indent}\t$ret$name(" . join(', ', @args) . ");$argname";
	say "${indent}\tbreak;";
	say "${indent}}" if $brace;

	say "\t */" if $s{skip};
}

print <<"EOL";
	default:
		ret = -1;
		errno = ENOSYS;
	}
	va_end(args);

	return ret;
}
EOL


sub parse_syscalls($syscall, $args)
{
	my %s = parse_syscall_h($syscall)->%*;

	my %a = parse_syscallargs_h($args)->%*;
	$s{$_}{argtypes} = $a{$_} for grep { $a{$_} } keys %s;

	return \%s;
}

sub parse_syscall_h($filename)
{
	my %s;
	open my $fh, '<', $filename;
	while (readline $fh) {
		if (m{^/\*
		    \s+ syscall: \s+ "(?<name>[^"]+)"
		    \s+	 ret: \s+ "(?<ret> [^"]+)"
		    \s+	args: \s+  (?<args>.*?)
		    \s* \*/
		  |
		    ^\#define \s+ (?<define>SYS_(?<name>\S+)) \s+ (?<id>\d+)
		}x)
		{
			my $name        = $+{name};
			$s{$name}{$_}   = $+{$_} for keys %+;
			$s{$name}{args} = [ $+{args} =~ /"(.*?)"/g ]
			    if exists $+{args};
		}
	}
	close $fh;

	foreach my $name (keys %s) {
		my %d = %{ $s{$name} };
		next unless $d{ret}; # the MAXSYSCALL

		my $ret = $d{ret};
		my @args = @{ $d{args} || [] };
		@args = 'void' unless @args;

		if ($args[-1] ne '...') {
			my @a;
			for (@args) {
				push @a, $_;
				last if $_ eq '...';
			}
			@args = @a;
		}

		my $args = join ", ", @args;
		$s{$name}{signature} = "$ret\t$name($args);" =~ s/\s+/ /gr;
		#print "    $s{$name}{signature}\n";
	}

	return \%s;
}

sub parse_syscallargs_h($filename)
{
	my %args;

	open my $fh, '<', $filename;
	while (readline $fh) {
		if (my ($syscall) = /^struct \s+ sys_(\w+)_args \s+ \{/x) {
			$args{$syscall} = [];
			while (readline $fh) {
				last if /^\s*\};\s*$/;
				if (/syscallarg
				    \(  (?<type> [^)]+ ) \)
				    \s+ (?<name>   \w+ ) \s* ;
				/x) {
					push @{$args{$syscall}}, {%+};
				}
			}
		}
	}
	close $fh;

	return \%args;
}

sub find_func_sig($content, $name, $s)
{
	my $re = $s->{re} //= qr{^
		(?<ret> \S+ (?: [^\S\n]+ \S+)? ) [^\S\n]* \n?
		\b \Q$name\E \( (?<args> [^)]* ) \)
		[^;]*;
	    }xms;

	$content =~ /$re/ || return !!0;
	my $ret  = $+{ret};
	my $args = $+{args};

	for ($ret, $args) {
		s/^\s+//;
		s/\s+$//;
		s/\s+/ /g;
	}

	# The actual functions may have this extra annotation
	$args =~ s/\*\s*__restrict/*/g;

	my %func_sig = ( ret => $ret, args => [ split /\s*,\s*/, $args ] );

	return "$ret $name($args);" =~ s/\s+/ /gr
	    unless sigs_match($s, \%func_sig);

	return \%func_sig;
}

# Tests whether two types are equivalent.
# Sometimes there are two ways to represent the same thing
# and it seems the functions and the syscalls
# differ a fair amount.
sub types_match($l, $r)
{
	state %m = (
	    caddr_t         => 'char *',
	    idtype_t        => 'int',
	    nfds_t          => 'u_int',
	    __off_t         => 'off_t',
	    pid_t           => 'int',
	    __size_t        => 'u_long',
	    size_t          => 'u_long',
	    'unsigned int'  => 'u_int',
	    'unsigned long' => 'u_long',
	);

	$l //= '__undef__';
	$r //= '__undef__';

	s/\b volatile \s+//x  for $l, $r;
	s/\b const    \s+//x  for $l, $r;
	s/\s* \[\d*\] $/ \*/x for $l, $r;

	my ($f, $s) = sort { length($a) <=> length($b) } $l, $r;
	if (index($s, $f) == 0) {
		$s =~ s/^\Q$f\E\s*//;
		if ( $s && $s =~ /^\w+$/ ) {
			#warn "prefix ['$f', '$s']\n";
			s/\s*\Q$s\E$// for $l, $r;
		}
	}

	$l = $m{$l} //= $l;
	$r = $m{$r} //= $r;

	return $l eq $r;
}


# Tests whether two function signatures match,
# expected to be left from syscall.h, right from the appopriate header.
sub sigs_match($l, $r)
{
	return !!0 unless types_match( $l->{ret}, $l->{ret} );

	my @l_args = @{ $l->{args} || [] };
	my @r_args = @{ $r->{args} || [] };

	for (\@l_args, \@r_args) {
		@{$_} = 'void' unless @{$_};
	}

	for my $i ( 0 .. $#l_args ) {
		return !!0 unless types_match($l_args[$i], $r_args[$i]);
		last if $l_args[$i] eq '...';
	}

	return !!1;
}
