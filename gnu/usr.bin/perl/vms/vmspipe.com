$! 'f$verify(0)         
$!  ---  protect against nonstandard definitions ---
$ perl_define = "define/nolog"
$ perl_on     = "on error then exit $STATUS"
$ perl_exit   = "exit"
$ perl_del    = "delete"
$ pif         = "if"
$!  --- define i/o redirection (sys$output set by lib$spawn)
$ pif perl_popen_in  .nes. "" then perl_define/user/name_attributes=confine sys$input  'perl_popen_in'
$ pif perl_popen_err .nes. "" then perl_define/user/name_attributes=confine sys$error  'perl_popen_err'
$ pif perl_popen_out .nes. "" then perl_define      sys$output 'perl_popen_out'
$ cmd = perl_popen_cmd
$!  --- get rid of global symbols
$ perl_del/symbol/global perl_popen_in
$ perl_del/symbol/global perl_popen_err
$ perl_del/symbol/global perl_popen_out
$ perl_del/symbol/global perl_popen_cmd
$ perl_on
$ 'cmd
$ perl_exit '$STATUS'
