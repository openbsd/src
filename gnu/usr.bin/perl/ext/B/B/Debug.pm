package B::Debug;

our $VERSION = '1.11';

use strict;
require 5.006;
use B qw(peekop class walkoptree walkoptree_exec
         main_start main_root cstring sv_undef);
use Config;
my (@optype, @specialsv_name);
require B;
if ($] < 5.009) {
  require B::Asmdata;
  B::Asmdata->import qw(@optype @specialsv_name);
} else {
  B->import qw(@optype @specialsv_name);
}
my $have_B_Flags;
if (!$ENV{PERL_CORE}){ # avoid CORE test crashes
  eval { require B::Flags and $have_B_Flags++ };
}
my %done_gv;

sub _printop {
  my $op = shift;
  my $addr = ${$op} ? $op->ppaddr : '';
  $addr =~ s/^PL_ppaddr// if $addr;
  return sprintf "0x%x %s %s", ${$op}, ${$op} ? class($op) : '', $addr;
}

sub B::OP::debug {
    my ($op) = @_;
    printf <<'EOT', class($op), $$op, $op->ppaddr, _printop($op->next), _printop($op->sibling), $op->targ, $op->type;
%s (0x%lx)
	op_ppaddr	%s
	op_next		%s
	op_sibling	%s
	op_targ		%d
	op_type		%d
EOT
    if ($] > 5.009) {
	printf <<'EOT', $op->opt;
	op_opt		%d
EOT
    } else {
	printf <<'EOT', $op->seq;
	op_seq		%d
EOT
    }
    if ($have_B_Flags) {
        printf <<'EOT', $op->flags, $op->flagspv, $op->private, $op->privatepv;
	op_flags	%d	%s
	op_private	%d	%s
EOT
    } else {
        printf <<'EOT', $op->flags, $op->private;
	op_flags	%d
	op_private	%d
EOT
    }
}

sub B::UNOP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    printf "\top_first\t%s\n", _printop($op->first);
}

sub B::BINOP::debug {
    my ($op) = @_;
    $op->B::UNOP::debug();
    printf "\top_last \t%s\n", _printop($op->last);
}

sub B::LOOP::debug {
    my ($op) = @_;
    $op->B::BINOP::debug();
    printf <<'EOT', _printop($op->redoop), _printop($op->nextop), _printop($op->lastop);
	op_redoop	%s
	op_nextop	%s
	op_lastop	%s
EOT
}

sub B::LOGOP::debug {
    my ($op) = @_;
    $op->B::UNOP::debug();
    printf "\top_other\t%s\n", _printop($op->other);
}

sub B::LISTOP::debug {
    my ($op) = @_;
    $op->B::BINOP::debug();
    printf "\top_children\t%d\n", $op->children;
}

sub B::PMOP::debug {
    my ($op) = @_;
    $op->B::LISTOP::debug();
    printf "\top_pmreplroot\t0x%x\n", $] < 5.008 ? ${$op->pmreplroot} : $op->pmreplroot;
    printf "\top_pmreplstart\t0x%x\n", ${$op->pmreplstart};
    printf "\top_pmnext\t0x%x\n", ${$op->pmnext} if $] < 5.009005;
    if ($Config{'useithreads'}) {
      printf "\top_pmstashpv\t%s\n", cstring($op->pmstashpv);
      printf "\top_pmoffset\t%d\n", $op->pmoffset;
    } else {
      printf "\top_pmstash\t%s\n", cstring($op->pmstash);
    }
    printf "\top_precomp\t%s\n", cstring($op->precomp);
    printf "\top_pmflags\t0x%x\n", $op->pmflags;
    printf "\top_reflags\t0x%x\n", $op->reflags if $] >= 5.009;
    printf "\top_pmpermflags\t0x%x\n", $op->pmpermflags if $] < 5.009;
    printf "\top_pmdynflags\t0x%x\n", $op->pmdynflags if $] < 5.009;
    $op->pmreplroot->debug if $] < 5.008;
}

sub B::COP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    my $cop_io = class($op->io) eq 'SPECIAL' ? '' : $op->io->as_string;
    printf <<'EOT', $op->label, $op->stashpv, $op->file, $op->cop_seq, $op->arybase, $op->line, ${$op->warnings}, cstring($cop_io);
	cop_label	"%s"
	cop_stashpv	"%s"
	cop_file	"%s"
	cop_seq		%d
	cop_arybase	%d
	cop_line	%d
	cop_warnings	0x%x
	cop_io		%s
