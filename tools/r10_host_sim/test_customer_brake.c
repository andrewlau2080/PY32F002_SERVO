#include <stdint.h>
#include <limits.h>
#include <stdio.h>

#include "lcdm_customer_brake.h"

static unsigned failures;

static void check(const char *name, uint16_t actual, uint16_t expected)
{
  if (actual == expected)
  {
    printf("PASS: %s (%u)\n", name, actual);
  }
  else
  {
    printf("FAIL: %s (actual=%u expected=%u)\n", name, actual, expected);
    failures++;
  }
}

int main(void)
{
  check("zero speed coasts", LcdmCustomerBrake_ScaleDuty(360U, 0, 1, 1000U, 100U), 0U);
  check("stop threshold coasts", LcdmCustomerBrake_ScaleDuty(360U, 100, 1, 1000U, 100U), 0U);
  check("mid speed scales duty", LcdmCustomerBrake_ScaleDuty(360U, 550, 1, 1000U, 100U), 180U);
  check("maximum speed keeps bounded request", LcdmCustomerBrake_ScaleDuty(360U, 1000, 1, 1000U, 100U), 360U);
  check("overspeed does not exceed base", LcdmCustomerBrake_ScaleDuty(360U, 1500, 1, 1000U, 100U), 360U);
  check("already reversing coasts", LcdmCustomerBrake_ScaleDuty(360U, -550, 1, 1000U, 100U), 0U);
  check("negative original direction scales", LcdmCustomerBrake_ScaleDuty(220U, -550, -1, 1000U, 100U), 110U);
  check("invalid velocity range coasts", LcdmCustomerBrake_ScaleDuty(220U, 500, 1, 100U, 100U), 0U);
  check("invalid direction coasts", LcdmCustomerBrake_ScaleDuty(220U, 500, 0, 1000U, 100U), 0U);
  check("maximum positive velocity caps safely",
        LcdmCustomerBrake_ScaleDuty(220U, INT32_MAX, 1, 1000U, 100U), 220U);
  check("minimum negative velocity caps safely",
        LcdmCustomerBrake_ScaleDuty(220U, INT32_MIN, -1, 1000U, 100U), 220U);
  check("zero base stays zero", LcdmCustomerBrake_ScaleDuty(0U, 800, 1, 1000U, 100U), 0U);
  return failures ? 1 : 0;
}
