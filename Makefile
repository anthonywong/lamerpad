OPTIMIZE=-g -Wall
#OPTIMIZE=-O2 

GTKLIBS=`gtk-config --libs`
GTKINC=`gtk-config --cflags`

PREFIX=/usr/local
DATADIR=$(PREFIX)/lib

# location in which binaries are installed
BINDIR=$(PREFIX)/bin
# location in which data files will be installed
KPLIBDIR=$(DATADIR)/lamerpad

INCDIR=-Iinclude/Xi18n -Iinclude/IMCore -IIMdkit/IMdkit/
LIBDIR=-L.

INSTALL=/usr/bin/install

####### No editing should be needed below here ##########

OBJS = kpengine.o scoring.o util.o
CFLAGS = $(OPTIMIZE) $(GTKINC) -DFOR_PILOT_COMPAT -DKP_LIBDIR=\"$(KPLIBDIR)\" \
         $(INCDIR) $(LIBDIR) -DDEBUG -g

all: kpengine lamerpad chardata.dat

scoring.o: jstroke/scoring.c jstroke/jstroke.h
	$(CC) -c -o scoring.o $(CFLAGS) -Ijstroke jstroke/scoring.c

util.o: jstroke/util.c jstroke/jstroke.h
	$(CC) -c -o util.o $(CFLAGS) -Ijstroke jstroke/util.c

kpengine: $(OBJS)
	$(CC) -o kpengine $(OBJS)

xim.c.o: xim.h

lamerpad: libXimd.a lamerpad.o padarea.o xim.o
	$(CC) -o $@ $^  $(INCDIR) $(LIBDIR) $(GTKLIBS) -lXimd -lX11

libXimd.a: IMdkit/IMdkit/libXimd.a
	ln -sf $^ .
IMdkit/IMdkit/libXimd.a:
	cd IMdkit; xmkmf -a; $(MAKE) -C IMdkit

# jdata.dat: jstroke/strokedata.h conv_jdata.pl
# 	perl conv_jdata.pl < jstroke/strokedata.h > jdata.dat

chardata.dat: uconv_jdata.pl chardata/unistrok.*
	cat chardata/unistrok.* | perl uconv_jdata.pl > chardata.dat

install: lamerpad kpengine chardata.dat
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m 0755 lamerpad $(BINDIR)/lamerpad
	$(INSTALL) -m 0755 kpengine $(BINDIR)/kpengine
	$(INSTALL) -d $(KPLIBDIR)
	$(INSTALL) -m 0644 chardata.dat $(KPLIBDIR)/chardata.dat

clean:
	rm -f *.o *.a chardata.dat kpengine lamerpad libXimd.a
	$(MAKE) -C IMdkit clean

distclean: clean

myclean:
	rm -f *.o *.a kpengine lamerpad

.PHONY: all install clean distclean myclean
