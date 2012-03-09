#       configure.ac
#       
#       Dustin Dorroh <ddorroh@aplopteng.com>
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([rhythmos], [0.1], [dustindorroh@gmail.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_SRCDIR([src/main.c])
#AC_CONFIG_HEADERS([config.h])


# Checks for programs.
#AC_PROG_CC
#AC_PROG_INSTALL
#AC_PROG_RANLIB
#AM_PROG_CC_C_O
#AM_PROG_AS
gl_EARLY
gl_INIT

AC_CHECK_PROG([QEMU], qemu, qemu, [], [$PATH])
AC_CHECK_PROG([MKDIR], mkdir, mkdir, [], [$PATH])
AC_CHECK_PROG([MAKEINFO], makeinfo, makeinfo, [], [$PATH])
AC_CHECK_PROG([CP], cp, cp, [], [$PATH])
AC_CHECK_PROG([TEXI2PDF], texi2pdf, texi2pdf, [], [$PATH])
AC_CHECK_PROG([TEXI2DVI], texi2dvi, texi2dvi, [], [$PATH])
AC_CHECK_PROG([TEXI2HTML], texi2html, texi2html, [], [$PATH])
AC_CHECK_PROG([DVIPS], dvips, dvips, [], [$PATH])

AC_CHECK_PROG([MDEL], mdel, mdel, [], [$PATH])
AC_CHECK_PROG([MCOPY], mcopy, mcopy, [], [$PATH])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([assert.h])
AC_CHECK_HEADERS([dirent.h])
AC_CHECK_HEADERS([errno.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([stdarg.h])
AC_CHECK_HEADERS([stdio.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([sys/stat.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([time.h])
AC_CHECK_HEADERS([unistd.h])

# Checks for libraries.
# Checks for typedefs, structures, and compiler characteristics.
# Checks for library functions.

AC_CONFIG_FILES([Makefile
		 src/Makefile
		 lib/Makefile])
AC_OUTPUT

echo \
"-------------------------------------------------

 ${PACKAGE_NAME} Version ${PACKAGE_VERSION}

 Prefix: '${prefix}'.
 Compiler: '${CC} ${CFLAGS} ${CPPFLAGS}'

 Package features:
   Async Execution: ${async_exec}

 Now type 'make @<:@target@:>@'

 General build targets:

       make - compile and link the kernel image and utils
       make fs   - build filesystem image
       make boot - build grub bootloader image
       make run  - run kernel in qemu
       make clean- clean the project

 Specific build targets:

       make kernel.img
       make filesystem.img
       make grub.img

 QEMU Examples:

       qemu -kernel src/kernel.img -initrd src/filesystem.img
       qemu -fda src/grub.img

 Generate Documentation:

       rhythmos.info
       rhythmos.html
       rhythmos.dvi
       rhythmos.pdf
       standards.pdf
       
Read the README and the INSTALL files for more info.
--------------------------------------------------"
