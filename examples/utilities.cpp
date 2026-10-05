// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: examples/utilities.cpp
// Description: Demonstrates random helpers, UUIDs, and singleton inheritance.
// -----------------------------------------------------------------------------

#include <kitzoo/utilities.hpp>

#include <cstdio>
#include <vector>

namespace {
class Settings : public kitzoo::util::Singleton<Settings> {
  friend class kitzoo::util::Singleton<Settings>;

public:
  int retries{3};

private:
  Settings() = default;
};
} // namespace

auto main() -> int {
  // random: thread-local engine, no setup required. NOT cryptographically
  // secure; use a CSPRNG for secrets.
  std::printf("die roll:    %d\n", kitzoo::util::random_int(1, 6));
  std::printf("probability: %.3f\n", kitzoo::util::random_real(0.0, 1.0));
  std::printf("token:       %s\n", kitzoo::util::random_string(12).c_str());

  std::vector<int> cards{1, 2, 3, 4, 5, 6};
  kitzoo::util::shuffle(cards);
  std::printf("shuffled:   ");
  for (int const c : cards)
    std::printf(" %d", c);
  std::printf("\n");
  std::printf("uuid:        %s\n", kitzoo::util::Uuid::random().to_string().c_str());
  std::printf("singleton:   %d retries\n", Settings::instance().retries);

  return 0;
}
