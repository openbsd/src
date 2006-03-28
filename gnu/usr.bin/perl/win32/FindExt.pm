package FindExt;

our $VERSION = '1.01';

use strict;
use warnings;

my $no = join('|',qw(GDBM_File ODBM_File NDBM_File DB_File
		     Syslog SysV Langinfo));
$no = qr/^(?:$no)$/i;

my %ext;
my $ext;
my %static;

sub getcwd {
    $ENV{'PWD'} = Win32::GetCwd();
    $ENV{'PWD'} =~ s:\\:/:g ;
    return $ENV{'PWD'};
}

sub set_static_extensions
{
    # adjust results of scan_ext, and also save
    # statics in case scan_ext hasn't been called yet.
    %static = ();
    for (@_) {
        $static{$_} = 1;
        $ext{$_} = 'static' if $ext{$_} && $ext{$_} eq 'dynamic';
    }
}

sub scan_ext
{
 my $here = getcwd();
 my $dir  = shift;
 chdir($dir) || die "Cannot cd to $dir\n";
 ($ext = getcwd()) =~ s,/,\\,g;
 find_ext('');
 chdir($here) || die "Cannot cd to $here\n";
 my @ext = extensions();
}

sub dynamic_ext
{
 return sort grep $ext{$_} eq 'dynamic',keys %ext;
}

sub static_ext
{
 return sort grep $ext{$_} eq 'static',keys %ext;
}

sub nonxs_ext
{
 return sort grep $ext{$_} eq 'nonxs',keys %ext;
}

sub extensions
{
 return sort grep $ext{$_} ne 'known',keys %ext;
}

sub known_extensions
{
 # faithfully copy Configure in not including nonxs extensions for the nonce
 return sort grep $ext{$_} ne 'nonxs',keys %ext;
}

sub is_static
{
 return $ext{$_[0]} eq 'static'
}

# Function to recursively find available extensions, ignoring DynaLoader
# NOTE: recursion limit of 10 to prevent runaway in case of symlink madness
sub find_ext
{
    opendir my $dh, '.';
    my @items = grep { !/^\.\.?$/ } readdir $dh;
    closedir $dh;
    for my $xxx (@items) {
        if ($xxx ne "DynaLoader") {
            if (-f "$xxx/$xxx.xs") {
                $ext{"$_[0]$xxx"} = $static{"$_[0]$xxx"} ? 'static' : 'dynamic';
            } elsif (-f "$xxx/Makefile.PL") {
                $ext{"$_[0]$xxx"} = 'nonxs';
            } else {
                if (-d $xxx && @_ < 10) {
                    chdir $xxx;
                    find_ext("$_[0]$xxx/", @_);
                    chdir "..";
                }
            }
            $ext{"$_[0]$xxx"} = 'known' if $ext{"$_[0]$xxx"} && $xxx =~ $no;
        }
    }

# Special case:  Add in threads/shared since it is not picked up by the
# recursive find above (and adding in general recursive finding breaks
# SDBM_File/sdbm).  A.D.  10/25/2001.

    if (!$_[0] && -d "threads/shared") {
        $ext{"threads/shared"} = 'dynamic';
    }
}

1;
