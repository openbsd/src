#!/usr/bin/perl

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't' if -d 't';
        @INC = '../lib';
    }
}
chdir 't';

use Config;
use ExtUtils::Embed;
use File::Spec;

open(my $fh,">embed_test.c") || die "Cannot open embed_test.c:$!";
print $fh <DATA>;
close($fh);

$| = 1;
print "1..9\n";
my $cc = $Config{'cc'};
my $cl  = ($^O eq 'MSWin32' && $cc eq 'cl');
my $borl  = ($^O eq 'MSWin32' && $cc eq 'bcc32');
my $skip_exe = $^O eq 'os2' && $Config{ldflags} =~ /(?<!\S)-Zexe\b/;
my $exe = 'embed_test';
$exe .= $Config{'exe_ext'} unless $skip_exe;	# Linker will auto-append it
my $obj = 'embed_test' . $Config{'obj_ext'};
my $inc = File::Spec->updir;
my $lib = File::Spec->updir;
my $libperl_copied;
my $testlib;
my @cmd;
my (@cmd2) if $^O eq 'VMS';

if ($^O eq 'VMS') {
    push(@cmd,$cc,"/Obj=$obj");
    my (@incs) = ($inc);
    my $crazy = ccopts();
    if ($crazy =~ s#/inc[^=/]*=([\w\$\_\-\.\[\]\:]+)##i) {
        push(@incs,$1);
    }
    if ($crazy =~ s/-I([a-zA-Z0-9\$\_\-\.\[\]\:]*)//) {
        push(@incs,$1);
    }
    $crazy =~ s#/Obj[^=/]*=[\w\$\_\-\.\[\]\:]+##i;
    push(@cmd,"/Include=(".join(',',@incs).")");
    push(@cmd,$crazy);
    push(@cmd,"embed_test.c");

    push(@cmd2,$Config{'ld'}, $Config{'ldflags'}, "/exe=$exe"); 
    push(@cmd2,"$obj,[-]perlshr.opt/opt,[-]perlshr_attr.opt/opt");

} else {
   if ($cl) {
    push(@cmd,$cc,"-Fe$exe");
   }
   elsif ($borl) {
    push(@cmd,$cc,"-o$exe");
   }
   else {
    push(@cmd,$cc,'-o' => $exe);
   }
   push(@cmd,"-I$inc",ccopts(),'embed_test.c');
   if ($^O eq 'MSWin32') {
    $inc = File::Spec->catdir($inc,'win32');
    push(@cmd,"-I$inc");
    $inc = File::Spec->catdir($inc,'include');
    push(@cmd,"-I$inc");
    if ($cc eq 'cl') {
	push(@cmd,'-link',"-libpath:$lib",$Config{'libperl'},$Config{'libs'});
    }
    else {
	push(@cmd,"-L$lib",File::Spec->catfile($lib,$Config{'libperl'}),$Config{'libc'});
    }
   }
   else { # Not MSWin32.
    push(@cmd,"-L$lib",'-lperl');
    local $SIG{__WARN__} = sub {
	warn $_[0] unless $_[0] =~ /No library found for .*perl/
    };
    push(@cmd, '-Zlinker', '/PM:VIO')	# Otherwise puts a warning to STDOUT!
	if $^O eq 'os2' and $Config{ldflags} =~ /(?<!\S)-Zomf\b/;
    push(@cmd,ldopts());
   }
   if ($borl) {
     @cmd = ($cmd[0],(grep{/^-[LI]/}@cmd[1..$#cmd]),(grep{!/^-[LI]/}@cmd[1..$#cmd]));
   }

   if ($^O eq 'aix') { # AIX needs an explicit symbol export list.
    my ($perl_exp) = grep { -f } qw(perl.exp ../perl.exp);
    die "where is perl.exp?\n" unless defined $perl_exp;
    for (@cmd) {
        s!-bE:(\S+)!-bE:$perl_exp!;
    }
   }
   elsif ($^O eq 'cygwin') { # Cygwin needs the shared libperl copied
     my $v_e_r_s = $Config{version};
     $v_e_r_s =~ tr/./_/;
     system("cp ../cygperl$v_e_r_s.dll ./");    # for test 1
   }
   elsif ($Config{'libperl'} !~ /\Alibperl\./) {
     # Everyone needs libperl copied if it's not found by '-lperl'.
     $testlib = $Config{'libperl'};
     my $srclib = $testlib;
     $testlib =~ s/.+(?=\.[^.]*)/libperl/;
     $testlib = File::Spec::->catfile($lib, $testlib);
     $srclib = File::Spec::->catfile($lib, $srclib);
     if (-f $srclib) {
       unlink $testlib if -f $testlib;
       my $ln_or_cp = $Config{'ln'} || $Config{'cp'};
       my $lncmd = "$ln_or_cp $srclib $testlib";
       #print "# $lncmd\n";
       $libperl_copied = 1	unless system($lncmd);
     }
   }
}
my $status;
# On OS/2 the linker will always emit an empty line to STDOUT; filter these
my $cmd = join ' ', @cmd;
chomp($cmd); # where is the newline coming from? ldopts()?
print "# $cmd\n";
my @out = `$cmd`;
$status = $?;
print "# $_\n" foreach @out;

if ($^O eq 'VMS' && !$status) {
  print "# @cmd2\n";
  $status = system(join(' ',@cmd2)); 
}
print (($status? 'not ': '')."ok 1\n");

my $embed_test = File::Spec->catfile(File::Spec->curdir, $exe);
$embed_test = "run/nodebug $exe" if $^O eq 'VMS';
print "# embed_test = $embed_test\n";
$status = system($embed_test);
print (($status? 'not ':'')."ok 9 # system returned $status\n");
unlink($exe,"embed_test.c",$obj);
unlink("$exe$Config{exe_ext}") if $skip_exe;
unlink("embed_test.map","embed_test.lis") if $^O eq 'VMS';
unlink(glob("./*.dll")) if $^O eq 'cygwin';
unlink($testlib)	       if $libperl_copied;

# gcc -g -I.. -L../ -o perl_test perl_test.c -lperl `../perl -I../lib -MExtUtils::Embed -I../ -e ccopts -e ldopts`

__END__

/* perl_test.c */

#include <EXTERN.h>
#include <perl.h>

#define my_puts(a) if(puts(a) < 0) exit(666)

static char *cmds[] = { "perl","-e", "$|=1; print qq[ok 5\\n]", NULL };

int main(int argc, char **argv, char **env)
{
    PerlInterpreter *my_perl;

    PERL_SYS_INIT3(&argc,&argv,&env);

    my_perl = perl_alloc();

    my_puts("ok 2");

    perl_construct(my_perl);

    my_puts("ok 3");

    perl_parse(my_perl, NULL, (sizeof(cmds)/sizeof(char *))-1, cmds, env);

    my_puts("ok 4");

    fflush(stdout);

    perl_run(my_perl);

    my_puts("ok 6");

    perl_destruct(my_perl);

    my_puts("ok 7");

    perl_free(my_perl);

    my_puts("ok 8");

    PERL_SYS_TERM();

    return 0;
}
