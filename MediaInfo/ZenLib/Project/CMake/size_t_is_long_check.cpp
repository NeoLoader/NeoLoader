#include <cstdlib>
void foo(signed int) {}
void foo(unsigned int) {}

int main ()
{
    foo(size_t(0));
    return 0;
}
