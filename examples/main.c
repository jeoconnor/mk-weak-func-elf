#include <stdio.h>

void f1();
void f2();

int main()
{
  printf("%s:%s\n", __FILE__, __func__);
  f1();
  f2();
  return(0);
}
