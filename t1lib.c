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
#include <stdlib.h>
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
translate_hex_string(char *s, char *saved_orphan)
{
  int c1 = *saved_orphan;
  char *start = s;
  char *t = s;
  for (; *s; s++) {
    if (isspace(*s))
      continue;
    if (c1) {
      *t++ = (hexval(c1) << 4) + hexval(*s);
      c1 = 0;
    } else
      c1 = *s;
  }
  *saved_orphan = c1;
  return t - start;
}

/* This function returns 1 if the string contains all '0's. */

static int
all_zeroes(char *s)
{
  if (*s == '\0' || *s == '\n')
    return 0;
  while (*s == '0')
    s++;
  return *s == '\0' || *s == '\n';
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
  char saved_orphan = 0;
  
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
    else if (c == '\r' && blocktyp != PFA_BINARY) {
      /* change CR or CR/LF into LF, unless reading binary data! (This
         condition was wrong before, caused Thanh problems - 6.Mar.2001) */
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
	int len = translate_hex_string(line, &saved_orphan);
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


#define DEFAULT_BLOCKLEN (1L<<12)

void
init_pfb_writer(struct pfb_writer *w, int blocklen, FILE *f)
{
  w->len = DEFAULT_BLOCKLEN;
  w->buf = (unsigned char *)malloc(w->len);
  if (!w->buf)
    fatal_error("out of memory");
  w->max_len = (blocklen <= 0 ? 0xFFFFFFFFU : blocklen);
  w->pos = 0;
  w->blocktyp = PFB_ASCII;
  w->binary_blocks_written = 0;
  w->f = f;
}

void
pfb_writer_output_block(struct pfb_writer *w)
{
  /* do nothing if nothing in block */
  if (w->pos == 0)
    return;
  
  /* output four-byte block length */
  putc(PFB_MARKER, w->f);
  putc(w->blocktyp, w->f);
  putc((int)(w->pos & 0xff), w->f);
  putc((int)((w->pos >> 8) & 0xff), w->f);
  putc((int)((w->pos >> 16) & 0xff), w->f);
  putc((int)((w->pos >> 24) & 0xff), w->f);
  
  /* output block data */
  fwrite(w->buf, 1, w->pos, w->f);
  
  /* mark block buffer empty and uninitialized */
  w->pos =  0;
  if (w->blocktyp == PFB_BINARY)
    w->binary_blocks_written++;
}

void
pfb_writer_grow_buf(struct pfb_writer *w)
{
  if (w->len < w->max_len) {
    /* grow w->buf */
    int new_len = w->len * 2;
    unsigned char *new_buf;
    if (new_len > w->max_len)
      new_len = w->max_len;
    new_buf = (unsigned char *)malloc(new_len);
    if (!new_buf) {
      error("out of memory; continuing with a smaller block size");
      w->max_len = w->len;
      pfb_writer_output_block(w);
    } else {
      memcpy(new_buf, w->buf, w->len);
      free(w->buf);
      w->buf = new_buf;
      w->len = new_len;
    }
    
  } else
    /* buf already the right size, just output the block */
    pfb_writer_output_block(w);
}

void
pfb_writer_end(struct pfb_writer *w)
{
  if (w->pos)
    pfb_writer_output_block(w);
  putc(PFB_MARKER, w->f);
  putc(PFB_DONE, w->f);
}



/*
 * CRC computation logic
 * 
 * The logic for this method of calculating the CRC 16 bit polynomial is taken
 * from an article by David Schwaderer in the April 1985 issue of PC Tech
 * Journal.
 */

static short crctab[] =		/* CRC lookup table */
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
crcbuf(int crc, unsigned int len, unsigned char *buf)
{
  register unsigned int i;
  for (i=0; i<len; i++)
    crc = ((crc >> 8) & 0xff) ^ crctab[(crc ^ *buf++) & 0xff];
  return crc & 0xFFFF;
}

/* This CRC table and routine were borrowed from macutils-2.0b3 */

static unsigned short hqx_crctab[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0,
};


int
hqx_crcbuf(int crc, unsigned int len, unsigned char *buf)
{
  while (len--)
    crc = ((crc << 8) & 0xFF00) ^ hqx_crctab[((crc>>8)&0xff) ^ *buf++];
  return crc;
}

#ifdef __cplusplus
}
#endif
