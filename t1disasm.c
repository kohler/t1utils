/* t1disasm
 *
 * This program `disassembles' Adobe Type-1 font programs in either PFB or PFA
 * format.  It produces a human readable/editable pseudo-PostScript file by
 * performing eexec and charstring decryption as specified in the `Adobe Type 1
 * Font Format' version 1.1 (the `black book').  There is a companion program,
 * t1asm, which `assembles' such a pseudo-PostScript file into either PFB or
 * PFA format.
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
 * Revision 1.4  92/07/10  10:55:08  ilh
 * Added support for additional PostScript after the closefile command
 * (ie., some fonts have {restore}if' after the cleartomark).  Also,
 * removed hardwired charstring start command (-| or RD) in favor of
 * automatically determining it.
 * 
 * Revision 1.3  92/06/23  10:57:53  ilh
 * MSDOS porting by Kai-Uwe Herbing (herbing@netmbx.netmbx.de)
 * incoporated.
 * 
 * Revision 1.2  92/05/22  12:05:33  ilh
 * Fixed bug where we were counting on sprintf to return its first
 * argument---not true in ANSI C.  This bug was detected by Piet
 * Tutelaers (rcpt@urc.tue.nl).  Also, fixed (signed) integer overflow
 * error when testing high-order bit of integer for possible
 * sign-extension by making comparison between unsigned integers.
 *
 * Revision 1.1  92/05/22  12:04:07  ilh
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

/* int32 must be at least 32-bit and uint16 must be at least 16-bit */
#if INT_MAX >= 0x7FFFFFFFUL
typedef int int32;
#else
typedef long int32;
#endif
#if USHRT_MAX >= 0xFFFFUL
typedef unsigned short uint16;
#else
typedef unsigned int uint16;
#endif

#define LINESIZE 512

#define cgetc()  cdecrypt((byte)(egetc() & 0xff))

typedef unsigned char byte;

static FILE *ifp;
static FILE *ofp;
static char line[LINESIZE + 1];	/* account for '\0' at end of line */
static int start_charstring = 0;
static int final_ascii = 0;
static int lenIV = 4;
static char cs_start[10];
static int unknown = 0;

/* decryption stuff */
static uint16 er, cr;
static uint16 c1 = 52845, c2 = 22719;

/* This function looks for `currentfile eexec' string and returns 1 once found.
   If c == 0, then simply check the status. */

static int eexec_scanner(int c)
{
  static char *key = "currentfile eexec\n";
  static char *p = 0;

  if (!p)
    p = key;

  if (c && *p) {
    if ((char) (c & 0xff) == *p)
      ++p;
    else
      p = key;
  }
  return *p == '\0';
}

/* This function returns the value of a single hex digit. */

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

/* This function returns a single character at a time from a PFA or PFB file.
   This stream is mixed ASCII and binary bytes.  For PFB files, the section
   headers are removed, and \r is replaced by \n in ASCII sections.  For PFA
   files, the hexdecimal data is turned into binary bytes. */

static int bgetc()
{
  static int first_byte = 1;
  static int is_pfa = 0;
  static int is_pfb = 0;
  static int32 pfb_remaining = 0;
  int c, val;

  /* is_pfa == 1 means PFA initial ASCII section
     is_pfa == 2 means PFA hexadecimal section
     is_pfb == 1 means PFB ASCII section
     is_pfb == 2 means PFB binary section */

  c = fgetc(ifp);
  
  if (c == EOF)
    return EOF;
  
  if (first_byte) {
    /* Determine if this is a PFA or PFB file by looking at first byte. */
    if (c == 0x80) {
      is_pfb = 1;
      is_pfa = 0;

#if defined(_MSDOS) || defined(_WIN32)
      /* If we are processing a PFB (binary) input  */
      /* file, we must set its file mode to binary. */
      _setmode(_fileno(ifp), _O_BINARY);
#endif

    } else {
      is_pfb = 0;
      is_pfa = 1;
    }
    first_byte = 0;
  }

  if (is_pfb) {
    /* PFB */
    if (pfb_remaining == 0) {
      /* beginning of block---we know c == 0x80 at this point */
      switch (fgetc(ifp)) {
      case 1:
	is_pfb = 1;
	break;
      case 2:
	is_pfb = 2;
	break;
      case 3:
	return EOF;
      default:
	fprintf(stderr, "error: is this really a PFB file?\n");
	exit(1);
      }
      /* get block length */
      pfb_remaining = (int32)(fgetc(ifp) & 0xff);
      pfb_remaining |= (int32)(fgetc(ifp) & 0xff) << 8;
      pfb_remaining |= (int32)(fgetc(ifp) & 0xff) << 16;
      pfb_remaining |= (int32)(fgetc(ifp) & 0xff) << 24;
      /* get character */
      c = fgetc(ifp);
      if (c == EOF)
	return EOF;
    }
    --pfb_remaining;
    /* in ASCII section change return to newline */
    if (is_pfb == 1 && c == '\r')
      c = '\n';
    (void) eexec_scanner(c);
    return c;
  } else {
    /* PFA */
    if (final_ascii) {
      return c;
    } else if (is_pfa == 1) {
      /* in initial ASCII */
      if (eexec_scanner(c))
	is_pfa = 2;
      return c;
    } else {
      /* in hexadecimal */
      while (isspace(c))
	c = fgetc(ifp);
      val = hexval((char)c) << 4;
      val |= hexval((char)(c = fgetc(ifp)));
      if (c == EOF)
	return EOF;
      return val;
    }
  }
}

