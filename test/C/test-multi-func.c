#include <stdio.h>

void func(void);
void not_mocked_func();
void mock_only_func();

int main(int argc, char** argv)
{
  printf("%s\n", argv[0]);

  func();
  not_mocked_func();
  mock_only_func();
  return 0;
}
