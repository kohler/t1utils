/* t1lib
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
#include <ctype.h>
#include <string.h>
#include "t1lib.h"
#ifdef __cplusplus
extern "C" {
#endif

#define PFA_ASCII	1
#define PFA_EEXEC_TEST	2
#define PFA_HEX		3
#define PFA_BINARY	4

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
  if (*string == '\0' || *string == '\n') return 0;
  while (*string == '0')
    string++;
  return *string == '\0' || *string == '\n';
}

/* This function handles the entire file. */

#define LINESIZE 1024

void
process_pfa(FILE *ifp, const char *ifp_filename, struct font_reader *fr)
{
  /* Loop until no more input. We need to look for `currentfile eexec' to
     start eexec section (hex to binary conversion) and line of all zeros to
     switch back to ASCII. */
  
  /* Don't use fgets() in case line-endings are indicated by bare \r's, as
     occurs in Macintosh fonts. */

  /* 2.Aug.1999 - At the behest of Tom Kacvinsky <tjk@ams.org>, support binary
     PFA fonts. */
  
  char line[LINESIZE];
  int c = 0;
  int blocktyp = PFA_ASCII;
  
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
    else if (c == '\r' && blocktyp == PFA_ASCII) {
      /* change CR or CR/LF into LF */
      c = getc(ifp);
      if (c != '\n') ungetc(c, ifp);
      *p++ = '\n';
    } else if (c != EOF)
      *p++ = c;
    
    *p = 0;
    
    /* check immediately after "currentfile eexec" for ASCII or binary */
    if (blocktyp == PFA_EEXEC_TEST) {
      char *last = p;
      for (p = line; *p && isspace(*p); p++) ;
      if (!*p)
	continue;
      else if (isxdigit(p[0]) && isxdigit(p[1])
	       && isxdigit(p[2]) && isxdigit(p[3]))
	blocktyp = PFA_HEX;
      else
	blocktyp = PFA_BINARY;
      memmove(line, p, last - p);
      p = line + (last - p);
    }
    
    /* now that we have the line, handle it */
    if (blocktyp == PFA_ASCII) {
      fr->output_ascii(line);
      if (strncmp(line, "currentfile eexec", 17) == 0) {
	for (p = line + 17; isspace(*p); p++) ;
	if (!*p) blocktyp = PFA_EEXEC_TEST;
      }
      
    } else { /* blocktyp == PFA_HEX || blocktyp == PFA_BINARY */
      if (all_zeroes(line)) {	/* XXX not safe */
	fr->output_ascii(line);
	blocktyp = PFA_ASCII;
      } else if (blocktyp == PFA_HEX) {
	int len = translate_hex_string(line);
	if (len) fr->output_binary((unsigned char *)line, len);
      } else
	fr->output_binary((unsigned char *)line, p - line);
    }
  }
  
  fr->output_end();
}

/* Process a PFB file. */

/* XXX Doesn't handle "currentfile eexec" as intelligently as process_pfa
   does. */

static int
handle_pfb_ascii(struct font_reader *fr, char *line, int len)
{
  /* Divide PFB_ASCII blocks into lines */
  int start = 0;
  
  while (1) {
    int pos = start;
    
    while (pos < len && line[pos] != '\n' && line[pos] != '\r')
      pos++;
    
    if (pos >= len) {
      if (pos == start)
	return 0;
      else if (start == 0 && pos == LINESIZE - 1) {
	line[pos] = 0;
	fr->output_ascii(line);
	return 0;
      } else {
	memmove(line, line + start, pos - start);
	return pos - start;
      }
      
    } else if (pos < len - 1 && line[pos] == '\r' && line[pos+1] == '\n') {
      line[pos] = '\n'; line[pos+1] = 0;
      fr->output_ascii(line + start);
      start = pos + 2;
      
    } else {
      char save = line[pos+1];
      line[pos] = '\n'; line[pos+1] = 0;
      fr->output_ascii(line + start);
      line[pos+1] = save;
      start = pos + 1;
    }
  }
}


void
process_pfb(FILE *ifp, const char *ifp_filename, struct font_reader *fr)
{
  int blocktyp = 0;
  int block_len = 0;
  int c = 0;
  int filepos = 0;
  int linepos = 0;
  char line[LINESIZE];
  
  while (1) {
    while (block_len == 0) {
      c = getc(ifp);
      blocktyp = getc(ifp);
      if (c != PFB_MARKER
	  || (blocktyp != PFB_ASCII && blocktyp != PFB_BINARY
	      && blocktyp != PFB_DONE)) {
	if (c == EOF || blocktyp == EOF)
	  error("%s corrupted: no end-of-file marker", ifp_filename);
	else
	  error("%s corrupted: bad block marker at position %d",
		ifp_filename, filepos);
	blocktyp = PFB_DONE;
      }
      if (blocktyp == PFB_DONE)
	goto done;
      
      block_len = getc(ifp) & 0xFF;
      block_len |= (getc(ifp) & 0xFF) << 8;
      block_len |= (getc(ifp) & 0xFF) << 16;
      block_len |= (getc(ifp) & 0xFF) << 24;
      if (feof(ifp)) {
	error("%s corrupted: bad block length at position %d",
	      ifp_filename, filepos);
	blocktyp = PFB_DONE;
	goto done;
      }
      filepos += 6;
    }

    /* read the block in its entirety, in LINESIZE chunks */
    while (block_len > 0) {
      int rest = LINESIZE - 1 - linepos; /* leave space for '\0' */
      int n = (block_len > rest ? rest : block_len);
      int actual = fread(line + linepos, 1, n, ifp);
      if (actual != n) {
	error("%s corrupted: block short by %d bytes at position %d",
	      ifp_filename, block_len - actual, filepos);
	block_len = actual;
      }
      
      if (blocktyp == PFB_BINARY)
	fr->output_binary((unsigned char *)line, actual);
      else
	linepos = handle_pfb_ascii(fr, line, linepos + actual);
      
      block_len -= actual;
      filepos += actual;
    }
    
    /* handle any leftover line */
    if (linepos > 0) {
      line[linepos] = 0;
      fr->output_ascii(line);
      linepos = 0;
    }
  }
  
 done:
  c = getc(ifp);
  if (c != EOF)
    error("%s corrupted: data after PFB end marker at position %d",
	  ifp_filename, filepos - 2);
  fr->output_end();
}

#ifdef __cplusplus
}
#endif
