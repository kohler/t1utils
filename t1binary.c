/* t1binary
 *
 * This program takes an Adobe Type-1 font program in ASCII (PFA) format and
 * converts it to binary (PFB) format.
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
 * Revision 1.2  92/06/23  10:58:08  ilh
 * MSDOS porting by Kai-Uwe Herbing (herbing@netmbx.netmbx.de)
 * incoporated.
 * 
 * Revision 1.1  92/05/22  11:58:17  ilh
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
# include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <errno.h>
#include "clp.h"
#include "t1lib.h"

/* int32 must be at least 32-bit */
#if INT_MAX >= 0x7FFFFFFFUL
typedef int int32;
typedef unsigned int uint32;
#else
typedef long int32;
typedef unsigned long uint32;
#endif

#define MAXBLOCKLEN (1L<<17)
#define DEFAULT_BLOCKLEN (1L<<12)

typedef unsigned char byte;

static FILE *ofp;

/* for PFB block buffering */
static byte *blockbuf = 0;
static uint32 blocklen = 0;
static uint32 max_blocklen = 0xFFFFFFFFUL;
static uint32 blockpos = 0;
static int blocktyp = ASCII;

static int binary_blocks_written = 0;

void fatal_error(const char *message, ...);
void error(const char *message, ...);


/* This function flushes a buffered PFB block. */

static void pfb_output_block()
{
  /* do nothing if nothing in block */
  if (blockpos == 0)
    return;
  
  /* output four-byte block length */
  putc(MARKER, ofp);
  putc(blocktyp, ofp);
  putc((int)(blockpos & 0xff), ofp);
  putc((int)((blockpos >> 8) & 0xff), ofp);
  putc((int)((blockpos >> 16) & 0xff), ofp);
  putc((int)((blockpos >> 24) & 0xff), ofp);
  
  /* output block data */
  fwrite(blockbuf, 1, blockpos, ofp);
  
  /* mark block buffer empty and uninitialized */
  blockpos =  0;
  if (blocktyp == BINARY)
    binary_blocks_written++;
}

static void
pfb_grow_block(void)
{
  if (!blockbuf) {
    /* first time through: allocate blockbuf */
    blocklen = DEFAULT_BLOCKLEN;
    blockbuf = (byte *)malloc(blocklen);
    if (!blockbuf)
      fatal_error("out of memory");
    
  } else if (blocklen < max_blocklen) {
    /* later: grow blockbuf */
    int new_blocklen = blocklen * 2;
    byte *new_blockbuf;
    if (new_blocklen > max_blocklen)
      new_blocklen = max_blocklen;
    new_blockbuf = (byte *)malloc(new_blocklen);
    if (!new_blockbuf) {
      error("out of memory; muddling on with a smaller block size");
      max_blocklen = blocklen;
      pfb_output_block();
    } else {
      memcpy(new_blockbuf, blockbuf, blocklen);
      free(blockbuf);
      blockbuf = new_blockbuf;
      blocklen = new_blocklen;
    }
    
  } else
    /* blockbuf already the right size, just output the block */
    pfb_output_block();
}

/* This function outputs a single byte.  If output is in PFB format then output
   is buffered through blockbuf[].  If output is in PFA format, then output
   will be hexadecimal if in_eexec is set, ASCII otherwise. */

static void pfb_output_byte(byte b)
{
  if (blockpos == blocklen)
    pfb_grow_block();
  blockbuf[blockpos++] = b;
}

/* PFB font_reader functions */

static void
pfb_output_ascii(char *s)
{
  if (blocktyp == BINARY) {
    pfb_output_block();
    blocktyp = ASCII;
  }
  for (; *s; s++)
    pfb_output_byte((byte)*s);
}

static void
pfb_output_binary(char *s, int len)
{
  if (blocktyp == ASCII) {
    pfb_output_block();
    blocktyp = BINARY;
  }
  for (; len > 0; len--, s++)
    pfb_output_byte((byte)*s);
}

static void
pfb_output_end()
{
  pfb_output_block();
  putc(MARKER, ofp);
  putc(DONE, ofp);
}


/*****
 * Command line
 **/

#define BLOCK_LEN_OPT	300
#define OUTPUT_OPT	301
#define VERSION_OPT	302
#define HELP_OPT	303

static Clp_Option options[] = {
  { "block-length", 'l', BLOCK_LEN_OPT, Clp_ArgInt, 0 },
  { "help", 0, HELP_OPT, 0, 0 },
  { "length", 0, BLOCK_LEN_OPT, Clp_ArgInt, 0 },
  { "output", 'o', OUTPUT_OPT, Clp_ArgString, 0 },
  { "version", 0, VERSION_OPT, 0, 0 },
};
static char *program_name;


void
fatal_error(const char *message, ...)
{
  va_list val;
  va_start(val, message);
  fprintf(stderr, "%s: ", program_name);
  vfprintf(stderr, message, val);
  putc('\n', stderr);
  exit(1);
}

void
error(const char *message, ...)
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
  fprintf(stderr, "Usage: %s [OPTION]... [INPUT [OUTPUT]]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  printf("\
`T1binary' translates a PostScript Type 1 font from ASCII (PFA) to compact\n\
binary (PFB) format. The result is written to the standard output unless an\n\
OUTPUT file is given.\n\
\n\
Usage: %s [OPTION]... [INPUT [OUTPUT]]\n\
\n\
Options:\n\
  -l, --block-length=NUM        Set max output block length.\n\
  -o, --output=FILE             Write output to FILE.\n\
  -h, --help                    Print this message and exit.\n\
      --version                 Print version number and warranty and exit.\n\
\n\
Report bugs to <eddietwo@lcs.mit.edu>.\n", program_name);
}


int main(int argc, char **argv)
{
  int c;
  FILE *ifp = 0;
  const char *ifp_filename = "<stdin>";
  struct font_reader fr;
  
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = (char *)Clp_ProgramName(clp);
  
  /* interpret command line arguments using CLP */
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case BLOCK_LEN_OPT:
      max_blocklen = clp->val.i;
      if (max_blocklen <= 0) {
	max_blocklen = 1;
	error("warning: block length raised to %d", max_blocklen);
      }
      break;
      
     output_file:
     case OUTPUT_OPT:
      if (ofp)
	fatal_error("output file already specified");
      if (strcmp(clp->arg, "-") == 0)
	ofp = stdout;
      else {
	ofp = fopen(clp->arg, "wb");
	if (!ofp) fatal_error("%s: %s", clp->arg, strerror(errno));
      }
      break;
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("t1binary (LCDF t1utils) %s\n", VERSION);
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
	ifp_filename = clp->arg;
	ifp = fopen(clp->arg, "r");
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
  /* As we are processing a PFB (binary) output */
  /* file, we must set its file mode to binary. */
  _setmode(_fileno(ofp), _O_BINARY);
#endif
  
  /* prepare font reader */
  fr.output_ascii = pfb_output_ascii;
  fr.output_binary = pfb_output_binary;
  fr.output_end = pfb_output_end;
  
  /* peek at first byte to see if it is the PFB marker 0x80 */
  c = getc(ifp);
  ungetc(c, ifp);
  
  /* do the file */
  if (c == MARKER)
    process_pfb(ifp, ifp_filename, &fr);
  else if (c == '%')
    process_pfa(ifp, ifp_filename, &fr);
  else
    fatal_error("%s does not start with font marker (`%' or 0x80)");
  
  fclose(ifp);
  fclose(ofp);
  
  if (!binary_blocks_written)
    fatal_error("no binary blocks written! Are you sure this was a font?");
  
  return 0;
}
