#	$OpenBSD: tk.pl,v 1.2 2001/01/29 01:58:48 niklas Exp $

# make sure every subprocess has it's exit and that the main one
# hasn't
sub fun {
    unless ($pid = fork) {
        unless (fork) {
            use Tk;
            $MW = MainWindow->new;
            $hello = $MW->Button(
                -text    => 'Hello, world',
                -command => sub {exit;},
            );
            $hello->pack;
            MainLoop;
        }
        exit 0;
    }
    waitpid($pid, 0);
}

1;
