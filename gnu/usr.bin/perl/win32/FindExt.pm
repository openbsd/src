package FindExt;

our $VERSION = '1.00';

# We (probably) have not got a Config.pm yet 
BEGIN { $INC{'Config.pm'} = __FILE__ };

use strict;
use File::Find;
use File::Basename;
use Cwd;

my $no = join('|',qw(DynaLoader GDBM_File ODBM_File NDBM_File DB_File
		     Syslog SysV Langinfo));
$no = qr/^(?:$no)$/i;

my %ext;
my $ext;
sub scan_ext
{
 my $here = getcwd();
 my $dir  = shift;
 chdir($dir) || die "Cannot cd to $dir\n";
 ($ext = getcwd()) =~ s,/,\\,g;
 find(\&find_ext,'.');
 chdir($here) || die "Cannot cd to $here\n";
 my @ext = extensions();
}

sub dynamic_extensions
{
 return grep $ext{$_} eq 'dynamic',keys %ext;
}

sub noxs_extensions
{
 return grep $ext{$_} eq 'nonxs',keys %ext;
}

sub extensions
{
 return keys %ext;
}

sub find_ext
{
 if (/^(.*)\.pm$/i || /^(.*)_pm\.PL$/i || /^(.*)\.xs$/i)
  {
   my $name = $1;
   return if $name =~ $no; 
   my $dir = $File::Find::dir; 
   $dir =~ s,./,,;
   return if exists $ext{$dir};
   return unless -f "$ext/$dir/Makefile.PL";
   if ($dir =~ /$name$/i)
    {
     if (-f "$ext/$dir/$name.xs")
      {
       $ext{$dir} = 'dynamic'; 
      }
     else
      {
       $ext{$dir} = 'nonxs'; 
      }
    }
  }
}

1;
