/* t1binary
 *
 * This file contains functions for reading PFA and PFB files.
 *
 * (C) 1999 Eddie Kohler <eddietwo@lcs.mit.edu>, after code by
 * I. Lee Hetherington <ilh@lcs.mit.edu>. All rights reserved.
 *
 * Permission is hereby granted to use, modify, and distribute this program
 * for any purpose provided this copyright notice remains intact.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include "t1lib.h"
#define LINESIZE 512

/* This function returns the value (0-15) of a single hex digit.  It returns
   0 for an invalid hex digit. */

static int
hexval(char c)
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

/* This function translates a string of hexadecimal digits into binary data.
   We allow an odd number of digits. Returns length of binary data. */

static int
translate_hex_string(char *s)
{
  static char saved_orphan = 0;
  int c1 = saved_orphan;
  char *start = s;
  char *t = s;
  for (; *s; s++) {
    if (isspace(*s)) continue;
    if (c1) {
      *t++ = (hexval(c1) << 4) + hexval(*s);
      c1 = 0;
    } else
      c1 = *s;
  }
  saved_orphan = c1;
  return t - start;
}

/* This function returns 1 if the string contains all '0's. */

static int
all_zeroes(char *string)
{
  while (*string == '0')
    string++;
  return *string == '\0' || *string == '\n';
}

/* This function handles the entire file. */

void
process_pfa(FILE *ifp, const char *ifp_filename, struct font_reader *fr)
{
  /* Loop until no more input. We need to look for `currentfile eexec' to
     start eexec section (hex to binary conversion) and line of all zeros to
     switch back to ASCII. */
  
  /* Don't use fgets() in case line-endings are indicated by bare \r's, as
     occurs in Macintosh fonts. */
  
  char line[LINESIZE];
  int c = 0;
  int blocktyp = ASCII;
  
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
    
    /* now that we have the line, handle it */
    if (blocktyp == ASCII) {
      fr->output_ascii(line);
      if (strncmp(line, "currentfile eexec", 17) == 0) {
	for (p = line + 17; isspace(*p); p++) ;
	if (!*p) blocktyp = BINARY;
      }
    } else { /* blocktyp == BINARY */
      if (all_zeroes(line)) {
	fr->output_ascii(line);
	blocktyp = ASCII;
      } else {
	int len = translate_hex_string(line);
	fr->output_binary(line, len);
      }
    }
  }
  
  fr->output_end();
}

/* Process a PFB file. */

void
process_pfb(FILE *ifp, const char *ifp_filename, struct font_reader *fr)
{
  int blocktyp = 0;
  int block_len = 0;
  int c = 0;
  int nblocks = 0;
  int filepos = 0;
  char line[LINESIZE + 1];
  
  while (1) {
    while (block_len == 0) {
      c = getc(ifp);
      blocktyp = getc(ifp);
      if (c != MARKER
	  || (blocktyp != ASCII && blocktyp != BINARY && blocktyp != DONE)) {
	if (c == EOF || blocktyp == EOF)
	  error("%s corrupted: no end-of-file marker");
	else
	  error("%s corrupted: bad block marker at position %d",
		ifp_filename, filepos);
	blocktyp = DONE;
      }
      if (blocktyp == DONE)
	goto done;
      
      block_len = getc(ifp) & 0xFF;
      block_len |= (getc(ifp) & 0xFF) << 8;
      block_len |= (getc(ifp) & 0xFF) << 16;
      block_len |= (getc(ifp) & 0xFF) << 24;
      if (feof(ifp)) {
	error("%s corrupted: bad block length at position %d",
	      ifp_filename, filepos);
	blocktyp = DONE;
	goto done;
      }
      filepos += 6;
    }
    
    while (block_len > 0) {
      int n = (block_len > LINESIZE ? LINESIZE : block_len);
      int actual = fread(line, 1, n, ifp);
      if (actual != n) {
	error("%s corrupted: block short by %d bytes at position %d",
	      ifp_filename, block_len - actual, filepos);
	block_len = actual;
      }
      
      if (blocktyp == ASCII) {
	line[actual] = 0;
	fr->output_ascii(line);
      } else
	fr->output_binary(line, actual);
      
      block_len -= actual;
      filepos += actual;
    }
  }
  
 done:
  c = getc(ifp);
  if (c != EOF)
    error("%s corrupted: data after PFB end marker at position %d",
	  ifp_filename, filepos - 2);
  fr->output_end();
}
