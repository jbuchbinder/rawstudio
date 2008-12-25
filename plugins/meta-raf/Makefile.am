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

lib_LTLIBRARIES = raf_meta.la

libdir = $(datadir)/rawstudio/plugins/

raf_meta_la_LIBADD = @PACKAGE_LIBS@
raf_meta_la_LDFLAGS = -module -avoid-version
raf_meta_la_SOURCES =  raf-meta.c
