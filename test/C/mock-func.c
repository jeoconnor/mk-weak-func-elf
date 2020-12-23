#include <stdio.h>

void func()
{
  printf("%s:%s\n", __FILE__, __func__);
}

void mock_only_func()
{
  printf("%s:%s\n", __FILE__, __func__);
}

