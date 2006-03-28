#!perl
# Reports, in a perl source tree, which dual-lived core modules have not the
# same version than the corresponding module on CPAN.

use 5.9.0;
use strict;
use Getopt::Std;
use ExtUtils::MM_Unix;
use lib 'Porting';
use Maintainers qw(get_module_files %Modules);

our $packagefile = '02packages.details.txt';

sub usage () {
    die <<USAGE;
$0 - report which core modules are outdated.
To be run at the root of a perl source tree.
Options :
-h : help
-v : verbose (print all versions of all files, not only those which differ)
-f : force download of $packagefile from CPAN
     (it's expected to be found in the current directory)
USAGE
}

sub get_package_details () {
    my $url = 'http://www.cpan.org/modules/02packages.details.txt.gz';
    unlink $packagefile;
    system("wget $url && gunzip $packagefile.gz") == 0
	or die "Failed to get package details\n";
}

getopts('fhv');
our $opt_h and usage;
our $opt_f or !-f $packagefile and get_package_details;

# Load the package details. All of them.
my %cpanversions;
open my $fh, $packagefile or die $!;
while (<$fh>) {
    my ($p, $v) = split ' ';
    $cpanversions{$p} = $v;
}
close $fh;

for my $dist (sort keys %Modules) {
    next unless $Modules{$dist}{CPAN};
    print "Module $dist...\n";
    for my $file (get_module_files($dist)) {
	next if $file !~ /\.pm\z/ or $file =~ m{^t/} or $file =~ m{/t/};
	my $vcore = MM->parse_version($file) // 'undef';
	my $module = $file;
	$module =~ s/\.pm\z//;
	# some heuristics to figure out the module name from the file name
	$module =~ s{^(lib|ext)/}{}
	    and $1 eq 'ext'
	    and ( $module =~ s{^(.*)/lib/\1\b}{$1},
		  $module =~ s{(\w+)/\1\b}{$1},
		  $module =~ s{^Encode/encoding}{encoding},
		  $module =~ s{^MIME/Base64/QuotedPrint}{MIME/QuotedPrint},
		  $module =~ s{^List/Util/lib/Scalar}{Scalar},
	        );
	$module =~ s{/}{::}g;
	my $vcpan = $cpanversions{$module} // 'not found';
	if (our $opt_v or $vcore ne $vcpan) {
	    print "    $file: core=$vcore, cpan=$vcpan\n";
	}
    }
}
