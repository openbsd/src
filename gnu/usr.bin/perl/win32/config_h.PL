#
BEGIN { warn "Running ".__FILE__."\n" };
BEGIN 
 {
  require "Config.pm";
  die "Config.pm:$@" if $@;
  Config::->import;
 }
use File::Compare qw(compare);
use File::Copy qw(copy);
use File::Basename qw(fileparse);
my ($name, $dir) = fileparse($0);
$name =~ s#^(.*)\.PL$#../$1.SH#;
my %opt;
while (@ARGV && $ARGV[0] =~ /^([\w_]+)=(.*)$/)
 {
  $opt{$1}=$2;
  shift(@ARGV);
 }

$opt{CONFIG_H} ||= 'config.h';
$opt{CORE_DIR} ||= '../lib/CORE';

warn "Writing $opt{CONFIG_H}\n";

my $patchlevel = $opt{INST_VER};
$patchlevel =~ s|^[\\/]||;
$patchlevel =~ s|~VERSION~|$Config{version}|g;
$patchlevel ||= $Config{version};
$patchlevel = qq["$patchlevel"];

open(SH,"<$name") || die "Cannot open $name:$!";
while (<SH>)
 {
  last if /^\s*sed/;
 }
($term,$file,$pat) = /^\s*sed\s+<<(\S+)\s+>(\S+)\s+(.*)$/;

$file =~ s/^\$(\w+)$/$opt{$1}/g;

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
#binmode H;		# no CRs (which cause a spurious rebuild)
while (<SH>)
 {
  last if /^$term$/o;
  s/\$([\w_]+)/Config($1)/eg;
  s/`([^\`]*)`/BackTick($1)/eg;
  munge();
  s/\\\$/\$/g;
  s#/[ *\*]*\*/#/**/#;
  s#(.)/\*\*/#$1/ **/# if(/^\/\*/); #avoid "/*" inside comments
  if (/^\s*#define\s+(PRIVLIB|SITELIB|VENDORLIB)_EXP/)
   {
     $_ = "#define ". $1 . "_EXP (win32_get_". lc($1) . "(PERL_VERSION_STRING, NULL))\t/**/\n";
   }
  # incpush() handles archlibs, so disable them
  elsif (/^\s*#define\s+(ARCHLIB|SITEARCH|VENDORARCH)_EXP/)
   {
     $_ = "/*#define ". $1 . "_EXP \"\"\t/ **/\n";
   }
  print H;
 }
close(H);
close(SH);


chmod(0666,"$opt{CORE_DIR}/$opt{CONFIG_H}");
copy("$file.new","$opt{CORE_DIR}/$opt{CONFIG_H}") || die "Cannot copy:$!";
chmod(0444,"$opt{CORE_DIR}/$opt{CONFIG_H}");

if (compare("$file.new",$file))
 {
  warn "$file has changed\n";
  chmod(0666,$file);
  unlink($file);
  rename("$file.new",$file);
  #chmod(0444,$file);
  exit(1);
 }
else
 {
  unlink ("$file.new");
  exit(0);
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