/* This functions returns a line of (non-decrypted) characters.  A line is
   terminated by length (including terminating null) greater than LINESIZE, a
   newline \n.  The line, including the terminating newline, is put into
   line[]. */

static void bgetline()
{
  int c;
  char *p = line;

  while (p < line + LINESIZE) {
    c = bgetc();
    if (c == EOF)
      break;
    *p++ = (char) c;
    if (c == '\r') {                              /* map \r to \n */
      p[-1] = '\n';
      break;
    }
    if (c == '\n')
      break;
  }
  *p = '\0';
}

/* Two separate decryption functions because eexec and charstring decryption
   must proceed in parallel. */

static byte edecrypt(byte cipher)
{
  byte plain;

  plain = (byte)(cipher ^ (er >> 8));
  er = (uint16)((cipher + er) * c1 + c2);
  return plain;
}

static byte cdecrypt(byte cipher)
{
  byte plain;

  plain = (byte)(cipher ^ (cr >> 8));
  cr = (uint16)((cipher + cr) * c1 + c2);
  return plain;
}

/* This function returns 1 the first time the eexec_scanner returns 1. */

static int immediate_eexec()
{
  static int reported = 0;

  if (!reported && eexec_scanner(0)) {
    reported = 1;
    return 1;
  } else {
    return 0;
  }
}

/* This function returns a single byte at a time through (possible) eexec
   decryption.  When immediate_eexec returns 1 it fires up the eexec decryption
   machinery. */

static int egetc()
{
  static int in_eexec = 0;
  int c;
  
  if ((c = bgetc()) == EOF)
    return EOF;
  
  if (!in_eexec) {
    if (immediate_eexec()) {
      /* start eexec decryption */
      in_eexec = 1;
      er = 55665;
      /* toss out four random bytes */
      (void) edecrypt((byte)(bgetc() & 0xff));
      (void) edecrypt((byte)(bgetc() & 0xff));
      (void) edecrypt((byte)(bgetc() & 0xff));
      (void) edecrypt((byte)(bgetc() & 0xff));
    }
    return c;
  } else {
    return (int)edecrypt((byte)(c & 0xff));
  }
}

/* This function returns a line of eexec decrypted characters.  A line is
   terminated by length (including terminating null) greater than LINESIZE, a
   newline \n, or the special charstring start sequence in cs_start[] (usually
   ` -| ' or ` RD ').  The line, including the terminating newline or
   charstring start sequence is put into line[].  If terminated by a charstring
   start sequence, the flag start_charstring is set to 1. */

static void egetline() { int c; int l = strlen(cs_start); char *p = line;

  start_charstring = 0;
  while (p < line + LINESIZE) {
    c = egetc();
    if (c == EOF)
      break;
    *p++ = (char) c;
    if (l > 0 &&
	p >= line + l + 2 &&
	p[-2 - l] == ' ' &&
	p[-1] == ' ' &&
	strncmp(p - l - 1, cs_start, l) == 0) {
      p -= l + 2;
      start_charstring = 1;
      break;
    }
    if (c == '\r') {                              /* map \r to \n */
      p[-1] = '\n';
      break;
    }
    if (c == '\n')
      break;
  }
  *p = '\0';
}

/* If the line contains an entry of the form `/lenIV <num>' then set the global
   lenIV to <num>.  This indicates the number of random bytes at the beginning
   of each charstring. */

static void set_lenIV()
{
  char *p = strstr(line, "/lenIV ");

  if (p && isdigit(p[7])) {
    lenIV = atoi(p + 7);
  }
}

