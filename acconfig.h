/* Process this file with autoheader to produce config.h.in */
#ifndef CONFIG_H
#define CONFIG_H

/* Package and version */
#define PACKAGE "t1utils"
#define VERSION "97"

/* Define if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define if you must include the <sys/types.h> header file. */
#undef NEED_SYS_TYPES_H

/* Define if you have u_intXX_t types but not uintXX_t types. */
#undef HAVE_U_INT_TYPES

@TOP@
@BOTTOM@

#ifdef __cplusplus
extern "C" {
#endif

/* Prototype strerror() if we don't have it. */
#ifndef HAVE_STRERROR
char *strerror(int errno);
#endif

/* Get the [u]int*_t typedefs. */
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef NEED_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_U_INT_TYPES
typedef u_int8_t uint8_t;
typedef u_int16_t uint16_t;
typedef u_int32_t uint32_t;
#endif

#ifdef __cplusplus
}
/* Get rid of a possible inline macro under C++. */
# define inline inline
#endif
#endif
