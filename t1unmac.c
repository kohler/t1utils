/* t1unmac/unpost
 *
 * This program converts Macintosh type-1 fonts stored in MacBinary (I or II)
 * format or raw resource fork to PFA and PFB formats.
 *
 * Copyright (c) 1992 by I. Lee Hetherington, all rights reserved.
 *
 * Permission is hereby granted to use, modify, and distribute this program
 * for any purpose provided this copyright notice and the one below remain
 * intact.
 *
 * I. Lee Hetherington (ilh@lcs.mit.edu)
 *
 * 1.5 and later versions contain changes by, and are maintained by,
 * Eddie Kohler <eddietwo@lcs.mit.edu>.
 *
 * New change log in `NEWS'. Old change log:
 *
 * Revision 1.2  92/06/23  10:57:33  ilh
 * MSDOS porting by Kai-Uwe Herbing (herbing@netmbx.netmbx.de)
 * incoporated.
 * 
 * Revision 1.1  92/05/22  12:07:49  ilh
 * initial version
 *
 * Ported to Microsoft C/C++ Compiler and MS-DOS operating system by
 * Kai-Uwe Herbing (herbing@netmbx.netmbx.de) on June 12, 1992. Code
 * specific to the MS-DOS version is encapsulated with #ifdef _MSDOS
 * ... #endif, where _MSDOS is an identifier, which is automatically
 * defined, if you compile with the Microsoft C/C++ Compiler.
 *
 */

/* Note: this is ANSI C. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#if defined(_MSDOS) || defined(_WIN32)
# include <fcntl.h>
# include <getopt.h>
# include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "clp.h"

/* int32 must be at least 32-bit */
#if INT_MAX >= 0x7FFFFFFFUL
typedef int int32;
#else
typedef long int32;
#endif

static int line_length = 64;

void fatal_error(char *message, ...);

/* Some functions to read one, two, three, and four byte integers in 68000
   byte order (most significant byte first). */

static int
read_one(FILE *fi)
{
  return getc(fi);
}

static int
read_two(FILE *fi)
{
  int val;

  val = read_one(fi);
  val = (val << 8) | read_one(fi);

  return val;
}

static int32
read_three(FILE *fi)
{
  int32 val;

  val = read_one(fi);
  val = (val << 8) | read_one(fi);
  val = (val << 8) | read_one(fi);

  return val;
}

static int32
read_four(FILE *fi)
{
  int32 val;

  val = read_one(fi);
  val = (val << 8) | read_one(fi);
  val = (val << 8) | read_one(fi);
  val = (val << 8) | read_one(fi);

  return val;
}

/* Function to write four byte length to PFB file: least significant byte
   first. */

static void
write_pfb_length(FILE *fo, int32 len)
{
  putc((int)(len & 0xff), fo);
  len >>= 8;
  putc((int)(len & 0xff), fo);
  len >>= 8;
  putc((int)(len & 0xff), fo);
  len >>= 8;
  putc((int)(len & 0xff), fo);
}

static void
reposition(FILE *fi, int32 absolute)
{
  if (fseek(fi, absolute, 0) == -1)
    fatal_error("can't seek to position %d\n\
   (The Mac file may be corrupted, or you may need the `-r' option.)",
		absolute);
}

static int hex_column = 0;	/* current column of hex ASCII output */

static void
output_hex_byte(FILE *fo, int b)
{
  static char *hex = "0123456789abcdef";
  
  if (hex_column >= line_length) {
    putc('\n', fo);
    hex_column = 0;
  }
  putc(hex[b >> 4], fo);
  putc(hex[b & 0xf], fo);
  hex_column += 2;
}

/* Function to extract a particular POST resource.  Offset points to the four
   byte length which is followed by the data.  The first byte of the POST data
   specifies resource type: 1 for ASCII, 2 for binary, and 5 for end.  The
   second byte is always zero. */