static void set_cs_start()
{
  char *p, *q, *r;

  if ((p = strstr(line, "string currentfile"))) {
    /* locate the name of the charstring start command */
    *p = '\0';					  /* damage line[] */
    q = strrchr(line, '/');
    if (q) {
      r = cs_start;
      ++q;
      while (!isspace(*q) && *q != '{')
	*r++ = *q++;
      *r = '\0';
    }
    *p = 's';					  /* repair line[] */
  }
}

/* Subroutine to output strings. */

static void output(char *string)
{
  fprintf(ofp, "%s", string);
}

/* Subroutine to neatly format output of charstring tokens.  If token = "\n",
   then a newline is output.  If at start of line (start == 1), prefix token
   with tab, otherwise a space. */

static void output_token(char *token)
{
  static int start = 1;

  if (strcmp(token, "\n") == 0) {
    fprintf(ofp, "\n");
    start = 1;
  } else {
    fprintf(ofp, "%s%s", start ? "\t" : " ", token);
    start = 0;
  }
}

/* Subroutine to decrypt and ASCII-ify tokens in charstring data.  First, the
   length (in bytes) of the charstring is determined from line[].  Then the
   charstring decryption machinery is fired up, skipping the first lenIV bytes.
   Finally, the decrypted tokens are expanded into human-readable form. */

static void do_charstring()
{
  int l = strlen(line);
  char *p = line + l - 1;
  int cs_len;
  int i;
  int b;
  int32 val;
  char buf[20];

  while (p >= line && *p != ' ' && *p != '\t')
    --p;
  cs_len = atoi(p);

  *p = '\0';
  output(line);
  output(" {\n");

  cr = 4330;
  for (i = 0; i < lenIV; i++, cs_len--)
    (void) cgetc();

  while (cs_len > 0) {
    --cs_len;
    b = cgetc();
    if (b >= 32) {
      if (b >= 32 && b <= 246) {
	val = b - 139;
      } else if (b >= 247 && b <= 250) {
	--cs_len;
	val = (b - 247)*256 + 108 + cgetc();
      } else if (b >= 251 && b <= 254) {
	--cs_len;
	val = -(b - 251)*256 - 108 - cgetc();
      } else {
	cs_len -= 4;
	val =  (cgetc() & 0xff) << 24;
	val |= (cgetc() & 0xff) << 16;
	val |= (cgetc() & 0xff) <<  8;
	val |= (cgetc() & 0xff) <<  0;
	/* in case an int32 is larger than four bytes---sign extend */
#if INT_MAX > 0x7FFFFFFFUL
	if (val & 0x80000000)
	  val |= ~0x7FFFFFFF;
#endif
      }
      sprintf(buf, "%d", val);
      output_token(buf);
    } else {
      switch (b) {
      case 0: output_token("error"); break;		/* special */
      case 1: output_token("hstem"); break;
      case 3: output_token("vstem"); break;
      case 4: output_token("vmoveto"); break;
      case 5: output_token("rlineto"); break;
      case 6: output_token("hlineto"); break;
      case 7: output_token("vlineto"); break;
      case 8: output_token("rrcurveto"); break;
      case 9: output_token("closepath"); break;		/* Type 1 ONLY */
      case 10: output_token("callsubr"); break;
      case 11: output_token("return"); break;
      case 13: output_token("hsbw"); break;		/* Type 1 ONLY */
      case 14: output_token("endchar"); break;
      case 16: output_token("blend"); break;		/* Type 2 */
      case 18: output_token("hstemhm"); break;		/* Type 2 */
      case 19: output_token("hintmask"); break;		/* Type 2 */
      case 20: output_token("cntrmask"); break;		/* Type 2 */
      case 21: output_token("rmoveto"); break;
      case 22: output_token("hmoveto"); break;
      case 23: output_token("vstemhm"); break;		/* Type 2 */
      case 24: output_token("rcurveline"); break;	/* Type 2 */
      case 25: output_token("rlinecurve"); break;	/* Type 2 */
      case 26: output_token("vvcurveto"); break;	/* Type 2 */
      case 27: output_token("hhcurveto"); break;	/* Type 2 */
      case 28: {		/* Type 2 */
	/* short integer */
	cs_len -= 2;
	val =  (cgetc() & 0xff) << 8;
	val |= (cgetc() & 0xff);
	if (val & 0x8000)
	  val |= ~0x7FFF;
	sprintf(buf, "%d", val);
	output_token(buf);
      }
      case 29: output_token("callgsubr"); break;	/* Type 2 */
      case 30: output_token("vhcurveto"); break;
      case 31: output_token("hvcurveto"); break;
      case 12:
	--cs_len;
	switch (b = cgetc()) {
	case 0: output_token("dotsection"); break;	/* Type 1 ONLY */
	case 1: output_token("vstem3"); break;		/* Type 1 ONLY */
	case 2: output_token("hstem3"); break;		/* Type 1 ONLY */
	case 3: output_token("and"); break;		/* Type 2 */
	case 4: output_token("or"); break;		/* Type 2 */
	case 5: output_token("not"); break;		/* Type 2 */
	case 6: output_token("seac"); break;		/* Type 1 ONLY */
	case 7: output_token("sbw"); break;		/* Type 1 ONLY */
	case 8: output_token("store"); break;		/* Type 2 */
	case 9: output_token("abs"); break;		/* Type 2 */
	case 10: output_token("add"); break;		/* Type 2 */
	case 11: output_token("sub"); break;		/* Type 2 */
	case 12: output_token("div"); break;
	case 13: output_token("load"); break;		/* Type 2 */
	case 14: output_token("neg"); break;		/* Type 2 */
	case 15: output_token("eq"); break;		/* Type 2 */
	case 16: output_token("callothersubr"); break;	/* Type 1 ONLY */
	case 17: output_token("pop"); break;		/* Type 1 ONLY */
	case 18: output_token("drop"); break;		/* Type 2 */
	case 20: output_token("put"); break;		/* Type 2 */
	case 21: output_token("get"); break;		/* Type 2 */
	case 22: output_token("ifelse"); break;		/* Type 2 */
	case 23: output_token("random"); break;		/* Type 2 */
	case 24: output_token("mul"); break;		/* Type 2 */
	case 26: output_token("sqrt"); break;		/* Type 2 */
	case 27: output_token("dup"); break;		/* Type 2 */
	case 28: output_token("exch"); break;		/* Type 2 */
	case 29: output_token("index"); break;		/* Type 2 */
	case 30: output_token("roll"); break;		/* Type 2 */
	case 33: output_token("setcurrentpoint"); break;/* Type 1 ONLY */
	case 34: output_token("hflex"); break;		/* Type 2 */
	case 35: output_token("flex"); break;		/* Type 2 */
	case 36: output_token("hflex1"); break;		/* Type 2 */
	case 37: output_token("flex1"); break;		/* Type 2 */
	default:
	  sprintf(buf, "escape_%d", b);
	  unknown++;
	  output_token(buf);
	  break;
	}
	break;
      default:
       sprintf(buf, "UNKNOWN_%d", b);
       unknown++;
       output_token(buf);
       break;
      }
      output_token("\n");
    }
  }
  output("\t}");
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
  fprintf(stderr, "Usage: %s [INPUT [OUTPUT]]\n\
Try `%s --help' for more information.\n",
	  program_name, program_name);
}


