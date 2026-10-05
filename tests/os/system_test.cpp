// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: tests/os/system_test.cpp
// Description: Verifies system queries, thread operations, and edge cases.
// -----------------------------------------------------------------------------

#include <kitzoo/os/system.hpp>

#include <cstdlib>
#include <filesystem>
#include <future>
#include <gtest/gtest.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

TEST(SystemPortableTest, CurrentThreadAndTemporaryPath) {
  namespace os = kitzoo::os;
  EXPECT_TRUE(os::set_current_thread_name("worker", "test"));
#if defined(_WIN32)
  EXPECT_EQ(os::get_thread_name(GetCurrentThread()), "test/worker");
#else
  EXPECT_EQ(os::get_thread_name(pthread_self()), "test/worker");
#endif
  auto before = os::get_cpu_timestamp_ns();
  EXPECT_GE(os::get_cpu_timestamp_ns(), before);
  auto path = os::generate_unique_domain_socket_address();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(std::filesystem::exists(path));
  EXPECT_TRUE(os::remove_unique_domain_socket_address(path));
  EXPECT_FALSE(os::remove_unique_domain_socket_address(path));
  std::thread inactive;
  EXPECT_FALSE(os::set_thread_name(inactive, "worker"));
  EXPECT_FALSE(os::set_thread_priority(inactive, 0));
  EXPECT_FALSE(os::bind_cpus(inactive, {0}));
}

TEST(SystemPortableTest, WorkerThreadOperations) {
  namespace os = kitzoo::os;
  std::promise<void> release;
  auto wait = release.get_future();
  std::thread worker([&wait] { wait.wait(); });
#if defined(__APPLE__)
  EXPECT_FALSE(os::set_thread_name(worker, "worker"));
  EXPECT_FALSE(os::bind_cpus(worker, {0}));
#else
  EXPECT_TRUE(os::set_thread_name(worker, "worker", "test"));
  EXPECT_EQ(os::get_thread_name(worker), "test/worker");
  EXPECT_TRUE(os::set_thread_priority(worker, 0));
#endif
  release.set_value();
  worker.join();
  EXPECT_FALSE(os::bind_current_cpus({}));
  EXPECT_FALSE(os::set_current_thread_nice(20));
}

#if defined(_WIN32)
TEST(SystemWindowsTest, PriorityAndAffinity) {
  namespace os = kitzoo::os;
  auto thread = GetCurrentThread();
  auto priority = GetThreadPriority(thread);
  EXPECT_TRUE(os::set_current_thread_priority(THREAD_PRIORITY_NORMAL));
  EXPECT_EQ(GetThreadPriority(thread), THREAD_PRIORITY_NORMAL);
  EXPECT_TRUE(os::set_current_thread_priority(priority));
  EXPECT_FALSE(os::set_current_thread_priority(0, kitzoo::os::SchedPolicy::Fifo));
  EXPECT_FALSE(os::set_current_thread_priority(3));
  DWORD_PTR process_mask = 0, system_mask = 0;
  ASSERT_TRUE(GetProcessAffinityMask(GetCurrentProcess(), &process_mask, &system_mask));
  for (std::uint32_t core = 0; core < sizeof(process_mask) * 8; ++core) {
    if ((process_mask & (DWORD_PTR{1} << core)) != 0) {
      auto previous = SetThreadAffinityMask(thread, process_mask);
      ASSERT_NE(previous, 0U);
      EXPECT_TRUE(os::bind_current_cpus({core}));
      EXPECT_NE(SetThreadAffinityMask(thread, previous), 0U);
      break;
    }
  }
}
#endif

#if defined(__linux__)
#include <cstdlib>
#include <filesystem>
#include <future>
#include <limits>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

TEST(SystemTest, LiveThreadSchedulingAndAffinity) {
  namespace os = kitzoo::os;
  std::promise<void> release;
  auto wait = release.get_future();
  std::thread worker([&wait] { wait.wait(); });
  EXPECT_TRUE(os::set_thread_name(worker, "worker", "test"));
  EXPECT_EQ(os::get_thread_name(worker), "test/worker");
  EXPECT_TRUE(os::set_thread_priority(worker, 0));
  cpu_set_t allowed;
  CPU_ZERO(&allowed);
  auto result = pthread_getaffinity_np(worker.native_handle(), sizeof(allowed), &allowed);
  EXPECT_EQ(result, 0);
  if (result == 0) {
    for (unsigned int core = 0; core < CPU_SETSIZE; ++core) {
      if (CPU_ISSET(core, &allowed)) {
        EXPECT_TRUE(os::bind_cpus(worker, {core, std::numeric_limits<std::uint32_t>::max()}));
        cpu_set_t actual;
        CPU_ZERO(&actual);
        EXPECT_EQ(pthread_getaffinity_np(worker.native_handle(), sizeof(actual), &actual), 0);
        EXPECT_EQ(CPU_COUNT(&actual), 1);
        EXPECT_TRUE(CPU_ISSET(core, &actual));
        break;
      }
    }
  }
  release.set_value();
  worker.join();
}

TEST(SystemTest, CpuCountAndCpuTime) {
  namespace os = kitzoo::os;
  EXPECT_GT(os::cpu_count(), 0U);
  rusage before{}, after{};
  ASSERT_EQ(getrusage(RUSAGE_SELF, &before), 0);
  auto value = os::get_cpu_timestamp_ns();
  ASSERT_EQ(getrusage(RUSAGE_SELF, &after), 0);
  auto ns = [](const rusage& r) {
    return static_cast<std::uint64_t>(r.ru_utime.tv_sec + r.ru_stime.tv_sec) * 1000000000ULL +
           static_cast<std::uint64_t>(r.ru_utime.tv_usec + r.ru_stime.tv_usec) * 1000ULL;
  };
  EXPECT_GE(value, ns(before));
  EXPECT_LE(value, ns(after));
}

