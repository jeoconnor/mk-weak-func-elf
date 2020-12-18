#include <stdio.h>

void func1() __attribute__((weak));

void func1(void)
{
  printf("%s:%d:%s\n", __FILE__, __LINE__, __func__);
}