void
usage(void)
{
  printf("\
`T1disasm' translates a PostScript Type 1 font into a human-readable,\n\
human-editable format. Output is written to standard out unless an OUTPUT file\n\
is given. Use `t1asm' to go the other way.\n\
\n\
Usage: %s [OPTION]... [INPUT [OUTPUT]]\n\
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
  Clp_Parser *clp =
    Clp_NewParser(argc, argv, sizeof(options) / sizeof(options[0]), options);
  program_name = (char *)Clp_ProgramName(clp);
  
  /* interpret command line arguments using CLP */
  while (1) {
    int opt = Clp_Next(clp);
    switch (opt) {
      
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
      
     case HELP_OPT:
      usage();
      exit(0);
      break;
      
     case VERSION_OPT:
      printf("t1disasm version %s\n", VERSION);
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
  
  /* main loop---normally done when reach `mark currentfile closefile' on
     output (rest is garbage). */
  
  *cs_start = 0;
  while (1) {
    egetline();
    if (line[0] == '\0')
      break;
    set_lenIV();
    if (!*cs_start) set_cs_start();
    if (start_charstring)
      do_charstring();
    else
      output(line);
    if (strcmp(line, "mark currentfile closefile\n") == 0)
      break;
    }
  
  /* Final wrap-up: check for any PostScript after the cleartomark. */
  final_ascii = 1;
  while (bgetline(), line[0] != '\0') {
    if (strncmp(line, "cleartomark", 11) == 0) {
      if (line[11] && line[11] != '\n')
	output(line + 11);
      while (bgetline(), line[0] != '\0')
	output(line);
      break;
    }
  }
  
  fclose(ifp);
  fclose(ofp);
  
  if (unknown)
    error((unknown > 1
	   ? "encountered %d unknown charstring commands"
	   : "encountered %d unknown charstring command"),
	  unknown);
  
  return 0;
}
