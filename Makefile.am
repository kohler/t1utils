## Process this file with automake to produce Makefile.in
AUTOMAKE_OPTIONS = no-dependencies foreign check-news

bin_PROGRAMS = t1ascii t1binary t1asm t1disasm t1unmac
man_MANS = t1ascii.1 t1binary.1 t1asm.1 t1disasm.1 t1unmac.1

t1ascii_SOURCES = clp.h clp.c t1lib.h t1lib.c t1ascii.c
t1binary_SOURCES = clp.h clp.c t1lib.h t1lib.c t1binary.c
t1asm_SOURCES = clp.h clp.c t1asm.c
t1disasm_SOURCES = clp.h clp.c t1disasm.c
t1unmac_SOURCES = clp.h clp.c t1unmac.c

LDADD = @LIBOBJS@

EXTRA_DIST = $(man_MANS) rpm.spec

srclinks:
	for i in clp.h clp.c strerror.c ; do \
	ln -sf ~/src/edlib/$$i $$i ; done

versionize:
	perl -pi -e "s/^\\.ds V.*/.ds V $(VERSION)/;" t1ascii.1 t1binary.1 t1disasm.1 t1asm.1 t1unmac.1
	perl -pi -e "s/^Version: .*/Version: $(VERSION)/; s/$(PACKAGE)-[\w.]+\.tar\.gz/$(PACKAGE)-$(VERSION).tar.gz/;" rpm.spec

rpm: dist
	buildarch=`rpm --showrc | awk '/^build arch/ { print $$4; }'` ; \
	mkdir -p /tmp/rpmb/SOURCES /tmp/rpmb/RPMS/$$buildarch \
	/tmp/rpmb/BUILD ; \
	echo 'topdir: /tmp/rpmb' > /tmp/rpmb/rc ; \
	cp $(PACKAGE)-$(VERSION).tar.gz /tmp/rpmb/SOURCES ; \
	rpm --rcfile /tmp/rpmb/rc -bb rpm.spec ; \
	cp /tmp/rpmb/RPMS/$$buildarch/*.rpm .
	rm -rf /tmp/rpmb

.PHONY: srclinks versionize rpm
