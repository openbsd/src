# osr5 needs to explicitly link against libc to pull in usleep
$self->{LIBS} = ['-lc'];