static void
extract_data(FILE *fi, FILE *fo, int32 offset, int binary)
{
  enum PS_type { PS_ascii = 1, PS_binary = 2, PS_end = 5 };
  static enum PS_type last_type = PS_ascii;
  int32 len, save_offset = ftell(fi);
  int c;
  
  reposition(fi, offset);
  len = read_four(fi) - 2;                        /* subtract type field */
  switch ((enum PS_type)read_one(fi)) {
  case PS_ascii:
    (void) read_one(fi);
    if (binary) {
      putc(128, fo);
      putc(1, fo);
      write_pfb_length(fo, len);
      while (len--) {
	if ((c = read_one(fi)) == '\r')           /* change \r to \n */
	  c = '\n';
	putc(c, fo);
      }
    } else {
      if (last_type == PS_binary)
	putc('\n', fo);
      while (len--) {
	if ((c = read_one(fi)) == '\r')           /* change \r to \n */
	  c = '\n';
	putc(c, fo);
      }
    }
    last_type = 1;
    break;
   case PS_binary:
    (void) read_one(fi);
    if (binary) {
      putc(128, fo);
      putc(2, fo);
      write_pfb_length(fo, len);
      while (len--)
	putc(read_one(fi), fo);
    } else {
      if (last_type != 2)
	hex_column = 0;
      while (len--)
	output_hex_byte(fo, read_one(fi));
      last_type = 2;
    }
    break;
   case PS_end:
    (void) read_one(fi);
    if (binary) {
      putc(128, fo);
      putc(3, fo);
    }
    break;
  }
  reposition(fi, save_offset);
}


/*****
 * Command line
 **/

#define OUTPUT_OPT	301
#define VERSION_OPT	302
#define HELP_OPT	303
#define PFB_OPT		304
#define PFA_OPT		305
#define MACBINARY_OPT	306
#define RAW_OPT		307
#define LINE_LEN_OPT	308

static Clp_Option options[] = {
  { "help", 0, HELP_OPT, 0, 0 },
  { "line-length", 'l', LINE_LEN_OPT, Clp_ArgUnsigned, 0 },
  { "macbinary", 0, MACBINARY_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "pfa", 'a', PFA_OPT, 0, 0 },
  { "pfb", 'b', PFB_OPT, 0, 0 },
  { "raw", 'r', RAW_OPT, 0, 0 },
  { "version", 0, VERSION_OPT, 0, 0 },
};
static char *program_name;

void
fatal_error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, message, val);
  putc('\n', stderr);
  exit(1);
}

void
error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, message, val);
  putc('\n', stderr);
}

