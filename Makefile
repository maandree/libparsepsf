.POSIX:

CONFIGFILE = config.mk
include $(CONFIGFILE)

OBJ =\
	libparsepsf.o

LIB_HDR =\
	libparsepsf.h

HDR =\
	$(LIB_HDR)

all: libparsepsf.a demo
$(OBJ): $(@:.o=.c) $(HDR)
libparsepsf.o: libparsepsf.c $(LIB_HDR)

libparsepsf.a: $(OBJ)
	$(AR) rc $@ $(OBJ)
	$(AR) ts $@ > /dev/null

.c.o:
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

demo: demo.o libparsepsf.a
	$(CC) -o $@ $@.o libparsepsf.a $(LDFLAGS)

install: libparsepsf.a
	mkdir -p -- "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/include"
	cp -- libparsepsf.a "$(DESTDIR)$(PREFIX)/lib"
	cp -- libparsepsf.h "$(DESTDIR)$(PREFIX)/include"

uninstall:
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.a"
	-rm -f -- "$(DESTDIR)$(PREFIX)/include/libparsepsf.h"

clean:
	-rm -f -- *.o *.lo *.su *.a *.so *.so.* demo

.SUFFIXES:
.SUFFIXES: .c .o

.PHONY: all install uninstall clean
