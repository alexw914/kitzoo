// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: src/log/log.cpp
// Description: Implements kitzoo logger construction, configuration, message
//              dispatch, and shutdown behavior.
// -----------------------------------------------------------------------------

#include <kitzoo/log/async_logger.hpp>
#include <kitzoo/log/logger.hpp>
#include <kitzoo/log/sinks.hpp>

#include <cstdio>
#include <functional>
#include <memory>
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace kitzoo::log {

namespace {
thread_local AsyncLogger* active_worker = nullptr;
thread_local Logger* active_error_handler = nullptr;

// Exposes spdlog's sink dispatch, which reports per-sink exceptions to the error
// handler, for records that keep their original timestamp and thread.
class NativeLogger final : public spdlog::logger {
public:
  using spdlog::logger::logger;

  auto write(const spdlog::details::log_msg& message) -> void { sink_it_(message); }
};

// %^ and %$ color the whole line on consoles; file sinks ignore the markers.
constexpr auto kDefaultPattern = "%^[%Y-%m-%d %H:%M:%S.%e][%t][%L][%s:%#,%*] %v%$";

// Start of the scope that ends at end: just past the nearest "::" outside brackets.
auto scope_begin(std::string_view signature, std::size_t end) -> std::size_t {
  int depth = 0;
  for (auto i = end; i > 0; --i) {
    const char c = signature[i - 1];
    if (c == ')' || c == '>' || c == ']' || c == '}')
      ++depth;
    else if (c == '(' || c == '<' || c == '[' || c == '{')
      --depth;
    else if (depth == 0 && c == ':' && i >= 2 && signature[i - 2] == ':')
      return i;
  }
  return 0;
}

// Name of one scope, without return type or parameters: "int run(int) const" -> "run".
auto scope_name(std::string_view scope) -> std::string_view {
  // The parameter list closes at the last ')' outside a trailing "[with ...]".
  std::size_t close = std::string_view::npos;
  for (int brackets = 0, i = static_cast<int>(scope.size()) - 1; i >= 0; --i) {
    const char c = scope[static_cast<std::size_t>(i)];
    if (c == ']')
      ++brackets;
    else if (c == '[')
      --brackets;
    else if (c == ')' && brackets == 0) {
      close = static_cast<std::size_t>(i);
      break;
    }
  }
  auto end = scope.size();
  if (close != std::string_view::npos) {
    int depth = 0;
    end = std::string_view::npos;
    for (auto i = close + 1; i-- > 0;) {
      if (scope[i] == ')') {
        ++depth;
      } else if (scope[i] == '(' && --depth == 0) {
        end = i;
        break;
      }
    }
    if (end == std::string_view::npos)
      return {};
  }
  // Balanced () and <> belong to the name, as in operator() or foo<int>.
  auto begin = end;
  for (int depth = 0; begin > 0; --begin) {
    const char c = scope[begin - 1];
    if (c == ')' || c == '>') {
      ++depth;
    } else if (c == '(' || c == '<') {
      if (depth == 0)
        break;
      --depth;
    } else if (depth == 0 && c == ' ') {
      break;
    }
  }
  return scope.substr(begin, end - begin);
}

auto is_lambda_class(std::string_view scope) -> bool {
  return scope.ends_with("(anonymous class)") || scope.find("(lambda at ") != std::string_view::npos ||
         scope.find("<lambda") != std::string_view::npos;
}

auto is_call_operator(std::string_view scope) -> bool {
  return scope.find("operator()") != std::string_view::npos || scope.find("operator ()") != std::string_view::npos;
}

// Reduces a source_location signature to the caller's unqualified name, such as
// "int app::Channel::run(int) const" -> "run". A lambda reports its nearest
// enclosing function, or "lambda" when it has none; signatures that cannot be
// parsed are kept whole.
auto caller_name(std::string_view signature) -> std::string_view {
  constexpr std::string_view kLambda = "lambda";
  auto end = signature.size();
  auto begin = scope_begin(signature, end);
  bool in_lambda = false;
  // MSVC writes enclosing functions without parameter lists, as in outer::<lambda_1>.
  bool msvc_lambda = false;
  for (;;) {
    const auto scope = signature.substr(begin, end - begin);
    const auto outer_end = begin >= 2 ? begin - 2 : std::string_view::npos;
    const auto outer =
        outer_end == std::string_view::npos
            ? std::string_view{}
            : signature.substr(scope_begin(signature, outer_end), outer_end - scope_begin(signature, outer_end));
    const bool lambda_body = is_call_operator(scope) && is_lambda_class(outer);
    if (lambda_body || is_lambda_class(scope)) {
      in_lambda = true;
      msvc_lambda = msvc_lambda || scope.find("<lambda_") != std::string_view::npos ||
                    outer.find("<lambda_") != std::string_view::npos;
      if (outer_end == std::string_view::npos)
        return kLambda;
      end = outer_end;
      begin = scope_begin(signature, end);
      continue;
    }
    const auto name = scope_name(scope);
    if (!in_lambda)
      return name.empty() ? signature : name;
    const bool function = msvc_lambda || scope.find('(') != std::string_view::npos;
    if (!function || name.empty() || name.find("anonymous") != std::string_view::npos)
      return kLambda;
    return name;
  }
}

// %* prints the calling function's unqualified name.
class FunctionNameFlag final : public spdlog::custom_flag_formatter {
public:
  auto format(const spdlog::details::log_msg& message, const std::tm&, spdlog::memory_buf_t& dest) -> void override {
    if (message.source.funcname == nullptr)
      return;
    const auto name = caller_name(message.source.funcname);
    dest.append(name.data(), name.data() + name.size());
  }

  auto clone() const -> std::unique_ptr<spdlog::custom_flag_formatter> override {
    return std::make_unique<FunctionNameFlag>();
  }
};

auto make_formatter(const std::string& pattern) -> std::unique_ptr<spdlog::formatter> {
  auto formatter = std::make_unique<spdlog::pattern_formatter>();
  formatter->add_flag<FunctionNameFlag>('*').set_pattern(pattern);
  return formatter;
}
} // namespace

auto Logger::to_spdlog(Level level) noexcept -> spdlog::level::level_enum {
  static_assert(static_cast<int>(Level::Trace) == static_cast<int>(spdlog::level::trace));
  static_assert(static_cast<int>(Level::Debug) == static_cast<int>(spdlog::level::debug));
  static_assert(static_cast<int>(Level::Info) == static_cast<int>(spdlog::level::info));
  static_assert(static_cast<int>(Level::Warn) == static_cast<int>(spdlog::level::warn));
  static_assert(static_cast<int>(Level::Error) == static_cast<int>(spdlog::level::err));
  static_assert(static_cast<int>(Level::Fatal) == static_cast<int>(spdlog::level::critical));
  return static_cast<spdlog::level::level_enum>(static_cast<int>(level));
}

Logger::Logger(std::string name, const Level level)
    : name_{name.data(), name.size()}, native_{memory::make_shared<NativeLogger>(std::move(name))},
      pattern_{kDefaultPattern} {
  native_->set_level(to_spdlog(level));
  native_->set_formatter(make_formatter(pattern_));
  native_->set_error_handler([this](const std::string& message) { report_error(message); });
}

Logger::Logger(std::string name, const LoggerOptions& options) : Logger(std::move(name), options.level) {
  if (!options.pattern.empty())
    set_pattern(options.pattern);
  for (const auto& sink : options.sinks)
    add_sink(sink);
  if (options.file)
    add_file_sink(*options.file);
  set_flush_level(options.flush_level);
  set_error_handler(options.error_handler);
}

auto Logger::set_flush_level(Level level) -> void {
  native_->flush_on(to_spdlog(level));
}

auto Logger::set_error_handler(ErrorHandler handler) -> void {
  std::lock_guard lock(error_mutex_);
  error_handler_ = std::move(handler);
}

auto Logger::report_error(std::string_view message) noexcept -> void {
  failed_.fetch_add(1, std::memory_order_relaxed);
  if (active_error_handler == this)
    return;
  auto* previous = active_error_handler;
  active_error_handler = this;
  ErrorHandler handler;
  {
    std::lock_guard lock(error_mutex_);
    handler = error_handler_;
  }
  if (handler)
    handler(message);
  else
    std::fprintf(stderr, "Logger: %.*s\n", static_cast<int>(message.size()), message.data());
  active_error_handler = previous;
}

auto Logger::add_sink(SinkPtr sink) -> void {
  if (!sink)
    throw std::invalid_argument("sink must not be null");
  sink->set_formatter(make_formatter(pattern_));
  native_->sinks().push_back(std::move(sink));
}

auto Logger::add_file_sink(const FileSinkOptions& options) -> memory::SharedPtr<RollingFileSink> {
  auto sink = memory::make_shared<RollingFileSink>(options);
  add_sink(sink);
  return sink;
}

auto Logger::set_pattern(std::string pattern) -> void {
  pattern_ = std::move(pattern);
  native_->set_formatter(make_formatter(pattern_));
}

auto Logger::set_level(const Level level) noexcept -> void {
  native_->set_level(to_spdlog(level));
}

auto Logger::write_record(const LogRecord& record) -> void {
  const auto loc = spdlog::source_loc{record.location.file_name(), static_cast<int>(record.location.line()),
                                      record.location.function_name()};
  const auto message = spdlog::string_view_t{record.message.data(), record.message.size()};
  spdlog::details::log_msg msg{record.timestamp, loc, record.logger_name, to_spdlog(record.level), message};
  msg.thread_id = std::hash<std::thread::id>{}(record.thread_id);
  static_cast<NativeLogger&>(*native_).write(msg);
}

auto Logger::log(const Level level, const std::string_view message, const std::source_location& loc) -> void {
  native_->log({loc.file_name(), static_cast<int>(loc.line()), loc.function_name()}, to_spdlog(level),
               {message.data(), message.size()});
}

auto Logger::log_at(std::chrono::system_clock::time_point timestamp, Level level, std::string_view message,
                    const std::source_location& loc) -> void {
  native_->log(timestamp, {loc.file_name(), static_cast<int>(loc.line()), loc.function_name()}, to_spdlog(level),
               {message.data(), message.size()});
}

auto Logger::flush() -> void {
  native_->flush();
}

auto default_logger() -> Logger& {
  static Logger instance{"default", [] {
                           LoggerOptions options;
                           options.sinks.push_back(memory::make_shared<ConsoleSink>());
                           return options;
                         }()};
  return instance;
}

auto init(const InitOptions& options) -> memory::SharedPtr<RollingFileSink> {
  memory::SharedPtr<RollingFileSink> file;
  if (!options.file.path.empty())
    file = memory::make_shared<RollingFileSink>(options.file);
  auto& logger = default_logger();
  logger.native_->sinks().clear();
  logger.set_pattern(options.pattern.empty() ? std::string{kDefaultPattern} : options.pattern);
  logger.set_level(options.level);
  logger.set_flush_level(options.flush_level);
  if (options.console)
    logger.add_sink(memory::make_shared<ConsoleSink>());
  if (file)
    logger.add_sink(file);
  return file;
}

AsyncLogger::AsyncLogger(std::shared_ptr<Logger> logger, AsyncLoggerOptions options)
    : logger_{std::move(logger)}, options_{options} {
  if (!logger_)
    throw std::invalid_argument("AsyncLogger requires a logger");
  if (!options_.capacity)
    throw std::invalid_argument("queue capacity must be positive");
  worker_ = std::jthread([this] { worker_loop(); });
}

AsyncLogger::~AsyncLogger() {
  close();
}

auto AsyncLogger::log(Level level, std::string_view message, const std::source_location& loc) -> void {
  if (!logger_->enabled(level))
    return;
  log_owned(level, memory::String(message.data(), message.size()), loc);
}

auto AsyncLogger::log_owned(Level level, memory::String message, const std::source_location& loc) -> void {
  LogRecord record{
      level, std::chrono::system_clock::now(), std::this_thread::get_id(), loc, logger_->name(), std::move(message)};
  std::unique_lock lock(mutex_);
  if (closed_) {
    ++rejected_;
    return;
  }
  if (queue_.size() >= options_.capacity) {
    // A callback on this worker must never wait for its own queue.
    if (options_.overflow_policy == OverflowPolicy::DropNewest || active_worker == this) {
      ++dropped_;
      return;
    }
    space_.wait(lock, [this] { return closed_ || queue_.size() < options_.capacity; });
    if (closed_) {
      ++rejected_;
      return;
    }
  }
  queue_.push_back({std::move(record), {}});
  lock.unlock();
  available_.notify_one();
}

auto AsyncLogger::flush() -> void {
  if (active_worker == this)
    throw std::logic_error("cannot flush from the logger worker");
  auto barrier = memory::make_shared<std::promise<void>>();
  auto complete = barrier->get_future();
  std::unique_lock lock(mutex_);
  space_.wait(lock, [this] { return closed_ || queue_.size() < options_.capacity; });
  if (closed_) {
    lock.unlock();
    close();
    return;
  }
  queue_.push_back({{}, std::move(barrier)});
  lock.unlock();
  available_.notify_one();
  complete.get();
}

auto AsyncLogger::worker_loop() -> void {
  active_worker = this;
  for (;;) {
    {
      std::unique_lock lock(mutex_);
      available_.wait(lock, [this] { return closed_ || !queue_.empty(); });
      if (queue_.empty())
        break;
      queue_.swap(batch_);
    }
    space_.notify_all();
    for (auto& work : batch_) {
      if (work.barrier) {
        logger_->flush();
        work.barrier->set_value();
      } else {
        logger_->write_record(work.record);
      }
    }
    batch_.clear();
  }
  active_worker = nullptr;
}

auto AsyncLogger::close() -> void {
  if (active_worker == this)
    throw std::logic_error("cannot close from the logger worker");
  std::lock_guard closing(close_mutex_);
  {
    std::lock_guard lock(mutex_);
    closed_ = true;
  }
  available_.notify_all();
  space_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
    logger_->flush();
  }
}

} // namespace kitzoo::log
