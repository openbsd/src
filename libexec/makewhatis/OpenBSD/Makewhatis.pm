# ex:ts=8 sw=4:
# $OpenBSD: Makewhatis.pm,v 1.8 2010/07/13 15:38:27 espie Exp $
# Copyright (c) 2000-2004 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;

# used to print everything. People using makewhatis internally can
# override this

package OpenBSD::Makewhatis::Printer;
sub new
{
	my $class = shift;
	return bless {}, $class;
}

sub print
{
	my $self = shift;
	print $self->f(@_);
}

sub errsay
{
	my $self = shift;
	print STDERR $self->f(@_), "\n";
}

sub fatal
{
	my $self = shift;
	die $self->f(@_);
}

sub f
{
	my $self = shift;
	if (@_ == 0) {
		return '';
	}
	my ($_, @l) = @_;
	# make it so that #0 is #
	unshift(@l, '#');
	s/\#(\d+)/$l[$1]/ge;
	return $_;
}

sub picky
{
	return shift->{picky};
}

sub testmode
{
	return shift->{testmode};
}

sub check_dir
{
	my ($self, $dir) = @_;
	unless (-d $dir) {
	    $self->fatal("#1: #2 is not a directory", $0, $dir);
	}
}

package OpenBSD::Makewhatis;


# $subjects = scan_manpages(\@list, $p)
#
#   scan a set of manpages, return list of subjects
#
sub scan_manpages
{
    my ($list, $p) = @_;
    my $_;
    my $done=[];

    for (@$list) {
	my ($file, $subjects);
	if (m/\.(?:Z|gz)$/) {
	    unless (open $file, '-|', "gzip -fdc $_") {
	    	$p->errsay("#1: can't decompress #2: #3", $0, $_, $!);
		next;
	    }
	    $_ = $`;
	} else {
	    unless (open $file, '<', $_) {
	    	$p->errsay("#1: can't read #2: #3", $0, $_, $!);
		next;
	    }
	}
	if (m/\.(?:[1-9ln][^.]*|tbl)$/) {
	    require OpenBSD::Makewhatis::Unformated;

	    $subjects = OpenBSD::Makewhatis::Unformated::handle($file, $_, $p);
	} elsif (m/\.0$/) {
	    require OpenBSD::Makewhatis::Formated;

	    $subjects = OpenBSD::Makewhatis::Formated::handle($file, $_, $p);
	    # in test mode, we try harder
	} elsif ($p->testmode) {
	    require OpenBSD::Makewhatis::Unformated;

	    $subjects = OpenBSD::Makewhatis::Unformated::handle($file, $_, $p);
	    if (@$subjects == 0) {
		require OpenBSD::Makewhatis::Formated;

	    	$subjects = OpenBSD::Makewhatis::Formated::handle($file, $_, $p);
	    }
	} else {
	    $p->errsay("Can't find type of #1", $_);
	    next;
	}
	if ($p->picky) {
		require OpenBSD::Makewhatis::Check;

		for my $s (@$subjects) {
			OpenBSD::Makewhatis::Check::verify_subject($s, $_, $p);
		}
	}
	push @$done, @$subjects;
    }
    return $done;
}

# build_index($dir, $p)
#
#   build index for $dir
#
sub build_index
{
    require OpenBSD::Makewhatis::Find;
    require OpenBSD::Makewhatis::Whatis;

    my ($dir, $p) = @_;
    my $list = OpenBSD::Makewhatis::Find::find_manpages($dir);
    my $subjects = scan_manpages($list, $p);
    OpenBSD::Makewhatis::Whatis::write($subjects, $dir, $p);
}

# merge($dir, \@pages, $p)
#
#   merge set of pages into directory index
#
sub merge
{
	require OpenBSD::Makewhatis::Whatis;

	my ($mandir, $args, $p) = @_;
	$p //= OpenBSD::Makewhatis::Printer->new;
	$p->check_dir($mandir);
	my $whatis = "$mandir/whatis.db";
	my $subjects = scan_manpages($args, $p);
	if (open(my $old, '<', $whatis)) {
		while (my $l = <$old>) {
		    chomp $l;
		    push(@$subjects, $l);
		}
		close($old);
	}
	OpenBSD::Makewhatis::Whatis::write($subjects, $mandir, $p);
}

# remove(dir, \@pages, $p)
#
#   remove set of pages from directory index
#
sub remove
{
	require OpenBSD::Makewhatis::Whatis;

	my ($mandir, $args, $p) = @_;
	$p //= OpenBSD::Makewhatis::Printer->new;
	$p->check_dir($mandir);
	my $whatis = "$mandir/whatis.db";
	open(my $old, '<', $whatis) or
	    $p->fatal("#1: can't open #2 to merge with: #3", $0, $whatis, $!);
	my $subjects = scan_manpages($args, $p);
	my %remove = map {$_ => 1 } @$subjects;
	$subjects = [];
	while (my $l = <$old>) {
	    chomp $l;
	    push(@$subjects, $l) unless defined $remove{$l};
	}
	close($old);
	OpenBSD::Makewhatis::Whatis::write($subjects, $mandir, $p);
}

# $dirs = default_dirs($p)
#
#   read list of default directories from man.conf
#
sub default_dirs
{
	my $p = shift;
	my $args=[];
	open(my $conf, '<', '/etc/man.conf') or 
	    $p->fatal("#1: can't open #2: #3", $0, "/etc/man.conf", $!);
	while (my $l = <$conf>) {
	    chomp $l;
	    push(@$args, $1) if $l =~ m/^_whatdb\s+(.*)\/whatis\.db\s*$/;
	}
	close $conf;
	return $args;
}

# makewhatis(\@args, \%opts)
#
#   glue for front-end, see makewhatis(8)
#
sub makewhatis
{
	my ($args, $opts) = @_;
	my $p = OpenBSD::Makewhatis::Printer->new;
	if (defined $opts->{'p'}) {
	    $p->{picky} = 1;
	}
	if (defined $opts->{'t'}) {
	    $p->{testmode} = 1;
	    my $subjects = scan_manpages($args, $p);
	    $p->print("#1", join("\n", @$subjects)."\n");
	    return;
	} 

	if (defined $opts->{'d'}) {
	    merge($opts->{'d'}, $args, $p);
	    return;
	}
	if (defined $opts->{'u'}) {
	    remove($opts->{'u'}, $args, $p);
	    return;
	}
	if (@$args == 0) {
	    $args = default_dirs($p);
	}
		
	for my $mandir (@$args) {
	    if (-d $mandir) {
		build_index($mandir, $p);
	    } elsif (-e $mandir || $p->picky) {
	    	$p->errsay("#1: #2 is not a directory", $0, $mandir);
	    }
	}
}

1;
