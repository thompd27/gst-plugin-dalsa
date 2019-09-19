# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstdalsa.la

# Path to installation of the Dalsa libraries 
DALSA_CFLAGS = -I/usr/dalsa/GigeV/include
DALSA_LIBS = -lGevApi -L/usr/dalsa/GigeV/lib

# sources used to compile this plug-in
libgstdalsa_la_SOURCES = gstdalsa.c gstdalsa.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstdalsa_la_CFLAGS = $(GST_CFLAGS) $(DALSA_CFLAGS)
libgstdalsa_la_LIBADD = $(GST_LIBS) $(DALSA_LIBS) -lgstvideo-1.0
libgstdalsa_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstdalsa_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstdalsa.h
