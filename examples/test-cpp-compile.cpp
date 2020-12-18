#include <iostream>

using namespace std;

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN void test();
static void test_local();
EXTERN void test_mock() __attribute__((section("mock")));
EXTERN void test_weak() __attribute__((weak));
EXTERN void test_mock_weak() __attribute__((section("mock"), weak));

EXTERN void test()
{
  cout << __FILE__ << ':' << __LINE__ << ':' << __func__ << '\n';
}

static void test_local()
{
  cout << __FILE__ << ':' << __LINE__ << ':' << __func__ << '\n';
}

EXTERN void test_mock()
{
  cout << __FILE__ << ':' << __LINE__ << ':' << __func__ << '\n';
}

EXTERN void test_weak()
{
  cout << __FILE__ << ':' << __LINE__ << ':' << __func__ << '\n';
}

EXTERN void test_mock_weak()
{
  cout << __FILE__ << ':' << __LINE__ << ':' << __func__ << '\n';
}

int main(int argc, char** argv)
{
  cout << argv[0] << endl;
  test();
  test_local();
  test_mock();
  test_weak();
  test_mock_weak();
  return 0;
}
