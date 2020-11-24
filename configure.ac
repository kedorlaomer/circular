#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([circular], [0.0], [aadler@iri.uni-frankfurt.de])
AC_CONFIG_SRCDIR([circular.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_PROG_RANLIB

AC_CONFIG_SUBDIRS([zlib])

# Checks for library functions.
AC_CHECK_FUNCS([memset select])
AM_INIT_AUTOMAKE

AC_CONFIG_FILES([Makefile zlib/Makefile])
AC_OUTPUT
