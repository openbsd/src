#!/usr/bin/perl -w
use strict;

# read embed.fnc and regen/opcodes, needed by regen/embed.pl and makedef.pl

require 5.004;	# keep this compatible, an old perl is all we may have before
                # we build the new one

# Records the current pre-processor state:
my @state;
# Nested structure to group functions by the pre-processor conditions that
# control when they are compiled:
my %groups;

sub current_group {
    my $group = \%groups;
    # Nested #if blocks are effectively &&ed together
    # For embed.fnc, ordering within the && isn't relevant, so we can
    # sort them to try to group more functions together.
    foreach (sort @state) {
	$group->{$_} ||= {};
	$group = $group->{$_};
    }
    return $group->{''} ||= [];
}

sub add_level {
    my ($level, $indent, $wanted) = @_;
    my $funcs = $level->{''};
    my @entries;
    if ($funcs) {
	if (!defined $wanted) {
	    @entries = @$funcs;
	} else {
	    foreach (@$funcs) {
		if ($_->[0] =~ /A/) {
		    push @entries, $_ if $wanted eq 'A';
		} elsif ($_->[0] =~ /E/) {
		    push @entries, $_ if $wanted eq 'E';
		} else {
		    push @entries, $_ if $wanted eq '';
		}
	    }
	}
	@entries = sort {$a->[2] cmp $b->[2]} @entries;
    }
    foreach (sort grep {length $_} keys %$level) {
	my @conditional = add_level($level->{$_}, $indent . '  ', $wanted);
	push @entries,
	    ["#${indent}if $_"], @conditional, ["#${indent}endif"]
		if @conditional;
    }
    return @entries;
}

sub setup_embed {
    my $prefix = shift || '';
    open IN, $prefix . 'embed.fnc' or die $!;

    my @embed;

    while (<IN>) {
	chomp;
	next if /^:/;
	next if /^$/;
	while (s|\\$||) {
	    $_ .= <IN>;
	    chomp;
	}
	s/\s+$//;
	my @args;
	if (/^\s*(#|$)/) {
	    @args = $_;
	}
	else {
	    @args = split /\s*\|\s*/, $_;
	}
	if (@args == 1 && $args[0] !~ /^#\s*(?:if|ifdef|ifndef|else|endif)/) {
	    die "Illegal line $. '$args[0]' in embed.fnc";
	}
	push @embed, \@args;
    }

    close IN or die "Problem reading embed.fnc: $!";

    open IN, $prefix . 'regen/opcodes' or die $!;
    {
	my %syms;

	while (<IN>) {
	    chomp;
	    next unless $_;
	    next if /^#/;
	    my $check = (split /\t+/, $_)[2];
	    next if $syms{$check}++;

	    # These are all indirectly referenced by globals.c.
	    push @embed, ['pR', 'OP *', $check, 'NN OP *o'];
	}
    }
    close IN or die "Problem reading regen/opcodes: $!";

    # Cluster entries in embed.fnc that have the same #ifdef guards.
    # Also, split out at the top level the three classes of functions.
    # Output structure is actually the same as input structure - an
    # (ordered) list of array references, where the elements in the
    # reference determine what it is - a reference to a 1-element array is a
    # pre-processor directive, a reference to 2+ element array is a function.

    my $current = current_group();

    foreach (@embed) {
	if (@$_ > 1) {
	    push @$current, $_;
	    next;
	}
	$_->[0] =~ s/^#\s+/#/;
	$_->[0] =~ /^\S*/;
	$_->[0] =~ s/^#ifdef\s+(\S+)/#if defined($1)/;
	$_->[0] =~ s/^#ifndef\s+(\S+)/#if !defined($1)/;
	if ($_->[0] =~ /^#if\s*(.*)/) {
	    push @state, $1;
	} elsif ($_->[0] =~ /^#else\s*$/) {
	    die "Unmatched #else in embed.fnc" unless @state;
	    $state[-1] = "!($state[-1])";
	} elsif ($_->[0] =~ m!^#endif\s*(?:/\*.*\*/)?$!) {
	    die "Unmatched #endif in embed.fnc" unless @state;
	    pop @state;
	} else {
	    die "Unhandled pre-processor directive '$_->[0]' in embed.fnc";
	}
	$current = current_group();
    }

    return ([add_level(\%groups, '')],
	    [add_level(\%groups, '', '')],    # core
	    [add_level(\%groups, '', 'E')],   # ext
	    [add_level(\%groups, '', 'A')]);  # api
}

1;
