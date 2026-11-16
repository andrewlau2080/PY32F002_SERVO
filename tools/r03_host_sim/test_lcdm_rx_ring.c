#include <stdio.h>
#include "lcdm_rx_ring.h"

static volatile uint8_t data[LCDM_RX_RING_SIZE];
static volatile uint8_t head, tail;
static volatile uint16_t overflow_count;
static unsigned failures;

static void check(const char *name, int actual, int expected)
{
  int pass = actual == expected;
  printf("%s: %s (actual=%d expected=%d)\n", pass ? "PASS" : "FAIL", name, actual, expected);
  if (!pass) failures++;
}

int main(void)
{
  uint8_t byte = 0U;
  check("ring capacity is power of two", (LCDM_RX_RING_SIZE & LCDM_RX_RING_MASK) == 0U, 1);
  for (uint8_t i = 0U; i < LCDM_RX_RING_SIZE - 1U; i++)
    check("accepts byte before full", LcdmRxRing_Push(data, &head, &tail, &overflow_count, i), 1);
  check("rejects byte when full", LcdmRxRing_Push(data, &head, &tail, &overflow_count, 0xEEU), 0);
  check("full-ring overflow counted", overflow_count, 1);
  for (uint8_t i = 0U; i < LCDM_RX_RING_SIZE - 1U; i++)
  {
    check("pop preserves FIFO order", LcdmRxRing_Pop(data, &head, &tail, &byte), 1);
    check("popped byte matches", byte, i);
  }
  check("empty ring reports empty", LcdmRxRing_Pop(data, &head, &tail, &byte), 0);
  for (uint8_t i = 0U; i < 27U; i++)
    (void)LcdmRxRing_Push(data, &head, &tail, &overflow_count, (uint8_t)(i + 40U));
  for (uint8_t i = 0U; i < 20U; i++)
    (void)LcdmRxRing_Pop(data, &head, &tail, &byte);
  for (uint8_t i = 0U; i < 20U; i++)
    check("wrapped ring accepts freed slot", LcdmRxRing_Push(data, &head, &tail, &overflow_count,
          (uint8_t)(i + 100U)), 1);
  for (uint8_t i = 20U; i < 27U; i++)
  {
    check("wrapped original bytes remain ordered", LcdmRxRing_Pop(data, &head, &tail, &byte), 1);
    check("wrapped original byte value", byte, (uint8_t)(i + 40U));
  }
  for (uint8_t i = 0U; i < 20U; i++)
  {
    check("wrapped appended bytes pop", LcdmRxRing_Pop(data, &head, &tail, &byte), 1);
    check("wrapped appended byte value", byte, (uint8_t)(i + 100U));
  }
  return failures ? 1 : 0;
}
