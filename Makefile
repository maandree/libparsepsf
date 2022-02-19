.POSIX:

CONFIGFILE = config.mk
include $(CONFIGFILE)

OS = linux
# Linux:   linux
# Mac OS:  macos
# Windows: windows
include mk/$(OS).mk

LIB_MAJOR = 1
LIB_MINOR = 0
LIB_VERSION = $(LIB_MAJOR).$(LIB_MINOR)


OBJ =\
	libparsepsf.o

HDR =\
	libparsepsf.h


all: libparsepsf.a libparsepsf.$(LIBEXT) demo
$(OBJ): $(HDR)
$(LOBJ): $(HDR)

.c.o:
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

.c.lo:
	$(CC) -fPIC -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

libparsepsf.$(LIBEXT): $(LOBJ)
	$(CC) $(LIBFLAGS) -o $@ $(LOBJ) $(LDFLAGS)

libparsepsf.a: $(OBJ)
	-rm -f -- $@
	$(AR) rc $@ $(OBJ)
	$(AR) ts $@ > /dev/null

demo: demo.o libparsepsf.a
	$(CC) -o $@ $@.o libparsepsf.a $(LDFLAGS)

install: libparsepsf.a
	mkdir -p -- "$(DESTDIR)$(PREFIX)/lib"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/include"
	cp -- libparsepsf.a "$(DESTDIR)$(PREFIX)/lib"
	cp -- libparsepsf.$(LIBEXT) "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMINOREXT)"
	$(FIX_INSTALL_NAME) "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMINOREXT)"
	ln -sf -- "libsha1.$(LIBMINOREXT)" "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMAJOREXT)"
	ln -sf -- "libsha1.$(LIBMAJOREXT)" "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBEXT)"
	cp -- libparsepsf.h "$(DESTDIR)$(PREFIX)/include"

uninstall:
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.a"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBEXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMAJOREXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMINOREXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/include/libparsepsf.h"

clean:
	-rm -f -- *.o *.lo *.su *.a *.so *.so.* demo

.SUFFIXES:
.SUFFIXES: .lo .o .c

.PHONY: all check install uninstall clean
