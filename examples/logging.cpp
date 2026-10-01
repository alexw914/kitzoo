// ---------------------------------------------------------------------------
// kitzoo example: logging macros and a configured asynchronous logger
// ---------------------------------------------------------------------------

#include <kitzoo/log.hpp>

#include <memory>

int main() {
    // Convenience macros use the process-wide synchronous logger.
    KZ_LOG_INFO("application started: pid={}", 42);
    KZ_LOG_WARN("this is a formatted warning: {}", "check configuration");

    // Configure an independent logger and send records through its worker.
    auto logger = std::make_shared<kitzoo::log::Logger>("async-example");
    logger->add_sink(std::make_shared<kitzoo::log::ConsoleSink>());
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    {
        kitzoo::log::AsyncLogger async{logger};
        async.log(kitzoo::log::Level::Info, "background log record");
        async.log(kitzoo::log::Level::Error, "operation failed; code=17");
    }  // destructor closes the queue, drains records, and joins the worker
}
