#include "weak-func.h"

void func1();

int main(int argc, char** argv)
{
  printf("%s\n", argv[0]);
  func1();
  return 0;
}

  
