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

lib_LTLIBRARIES = x3f_meta.la

libdir = $(datadir)/rawstudio/plugins/

x3f_meta_la_LIBADD = @PACKAGE_LIBS@
x3f_meta_la_LDFLAGS = -module -avoid-version
x3f_meta_la_SOURCES =  x3f-meta.c
