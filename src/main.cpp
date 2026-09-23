#include <iostream>

#include "core/version.hpp"

int main() {
    // Placeholder until Phase 1+ wires PlayerController to the real adapters.
    std::cout << "WinampDeck controller " << winampdeck::version() << '\n';
    return 0;
}