EOT
}

sub B::SVOP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    printf "\top_sv\t\t0x%x\n", ${$op->sv};
    $op->sv->debug;
}

sub B::PVOP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    printf "\top_pv\t\t%s\n", cstring($op->pv);
}

sub B::PADOP::debug {
    my ($op) = @_;
    $op->B::OP::debug();
    printf "\top_padix\t%ld\n", $op->padix;
}

sub B::NULL::debug {
    my ($sv) = @_;
    if ($$sv == ${sv_undef()}) {
	print "&sv_undef\n";
    } else {
	printf "NULL (0x%x)\n", $$sv;
    }
}

sub B::SV::debug {
    my ($sv) = @_;
    if (!$$sv) {
	print class($sv), " = NULL\n";
	return;
    }
    printf <<'EOT', class($sv), $$sv, $sv->REFCNT, $sv->FLAGS;
%s (0x%x)
	REFCNT		%d
	FLAGS		0x%x
EOT
}

sub B::RV::debug {
    my ($rv) = @_;
    B::SV::debug($rv);
    printf <<'EOT', ${$rv->RV};
	RV		0x%x
EOT
    $rv->RV->debug;
}

sub B::PV::debug {
    my ($sv) = @_;
    $sv->B::SV::debug();
    my $pv = $sv->PV();
    printf <<'EOT', cstring($pv), length($pv);
	xpv_pv		%s
	xpv_cur		%d
EOT
}

sub B::IV::debug {
    my ($sv) = @_;
    $sv->B::SV::debug();
    printf "\txiv_iv\t\t%d\n", $sv->IV;
}

sub B::NV::debug {
    my ($sv) = @_;
    $sv->B::IV::debug();
    printf "\txnv_nv\t\t%s\n", $sv->NV;
}

sub B::PVIV::debug {
    my ($sv) = @_;
    $sv->B::PV::debug();
    printf "\txiv_iv\t\t%d\n", $sv->IV;
}

sub B::PVNV::debug {
    my ($sv) = @_;
    $sv->B::PVIV::debug();
    printf "\txnv_nv\t\t%s\n", $sv->NV;
}

sub B::PVLV::debug {
    my ($sv) = @_;
    $sv->B::PVNV::debug();
    printf "\txlv_targoff\t%d\n", $sv->TARGOFF;
    printf "\txlv_targlen\t%u\n", $sv->TARGLEN;
    printf "\txlv_type\t%s\n", cstring(chr($sv->TYPE));
}

sub B::BM::debug {
    my ($sv) = @_;
    $sv->B::PVNV::debug();
    printf "\txbm_useful\t%d\n", $sv->USEFUL;
    printf "\txbm_previous\t%u\n", $sv->PREVIOUS;
    printf "\txbm_rare\t%s\n", cstring(chr($sv->RARE));
}

sub B::CV::debug {
    my ($sv) = @_;
    $sv->B::PVNV::debug();
    my ($stash) = $sv->STASH;
    my ($start) = $sv->START;
    my ($root) = $sv->ROOT;
    my ($padlist) = $sv->PADLIST;
    my ($file) = $sv->FILE;
    my ($gv) = $sv->GV;
    printf <<'EOT', $$stash, $$start, $$root, $$gv, $file, $sv->DEPTH, $padlist, ${$sv->OUTSIDE}, $sv->OUTSIDE_SEQ;
	STASH		0x%x
	START		0x%x
	ROOT		0x%x
	GV		0x%x
	FILE		%s
	DEPTH		%d
	PADLIST		0x%x
	OUTSIDE		0x%x
	OUTSIDE_SEQ	%d
EOT
    $start->debug if $start;
    $root->debug if $root;
    $gv->debug if $gv;
    $padlist->debug if $padlist;
}

sub B::AV::debug {
    my ($av) = @_;
    $av->B::SV::debug;
    # tied arrays may leave out FETCHSIZE
    my (@array) = eval { $av->ARRAY; };
    print "\tARRAY\t\t(", join(", ", map("0x" . $$_, @array)), ")\n";
    my $fill = eval { scalar(@array) };
    if ($Config{'useithreads'}) {
      printf <<'EOT', $fill, $av->MAX, $av->OFF;
	FILL		%d
	MAX		%d
	OFF		%d
EOT
    } else {
      printf <<'EOT', $fill, $av->MAX;
	FILL		%d
	MAX		%d
EOT
    }
    printf <<'EOT', $av->AvFLAGS if $] < 5.009;
	AvFLAGS		%d
