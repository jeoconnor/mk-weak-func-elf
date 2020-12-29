#include <stdio.h>

void f1()
{
  printf("%s:%s\n", __FILE__, __func__);
}

void f2()
{
  printf("%s:%s\n", __FILE__, __func__);
}

