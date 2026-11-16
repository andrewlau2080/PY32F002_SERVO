#include "lcdm_format.h"
#include <stdarg.h>
#include <stdbool.h>

typedef struct
{
  char *dest;
  size_t size;
  size_t count;
} LcdmFormatOutput;

static void put_char(LcdmFormatOutput *output, char value)
{
  if ((output->size != 0U) && (output->count < output->size - 1U))
  {
    output->dest[output->count] = value;
  }
  output->count++;
}

static void put_unsigned(LcdmFormatOutput *output, unsigned long value)
{
  char digits[3U * sizeof(unsigned long)];
  size_t count = 0U;
  do
  {
    digits[count++] = (char)('0' + value % 10UL);
    value /= 10UL;
  } while (value != 0UL);
  while (count != 0U)
  {
    put_char(output, digits[--count]);
  }
}

int lcdm_snprintf(char *dest, size_t size, const char *format, ...)
{
  LcdmFormatOutput output = {dest, size, 0U};
  va_list args;
  va_start(args, format);
  while (*format != '\0')
  {
    if (*format != '%')
    {
      put_char(&output, *format++);
      continue;
    }
    format++;
    bool is_long = (*format == 'l');
    if (is_long)
    {
      format++;
    }
    char conversion = *format;
    if (conversion == '\0')
    {
      break;
    }
    format++;
    if (conversion == 's')
    {
      const char *text = va_arg(args, const char *);
      if (text == NULL)
      {
        text = "(null)";
      }
      while (*text != '\0')
      {
        put_char(&output, *text++);
      }
    }
    else if (conversion == 'u')
    {
      unsigned long value = is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
      put_unsigned(&output, value);
    }
    else if ((conversion == 'd') || (conversion == 'i'))
    {
      long value = is_long ? va_arg(args, long) : va_arg(args, int);
      unsigned long magnitude = (unsigned long)value;
      if (value < 0)
      {
        put_char(&output, '-');
        magnitude = 0UL - magnitude;
      }
      put_unsigned(&output, magnitude);
    }
    else
    {
      put_char(&output, conversion);
    }
  }
  va_end(args);
  if (size != 0U)
  {
    size_t end = (output.count < size) ? output.count : size - 1U;
    dest[end] = '\0';
  }
  return (int)output.count;
}
