#! /usr/bin/perl
# ex:ts=8 sw=4:
# $OpenBSD: PkgSign.pm,v 1.17 2019/07/08 10:55:39 espie Exp $
#
# Copyright (c) 2003-2014 Marc Espie <espie@openbsd.org>
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

use OpenBSD::AddCreateDelete;
use OpenBSD::Signer;

package OpenBSD::PkgSign::State;
our @ISA = qw(OpenBSD::CreateSign::State);

sub handle_options
{
	my $state = shift;

	$state->{extra_stats} = 0;
	$state->{opt} = {
	    'o' =>
		    sub {
			    $state->{output_dir} = shift;
		    },
	    'S' =>
		    sub {
			    $state->{source} = shift;
		    },
	    's' =>
		    sub { 
			    push(@{$state->{signature_params}}, shift);
		    },
	    'V' =>
		    sub {
			    $state->{extra_stats}++;
		    },
	};
	$state->{signature_style} = 'unsigned';

	$state->SUPER::handle_options('Cij:o:S:s:V',
	    '[-CvV] [-D name[=value]] -s signify2 -s priv',
	    '[-o dir] [-S source] [pkg-name...]');
	if (defined $state->{signature_params}) {
		$state->{signer} = OpenBSD::Signer->factory($state);
	}
    	if (!defined $state->{signer}) {
		$state->usage("Can't invoke command without valid signing parameters");
	}
	$state->{output_dir} //= ".";
	if (!-d $state->{output_dir}) {
		require File::Path;
		File::Path::make_path($state->{output_dir})
		    or $state->usage("can't create dir");
	}
	$state->{wantntogo} = $state->{extra_stats};
}

package OpenBSD::PkgSign;
use OpenBSD::Temp;
use OpenBSD::PackingList;
use OpenBSD::PackageInfo;

sub sign_existing_package
{
	my ($self, $state, $pkg) = @_;
	my $output = $state->{output_dir};
	my $dest = $output.'/'.$pkg->name.".tgz";
	if ($state->opt('i')) {
		if (-f $dest) {
			return;
	    	}
	}
	my (undef, $tmp) = OpenBSD::Temp::permanent_file($output, "pkg") or
	    die $state->fatal(OpenBSD::Temp->last_error);
	$state->{signer}->sign($pkg, $state, $tmp);

	chmod((0666 & ~umask), $tmp);
	rename($tmp, $dest) or
	    $state->fatal("Can't create final signed package: #1", $!);
	if ($state->opt('C')) {
		$state->system(sub {
		    chdir($output);
		    open(STDOUT, '>>', 'SHA256');
		    },
		    OpenBSD::Paths->sha256, '-b', $pkg->name.".tgz");
    	}
}

sub sign_list
{
	my ($self, $l, $repo, $maxjobs, $state) = @_;
	$state->{total} = scalar @$l;
	$maxjobs //= 1;
	my $code = sub {
		my $name = shift;
		my $pkg = $repo->find($name);
		if (!defined $pkg) {
			$state->errsay("#1 not found", $name);
		} else {
			$self->sign_existing_package($state, $pkg);
		}
	    };
	my $display = $state->verbose ?
	    sub {
		$state->progress->set_header("Signed ".shift);
		$state->{done}++;
		$state->progress->next($state->ntogo);
	    } :
	    sub {
	    };
	if ($maxjobs > 1) {
		my $jobs = {};
		my $n = 0;
		my $reap_job = sub {
			my $pid = wait;
			if (!defined $jobs->{$pid}) {
				$state->fatal("Wait returned #1: unknown process", $pid);
			}
			if ($? != 0) {
				$state->fatal("Signature of #1 failed\n", 
				    $jobs->{$pid});
			}
			$n--;
			&$display($jobs->{$pid});
			delete $state->{signer}{pubkey};
			delete $jobs->{$pid};
		};
			
		while (@$l > 0) {
			my $name = shift @$l;
			my $pid = fork();
			if ($pid == 0) {
				$repo->reinitialize;
				&$code($name);
				exit(0);
			} else {
				$jobs->{$pid} = $name;
				$n++;
			}
			if ($n >= $maxjobs) {
				&$reap_job;
			}
		}
		while ($n != 0) {
			&$reap_job;
		}
	} else {
		for my $name (@$l) {
			&$code($name);
			&$display($name);
			delete $state->{signer}{pubkey};
		}
	}
	if ($state->opt('C')) {
		$state->system(sub {
		    chdir($state->{output_dir});
		    open(STDOUT, '>', 'SHA256.new');
		    }, 'sort', 'SHA256');
		rename($state->{output_dir}.'/SHA256.new', 
		    $state->{output_dir}.'/SHA256');
	}
}

sub sign_existing_repository
{
	my ($self, $state, $source) = @_;
	require OpenBSD::PackageRepository;
	my $repo = OpenBSD::PackageRepository->new($source, $state);
	if ($state->{signer}->want_local && !$repo->is_local_file) {
		$state->fatal("Signing distant source is not supported");
	}
	my @list = sort @{$repo->list};
	if (@list == 0) {
		$state->errsay('Source repository "#1" is empty', $source);
    	}
	$self->sign_list(\@list, $repo, $state->opt('j'), $state);
}


sub parse_and_run
{
	my ($self, $cmd) = @_;
	my $state = OpenBSD::PkgSign::State->new($cmd);
	$state->handle_options;
	if (!defined $state->{source} && @ARGV == 0) {
		$state->usage("Nothing to sign");
	}
	if (defined $state->{source}) {
		$self->sign_existing_repository($state, 
		    $state->{source});
	}
	$self->sign_list(\@ARGV, $state->repo, $state->opt('j'), 
	    $state);
	return 0;
}

