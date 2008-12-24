AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(GTK_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

library_includedir=$(includedir)/@PACKAGE@-@VERSION@/
library_include_HEADERS = rawstudio.h rs-types.h \
	rs-image.h \
	rs-filter.h \
	rs-plugin-manager.h \
	rs-utils.h \
	rs-math.h \
	rs-settings.h \
	null.h color.h

lib_LTLIBRARIES = librawstudio-0.1.la
librawstudio_0_1_la_SOURCES = \
	rs-plugin.c rs-plugin.h \
	rs-image.c rs-image.h \
	rs-filter.c rs-filter.h \
	rs-plugin-manager.c rs-plugin-manager.h \
	rs-utils.c rs-utils.h \
	rs-math.c rs-math.h \
	rs-settings.c rs-settings.h \
	null.c null.h color.h

librawstudio_0_1_la_LIBADD = @PACKAGE_LIBS@ @LIBJPEG@ @LIBTIFF@ $(INTLLIBS)

