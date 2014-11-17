package ManifypodsWithUtf8;
use strict;
use utf8;
use Test::More;

use lib 't/lib';
blib_load('Module::Build');
blib_load('Module::Build::ConfigData');

SKIP: {
   unless ( Module::Build::ConfigData->feature('manpage_support') ) {
     skip 'manpage_support feature is not enabled';
   }
}

use MBTest tests => 2;
use File::Spec::Functions qw( catdir );

use Cwd ();
my $cwd = Cwd::cwd;
my $tmp = MBTest->tmpdir;

use DistGen;
my $dist = DistGen->new( dir => $tmp );
my $content = <<'---';

=encoding utf8

=head1 NAME

Simple::PodWithUtf8 - POD with some (ç á à ô) special chars

=cut
---
utf8::encode($content);
$dist->add_file( 'lib/Simple/PodWithUtf8.pod', $content);
$dist->regen;
$dist->chdir_in;

my $destdir = catdir($cwd, 't', 'install_test' . $$);

my $mb = Module::Build->new(
			    module_name      => $dist->name,
			    install_base     => $destdir,

			    # need default install paths to ensure manpages get generated
			    installdirs => 'site',
			    config => {
			        installsiteman1dir  => catdir($tmp, 'site', 'man', 'man1'),
			        installsiteman3dir  => catdir($tmp, 'site', 'man', 'man3'),
			    },
			    extra_manify_args => { utf8 => 1 },
			);
$mb->add_to_cleanup($destdir);


$mb->dispatch('build');
my $sep = $mb->manpage_separator;
my $ext3 = $mb->config('man3ext');
my $to = File::Spec->catfile('blib', 'libdoc', "Simple${sep}PodWithUtf8.${ext3}");

ok(-e $to, "Manpage is found at $to");
open my $pod, '<:encoding(utf-8)', $to or diag "Could not open $to: $!";
my $pod_content = do { local $/; <$pod> };
close $pod;

like($pod_content, qr/ \(ç á à ô\) /, "POD should contain special characters");

