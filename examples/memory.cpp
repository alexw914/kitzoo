#include <kitzoo/thread/blocking_object_pool.hpp>
#include <kitzoo/thread/concurrent_object_pool.hpp>
#include <kitzoo/thread/object_pool.hpp>

#include <cstdio>
#include <memory>
#include <string>
#include <thread>

int main() {
    kitzoo::thread::ObjectPool<std::string> local_pool;
    {
        auto value = local_pool.acquire("reused local resource");
        std::printf("ObjectPool: %s\n", value->c_str());
    }

    kitzoo::thread::BlockingObjectPool<std::string> blocking_pool;
    blocking_pool.add(std::make_unique<std::string>("reused blocking resource"));
    {
        auto value = blocking_pool.acquire();
        std::printf("BlockingObjectPool: %s\n", value->c_str());
    }

    kitzoo::thread::ConcurrentObjectPool<std::string> shared_pool;
    shared_pool.add(std::make_unique<std::string>("reused concurrent resource"));
    std::thread worker{[&] {
        auto value = shared_pool.acquire();
        std::printf("ConcurrentObjectPool: %s\n", value->c_str());
    }};
    worker.join();
}
