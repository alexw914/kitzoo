// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/memory.hpp
// Description: Provides container aliases, ownership helpers, and resource-selectable allocation.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_MEMORY_HPP
#define KITZOO_MEMORY_MEMORY_HPP

#include <kitzoo/memory/miallocator.hpp>

#include <deque>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <memory_resource>
#include <new>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace kitzoo::memory {

template <typename A, typename B>
using Pair = std::pair<A, B>;

using String = std::basic_string<char, std::char_traits<char>, MiAllocator<char>>;

template <typename T>
using Vector = std::vector<T, MiAllocator<T>>;

template <typename T>
using List = std::list<T, MiAllocator<T>>;

template <typename T>
using Deque = std::deque<T, MiAllocator<T>>;

template <typename T>
using Queue = std::queue<T, Deque<T>>;

template <typename T, typename Container = Vector<T>, typename Compare = std::less<T>>
using PriorityQueue = std::priority_queue<T, Container, Compare>;

template <typename T>
using Stack = std::stack<T, Deque<T>>;

template <typename Key, typename T, typename Compare = std::less<Key>>
using Map = std::map<Key, T, Compare, MiAllocator<Pair<const Key, T>>>;

template <typename Key, typename T, typename Compare = std::less<Key>>
using MultiMap = std::multimap<Key, T, Compare, MiAllocator<Pair<const Key, T>>>;

template <typename Key, typename T, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
using UnorderedMap = std::unordered_map<Key, T, Hash, Equal, MiAllocator<Pair<const Key, T>>>;

template <typename Key, typename T, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
using UnorderedMultiMap = std::unordered_multimap<Key, T, Hash, Equal, MiAllocator<Pair<const Key, T>>>;

template <typename Key, typename Compare = std::less<Key>>
using Set = std::set<Key, Compare, MiAllocator<Key>>;

template <typename Key, typename Compare = std::less<Key>>
using MultiSet = std::multiset<Key, Compare, MiAllocator<Key>>;

template <typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
using UnorderedSet = std::unordered_set<Key, Hash, Equal, MiAllocator<Key>>;

template <typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
using UnorderedMultiSet = std::unordered_multiset<Key, Hash, Equal, MiAllocator<Key>>;

template <typename T>
using WeakPtr = std::weak_ptr<T>;

template <typename T>
using SharedPtr = std::shared_ptr<T>;

template <typename T>
struct MiDeleter {
  static_assert(!std::is_array_v<T>);

  auto operator()(T* address) const noexcept -> void {
    if (!address)
      return;
    std::destroy_at(address);
    MiAllocator<T>{}.deallocate(address, 1);
  }
};

template <typename T, typename Deleter = MiDeleter<T>>
using UniquePtr = std::unique_ptr<T, Deleter>;

template <typename T, typename Deleter, typename... Args>
  requires(!std::is_array_v<T> && std::is_nothrow_move_constructible_v<Deleter>)
auto make_unique_with_deleter(Deleter deleter, Args&&... args) -> UniquePtr<T, Deleter> {
  // The deleter must reclaim mimalloc storage or transfer its ownership.
  MiAllocator<T> allocator;
  auto* address = allocator.allocate(1);
  try {
    std::construct_at(address, std::forward<Args>(args)...);
  } catch (...) {
    allocator.deallocate(address, 1);
    throw;
  }
  return UniquePtr<T, Deleter>{address, std::move(deleter)};
}

template <typename T, typename Deleter = MiDeleter<T>, typename... Args>
  requires(!std::is_array_v<T> && std::is_nothrow_move_constructible_v<Deleter>)
auto make_unique(Args&&... args) -> UniquePtr<T, Deleter> {
  return make_unique_with_deleter<T>(Deleter{}, std::forward<Args>(args)...);
}

template <typename T, typename Deleter, typename... Args>
  requires(!std::is_array_v<T> && std::is_nothrow_move_constructible_v<Deleter>)
auto make_shared_with_deleter(Deleter deleter, Args&&... args) -> SharedPtr<T> {
  if constexpr (std::is_same_v<Deleter, MiDeleter<T>>) {
    return std::allocate_shared<T>(MiAllocator<T>{}, std::forward<Args>(args)...);
  } else {
    // Custom deleters require separate object and control-block allocations.
    MiAllocator<T> allocator;
    auto* address = allocator.allocate(1);
    try {
      std::construct_at(address, std::forward<Args>(args)...);
    } catch (...) {
      allocator.deallocate(address, 1);
      throw;
    }
    // The shared constructor invokes the deleter if control-block allocation fails.
    return SharedPtr<T>{address, std::move(deleter), MiAllocator<T>{}};
  }
}

template <typename T, typename Deleter = MiDeleter<T>, typename... Args>
auto make_shared(Args&&... args) -> SharedPtr<T> {
  if constexpr (std::is_array_v<T> && std::is_same_v<Deleter, MiDeleter<T>>) {
    return std::allocate_shared<T>(MiAllocator<T>{}, std::forward<Args>(args)...);
  } else {
    return make_shared_with_deleter<T>(Deleter{}, std::forward<Args>(args)...);
  }
}

template <typename T, typename... Args>
auto make_shared_with_resource(std::pmr::memory_resource* resource, Args&&... args) -> SharedPtr<T> {
  return std::allocate_shared<T>(std::pmr::polymorphic_allocator<T>{resource}, std::forward<Args>(args)...);
}

template <typename A, typename B>
auto make_pair(A&& first, B&& second) -> Pair<std::decay_t<A>, std::decay_t<B>> {
  return std::make_pair(std::forward<A>(first), std::forward<B>(second));
}

class Memory {
public:
  explicit Memory(std::pmr::memory_resource* resource = nullptr) noexcept : resource_(resource) {}

  template <typename T, typename... Args>
  auto alloc_object(Args&&... args) -> SharedPtr<T> {
    if (resource_)
      return memory::make_shared_with_resource<T>(resource_, std::forward<Args>(args)...);
    return memory::make_shared<T>(std::forward<Args>(args)...);
  }

  // Value-initializes elements and destroys each one before freeing storage.
  template <typename T>
  auto alloc_basic_array(std::size_t count) -> SharedPtr<T[]> {
    if (count == 0)
      return {};
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_array_new_length{};
    if (resource_)
      return alloc_array<T>(std::pmr::polymorphic_allocator<T>{resource_}, count);
    return alloc_array<T>(MiAllocator<T>{}, count);
  }

private:
  template <typename T, typename Allocator>
  static auto alloc_array(Allocator allocator, std::size_t count) -> SharedPtr<T[]> {
    using Traits = std::allocator_traits<Allocator>;
    auto* address = allocator.allocate(count);
    std::size_t constructed = 0;
    try {
      for (; constructed < count; ++constructed)
        Traits::construct(allocator, address + constructed);
    } catch (...) {
      std::destroy_n(address, constructed);
      allocator.deallocate(address, count);
      throw;
    }
    // shared_ptr invokes the deleter if allocating its control block fails.
    return SharedPtr<T[]>(
        address,
        [count, allocator](T* pointer) mutable -> void {
          std::destroy_n(pointer, count);
          allocator.deallocate(pointer, count);
        },
        allocator);
  }

  std::pmr::memory_resource* resource_;
};

} // namespace kitzoo::memory

#endif // KITZOO_MEMORY_MEMORY_HPP
