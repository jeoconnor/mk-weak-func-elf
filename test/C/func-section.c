#include <stdio.h>

#ifdef CUSTOM_SECTION
#define stringify(x) _str(x)
#define _str(x)      #x
#define SECTION_ATTRIBUTE __attribute__((section(stringify(CUSTOM_SECTION))))
#else
#define SECTION_ATTRIBUTE 
#endif

void func() SECTION_ATTRIBUTE;

void func()
{
  printf("%s:%s\n", __FILE__, __func__);
}

void mock_only_func()
{
  printf("%s:%s\n", __FILE__, __func__);
}

