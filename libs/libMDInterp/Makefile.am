noinst_LTLIBRARIES = libMDInterp.la
pkginclude_HEADERS = libMDInterp.h
libMDInterp_la_SOURCES = BinSearch.cc Interp1D.cc Interp2D.cc Interp3D.cc Interp4D.cc InterpND.cc freadEC.cc libMDInterp.h

#check_PROGRAMS      = tInterpND

#tlibMDInterp_SOURCES = tlibMDInterp.cc
#tlibMDInterp_LDADD   = ../libscuff/libscuff.la

#tInterpND_SOURCES = tInterpND.cc
#tInterpND_LDADD   = ../libscuff/libscuff.la   \
#                    ../libhmat/libhmat.la     \
#                    ../libhrutil/libhrutil.la

#TESTS = tInterpND
#TESTS = 

EXTRA_DIST = COPYRIGHT

AM_CPPFLAGS = -I$(top_srcdir)/libs/libhrutil	\
              -I$(top_srcdir)/libs/libhmat \
              -I$(top_builddir) # for config.h
