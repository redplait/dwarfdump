#include <stdio.h>
#include <tuple>

template<class T>
void my_swap(T& a, T& b) noexcept
{
    static_assert(std::is_copy_constructible_v<T>,
                  "Swap requires copying");
    static_assert(std::is_nothrow_copy_constructible_v<T> &&
                  std::is_nothrow_copy_assignable_v<T>,
                  "Swap requires nothrow copy/assign");
    auto c = b;
    b = a;
    a = c;
}

int do_something(int reps)
{
  std::tuple<int, float> f1{ reps - 1, 1.0} , f2{ 0, 2.0 };
  for ( int i = 0; i < reps; i++ )
  {
    if ( reps )
      std::get<0>(f1) = ++std::get<0>(f2);
    my_swap(f1, f2);
  }
  return std::get<0>(f1);
}