void
short_usage(void)
{
  fprintf(stderr, "Usage: %s [OPTION]... INPUT [OUTPUT]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}

void
usage(void)
{
  printf("\
`T1unmac' extracts a PostScript Type 1 font from a Macintosh resource fork\n\
or MacBinary file. The result is written to the standard output unless an\n\
OUTPUT file is given.\n\
\n\
Usage: %s [OPTION]... INPUT [OUTPUT]\n\
\n\
Options:\n\
  -r, --raw                     Input font is raw Macintosh resource fork.\n\
      --macbinary               Input font is a MacBinary file. This is the\n\
                                default.\n\
  -a, --pfa                     Output font in ASCII (PFA) format.\n\
  -b, --pfb                     Output font in binary (PFB) format. This is\n\
                                the default.\n\
  -l, --line-length=N           Set line length for PFA output.\n\
  -o, --output=FILE             Write output to FILE.\n\
  -h, --help                    Print this message and exit.\n\
      --version                 Print version number and warranty and exit.\n\
\n\
Report bugs to <eddietwo@lcs.mit.edu>.\n", program_name);
}


/*
 * CRC computation logic
 * 
 * The logic for this method of calculating the CRC 16 bit polynomial is taken
 * from an article by David Schwaderer in the April 1985 issue of PC Tech
 * Journal.
 */

static short      crctab[] =    /* CRC lookup table */
{
 0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
 0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
 0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
 0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
 0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
 0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
 0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
 0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
 0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
 0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
 0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
 0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
 0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
 0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
 0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
 0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
 0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
 0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
 0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
 0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
 0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
 0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
 0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
 0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
 0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
 0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
 0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
 0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
 0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
 0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
 0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
 0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/*
 * Update a CRC check on the given buffer.
 */

int
crcbuf(crc, len, buf)
        register int    crc;    /* running CRC value */
        register u_int  len;
        register u_char *buf;
{
        register u_int  i;

        for (i=0; i<len; i++)
                crc = ((crc >> 8) & 0xff) ^ crctab[(crc ^ *buf++) & 0xff];
        
        return (crc);
}


static const char *
check_macbinary(FILE *ifp)
{
  int i, j;
  char buf[124];

  /* check "version" bytes at offsets 0 and 74 */
  reposition(ifp, 0);
  if (read_one(ifp) != 0)
    return "bad version byte";
  reposition(ifp, 74);
  if (read_one(ifp) != 0)
    return "bad version byte";

  /* check file length */
  reposition(ifp, 1);
  i = read_one(ifp);
  if (i > 63)
    return "bad length";
  reposition(ifp, 83);
  i = read_four(ifp);
  j = read_four(ifp);
  if (i < 0 || j < 0 || i >= 0x800000 || j >= 0x800000)
    return "bad length";

  /* check reserved area */
  for (i = 101; i < 116; i++) {
    reposition(ifp, i);
    if (read_one(ifp) != 0)
      return "bad reserved area";
  }

  /* check CRC */
  reposition(ifp, 0);
  fread(buf, 1, 124, ifp);
  if (crcbuf(0, 124, buf) != read_two(ifp)) {
    reposition(ifp, 82);
    if (read_one(ifp) != 0)
      return "bad checksum";
  }

  return 0;
}

#define APPLESINGLE_MAGIC 0x00051600
#define APPLEDOUBLE_MAGIC 0x00051607

const char *
check_appledouble(FILE *ifp)
{
  int i;
  reposition(ifp, 0);
  i = read_four(ifp);
  if (i != APPLEDOUBLE_MAGIC && i != APPLESINGLE_MAGIC)
    return "bad magic number";

  return 0;
}

int
main(int argc, char **argv)
{
  FILE *ifp = 0;
  FILE *ofp = 0;
  const char *ifp_name = "<stdin>";
  int32 res_offset, res_data_offset, res_map_offset, type_list_offset;
  int32 post_type;
  int num_types, num_of_type, num_extracted = 0, binary = 1;
  int raw = 0, macbinary = 0, appledouble = 0;
  
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = (char *)Clp_ProgramName(clp);
  
  /* interpret command line arguments using CLP */
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case RAW_OPT:
      raw = 1;
      macbinary = appledouble = 0;
      break;
      
     case MACBINARY_OPT:
      macbinary = 1;
      raw = appledouble = 0;
      break;
      
     output_file:
     case OUTPUT_OPT:
      if (ofp)
	fatal_error("output file already specified");
      if (strcmp(clp->arg, "-") == 0)
	ofp = stdout;
      else {
	ofp = fopen(clp->arg, "w");
	if (!ofp) fatal_error("%s: %s", clp->arg, strerror(errno));
      }
      break;
      
     case PFB_OPT:
      binary = 1;
      break;
      
     case PFA_OPT:
      binary = 0;
      break;
      
     case LINE_LEN_OPT:
      line_length = clp->val.i;
      if (line_length < 4) {
	line_length = 4;
	error("warning: line length raised to %d", line_length);
      } else if (line_length > 1024) {
	line_length = 1024;
	error("warning: line length lowered to %d", line_length);
      }
      break;
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("t1unmac (LCDF t1utils) %s\n", VERSION);
      printf("Copyright (C) 1992-9 I. Lee Hetherington, Eddie Kohler et al.\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
      exit(0);
      break;
      
     case Clp_NotOption:
      if (ifp && ofp)
	fatal_error("too many arguments");
      else if (ifp)
	goto output_file;
      if (strcmp(clp->arg, "-") == 0)
	ifp = stdin;
      else {
	ifp_name = clp->arg;
	ifp = fopen(clp->arg, "rb");
	if (!ifp) fatal_error("%s: %s", clp->arg, strerror(errno));
      }
      break;
      
     case Clp_Done:
      goto done;
      
     case Clp_BadOption:
      short_usage();
      exit(1);
      break;
      
    }
  }
  
 done:
  if (!ifp) ifp = stdin;
  if (!ofp) ofp = stdout;
  
#if defined(_MSDOS) || defined(_WIN32)
  _setmode(_fileno(ifp), _O_BINARY);
  /* If we are processing a PFB (binary) output */
  /* file, we must set its file mode to binary. */
  if (binary)
    _setmode(_fileno(ofp), _O_BINARY);
#endif
  
  /* check for non-seekable input */
  if (fseek(ifp, 0, 0))
    fatal_error("%s: isn't seekable\n\
  (I can't read from stdin; give me a filename on the command line instead.)",
		ifp_name);
  
  /* check for empty file */
  fseek(ifp, 0, 2);
  if (ftell(ifp) == 0)
    fatal_error("%s: empty file\n\
  (Try re-transferring the files using MacBinary format.)",
		ifp_name);

  if (!raw && !macbinary && !appledouble) {
    /* check magic number, try to figure out what it is */
    reposition(ifp, 0);
    switch (read_four(ifp)) {

     case APPLESINGLE_MAGIC:
     case APPLEDOUBLE_MAGIC:
      appledouble = 1;
      break;

     default:
      macbinary = 1;
      break;

    }
  }
    
  if (raw) {
    /* raw resource file */
    res_offset = 0;
    
  } else if (macbinary) {	/* MacBinary (I or II) file */
    const char *check;
    int32 data_fork_size;

    /* check integrity of file */
    check = check_macbinary(ifp);
    if (check)
      fatal_error("%s: not a MacBinary file (%s)", ifp_name, check);
    
    /* read data and resource fork sizes in MacBinary header */
    reposition(ifp, 83);
    data_fork_size = read_four(ifp);
    (void) read_four(ifp);
    
    /* round data_fork_size up to multiple of 128 */
    if (data_fork_size % 128)
      data_fork_size += 128 - data_fork_size % 128;
    
    res_offset = 128 + data_fork_size;

  } else if (appledouble) {	/* AppleDouble file */
    const char *check;
    const char *applewhat;
    int i, n;

    /* check integrity of file */
    check = check_appledouble(ifp);
    if (check)
      fatal_error("%s: not an AppleDouble file (%s)", ifp_name, check);
    reposition(ifp, 0);
    if (read_four(ifp) == APPLESINGLE_MAGIC)
      applewhat = "AppleSingle";
    else
      applewhat = "AppleDouble";
    
    /* find offset to resource and/or data fork */
    reposition(ifp, 24);
    n = read_two(ifp);
    res_offset = -1;
    for (i = 0; i < n; i++) {
      int type = read_four(ifp);
      if (type == 0)
	fatal_error("%s: bad %s file (bad entry descriptor)", ifp_name, applewhat);
      if (type == 2)
	res_offset = read_four(ifp);
      else
	(void) read_four(ifp);
      (void) read_four(ifp);
    }
    if (res_offset < 0)
      fatal_error("%s: bad %s file (no resource fork)", ifp_name, applewhat);
    
  } else {
    fatal_error("%s: can't read strange format", ifp_name);
    exit(1);
  }
  
  /* read offsets from resource fork header */
  reposition(ifp, res_offset);
  res_data_offset = res_offset + read_four(ifp);
  res_map_offset = res_offset + read_four(ifp);
  
  /* read type list offset from resource map header */
  reposition(ifp, res_map_offset + 24);
  type_list_offset = res_map_offset + read_two(ifp);
  
  /* read type list */
  reposition(ifp, type_list_offset);
  num_types = read_two(ifp) + 1;
  
  /* find POST type */
  post_type =  (int32)('P' & 0xff) << 24;
  post_type |= (int32)('O' & 0xff) << 16;
  post_type |= (int32)('S' & 0xff) << 8;
  post_type |= (int32)('T' & 0xff);
  
  while (num_types--) {
    if (read_four(ifp) == post_type) {
      num_of_type = 1 + read_two(ifp);
      reposition(ifp, type_list_offset + read_two(ifp));
      while (num_of_type--) {
	(void) read_two(ifp);                      /* ID */
	(void) read_two(ifp);
	(void) read_one(ifp);
	extract_data(ifp, ofp, res_data_offset + read_three(ifp), binary);
	++num_extracted;
	(void) read_four(ifp);
      }
      break;
    } else {
      (void) read_two(ifp);
      (void) read_two(ifp);
    }
  }
  
  if (num_extracted == 0)
    error("this file doesn't seem to be a Macintosh font");
  
  fclose(ifp);
  fclose(ofp);
  return 0;
}
