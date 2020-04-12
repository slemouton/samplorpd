current:
	echo make pd_linux, pd_nt, pd_irix5, or pd_irix6

clean: ; rm -f *.pd_linux *.pd_darwin *.o

# ----------------------- NT -----------------------

pd_nt: obj1.dll obj2.dll obj3.dll obj4.dll obj5.dll dspobj~.dll

.SUFFIXES: .obj .dll

PDNTCFLAGS = /W3 /WX /DNT /DPD /nologo
VC="D:\Program Files\Microsoft Visual Studio\Vc98"

PDNTINCLUDE = /I. /I\tcl\include /I..\..\src /I$(VC)\include

PDNTLDIR = $(VC)\lib
PDNTLIB = $(PDNTLDIR)\libc.lib \
	$(PDNTLDIR)\oldnames.lib \
	$(PDNTLDIR)\kernel32.lib \
	..\..\bin\pd.lib 

.c.dll:
	cl $(PDNTCFLAGS) $(PDNTINCLUDE) /c $*.c
	link /dll /export:$*_setup $*.obj $(PDNTLIB)

# override explicitly for tilde objects like this:
dspobj~.dll: dspobj~.c; 
	cl $(PDNTCFLAGS) $(PDNTINCLUDE) /c $*.c
	link /dll /export:dspobj_tilde_setup $*.obj $(PDNTLIB)

# ----------------------- IRIX 5.x -----------------------

pd_irix5: obj1.pd_irix5 obj2.pd_irix5 \
    obj3.pd_irix5 obj4.pd_irix5 obj5.pd_irix5 dspobj~.pd_irix5

.SUFFIXES: .pd_irix5

SGICFLAGS5 = -o32 -DPD -DUNIX -DIRIX -O2


SGIINCLUDE =  -I../../src/

.c.pd_irix5:
	cc $(SGICFLAGS5) $(SGIINCLUDE) -o $*.o -c $*.c
	ld -elf -shared -rdata_shared -o $*.pd_irix5 $*.o
	rm $*.o

# ----------------------- LINUX i386 -----------------------

pd_linux: obj1.pd_linux obj2.pd_linux obj3.pd_linux obj4.pd_linux \
    obj5.pd_linux dspobj~.pd_linux

.SUFFIXES: .pd_linux

LINUXCFLAGS = -DPD -O2 -funroll-loops -fomit-frame-pointer \
    -Wall -W -Wshadow -Wstrict-prototypes -Werror \
    -Wno-unused -Wno-parentheses -Wno-switch

#LINUXINCLUDE =  -I../../src
LINUXINCLUDE = -I/Applications/Pd-0.48-1.app/Contents/Resources/src

.c.pd_linux:
	cc $(LINUXCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	ld -export_dynamic  -shared -o $*.pd_linux $*.o -lc -lm
	strip --strip-unneeded $*.pd_linux
	rm $*.o

# ----------------------- Mac OSX -----------------------

pd_darwin: obj1.pd_darwin obj2.pd_darwin \
     obj3.pd_darwin obj4.pd_darwin obj5.pd_darwin dspobj~.pd_darwin

dspobj : dspobj~.pd_darwin
samplorpd : slm1.o linkedlist.o samplorpd~.pd_darwin

.SUFFIXES: .pd_darwin

DARWINCFLAGS = -DPD -O2 -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch

linkedlist.o:
	cc $(DARWINCFLAGS) $(LINUXINCLUDE) -o linkedlist.o -c linkedlist.c
slm1.o:
	cc $(DARWINCFLAGS) $(LINUXINCLUDE) -o slm1.o -c slm1.c

.c.pd_darwin:
	cc $(DARWINCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	cc -bundle -undefined suppress -flat_namespace -o $*.pd_darwin $*.o linkedlist.o slm1.o
	rm -f $*.o

install:
	cp samplorpd~.pd_darwin /Applications/Pd-0.48-1.app/Contents/Resources/extra
