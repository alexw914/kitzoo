// -----------------------------------------------------------------------------
// kitzoo | C++20 Foundation Library
// File: include/kitzoo/core/result.hpp
// Description: Declares Result, a value-or-error return type modelled on a
//              subset of C++23 std::expected.
// -----------------------------------------------------------------------------

#ifndef KITZOO_CORE_RESULT_HPP
#define KITZOO_CORE_RESULT_HPP

#include <kitzoo/core/macro.hpp>

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace kitzoo::core {

template <typename E>
struct Unexpected {
  E error;
};

template <typename E>
Unexpected(E) -> Unexpected<E>;

namespace detail {
template <typename T>
struct IsUnexpected : std::false_type {};

template <typename E>
struct IsUnexpected<Unexpected<E>> : std::true_type {};
} // namespace detail

// Holds either a value or an error. Accessing the absent side throws
// std::bad_variant_access.
template <typename T, typename E>
class Result {
public:
  template <typename U = T>
    requires std::constructible_from<T, U&&> && (!std::same_as<std::remove_cvref_t<U>, Result>) &&
             (!detail::IsUnexpected<std::remove_cvref_t<U>>::value)
  Result(U&& value) : data_(std::in_place_index<0>, std::forward<U>(value)) {}

  template <typename G>
    requires std::constructible_from<E, G&&>
  Result(Unexpected<G> error) : data_(std::in_place_index<1>, std::move(error.error)) {}

  KZ_NODISCARD auto has_value() const noexcept -> bool { return data_.index() == 0; }

  KZ_NODISCARD explicit operator bool() const noexcept { return has_value(); }

  KZ_NODISCARD auto value() & -> T& { return std::get<0>(data_); }

  KZ_NODISCARD auto value() const& -> const T& { return std::get<0>(data_); }

  KZ_NODISCARD auto value() && -> T&& { return std::get<0>(std::move(data_)); }

  KZ_NODISCARD auto error() & -> E& { return std::get<1>(data_); }

  KZ_NODISCARD auto error() const& -> const E& { return std::get<1>(data_); }

  KZ_NODISCARD auto error() && -> E&& { return std::get<1>(std::move(data_)); }

  template <typename U>
    requires std::convertible_to<U&&, T>
  KZ_NODISCARD auto value_or(U&& fallback) const& -> T {
    if (has_value())
      return std::get<0>(data_);
    return std::forward<U>(fallback);
  }

  KZ_NODISCARD auto operator*() & -> T& { return value(); }

  KZ_NODISCARD auto operator*() const& -> const T& { return value(); }

  KZ_NODISCARD auto operator->() -> T* { return &value(); }

  KZ_NODISCARD auto operator->() const -> const T* { return &value(); }

private:
  std::variant<T, E> data_;
};

template <typename E>
class Result<void, E> {
public:
  Result() = default;

  template <typename G>
    requires std::constructible_from<E, G&&>
  Result(Unexpected<G> error) : error_(std::move(error.error)) {}

  KZ_NODISCARD auto has_value() const noexcept -> bool { return !error_.has_value(); }

  KZ_NODISCARD explicit operator bool() const noexcept { return has_value(); }

  auto value() const -> void {
    if (error_)
      throw std::bad_variant_access{};
  }

  KZ_NODISCARD auto error() const& -> const E& {
    if (!error_)
      throw std::bad_variant_access{};
    return *error_;
  }

private:
  std::optional<E> error_;
};

} // namespace kitzoo::core

#endif // KITZOO_CORE_RESULT_HPP
