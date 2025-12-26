use Test2::V0 -target => 'Test2::Tools::GenTemp';

use ok $CLASS => 'gen_temp';

use File::Spec;

use IO::Handle;

imported_ok qw/gen_temp/;

my $tmp = gen_temp(
    -tempdir => [CLEANUP => 1, TMPDIR => 1],
    foo => "foo\n",
    bar => "bar\n",
    subdir => {
        baz => "baz\n",
        nested => {
            bat => "bat",
        },
    },
);

ok($tmp, "Got a temp dir ($tmp)");

ok(-d File::Spec->canonpath($_), "Created dir $_") for (
    $tmp,
    File::Spec->catdir($tmp, 'subdir'),
    File::Spec->catdir($tmp, 'subdir', 'nested'),
);

for my $file (qw{foo bar subdir/baz subdir/nested/bat}) {
    my $cp = File::Spec->catfile($tmp, $file);
    ok(-f $cp, "Created file $file");
    open(my $fh, '<', $cp) or die "Could not open file '$cp': $!";
    my $content = $file;
    $content =~ s{^.*/}{}g;
    $content .= "\n" unless $content eq 'bat';
    my $printable = $content;
    $printable =~ s/\n/\\n/;
    is(<$fh>, $content, "Got content ($printable)");
    ok($fh->eof, "$file At EOF");
}

done_testing;
