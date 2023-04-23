#include "test.h"
#include <lib.hpp>

int main() {
#ifdef DEBUG
    hello("Debug");
#endif
    hello(getSubject());
}
