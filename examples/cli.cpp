#include <kitzoo/cli.hpp>

#include <cstdio>
#include <exception>
#include <string>

int main(int argc, char const* const* argv) {
    kitzoo::cli::Options options{argv[0], "CLI module example"};
    options.add_options()("n,name", "Name to greet",
                          kitzoo::cli::value<std::string>()->default_value("world"))(
        "v,verbose", "Enable verbose output")("h,help", "Show help");

    try {
        auto const result = options.parse(argc, argv);
        if (result["help"].as<bool>()) {
            std::puts(options.help().c_str());
            return 0;
        }
        std::printf("hello, %s%s\n", result["name"].as<std::string>().c_str(),
                    result["verbose"].as<bool>() ? " (verbose)" : "");
    } catch (std::exception const& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 2;
    }
}
