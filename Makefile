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

MAN0 =\
	libparsepsf.h.0

MAN3 =\
	libparsepsf_destroy_font.3\
	libparsepsf_get_glyph.3\
	libparsepsf_parse_font.3

MAN7 =\
	libparsepsf.7


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
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man0"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man3"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man7"
	cp -- libparsepsf.a "$(DESTDIR)$(PREFIX)/lib"
	cp -- libparsepsf.$(LIBEXT) "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMINOREXT)"
	$(FIX_INSTALL_NAME) "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMINOREXT)"
	ln -sf -- "libparsepsf.$(LIBMINOREXT)" "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMAJOREXT)"
	ln -sf -- "libparsepsf.$(LIBMAJOREXT)" "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBEXT)"
	cp -- libparsepsf.h "$(DESTDIR)$(PREFIX)/include"
	cp -P -- $(MAN0) "$(DESTDIR)$(MANPREFIX)/man0"
	cp -P -- $(MAN3) "$(DESTDIR)$(MANPREFIX)/man3"
	cp -P -- $(MAN7) "$(DESTDIR)$(MANPREFIX)/man7"

uninstall:
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.a"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBEXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMAJOREXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/lib/libparsepsf.$(LIBMINOREXT)"
	-rm -f -- "$(DESTDIR)$(PREFIX)/include/libparsepsf.h"
	-cd -- "$(DESTDIR)$(MANPREFIX)/man0" && rm -f -- $(MAN0)
	-cd -- "$(DESTDIR)$(MANPREFIX)/man3" && rm -f -- $(MAN3)
	-cd -- "$(DESTDIR)$(MANPREFIX)/man7" && rm -f -- $(MAN7)

clean:
	-rm -f -- *.o *.lo *.su *.a *.so *.so.* demo

.SUFFIXES:
.SUFFIXES: .lo .o .c

.PHONY: all check install uninstall clean
