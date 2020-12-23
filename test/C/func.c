#include <stdio.h>

void func()
{
  printf("%s:%s\n", __FILE__, __func__);
}

void not_mocked_func()
{
  printf("%s:%s\n", __FILE__, __func__);
}

