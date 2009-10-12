#!perl
# Generates info for Module::CoreList from this perl tree
# run this from the root of a perl tree
#
# Data is on STDOUT.
#
# With an optional arg specifying the root of a CPAN mirror, outputs the
# %upstream and %bug_tracker hashes too.

use strict;
use warnings;
use File::Find;
use ExtUtils::MM_Unix;
use version;
use lib "Porting";
use Maintainers qw(%Modules files_to_modules);
use File::Spec;
use Parse::CPAN::Meta;

my $corelist_file = 'lib/Module/CoreList.pm';

my %lines;
my %module_to_file;
my %modlist;

die "usage: $0 [ cpan-mirror/ ] [ 5.x.y] \n" unless @ARGV <= 2;
my $cpan         = shift;
my $raw_version  = shift || $];
my $perl_version = version->parse("$raw_version");
my $perl_vnum    = $perl_version->numify;
my $perl_vstring = $perl_version->normal; # how do we get version.pm to not give us leading v?
$perl_vstring =~ s/^v//;

if ( !-f 'MANIFEST' ) {
    die "Must be run from the root of a clean perl tree\n";
}

open( my $corelist_fh, '<', $corelist_file ) || die "Could not open $corelist_file: $!";
my $corelist = join( '', <$corelist_fh> );

if ($cpan) {
    my $modlistfile = File::Spec->catfile( $cpan, 'modules', '02packages.details.txt' );
    my $content;

    my $fh;
    if ( -e $modlistfile ) {
        warn "Reading the module list from $modlistfile";
        open $fh, '<', $modlistfile or die "Couldn't open $modlistfile: $!";
    } elsif ( -e $modlistfile . ".gz" ) {
        warn "Reading the module list from $modlistfile.gz";
        open $fh, '-|', "gzcat $modlistfile.gz" or die "Couldn't zcat $modlistfile.gz: $!";
    } else {
        warn "About to fetch 02packages from ftp.funet.fi. This may take a few minutes\n";
        $content = fetch_url('http://ftp.funet.fi/pub/CPAN/modules/02packages.details.txt');
        unless ($content) {
            die "Unable to read 02packages.details.txt from either your CPAN mirror or ftp.funet.fi";
        }
    }

    if ( $fh and !$content ) {
        local $/ = "\n";
        $content = join( '', <$fh> );
    }

    die "Incompatible modlist format"
        unless $content =~ /^Columns: +package name, version, path/m;

    # Converting the file to a hash is about 5 times faster than a regexp flat
    # lookup.
    for ( split( qr/\n/, $content ) ) {
        next unless /^([A-Za-z_:0-9]+) +[-0-9.undefHASHVERSIONvsetwhenloadingbogus]+ +(\S+)/;
        $modlist{$1} = $2;
    }
}

find(
    sub {
        /(\.pm|_pm\.PL)$/ or return;
        /PPPort\.pm$/ and return;
        my $module = $File::Find::name;
        $module =~ /\b(demo|t|private)\b/ and return;    # demo or test modules
        my $version = MM->parse_version($_);
        defined $version or $version = 'undef';
        $version =~ /\d/ and $version = "'$version'";

        # some heuristics to figure out the module name from the file name
        $module =~ s{^(lib|(win32/|vms/|symbian/)?ext)/}{}
            and $1 ne 'lib'
            and (
            $module =~ s{\b(\w+)/\1\b}{$1},
            $module =~ s{^B/O}{O},
            $module =~ s{^Devel-PPPort}{Devel},
            $module =~ s{^Encode/encoding}{encoding},
            $module =~ s{^IPC-SysV/}{IPC/},
            $module =~ s{^MIME-Base64/QuotedPrint}{MIME/QuotedPrint},
            $module =~ s{^(?:DynaLoader|Errno|Opcode)/}{},
            );
        $module =~ s{/}{::}g;
        $module =~ s{-}{::}g;
        $module =~ s{^.*::lib::}{};
        $module =~ s/(\.pm|_pm\.PL)$//;
        $lines{$module}          = $version;
        $module_to_file{$module} = $File::Find::name;
    },
    'lib',
    'ext',
    'vms/ext',
    'symbian/ext'
);

-e 'configpm' and $lines{Config} = 'undef';

if ( open my $ucdv, "<", "lib/unicore/version" ) {
    chomp( my $ucd = <$ucdv> );
    $lines{Unicode} = "'$ucd'";
    close $ucdv;
}

my $versions_in_release = "    " . $perl_vnum . " => {\n";
foreach my $key ( sort keys %lines ) {
    $versions_in_release .= sprintf "\t%-24s=> %s,\n", "'$key'", $lines{$key};
}
$versions_in_release .= "    },\n";

$corelist =~ s/^(%version\s*=\s*.*?)(^\);)$/$1$versions_in_release$2/xism;

exit unless %modlist;

