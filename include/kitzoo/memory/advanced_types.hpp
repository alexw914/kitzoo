// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/memory/advanced_types.hpp
// Description: Provides object lifetime helpers, smart pointers and container aliases.
// -----------------------------------------------------------------------------

#ifndef KITZOO_MEMORY_ADVANCED_TYPES_HPP
#define KITZOO_MEMORY_ADVANCED_TYPES_HPP

#include <kitzoo/memory/mi_allocator.hpp>

#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <memory_resource>
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
using Map = std::map<Key, T, Compare, MiAllocator<Pair<Key const, T>>>;

template <typename Key, typename T, typename Compare = std::less<Key>>
using MultiMap = std::multimap<Key, T, Compare, MiAllocator<Pair<Key const, T>>>;

template <typename Key, typename T, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
using UnorderedMap = std::unordered_map<Key, T, Hash, Equal, MiAllocator<Pair<Key const, T>>>;

template <typename Key, typename T, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>>
using UnorderedMultiMap = std::unordered_multimap<Key, T, Hash, Equal, MiAllocator<Pair<Key const, T>>>;

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

template <typename T, typename... Args>
auto make_shared(Args&&... args) -> SharedPtr<T> {
  return std::allocate_shared<T>(MiAllocator<T>{}, std::forward<Args>(args)...);
}

template <typename T, typename... Args>
auto make_shared_with_resource(std::pmr::memory_resource* resource, Args&&... args) -> SharedPtr<T> {
  return std::allocate_shared<T>(std::pmr::polymorphic_allocator<T>{resource}, std::forward<Args>(args)...);
}

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

template <typename T>
using UniquePtr = std::unique_ptr<T, MiDeleter<T>>;

template <typename T, typename... Args>
  requires(!std::is_array_v<T>)
auto make_unique(Args&&... args) -> UniquePtr<T> {
  MiAllocator<T> allocator;
  auto* address = allocator.allocate(1);
  try {
    std::construct_at(address, std::forward<Args>(args)...);
  } catch (...) {
    allocator.deallocate(address, 1);
    throw;
  }
  return UniquePtr<T>{address};
}

template <typename A, typename B>
auto make_pair(A&& first, B&& second) -> Pair<std::decay_t<A>, std::decay_t<B>> {
  return std::make_pair(std::forward<A>(first), std::forward<B>(second));
}

} // namespace kitzoo::memory

#endif // KITZOO_MEMORY_ADVANCED_TYPES_HPP
