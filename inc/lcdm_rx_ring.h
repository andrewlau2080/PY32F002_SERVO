#ifndef LCDM_RX_RING_H
#define LCDM_RX_RING_H

#include <stdbool.h>
#include <stdint.h>

#define LCDM_RX_RING_SIZE 32U
#define LCDM_RX_RING_MASK (LCDM_RX_RING_SIZE - 1U)

static inline bool LcdmRxRing_Push(volatile uint8_t *data,
                                   volatile uint8_t *head,
                                   volatile uint8_t *tail,
                                   volatile uint16_t *overflow_count,
                                   uint8_t byte)
{
  uint8_t next = (uint8_t)((*head + 1U) & LCDM_RX_RING_MASK);
  if (next == *tail)
  {
    (*overflow_count)++;
    return false;
  }
  data[*head] = byte;
  *head = next;
  return true;
}

static inline bool LcdmRxRing_Pop(volatile uint8_t *data,
                                  volatile uint8_t *head,
                                  volatile uint8_t *tail,
                                  uint8_t *byte)
{
  if (*tail == *head) return false;
  *byte = data[*tail];
  *tail = (uint8_t)((*tail + 1U) & LCDM_RX_RING_MASK);
  return true;
}

#endif