# We have to go through this two stage lookup, given how Maintainers.pl keys its
# data by "Module", which is really a dist.
my $file_to_M = files_to_modules( values %module_to_file );

my %module_to_upstream;
my %module_to_dist;
my %dist_to_meta_YAML;
while ( my ( $module, $file ) = each %module_to_file ) {
    my $M = $file_to_M->{$file};
    next unless $M;
    next if $Modules{$M}{MAINTAINER} eq 'p5p';
    $module_to_upstream{$module} = $Modules{$M}{UPSTREAM};
    next
        if defined $module_to_upstream{$module}
            && $module_to_upstream{$module} =~ /^(?:blead|first-come)$/;
    my $dist = $modlist{$module};
    unless ($dist) {
        warn "Can't find a distribution for $module\n";
        next;
    }
    $module_to_dist{$module} = $dist;

    next if exists $dist_to_meta_YAML{$dist};

    $dist_to_meta_YAML{$dist} = undef;

    # Like it or lump it, this has to be Unix format.
    my $meta_YAML_path = "authors/id/$dist";
    $meta_YAML_path =~ s/(?:tar\.gz|zip)$/meta/ or die "$meta_YAML_path";
    my $meta_YAML_url = 'http://ftp.funet.fi/pub/CPAN/' . $meta_YAML_path;

    if ( -e "$cpan/$meta_YAML_path" ) {
        $dist_to_meta_YAML{$dist} = Parse::CPAN::Meta::LoadFile( $cpan . "/" . $meta_YAML_path );
    } elsif ( my $content = fetch_url($meta_YAML_url) ) {
        unless ($content) {
            warn "Failed to fetch $meta_YAML_url\n";
            next;
        }
        eval { $dist_to_meta_YAML{$dist} = Parse::CPAN::Meta::Load($content); };
        if ( my $err = $@ ) {
            warn "$meta_YAML_path: ".$err;
            next;
        }
    } else {
        warn "$meta_YAML_path does not exist for $module\n";

        # I tried code to open the tarballs with Archive::Tar to find and
        # extract META.yml, but only Text-Tabs+Wrap-2006.1117.tar.gz had one,
        # so it's not worth including.
        next;
    }
}

my $upstream_stanza = "%upstream = (\n";
foreach my $module ( sort keys %module_to_upstream ) {
    my $upstream = defined $module_to_upstream{$module} ? "'$module_to_upstream{$module}'" : 'undef';
    $upstream_stanza .= sprintf "    %-24s=> %s,\n", "'$module'", $upstream;
}
$upstream_stanza .= ");";

$corelist =~ s/^%upstream .*? ;$/$upstream_stanza/ismx;

my $tracker = "%bug_tracker = (\n";
foreach my $module ( sort keys %module_to_upstream ) {
    my $upstream = defined $module_to_upstream{$module};
    next
        if defined $upstream
            and $upstream eq 'blead' || $upstream eq 'first-come';

    my $bug_tracker;

    my $dist = $module_to_dist{$module};
    $bug_tracker = $dist_to_meta_YAML{$dist}->{resources}{bugtracker}
        if $dist;

    $bug_tracker = defined $bug_tracker ? "'$bug_tracker'" : 'undef';
    next if $bug_tracker eq "'http://rt.perl.org/perlbug/'";
    $tracker .= sprintf "    %-24s=> %s,\n", "'$module'", $bug_tracker;
}
$tracker .= ");";

$corelist =~ s/^%bug_tracker .*? ;/$tracker/eismx;

unless ( $corelist =~ /and $perl_vstring releases of perl/ ) {
    warn "Adding $perl_vstring to the list of perl versions covered by Module::CoreList\n";
    $corelist =~ s/\s*and (.*?) releases of perl/, $1 and $perl_vstring releases of perl/ism;
}

unless (
    $corelist =~ /^%released \s* = \s* \( 
        .*? 
        $perl_vnum => .*? 
        \);/ismx
    )
{
    warn "Adding $perl_vnum to the list of released perl versions. Please consider adding a release date.\n";
    $corelist =~ s/^(%released \s* = \s* .*?) ( \) )
                /$1 $perl_vnum => '????-??-??',\n  $2/ismx;
}

write_corelist($corelist);

warn "All done. Please check over lib/Module/CoreList.pm carefully before committing. Thanks!\n";


sub write_corelist {
    my $content = shift;
    open ( my $clfh, ">", "lib/Module/CoreList.pm") || die "Failed to open lib/Module/CoreList.pm for writing: $!";
    print $clfh $content || die "Failed to write the new CoreList.pm: $!";
    close($clfh);
}

sub fetch_url {
    my $url = shift;
    eval { require LWP::Simple };
    if ( LWP::Simple->can('get') ) {
        return LWP::Simple->get($url);
    } elsif (`which curl`) {
        return `curl -s $url`;
    } elsif (`which wget`) {
        return `wget -q -O - $url`;
    }
}
