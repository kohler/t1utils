/* Process this file with autoheader to produce config.h.in */
#ifndef CONFIG_H
#define CONFIG_H

/* Package and version */
#define PACKAGE "t1utils"
#define VERSION "97"

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
#undef NEED_SYS_TYPES_H
#ifdef NEED_SYS_TYPES_H
# include <sys/types.h>
#endif
#undef HAVE_INTTYPES_H
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#undef uint16_t
#undef uint32_t
#undef int32_t

#ifdef __cplusplus
}
/* Get rid of inline macro under C++ */
# undef inline
#endif
#endif
