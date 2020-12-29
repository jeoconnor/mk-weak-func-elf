#include <stdio.h>

void f1() __attribute__((section(".stub")));

void f1()
{
  printf("%s:%s\n", __FILE__, __func__);
}
