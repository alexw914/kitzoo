// ---------------------------------------------------------------------------
// kitzoo example: random & system utilities
//
// Demonstrates:
//   - util: random helpers, UUIDs, and singletons
//   - sys: OS introspection — hostname, cpu_count, pid, environment
// ---------------------------------------------------------------------------

#include <kitzoo/system/system.hpp>
#include <kitzoo/utilities.hpp>

#include <cstdio>
#include <vector>

namespace {
struct Settings {
    int retries{3};
};
}  // namespace

int main() {
    // random: thread-local engine, no setup required. NOT cryptographically
    // secure — use a CSPRNG (e.g. kitzoo::crypto) for secrets.
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
    std::printf("singleton:   %d retries\n", kitzoo::util::Singleton<Settings>::instance().retries);

    // sys: thin wrappers over OS facilities, no global state.
    std::printf("host: %s, user: %s\n", kitzoo::sys::hostname().c_str(),
                kitzoo::sys::username().c_str());
    std::printf("cpus: %u, page size: %zu, pid: %ld\n", kitzoo::sys::cpu_count(),
                kitzoo::sys::page_size(), kitzoo::sys::current_pid());
    if (auto const home = kitzoo::sys::get_env("HOME"); home.has_value()) {
        std::printf("HOME=%s\n", home->c_str());
    }
    return 0;
}
