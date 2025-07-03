current:
	echo make pd_linux

clean: ; rm -f *.pd_linux *.pd_darwin *.o


# ----------------------- LINUX i386 -----------------------

pd_linux: obj1.pd_linux obj2.pd_linux obj3.pd_linux obj4.pd_linux \
    obj5.pd_linux dspobj~.pd_linux

.SUFFIXES: .pd_linux

LINUXCFLAGS = -DPD -O2 -funroll-loops -fomit-frame-pointer \
    -Wall -W -Wshadow -Wstrict-prototypes -Werror \
    -Wno-unused -Wno-parentheses -Wno-switch

#LINUXINCLUDE =  -I../../src
LINUXINCLUDE = -I/Applications/Pd-0.55-2.app/Contents/Resources/src -I/usr/include/libxml2 \
	-I/usr/local/opt/libxml2/include

  
# ----------------------- Mac OSX -----------------------

pd_darwin: obj1.pd_darwin obj2.pd_darwin \
     obj3.pd_darwin obj4.pd_darwin obj5.pd_darwin dspobj~.pd_darwin

dspobj : dspobj~.pd_darwin
samplorkg : samplorkg.pd_darwin

.SUFFIXES: .pd_darwin

DARWINCFLAGS = -DPD -O2 -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-unused-parameter -Wno-visibility

.c.pd_darwin:
	cc $(DARWINCFLAGS) $(LINUXINCLUDE) -o $*.o -c $*.c
	cc -bundle -undefined suppress -flat_namespace -L/usr/local/opt/libxml2/lib -o $*.pd_darwin $*.o
	rm -f $*.o

install:
	cp samplorkg.pd_darwin /Applications/Pd-0.51-4.app/Contents/Resources/extra

install_lib:
	cp samplorkg.pd_darwin /Users/lemouton/Projets/Co/Stockhausen/Mantra/mantra2019-pd_48/lib

run:
	/Applications/Pd-0.51-4.app/Contents/MacOS/Pd samplork.help.pd
