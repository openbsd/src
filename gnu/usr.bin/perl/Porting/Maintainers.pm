#
# Maintainers.pm - show information about maintainers
#

package Maintainers;

use strict;

use lib "Porting";
# Please don't use post 5.008 features as this module is used by
# Porting/makemeta, and that in turn has to be run by the perl just built.
use 5.008;

require "Maintainers.pl";
use vars qw(%Modules %Maintainers);

use vars qw(@ISA @EXPORT_OK $VERSION);
@ISA = qw(Exporter);
@EXPORT_OK = qw(%Modules %Maintainers
		get_module_files get_module_pat
		show_results process_options files_to_modules
		reload_manifest);
$VERSION = 0.03;
require Exporter;

use File::Find;
use Getopt::Long;

my %MANIFEST;

# (re)read the MANIFEST file, blowing away any previous effort

sub reload_manifest {
    %MANIFEST = ();
    if (open(MANIFEST, "MANIFEST")) {
	while (<MANIFEST>) {
	    if (/^(\S+)/) {
		$MANIFEST{$1}++;
	    }
	    else {
		warn "MANIFEST:$.: malformed line: $_\n";
	    }
	}
	close MANIFEST;
    } else {
	die "$0: Failed to open MANIFEST for reading: $!\n";
    }
}

reload_manifest;


sub get_module_pat {
    my $m = shift;
    split ' ', $Modules{$m}{FILES};
}

# exand dir/ or foo* into a full list of files
#
sub expand_glob {
    sort { lc $a cmp lc $b }
	map {
	    -f $_ ? # File as-is.
		$_ :
		-d _ ? # Recurse into directories.
		do {
		    my @files;
		    find(
			 sub {
			     push @files, $File::Find::name
				 if -f $_ && exists $MANIFEST{$File::Find::name};
			 }, $_);
		    @files;
		}
	    # The rest are globbable patterns; expand the glob, then
	    # recurively perform directory expansion on any results
	    : expand_glob(grep -e $_,glob($_))
	    } @_;
}

sub get_module_files {
    my $m = shift;
    my %exclude;
    my @files;
    for (get_module_pat($m)) {
	if (s/^!//) {
	    $exclude{$_}=1 for expand_glob($_);
	}
	else {
	    push @files, expand_glob($_);
	}
    }
    return grep !$exclude{$_}, @files;
}


sub get_maintainer_modules {
    my $m = shift;
    sort { lc $a cmp lc $b }
    grep { $Modules{$_}{MAINTAINER} eq $m }
    keys %Modules;
}

sub usage {
    warn <<__EOF__;
$0: Usage:
    --maintainer M | --module M [--files]
		List modules or maintainers matching the pattern M.
		With --files, list all the files associated with them
or
    --check | --checkmani [commit | file ... | dir ... ]
		Check consistency of Maintainers.pl
			with a file	checks if it has a maintainer
			with a dir	checks all files have a maintainer
			with a commit   checks files modified by that commit
			no arg		checks for multiple maintainers
	       --checkmani is like --check, but only reports on unclaimed
	       files if they are in MANIFEST
or
    --opened  | file ....
		List the module ownership of modified or the listed files

Matching is case-ignoring regexp, author matching is both by
the short id and by the full name and email.  A "module" may
not be just a module, it may be a file or files or a subdirectory.
The options may be abbreviated to their unique prefixes
__EOF__
    exit(0);
}

my $Maintainer;
my $Module;
my $Files;
my $Check;
my $Checkmani;
my $Opened;

sub process_options {
    usage()
	unless
	    GetOptions(
		       'maintainer=s'	=> \$Maintainer,
		       'module=s'	=> \$Module,
		       'files'		=> \$Files,
		       'check'		=> \$Check,
		       'checkmani'	=> \$Checkmani,
		       'opened'		=> \$Opened,
		      );

    my @Files;

    if ($Opened) {
	usage if @ARGV;
	chomp (@Files = `git ls-files -m --full-name`);
	die if $?;
    } elsif (@ARGV == 1 &&
	     $ARGV[0] =~ /^(?:HEAD|[0-9a-f]{4,40})(?:~\d+)?\^*$/) {
	my $command = "git diff --name-only $ARGV[0]^ $ARGV[0]";
	chomp (@Files = `$command`);
	die "'$command' failed: $?" if $?;
    } else {
	@Files = @ARGV;
    }

    usage() if @Files && ($Maintainer || $Module || $Files);

    for my $mean ($Maintainer, $Module) {
	warn "$0: Did you mean '$0 $mean'?\n"
	    if $mean && -e $mean && $mean ne '.' && !$Files;
    }

    warn "$0: Did you mean '$0 -mo $Maintainer'?\n"
	if defined $Maintainer && exists $Modules{$Maintainer};

    warn "$0: Did you mean '$0 -ma $Module'?\n"
	if defined $Module     && exists $Maintainers{$Module};

    return ($Maintainer, $Module, $Files, @Files);
}

