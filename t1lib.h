#ifndef T1LIB_H
#define T1LIB_H
#ifdef __cplusplus
extern "C" {
#endif

#define MARKER   128
#define ASCII    1
#define BINARY   2
#define DONE     3

struct font_reader {
  void (*output_ascii)(char *);
  void (*output_binary)(char *, int);
  void (*output_end)();
};

void process_pfa(FILE *, const char *filename, struct font_reader *);
void process_pfb(FILE *, const char *filename, struct font_reader *);

/* whoever uses this code must provide a definition for this function */
extern void error(const char *, ...);

#ifdef __cplusplus
}
#endif
#endif
