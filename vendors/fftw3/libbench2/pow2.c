#include "libbench2/bench.h"

int power_of_two(int n)
{
     return (((n) > 0) && (((n) & ((n) - 1)) == 0));
}