sub files_to_modules {
    my @Files = @_;
    my %ModuleByFile;

    for (@Files) { s:^\./:: }

    @ModuleByFile{@Files} = ();

    # First try fast match.

    my %ModuleByPat;
    for my $module (keys %Modules) {
	for my $pat (get_module_pat($module)) {
	    $ModuleByPat{$pat} = $module;
	}
    }
    # Expand any globs.
    my %ExpModuleByPat;
    for my $pat (keys %ModuleByPat) {
	if (-e $pat) {
	    $ExpModuleByPat{$pat} = $ModuleByPat{$pat};
	} else {
	    for my $exp (glob($pat)) {
		$ExpModuleByPat{$exp} = $ModuleByPat{$pat};
	    }
	}
    }
    %ModuleByPat = %ExpModuleByPat;
    for my $file (@Files) {
	$ModuleByFile{$file} = $ModuleByPat{$file}
	    if exists $ModuleByPat{$file};
    }

    # If still unresolved files...
    if (my @ToDo = grep { !defined $ModuleByFile{$_} } keys %ModuleByFile) {

	# Cannot match what isn't there.
	@ToDo = grep { -e $_ } @ToDo;

	if (@ToDo) {
	    # Try prefix matching.

	    # Need to try longst prefixes first, else lib/CPAN may match
	    # lib/CPANPLUS/... and similar

	    my @OrderedModuleByPat
		= sort {length $b <=> length $a} keys %ModuleByPat;

	    # Remove trailing slashes.
	    for (@ToDo) { s|/$|| }

	    my %ToDo;
	    @ToDo{@ToDo} = ();

	    for my $pat (@OrderedModuleByPat) {
		last unless keys %ToDo;
		if (-d $pat) {
		    my @Done;
		    for my $file (keys %ToDo) {
			if ($file =~ m|^$pat|i) {
			    $ModuleByFile{$file} = $ModuleByPat{$pat};
			    push @Done, $file;
			}
		    }
		    delete @ToDo{@Done};
		}
	    }
	}
    }
    \%ModuleByFile;
}
sub show_results {
    my ($Maintainer, $Module, $Files, @Files) = @_;

    if ($Maintainer) {
	for my $m (sort keys %Maintainers) {
	    if ($m =~ /$Maintainer/io || $Maintainers{$m} =~ /$Maintainer/io) {
		my @modules = get_maintainer_modules($m);
		if ($Module) {
		    @modules = grep { /$Module/io } @modules;
		}
		if ($Files) {
		    my @files;
		    for my $module (@modules) {
			push @files, get_module_files($module);
		    }
		    printf "%-15s @files\n", $m;
		} else {
		    if ($Module) {
			printf "%-15s @modules\n", $m;
		    } else {
			printf "%-15s $Maintainers{$m}\n", $m;
		    }
		}
	    }
	}
    } elsif ($Module) {
	for my $m (sort { lc $a cmp lc $b } keys %Modules) {
	    if ($m =~ /$Module/io) {
		if ($Files) {
		    my @files = get_module_files($m);
		    printf "%-15s @files\n", $m;
		} else {
		    printf "%-15s %-12s %s\n", $m, $Modules{$m}{MAINTAINER}, $Modules{$m}{UPSTREAM}||'unknown';
		}
	    }
	}
    } elsif ($Check or $Checkmani) {
        if( @Files ) {
	    missing_maintainers(
		$Checkmani
		    ? sub { -f $_ and exists $MANIFEST{$File::Find::name} }
		    : sub { /\.(?:[chty]|p[lm]|xs)\z/msx },
		@Files
	    );
	}
	else { 
	    duplicated_maintainers();
	}
    } elsif (@Files) {
	my $ModuleByFile = files_to_modules(@Files);
	for my $file (@Files) {
	    if (defined $ModuleByFile->{$file}) {
		my $module     = $ModuleByFile->{$file};
		my $maintainer = $Modules{$ModuleByFile->{$file}}{MAINTAINER};
		my $upstream   = $Modules{$module}{UPSTREAM}||'unknown';
		printf "%-15s [%-7s] $module $maintainer $Maintainers{$maintainer}\n", $file, $upstream;
	    } else {
		printf "%-15s ?\n", $file;
	    }
	}
    }
    elsif ($Opened) {
	print STDERR "(No files are modified)\n";
    }
    else {
	usage();
    }
}

my %files;

sub maintainers_files {
    %files = ();
    for my $k (keys %Modules) {
	for my $f (get_module_files($k)) {
	    ++$files{$f};
	}
    }
}

sub duplicated_maintainers {
    maintainers_files();
    for my $f (keys %files) {
	if ($files{$f} > 1) {
	    warn "File $f appears $files{$f} times in Maintainers.pl\n";
	}
    }
}

sub warn_maintainer {
    my $name = shift;
    warn "File $name has no maintainer\n" if not $files{$name};
}

sub missing_maintainers {
    my($check, @path) = @_;
    maintainers_files();
    my @dir;
    for my $d (@path) {
	if( -d $d ) { push @dir, $d } else { warn_maintainer($d) }
    }
    find sub { warn_maintainer($File::Find::name) if $check->() }, @dir
	if @dir;
}

1;

