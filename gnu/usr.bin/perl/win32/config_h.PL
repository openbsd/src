#
use Config;
use File::Compare qw(compare);
use File::Copy qw(copy);
my $name = $0;
$name =~ s#^(.*)\.PL$#../$1.SH#;
open(SH,"<$name") || die "Cannot open $name:$!";
while (<SH>)
 {
  last if /^sed/;
 }
($term,$file,$pat) = /^sed\s+<<(\S+)\s+>(\S+)\s+(.*)$/;

my $str = "sub munge\n{\n";

while ($pat =~ s/-e\s+'([^']*)'\s*//)
 {
  my $e = $1;
  $e =~ s/\\([\(\)])/$1/g;
  $e =~ s/\\(\d)/\$$1/g; 
  $str .= "$e;\n";
 }
$str .= "}\n";

eval $str;

die "$str:$@" if $@;

open(H,">$file.new") || die "Cannot open $file.new:$!";
while (<SH>)
 {
  last if /^$term$/o;
  s/\$([\w_]+)/Config($1)/eg;
  s/`([^\`]*)`/BackTick($1)/eg;
  munge();
  s/\\\$/\$/g;
  s#/[ *\*]*\*/#/**/#;
  if (/^\s*#define\s+ARCHLIB_EXP/)
   {
     $_ = "#define ARCHLIB_EXP (win32PerlLibPath())\t/**/\n"
        . "#define APPLLIB_EXP (win32SiteLibPath())\t/**/\n";
   }
  print H;
 }
print H "#include <win32.h>
#ifndef DEBUGGING
#define DEBUGGING
#endif
";
close(H);
close(SH);


chmod(0666,"../lib/CORE/config.h");
copy("$file.new","../lib/CORE/config.h") || die "Cannot copy:$!";
chmod(0444,"../lib/CORE/config.h");

if (compare("$file.new",$file))
 {
  warn "$file has changed\n";
  chmod(0666,$file);
  unlink($file);
  rename("$file.new",$file);
  chmod(0444,$file);
  exit(1);
 }

sub Config
{
 my $var = shift;
 my $val = $Config{$var};
 $val = 'undef' unless defined $val;
 $val =~ s/\\/\\\\/g;
 return $val;
}

sub BackTick
{
 my $cmd = shift;
 if ($cmd =~ /^echo\s+(.*?)\s*\|\s+sed\s+'(.*)'\s*$/)
  {
   local ($data,$pat) = ($1,$2);
   $data =~ s/\s+/ /g;
   eval "\$data =~ $pat";
   return $data;
  }
 else
  {
   die "Cannot handle \`$cmd\`";
  }
 return $cmd;
}
