/* unpost
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
 * The 1.5 versions are maintained by eddietwo@lcs.mit.edu.
 *
 * $Log: t1unmac.c,v $
 * Revision 1.2  1998/03/27 19:28:01  eddietwo
 * change --output FIEL to --output=FILE
 *
 * Revision 1.1.1.1  1998/03/05 16:28:46  eddietwo
 * initial version
 *
 * Revision 1.5  eddietwo
 * These changes by Eddie Kohler (eddietwo@lcs.mit.edu) not sanctioned
 * by I. Lee Hetherington.
 *  * Changed default output to PFB.
 *  * Removed banner, replaced getopt with CLP.
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

#ifndef lint
static char rcsid[] =
  "@(#) $Id: t1unmac.c,v 1.2 1998/03/27 19:28:01 eddietwo Exp $";
static char copyright[] =
  "@(#) Copyright (c) 1992 by I. Lee Hetherington, all rights reserved.";
#ifdef _MSDOS
static char portnotice[] =
  "@(#) Ported to MS-DOS by Kai-Uwe Herbing (herbing@netmbx.netmbx.de).";
#endif
#endif

#ifdef _MSDOS
  #include <fcntl.h>
  #include <getopt.h>
  #include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include "clp.h"

/* int32 must be at least 32-bit */
#if INT_MAX >= 0x7FFFFFFFUL
typedef int int32;
#else
typedef long int32;
#endif

void fatal_error(char *message, ...);

/* Some functions to read one, two, three, and four byte integers in 68000
   byte order (most significant byte first). */

static int read_one(FILE *fi)
{
  return fgetc(fi);
}

static int read_two(FILE *fi)
{
  int val;

  val = read_one(fi);
  val = (val << 8) + read_one(fi);

  return val;
}

static int32 read_three(FILE *fi)
{
  int32 val;

  val = read_one(fi);
  val = (val << 8) + read_one(fi);
  val = (val << 8) + read_one(fi);

  return val;
}

static int32 read_four(FILE *fi)
{
  int32 val;

  val = read_one(fi);
  val = (val << 8) + read_one(fi);
  val = (val << 8) + read_one(fi);
  val = (val << 8) + read_one(fi);

  return val;
}

/* Function to write four byte length to PFB file: least significant byte
   first. */

static void write_pfb_length(FILE *fo, int32 len)
{
  fputc((int)(len & 0xff), fo);
  len >>= 8;
  fputc((int)(len & 0xff), fo);
  len >>= 8;
  fputc((int)(len & 0xff), fo);
  len >>= 8;
  fputc((int)(len & 0xff), fo);
}

static void reposition(FILE *fi, int32 absolute)
{
  if (fseek(fi, absolute, 0) == -1)
    fatal_error("can't seek to position %d\n\
   (You can't pipe me data. Give me a filename instead.)", absolute);
}

static int hex_column = 0;                        /* current column of hex */
						  /* ASCII output */

static void output_hex_byte(FILE *fo, int b)
{
  static char *hex = "0123456789ABCDEF";

  if (hex_column > 62) {                          /* 64 column output */
    fputc('\n', fo);
    hex_column = 0;
  }
  fputc(hex[b >> 4], fo);
  fputc(hex[b & 0xf], fo);
  hex_column += 2;
}

/* Function to extract a particular POST resource.  Offset points to the four
   byte length which is followed by the data.  The first byte of the POST data
   specifies resource type: 1 for ASCII, 2 for binary, and 5 for end.  The
   second byte is always zero. */

