$ CC :== CC/NOOPT/DEB/STANDARD=VAXC/DEFINE=HAVE_CONFIG_H-
/INCLUDE_DIR=([-],[-.VMS],[-.LIB])/PREFIX=ALL
$ CC add.c
$ CC admin.c
$ CC buffer.c
$ CC checkin.c
$ CC checkout.c
$ CC classify.c
$ CC client.c
$ CC commit.c
$ CC create_adm.c
$ CC cvsrc.c
$ CC diff.c
$ CC edit.c
$ CC entries.c
$ CC error.c
$ CC expand_path.c
$ CC fileattr.c
$ CC find_names.c
$ CC hash.c
$ CC history.c
$ CC ignore.c
$ CC import.c
$ CC lock.c
$ CC log.c
$ CC login.c
$ CC logmsg.c
$ CC main.c
$ CC mkmodules.c
$ CC modules.c
$ CC myndbm.c
$ CC no_diff.c
$ CC parseinfo.c
$ CC patch.c
$ CC rcs.c
$ CC/INCLUDE_DIR=([-],[-.VMS],[-.LIB],[-.diff]) rcscmds.c
$ CC recurse.c
$ CC release.c
$ CC remove.c
$ CC repos.c
$ CC root.c
$ CC run.c
$ CC scramble.c
$ CC server.c
$ CC status.c
$ CC subr.c
$ CC tag.c
$ CC update.c
$ CC version.c
$ CC vers_ts.c
$ CC watch.c
$ CC wrapper.c
$ CC/INCLUDE_DIR=([-],[-.VMS],[-.LIB],[-.zlib]) zlib.c
$ LIBRARY/CREATE cvslib.olb add.obj,admin.obj,buffer.obj,checkin.obj,-
checkout.obj,-
classify.obj,client.obj,commit.obj,create_adm.obj,cvsrc.obj,-
diff.obj,edit.obj,entries.obj,error.obj,expand_path.obj,fileattr.obj,-
find_names.obj,hash.obj,history.obj,ignore.obj,import.obj,-
lock.obj,log.obj,login.obj,logmsg.obj,mkmodules.obj,modules.obj,myndbm.obj,-
no_diff.obj,-
parseinfo.obj,patch.obj,rcs.obj,rcscmds.obj,recurse.obj,release.obj,-
remove.obj,repos.obj,root.obj,run.obj,scramble.obj,server.obj,-
status.obj,-
subr.obj,tag.obj,update.obj,version.obj,vers_ts.obj,watch.obj,wrapper.obj,-
zlib.obj
$ link/nodeb/exe=cvs.exe main.obj,cvslib.olb/lib,[-.vms]openvmslib.olb/lib,-
    [-.zlib]zlib.olb/lib,[-.diff]diff.olb/lib,[-.lib]gnulib.olb/lib
