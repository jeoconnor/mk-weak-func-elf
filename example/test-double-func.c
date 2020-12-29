#include <stdio.h>

void func() __attribute__((section("test-double")));

void func()
{
  printf("test-double:func\n");
}