static void extract_data(FILE *fi, FILE *fo, int32 offset, int binary)
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
      fputc(128, fo);
      fputc(1, fo);
      write_pfb_length(fo, len);
      while (len--) {
	if ((c = read_one(fi)) == '\r')           /* change \r to \n */
	  c = '\n';
	fputc(c, fo);
      }
    } else {
      if (last_type == PS_binary)
	fputc('\n', fo);
      while (len--) {
	if ((c = read_one(fi)) == '\r')           /* change \r to \n */
	  c = '\n';
	fputc(c, fo);
      }
    }
    last_type = 1;
    break;
  case PS_binary:
    (void) read_one(fi);
    if (binary) {
      fputc(128, fo);
      fputc(2, fo);
      write_pfb_length(fo, len);
      while (len--)
	fputc(read_one(fi), fo);
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
      fputc(128, fo);
      fputc(3, fo);
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
#define RAW_RES_OPT	307

static Clp_Option options[] = {
  { "help", 0, HELP_OPT, 0, 0 },
  { "macbinary", 0, MACBINARY_OPT, 0, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "pfa", 'a', PFA_OPT, 0, 0 },
  { "pfb", 'b', PFB_OPT, 0, 0 },
  { "raw", 'r', RAW_RES_OPT, 0, 0 },
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
  fputc('\n', stderr);
  exit(1);
}


void
error(char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, message, val);
  fputc('\n', stderr);
}


void
short_usage(void)
{
  fprintf(stderr, "Usage: %s [-abr] input [output]\n\
Type %s --help for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  fprintf(stderr, "Usage: %s [options] input [output]\n\
Options:\n\
  --raw, -r                     Input font is raw Macintosh resource fork.\n\
  --macbinary                   Input font is a MacBinary file. This is the\n\
                                default.\n\
  --pfa, -a                     Output font in ASCII (PFA) format.\n\
  --pfb, -b                     Output font in binary (PFB) format. This is\n\
                                the default.\n\
  --output=FILE, -o FILE        Write output to FILE.\n\
  --help, -h                    Print this message and exit.\n\
  --version                     Print version number and warranty and exit.\n\
",
	  program_name);
}

int main(int argc, char **argv)
{
  FILE *ifp = stdin;
  FILE *ofp = stdout;
  int32 data_fork_size;
  int32 res_offset, res_data_offset, res_map_offset, type_list_offset;
  int32 post_type;
  int num_types, num_of_type, num_extracted = 0, binary = 0, raw = 0;
  int input_given = 0;
  int output_given = 0;
  
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = (char *)Clp_ProgramName(clp);
  
  /* interpret command line arguments using CLP */
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     output_file:
     case OUTPUT_OPT:
      if (output_given)
	fatal_error("output file already specified");
      output_given = 1;
      if (strcmp(clp->arg, "-") == 0)
	ofp = stdout;
      else {
	ofp = fopen(clp->arg, "w");
	if (!ofp) fatal_error("can't open %s for writing", clp->arg);
      }
      break;

     case PFB_OPT:
      binary = 1;
      break;

     case PFA_OPT:
      binary = 0;
      break;
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("t1unmac version %s\n", VERSION);
      printf("Copyright (C) 1992-8 I. Lee Hetherington et al.\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose. That's right: you're on your own!\n");
      exit(0);
      break;
      
     case Clp_NotOption:
      if (input_given && output_given)
	fatal_error("too many arguments");
      else if (input_given)
	goto output_file;
      input_given = 1;
      if (strcmp(clp->arg, "-") == 0)
	ifp = stdin;
      else {
	ifp = fopen(clp->arg, "rb");
	if (!ifp) fatal_error("can't open %s for reading", clp->arg);
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
  #ifdef _MSDOS
  _setmode(_fileno(ifp), _O_BINARY);
  /* If we are processing a PFB (binary) output */
  /* file, we must set its file mode to binary. */
  if (binary)
    _setmode(_fileno(ofp), _O_BINARY);
  #endif

  if (raw) {
    /* raw resource file */
    res_offset = 0;

  } else {
    /* MacBinary (I or II) file */

    /* SHOULD CHECK INTEGRITY OF MACBINARY HEADER HERE TO VERIFY THAT WE
       REALLY HAVE A MACBINARY FILE.  MACBINARY-II-STANDARD.TXT DESCRIBES
       AN APPROPRIATE VERIFICATION PROCEDURE. */

    /* read data and resource fork sizes in MacBinary header */
    reposition(ifp, 83);
    data_fork_size = read_four(ifp);
    (void) read_four(ifp);

    /* round data_fork_size up to multiple of 128 */
    if (data_fork_size % 128)
      data_fork_size += 128 - data_fork_size % 128;

    res_offset = 128 + data_fork_size;
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
