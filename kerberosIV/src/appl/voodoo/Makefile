LD = link

CPPFLAGS = /nologo /Zi /I ../../include /I ../../include/win32 /I ..\..\lib/des /I ../../lib/krb /I ..\..\lib\kclient /I ../../lib/roken

OBJECTS = \
	AuthOption.obj				\
	CharStream.obj				\
	CryptoEngine.obj			\
	DenyAllOption.obj			\
	EmulatorEngine.obj			\
	EncryptOption.obj			\
	Negotiator.obj				\
	Option.obj				\
	TelnetApp.obj				\
	TelnetEngine.obj			\
	TelnetSession.obj			\
	TerminalEngine.obj			\
	WinSizeOption.obj			\
	YesNoOptions.obj


LDFLAGS = /nologo /subsystem:windows /incremental:no /machine:I386

LIBS =..\..\lib\kclient\Debug\kclnt32.lib	\
	..\..\lib\krb\Debug\krb.lib 		\
	..\..\lib\des\Debug\des.lib 		\
	wsock32.lib kernel32.lib user32.lib	\
	gdi32.lib winspool.lib			\
	comdlg32.lib advapi32.lib shell32.lib 	\
	ole32.lib oleaut32.lib uuid.lib		\
	odbc32.lib odbccp32.lib




voodoo.exe: $(OBJECTS)
	$(LD) /out:voodoo.exe /pdb:voodoo.pdb $(OBJECTS) TelnetResource.res \
		$(LDFLAGS) $(LIBS)


clean:
	erase $(OBJECTS) voodoo.exe