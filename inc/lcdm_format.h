#ifndef LCDM_FORMAT_H
#define LCDM_FORMAT_H

#include <stddef.h>

/* LCDM commands use only strings, integer conversions and literal percent. */
int lcdm_snprintf(char *dest, size_t size, const char *format, ...);

#endif
