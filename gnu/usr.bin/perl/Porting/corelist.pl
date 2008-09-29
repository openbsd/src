#!perl
# Generates info for Module::CoreList from this perl tree
# run this from the root of a clean perl tree

use strict;
use warnings;
use File::Find;
use ExtUtils::MM_Unix;

my %lines;
find(sub {
    /(\.pm|_pm\.PL)$/ or return;
    /PPPort\.pm$/ and return;
    my $module = $File::Find::name;
    $module =~ /\b(demo|t|private)\b/ and return; # demo or test modules
    my $version = MM->parse_version($_);
    defined $version or $version = 'undef';
    $version =~ /\d/ and $version = "'$version'";
    # some heuristics to figure out the module name from the file name
    $module =~ s{^(lib|(win32/|vms/|symbian/)?ext)/}{}
	and $1 ne 'lib'
	and ( $module =~ s{^(.*)/lib/\1\b}{$1},
	      $module =~ s{(\w+)/\1\b}{$1},
	      $module =~ s{^B/O}{O},
	      $module =~ s{^IO_Compress_Base/lib/}{},
	      $module =~ s{^IO_Compress_Zlib/(?:lib/)?}{},
	      $module =~ s{^Devel/PPPort}{Devel},
	      $module =~ s{^Encode/encoding}{encoding},
	      $module =~ s{^IPC/SysV/}{IPC/},
	      $module =~ s{^List/Util/lib/Scalar}{Scalar},
	      $module =~ s{^MIME/Base64/QuotedPrint}{MIME/QuotedPrint},
	      $module =~ s{^(?:DynaLoader|Errno|Opcode)/}{},
	    );
    $module =~ s{/}{::}g;
    $module =~ s/(\.pm|_pm\.PL)$//;
    $lines{sprintf "\t%-24s=> $version,\n", "'$module'"}++;
}, 'lib', 'ext', 'vms/ext', 'symbian/ext');

if (open my $ucdv, "<", "lib/unicore/version") {
    chomp (my $ucd = <$ucdv>);
    $lines{sprintf "\t%-24s=> '$ucd',\n", "'Unicode'"}++;
    close $ucdv;
    }
print "    $] => {\n";
print sort keys %lines;
print "    },\n";