EOT
}

sub B::GV::debug {
    my ($gv) = @_;
    if ($done_gv{$$gv}++) {
	printf "GV %s::%s\n", $gv->STASH->NAME, $gv->SAFENAME;
	return;
    }
    my ($sv) = $gv->SV;
    my ($av) = $gv->AV;
    my ($cv) = $gv->CV;
    $gv->B::SV::debug;
    printf <<'EOT', $gv->SAFENAME, $gv->STASH->NAME, $gv->STASH, $$sv, $gv->GvREFCNT, $gv->FORM, $$av, ${$gv->HV}, ${$gv->EGV}, $$cv, $gv->CVGEN, $gv->LINE, $gv->FILE, $gv->GvFLAGS;
	NAME		%s
	STASH		%s (0x%x)
	SV		0x%x
	GvREFCNT	%d
	FORM		0x%x
	AV		0x%x
	HV		0x%x
	EGV		0x%x
	CV		0x%x
	CVGEN		%d
	LINE		%d
	FILE		%s
	GvFLAGS		0x%x
EOT
    $sv->debug if $sv;
    $av->debug if $av;
    $cv->debug if $cv;
}

sub B::SPECIAL::debug {
    my $sv = shift;
    print $specialsv_name[$$sv], "\n";
}

sub compile {
    my $order = shift;
    B::clearsym();
    if ($order && $order eq "exec") {
        return sub { walkoptree_exec(main_start, "debug") }
    } else {
        return sub { walkoptree(main_root, "debug") }
    }
}

1;

__END__

=head1 NAME

B::Debug - Walk Perl syntax tree, printing debug info about ops

=head1 SYNOPSIS

	perl -MO=Debug[,OPTIONS] foo.pl

=head1 DESCRIPTION

See F<ext/B/README> and the newer L<B::Concise>, L<B::Terse>.

=head1 OPTIONS

With option -exec, walks tree in execute order,
otherwise in basic order.

=head1 Changes

  1.11 2008-07-14 rurban
	avoid B::Flags in CORE tests not to crash on old XS in @INC

  1.10 2008-06-28 rurban
	require 5.006; Test::More not possible in 5.00505
	our => my
	
  1.09 2008-06-18 rurban
	minor META.yml syntax fix
	5.8.0 ending nextstate test failure: be more tolerant
	PREREQ_PM Test::More

  1.08 2008-06-17 rurban
	support 5.00558 - 5.6.2

  1.07 2008-06-16 rurban
	debug.t: fix strawberry perl quoting issue

  1.06 2008-06-11 rurban
	added B::Flags output
	dual-life CPAN as B-Debug-1.06 and CORE
	protect scalar(@array) if tied arrays leave out FETCHSIZE

  1.05_03 2008-04-16 rurban
	ithread fixes in B::AV
	B-C-1.04_??

  B-C-1.04_09 2008-02-24 rurban
	support 5.8 (import Asmdata)

  1.05_02 2008-02-21 rurban
	added _printop
	B-C-1.04_08 and CORE

  1.05_01 2008-02-05 rurban
	5.10 fix for op->seq
	B-C-1.04_04

=head1 AUTHOR

Malcolm Beattie, C<mbeattie@sable.ox.ac.uk>
Reini Urban C<rurban@cpan.org>

=head1 LICENSE

Copyright (c) 1996, 1997 Malcolm Beattie
Copyright (c) 2008 Reini Urban

	This program is free software; you can redistribute it and/or modify
	it under the terms of either:

	a) the GNU General Public License as published by the Free
	Software Foundation; either version 1, or (at your option) any
	later version, or

	b) the "Artistic License" which comes with this kit.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either
    the GNU General Public License or the Artistic License for more details.

    You should have received a copy of the Artistic License with this kit,
    in the file named "Artistic".  If not, you can get one from the Perl
    distribution. You should also have received a copy of the GNU General
    Public License, in the file named "Copying". If not, you can get one
    from the Perl distribution or else write to the Free Software Foundation,
    Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.

=cut
