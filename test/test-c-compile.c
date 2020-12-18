#include <stdio.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN void test();
EXTERN static void test_local();
EXTERN void test_mock() __attribute__((section(".mock")));
EXTERN void test_weak() __attribute__((weak));
EXTERN void test_mock_weak() __attribute__((section(".mock"), weak));

EXTERN void test()
{
  printf("%s:%d:%s\n", __FILE__, __LINE__, __func__);
}

EXTERN static void test_local()
{
  printf("%s:%d:%s\n", __FILE__, __LINE__, __func__);
}

EXTERN void test_mock()
{
  printf("%s:%d:%s\n", __FILE__, __LINE__, __func__);
}

EXTERN void test_weak()
{
  printf("%s:%d:%s\n", __FILE__, __LINE__, __func__);
}

EXTERN void test_mock_weak()
{
  printf("%s:%d:%s\n", __FILE__, __LINE__, __func__);
}

int main(int argc, char** argv)
{
  printf("%s\n", argv[0]);

  test();
  test_local();
  test_mock();
  test_weak();
  test_mock_weak();
  return 0;
}
