pconnectdir = $(datadir)/$(PACKAGE)
pconnect_DATA = pconnect.html

pconnect.html: pconnect.1
	man -l -Thtml $< > $@

EXTRA_DIST = $(pconnect_DATA)
MAINTAINERCLEANFILES = Makefile.in

.PRECIOUS: pconnect.html
