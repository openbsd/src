use ExtUtils::MakeMaker;

$define = '-DSDBM -DDUFF';
$define .= ' -DWIN32' if ($^O eq 'MSWin32');

WriteMakefile(
    NAME      => 'sdbm', # (doesn't matter what the name is here) oh yes it does
    LINKTYPE  => 'static',
    DEFINE    => $define,
    INC       => '-I$(PERL_INC)', # force PERL_INC dir ahead of system -I's
    SKIP      => [qw(dynamic dynamic_lib)],
    OBJECT    => '$(O_FILES)',
    clean     => {'FILES' => 'dbu libsdbm.a dbd dba dbe x-dbu *.dir *.pag'},
    H         => [qw(tune.h sdbm.h pair.h $(PERL_INC)/config.h)],
    C         => [qw(sdbm.c pair.c hash.c)]
);

sub MY::post_constants {
'
INST_STATIC = libsdbm$(LIB_EXT)
'
}

sub MY::top_targets {
	'
all :: static

config ::

lint:
	lint -abchx $(LIBSRCS)
';
}
