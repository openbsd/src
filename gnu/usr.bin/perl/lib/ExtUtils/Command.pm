package ExtUtils::Command;

use 5.00503;
use strict;
use Carp;
use File::Copy;
use File::Compare;
use File::Basename;
use File::Path qw(rmtree);
require Exporter;
use vars qw(@ISA @EXPORT $VERSION);
@ISA     = qw(Exporter);
@EXPORT  = qw(cp rm_f rm_rf mv cat eqtime mkpath touch test_f);
$VERSION = '1.05';

my $Is_VMS = $^O eq 'VMS';

=head1 NAME

ExtUtils::Command - utilities to replace common UNIX commands in Makefiles etc.

=head1 SYNOPSIS

  perl -MExtUtils::Command       -e cat files... > destination
  perl -MExtUtils::Command       -e mv source... destination
  perl -MExtUtils::Command       -e cp source... destination
  perl -MExtUtils::Command       -e touch files...
  perl -MExtUtils::Command       -e rm_f files...
  perl -MExtUtils::Command       -e rm_rf directories...
  perl -MExtUtils::Command       -e mkpath directories...
  perl -MExtUtils::Command       -e eqtime source destination
  perl -MExtUtils::Command       -e test_f file
  perl -MExtUtils::Command=chmod -e chmod mode files...

=head1 DESCRIPTION

The module is used to replace common UNIX commands.  In all cases the
functions work from @ARGV rather than taking arguments.  This makes
them easier to deal with in Makefiles.

  perl -MExtUtils::Command -e some_command some files to work on

I<NOT>

  perl -MExtUtils::Command -e 'some_command qw(some files to work on)'

Filenames with * and ? will be glob expanded.

=over 4

=cut

# VMS uses % instead of ? to mean "one character"
my $wild_regex = $Is_VMS ? '*%' : '*?';
sub expand_wildcards
{
 @ARGV = map(/[$wild_regex]/o ? glob($_) : $_,@ARGV);
}


=item cat 

Concatenates all files mentioned on command line to STDOUT.

=cut 

sub cat ()
{
 expand_wildcards();
 print while (<>);
}

=item eqtime src dst

Sets modified time of dst to that of src

=cut 

sub eqtime
{
 my ($src,$dst) = @ARGV;
 local @ARGV = ($dst);  touch();  # in case $dst doesn't exist
 utime((stat($src))[8,9],$dst);
}

=item rm_rf files....

Removes directories - recursively (even if readonly)

=cut 

sub rm_rf
{
 expand_wildcards();
 rmtree([grep -e $_,@ARGV],0,0);
}

=item rm_f files....

Removes files (even if readonly)

=cut 

sub rm_f
{
 expand_wildcards();
 foreach (@ARGV)
  {
   next unless -f $_;
   next if unlink($_);
   chmod(0777,$_);
   next if unlink($_);
   carp "Cannot delete $_:$!";
  }
}

=item touch files ...

Makes files exist, with current timestamp 

=cut 

sub touch {
    my $t    = time;
    expand_wildcards();
    foreach my $file (@ARGV) {
        open(FILE,">>$file") || die "Cannot write $file:$!";
        close(FILE);
        utime($t,$t,$file);
    }
}

=item mv source... destination

Moves source to destination.
Multiple sources are allowed if destination is an existing directory.

=cut 

sub mv {
    my $dst = pop(@ARGV);
    expand_wildcards();
    croak("Too many arguments") if (@ARGV > 1 && ! -d $dst);
    foreach my $src (@ARGV) {
        move($src,$dst);
    }
}

=item cp source... destination

Copies source to destination.
Multiple sources are allowed if destination is an existing directory.

=cut

sub cp {
    my $dst = pop(@ARGV);
    expand_wildcards();
    croak("Too many arguments") if (@ARGV > 1 && ! -d $dst);
    foreach my $src (@ARGV) {
        copy($src,$dst);
    }
}

=item chmod mode files...

Sets UNIX like permissions 'mode' on all the files.  e.g. 0666

=cut 

sub chmod {
    my $mode = shift(@ARGV);
    expand_wildcards();
    chmod(oct $mode,@ARGV) || die "Cannot chmod ".join(' ',$mode,@ARGV).":$!";
}

=item mkpath directory...

Creates directory, including any parent directories.

=cut 

sub mkpath
{
 expand_wildcards();
 File::Path::mkpath([@ARGV],0,0777);
}

=item test_f file

Tests if a file exists

=cut 

sub test_f
{
 exit !-f shift(@ARGV);
}


1;
__END__ 

=back

=head1 BUGS

Should probably be Auto/Self loaded.

=head1 SEE ALSO 

ExtUtils::MakeMaker, ExtUtils::MM_Unix, ExtUtils::MM_Win32

=head1 AUTHOR

Nick Ing-Simmons <F<nick@ni-s.u-net.com>>.

=cut

