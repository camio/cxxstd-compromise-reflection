#include <iostream>

double pow( double base, int iexp )
{
    (void)base;
    (void)iexp;

    return 1.0;
}
double pow( double base, int iexp ) __attribute__((enable_if(iexp == 2, "chosen when 'c' is out of range")))
{
    (void)base;
    (void)iexp;

    return 2.0;
}

void f(int exp)
{
    std::cout << pow(1.0, exp) << std::endl;;
}

template<bool b=false>
void g()
{
    static_assert(b, "bar");
}

int main()
{
    std::cout << pow(1.0, 1) << std::endl;;
    std::cout << pow(1.0, 2) << std::endl;;
    f(1);
    f(2);
}
