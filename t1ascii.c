/* t1ascii
 *
 * This program takes an Adobe Type-1 font program in binary (PFB) format and
 * converts it to ASCII (PFA) format.
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
 * Revision 1.2  92/06/23  10:58:43  ilh
 * MSDOS porting by Kai-Uwe Herbing (herbing@netmbx.netmbx.de)
 * incoporated.
 * 
 * Revision 1.1  92/05/22  11:47:24  ilh
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

#define MARKER   128
#define ASCII    1
#define BINARY   2
#define DONE     3

static FILE *ifp = stdin;
static FILE *ofp = stdout;

/* This function reads a four-byte block length. */

static int32 read_length()
{
  int32 length;

  length = (int32)(getc(ifp) & 0xff);
  length |= (int32)(getc(ifp) & 0xff) << 8;
  length |= (int32)(getc(ifp) & 0xff) << 16;
  length |= (int32)(getc(ifp) & 0xff) << 24;

  return length;
}

/* This function outputs a single byte in hexadecimal.  It limits hexadecimal
   output to 64 columns. */

static void output_hex(int b)
{
  static char *hexchar = "0123456789ABCDEF";
  static int hexcol = 0;

  /* trim hexadecimal lines to 64 columns */
  if (hexcol >= 64) {
    putc('\n', ofp);
    hexcol = 0;
  }
  putc(hexchar[(b >> 4) & 0xf], ofp);
  putc(hexchar[b & 0xf], ofp);
  hexcol += 2;
}


/*****
 * Command line
 **/

#define OUTPUT_OPT	301
#define VERSION_OPT	302
#define HELP_OPT	303

static Clp_Option options[] = {
  { "help", 0, HELP_OPT, 0, 0 },
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
  fprintf(stderr, "Usage: %s [input [output]]\n\
Type %s --help for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  printf("\
`T1ascii' translates a PostScript Type 1 font from binary to ASCII format.\n\
Output is written to standard out. Use `t1binary' to go the other way.\n\
\n\
Usage: %s [options] [input [output]]\n\
\n\
Options:\n\
  --output=FILE, -o FILE        Write output to FILE.\n\
  --help, -h                    Print this message and exit.\n\
  --version                     Print version number and warranty and exit.\n\
\n\
Report bugs to <eddietwo@lcs.mit.edu>.\n", program_name);
}


int main(int argc, char **argv)
{
  int32 length;
  int c, block = 1, last_type = ASCII;
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
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("t1ascii version %s\n", VERSION);
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
    /* As we are processing a PFB (binary) input */
    /* file, we must set its file mode to binary. */
    _setmode(_fileno(ifp), _O_BINARY);
  #endif

  /* main loop through blocks */
  
  for (;;) {
    c = getc(ifp);
    if (c == EOF) {
      break;
    }
    if (c != MARKER) {
      if (block == 1)
	fatal_error("this file doesn't seem to be a PFB");
      else
	fatal_error("corrupt PFB: marker missing before block %d", block);
    }
    switch (c = getc(ifp)) {
    case ASCII:
      if (last_type != ASCII)
	putc('\n', ofp);
      last_type = ASCII;
      for (length = read_length(); length > 0; length--)
	if ((c = getc(ifp)) == '\r')
	  putc('\n', ofp);
	else
	  putc(c, ofp);
      break;
    case BINARY:
      last_type = BINARY;
      for (length = read_length(); length > 0; length--)
	output_hex(getc(ifp));
      break;
    case DONE:
      /* nothing to be done --- will exit at top of loop with EOF */
      break;
    default:
      fatal_error("corrupt PFB: bad block type `%d'", c);
      break;
    }
    block++;
  }
  fclose(ifp);
  fclose(ofp);

  return 0;
}
