plugindir = $(libdir)/rawstudio/plugins

AM_CFLAGS =\
	-Wall\
	-O4

AM_CXXFLAGS = $(AM_CFLAGS)

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../../librawstudio/

lib_LTLIBRARIES = ciff_meta.la

libdir = $(datadir)/rawstudio/plugins/

ciff_meta_la_LIBADD = @PACKAGE_LIBS@
ciff_meta_la_LDFLAGS = -module -avoid-version
ciff_meta_la_SOURCES =  ciff-meta.c
