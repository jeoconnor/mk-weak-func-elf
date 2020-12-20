#include <stdio.h>

#ifdef MOCK_LABEL
#define stringify(x) _str(x)
#define _str(x)      #x
#define MOCK_SECTION __attribute__((section(stringify(MOCK_LABEL))))
#else
#define MOCK_SECTION 
#endif

void func() MOCK_SECTION;

void func()
{
  printf("%s:%s\n", __FILE__, __func__);
}

void mock_only_func()
{
  printf("%s:%s\n", __FILE__, __func__);
}

