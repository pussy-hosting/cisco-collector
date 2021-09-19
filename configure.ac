AC_INIT([cisco-collector], [0.0.1])
AM_INIT_AUTOMAKE([-Wall -Werror])
AC_PROG_CC
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_FILES([
                 Makefile
                 src/Makefile])
AC_HEADER_STDBOOL
AC_CHECK_FUNCS([inet_ntoa])
AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([netinet/in.h])
AC_CHECK_HEADERS([winsock2.h])
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_UINT64_T
AC_OUTPUT

