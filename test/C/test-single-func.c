#include <stdio.h>

void func(void);

int main(int argc, char** argv)
{
  printf("%s\n", argv[0]);

  func();
  return 0;
}
