/**
 * @file types.hpp
 * @brief Common type aliases for the Photon programming language compiler
 * @author Photon Compiler Team
 * @version 1.0.0
 * 
 * This module provides standard type aliases following modern C++20 practices
 * for consistent usage across the compiler codebase.
 */

#pragma once

#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <variant>
#include <unordered_map>
#include <unordered_set>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace photon {

// Smart pointer aliases
template<typename T>
using Ptr = std::unique_ptr<T>;

template<typename T>
using Ref = std::shared_ptr<T>;

template<typename T>
using WeakRef = std::weak_ptr<T>;

// Container aliases
template<typename T>
using Vec = std::vector<T>;

template<typename T>
using Opt = std::optional<T>;

// Simple Result type implementation (similar to std::expected)
template<typename T, typename E>
class Result {
private:
    std::variant<T, E> data_;
    
public:
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    Result(const E& error) : data_(error) {}
    Result(E&& error) : data_(std::move(error)) {}
    
    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(data_);
    }
    
    [[nodiscard]] explicit operator bool() const noexcept {
        return has_value();
    }
    
    [[nodiscard]] const T& value() const & {
        if (!has_value()) {
            throw std::runtime_error("Result contains error");
        }
        return std::get<T>(data_);
    }
    
    [[nodiscard]] T& value() & {
        if (!has_value()) {
            throw std::runtime_error("Result contains error");
        }
        return std::get<T>(data_);
    }
    
    [[nodiscard]] T&& value() && {
        if (!has_value()) {
            throw std::runtime_error("Result contains error");
        }
        return std::get<T>(std::move(data_));
    }
    
    [[nodiscard]] const E& error() const & {
        if (has_value()) {
            throw std::runtime_error("Result contains value");
        }
        return std::get<E>(data_);
    }
    
    [[nodiscard]] E& error() & {
        if (has_value()) {
            throw std::runtime_error("Result contains value");
        }
        return std::get<E>(data_);
    }
    
    template<typename U>
    [[nodiscard]] T value_or(U&& default_value) const & {
        return has_value() ? value() : static_cast<T>(std::forward<U>(default_value));
    }
};

template<typename K, typename V>
using Map = std::unordered_map<K, V>;

template<typename T>
using Set = std::unordered_set<T>;

// String aliases
using String = std::string;
using StringView = std::string_view;

// Integral types
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using usize = std::size_t;
using isize = std::ptrdiff_t;

// Floating point types
using f32 = float;
using f64 = double;

// Smart pointer factory functions
template<typename T, typename... Args>
[[nodiscard]] auto make_ptr(Args&&... args) -> Ptr<T> {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
[[nodiscard]] auto make_ref(Args&&... args) -> Ref<T> {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

// Result factory functions
template<typename T>
[[nodiscard]] auto Ok(T&& value) -> Result<std::decay_t<T>, std::monostate> {
    return Result<std::decay_t<T>, std::monostate>(std::forward<T>(value));
}

template<typename T, typename E>
[[nodiscard]] auto Ok(T&& value) -> Result<std::decay_t<T>, E> {
    return Result<std::decay_t<T>, E>(std::forward<T>(value));
}

template<typename E>
[[nodiscard]] auto Err(E&& error) -> Result<std::monostate, std::decay_t<E>> {
    return Result<std::monostate, std::decay_t<E>>(std::forward<E>(error));
}

template<typename T, typename E>
[[nodiscard]] auto Err(E&& error) -> Result<T, std::decay_t<E>> {
    return Result<T, std::decay_t<E>>(std::forward<E>(error));
}

} // namespace photon