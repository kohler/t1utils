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
 * Old change log:
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

#ifdef _MSDOS
  #include <fcntl.h>
  #include <io.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include "clp.h"

/* int32 must be at least 32-bit */
#if INT_MAX >= 0x7FFFFFFFUL
typedef int int32;
#else
typedef long int32;
#endif

#define MAXBLOCKLEN ((1L<<17)-6)
#define MINBLOCKLEN ((1L<<8)-6)

#define MARKER   128
#define ASCII    1
#define BINARY   2
#define DONE     3

typedef unsigned char byte;

static FILE *ifp = stdin;
static FILE *ofp = stdout;

/* for PFB block buffering */
static byte blockbuf[MAXBLOCKLEN];
static int32 blocklen = MAXBLOCKLEN;
static int32 blockpos = -1;
static int blocktyp = ASCII;

static int binary_blocks_written = 0;

/* This function flushes a buffered PFB block. */

static void output_block()
{
  int32 i;

  /* output four-byte block length */
  putc((int)(blockpos & 0xff), ofp);
  putc((int)((blockpos >> 8) & 0xff), ofp);
  putc((int)((blockpos >> 16) & 0xff), ofp);
  putc((int)((blockpos >> 24) & 0xff), ofp);

  /* output block data */
  for (i = 0; i < blockpos; i++)
    putc(blockbuf[i], ofp);

  /* mark block buffer empty and uninitialized */
  blockpos =  -1;
  if (blocktyp == BINARY)
    binary_blocks_written++;
}

/* This function outputs a single byte.  If output is in PFB format then output
   is buffered through blockbuf[].  If output is in PFA format, then output
   will be hexadecimal if in_eexec is set, ASCII otherwise. */

static void output_byte(byte b)
{
  if (blockpos < 0) {
    putc(MARKER, ofp);
    putc(blocktyp, ofp);
    blockpos = 0;
  }
  blockbuf[blockpos++] = b;
  if (blockpos == blocklen)
    output_block();
}

/* This function outputs a null-terminated string through the PFB buffering. */

static void output_string(char *string)
{
  while (*string)
    output_byte((byte) *string++);
}

/* This function returns the value (0-15) of a single hex digit.  It returns
   0 for an invalid hex digit. */

static int hexval(char c)
{
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  else if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  else if (c >= '0' && c <= '9')
    return c - '0';
  else
    return 0;
}

/* This function outputs the binary data associated with a string of
   hexadecimal digits.  We allow an odd number of digits. */

static void output_hex_string(char *string)
{
  static char saved_orphan = 0;
  if (saved_orphan && string[0] && string[0] != '\n') {
    output_byte((byte)((hexval(saved_orphan) << 4) + hexval(string[0])));
    string++;
    saved_orphan = 0;
  }
  while (string[0] && string[0] != '\n') {
    if (!string[1]) {
      saved_orphan = string[0];
      return;
    }
    output_byte((byte)((hexval(string[0]) << 4) + hexval(string[1])));
    string += 2;
  }
}

/* This function returns 1 if the string contains all '0's. */

static int all_zeroes(char *string)
{
  while (*string == '0')
    string++;
  return *string == '\0' || *string == '\n';
}

/* This function handles a single line, which should be terminated by \n\0. */

static void handle_line(char *line)
{
  if (blocktyp == ASCII && strcmp(line, "currentfile eexec\n") == 0) {
    output_string(line);
    output_block();
    blocktyp = BINARY;
  } else if (blocktyp == BINARY && all_zeroes(line)) {
    output_block();
    blocktyp = ASCII;
    output_string(line);
  } else if (blocktyp == ASCII) {
    output_string(line);
  } else {
    output_hex_string(line);
  }
}

/* This function handles the entire file. */
#define LINESIZE 512

static void process(FILE *ifp, FILE *ofp)
{
  /* Finally, we loop until no more input.  We need to look for `currentfile
     eexec' to start eexec section (hex to binary conversion) and line of all
     zeros to switch back to ASCII. */
  
  /* Don't use fgets() in case line-endings are indicated by bare \r's, as
     occurs in Macintosh fonts. */
  
  char line[LINESIZE];
  int c = 0;
  while (c != EOF) {
    char *p = line;
    c = getc(ifp);
    while (c != EOF && c != '\r' && c != '\n' && p < line + LINESIZE - 1) {
      *p++ = c;
      c = getc(ifp);
    }

    /* handle the end of the line */
    if (p == line + LINESIZE - 1)
      /* buffer overrun: don't append newline even if we have it */
      ungetc(c, ifp);
    else if (c == '\r') {
      /* change CR or CR/LF into LF */
      c = getc(ifp);
      if (c != '\n') ungetc(c, ifp);
      *p++ = '\n';
    } else if (c == '\n')
      *p++ = '\n';
    
    *p = 0;
    handle_line(line);
  }
  output_block();
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
  fprintf(stderr, "Usage: %s [-l NUM] [input [output]]\n\
Type %s --help for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  printf("\
`T1binary' translates a PostScript Type 1 font from ASCII to binary format.\n\
Output is written to standard out. Use `t1ascii' to go the other way.\n\
\n\
Usage: %s [options] [input [output]]\n\
\n\
Options:\n\
  --block-length=NUM, -l NUM    Output blocks will have size NUM.\n\
  --output=FILE, -o FILE        Write output to FILE.\n\
  --help, -h                    Print this message and exit.\n\
  --version                     Print version number and warranty and exit.\n\
\n\
Report bugs to <eddietwo@lcs.mit.edu>.\n", program_name);
}


int main(int argc, char **argv)
{
  int c;
  int input_given = 0;
  int output_given = 0;
  
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = (char *)Clp_ProgramName(clp);
  
  /* interpret command line arguments using CLP */
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
     case BLOCK_LEN_OPT:
      blocklen = clp->val.i;
      if (blocklen < MINBLOCKLEN) {
	blocklen = MINBLOCKLEN;
	error("warning: block length raised to %d", blocklen);
      } else if (blocklen > MAXBLOCKLEN) {
	blocklen = MAXBLOCKLEN;
	error("warning: block length lowered to %d", blocklen);
      }
      break;
      
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
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("t1binary version %s\n", VERSION);
      printf("Copyright (C) 1992-8 I. Lee Hetherington, Eddie Kohler et al.\n\
This is free software; see the source for copying conditions.\n\
There is NO warranty, not even for merchantability or fitness for a\n\
particular purpose.\n");
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
	ifp = fopen(clp->arg, "r");
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
    /* As we are processing a PFB (binary) output */
    /* file, we must set its file mode to binary. */
    _setmode(_fileno(ofp), _O_BINARY);
  #endif

  /* peek at first byte to see if it is the PFB marker 0x80 */
  c = getc(ifp);
  if (c == MARKER) {
    fprintf(stderr,
	    "error: input may already be binary (starts with 0x80)\n");
    exit(1);
  }
  ungetc(c, ifp);

  /* do the file */
  process(ifp, ofp);
  
  fclose(ifp);
  fclose(ofp);
  
  if (!binary_blocks_written) {
    fprintf(stderr, "error: no binary blocks written! \
Are you sure this was a font?\n");
    exit(1);
  }
  
  return 0;
}