TEST(SystemTest, ThreadNameRules) {
  namespace os = kitzoo::os;
  auto old = os::get_thread_name(pthread_self());
  EXPECT_TRUE(os::set_current_thread_name("worker", "test"));
  EXPECT_EQ(os::get_thread_name(pthread_self()), "test/worker");
  EXPECT_TRUE(os::set_current_thread_name("long/path/worker0123456789"));
  EXPECT_EQ(os::get_thread_name(pthread_self()), "kz/worker012345");
  EXPECT_FALSE(os::set_current_thread_name(""));
  EXPECT_FALSE(os::set_current_thread_name("worker", std::string(16, 'x')));
  EXPECT_EQ(pthread_setname_np(pthread_self(), old.c_str()), 0);
}

TEST(SystemTest, RejectInvalidInputsAndInactiveThread) {
  namespace os = kitzoo::os;
  std::thread inactive;
  EXPECT_FALSE(os::set_thread_name(inactive, "worker"));
  EXPECT_TRUE(os::get_thread_name(inactive).empty());
  EXPECT_FALSE(os::set_thread_priority(inactive, 0));
  EXPECT_FALSE(os::bind_cpus(inactive, {0}));
  EXPECT_FALSE(os::bind_current_cpus({}));
  EXPECT_FALSE(os::bind_current_cpus({std::numeric_limits<std::uint32_t>::max()}));
  EXPECT_FALSE(os::set_current_thread_priority(-1, kitzoo::os::SchedPolicy::Fifo));
  EXPECT_FALSE(os::set_current_thread_nice(20));
}

TEST(SystemTest, UniqueSocketPathsAndNoFdLeak) {
  namespace os = kitzoo::os;
  auto fd_count = [] {
    return std::distance(std::filesystem::directory_iterator("/proc/self/fd"), std::filesystem::directory_iterator{});
  };
  auto count = fd_count();
  auto first = os::generate_unique_domain_socket_address();
  auto second = os::generate_unique_domain_socket_address();
  EXPECT_FALSE(first.empty());
  EXPECT_FALSE(second.empty());
  EXPECT_NE(first, second);
  EXPECT_EQ(fd_count(), count);
  EXPECT_TRUE(std::filesystem::exists(first));
  EXPECT_TRUE(os::remove_unique_domain_socket_address(first));
  EXPECT_TRUE(os::remove_unique_domain_socket_address(second));
  EXPECT_FALSE(os::remove_unique_domain_socket_address(first));
}
#endif

TEST(SystemQueryTest, GetEnvExisting) {
  // PATH exists on every supported platform
  const auto path = kitzoo::os::get_env("PATH");
  ASSERT_TRUE(path.has_value());
  EXPECT_FALSE(path->empty());
}

TEST(SystemQueryTest, GetEnvMissing) {
  EXPECT_FALSE(kitzoo::os::get_env("KITZOO_DEFINITELY_NOT_SET_12345").has_value());
}

TEST(SystemQueryTest, Hostname) {
  const auto hn = kitzoo::os::hostname();
  EXPECT_FALSE(hn.empty());
}

TEST(SystemQueryTest, CpuCount) {
  EXPECT_GE(kitzoo::os::cpu_count(), 1u);
}

TEST(SystemQueryTest, Pid) {
  EXPECT_GT(kitzoo::os::current_pid(), 0L);
  EXPECT_EQ(kitzoo::os::current_pid(),
            kitzoo::os::current_pid()); // stable within process
}

TEST(SystemQueryTest, PageSize) {
  const auto ps = kitzoo::os::page_size();
  EXPECT_GT(ps, 0u);
  EXPECT_EQ(ps & (ps - 1), 0u); // power of two
}

TEST(SystemQueryTest, TotalMemory) {
  EXPECT_GT(kitzoo::os::total_memory(), 0u);
}

TEST(SystemQueryTest, Username) {
  // Environment-dependent: containers often lack $USER AND a utmp entry for
  // getlogin_r. If both are unavailable, username() legitimately returns
  // "".
  const auto name = kitzoo::os::username();
  if (name.empty()) {
    GTEST_SKIP() << "no USER env and no login session (container environment)";
  }
  EXPECT_FALSE(name.empty());
}

TEST(SystemQueryTest, HomeDir) {
  if (!kitzoo::os::get_env("HOME").has_value() && !kitzoo::os::get_env("USERPROFILE").has_value()) {
    GTEST_SKIP() << "no HOME/USERPROFILE in this environment";
  }
  EXPECT_FALSE(kitzoo::os::home_dir().empty());
}

TEST(SystemQueryTest, StacktraceCapturesFrames) {
  const auto frames = kitzoo::os::stacktrace();
  // Symbol names are only resolvable when the binary exports them
  // (-rdynamic); without it we still get return addresses. Only assert
  // that frames were captured at all.
  EXPECT_GT(frames.size(), 1u);
  for (const auto& frame : frames)
    EXPECT_FALSE(frame.empty());
}

TEST(SystemQueryTest, StacktraceRespectsFrameLimit) {
  EXPECT_EQ(kitzoo::os::stacktrace(1).size(), 1u);
}

TEST(SystemQueryTest, StacktraceRejectsNonPositiveFrameLimit) {
  EXPECT_TRUE(kitzoo::os::stacktrace(0).empty());
  EXPECT_TRUE(kitzoo::os::stacktrace(-1).empty());
}
