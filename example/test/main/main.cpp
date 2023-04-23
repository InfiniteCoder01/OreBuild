#include "test.h"
#include <lib.hpp>
#include <stdio.h>

int main() {
#ifdef DEBUG
    puts("Debug");
#endif
#ifdef NRELEASE
    puts("Not release");
#endif
    hello(getSubject());
}
