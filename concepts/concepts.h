#pragma once

#include <type_traits>
#include <optional>
#include <variant>
#include <string>
#include <concepts>
#include <ranges>
#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <array>
#include <map>
#include <unordered_map>
#include <string_view>
#include <utility>

template <class P, class T>
concept Predicate = requires(P p, T t) {
    { p(t) } -> std::same_as<bool>;
};

template <class T>
concept Indexable = requires(T s, std::size_t i) {
    {
        s[i]
    }
    -> std::same_as<typename std::enable_if<!std::is_void_v<decltype(s[i])>, decltype(s[i])>::type>;
};

template <typename T>
inline constexpr bool kIsSerializableToJsonV =
    std::integral<T> || std::floating_point<T> || std::same_as<T, bool> ||
    std::convertible_to<T, std::string> || std::is_convertible_v<T, std::string_view>;

template <typename T>
inline constexpr bool kIsSerializableToJsonV<std::optional<T>> = kIsSerializableToJsonV<T>;

template <typename... Types>
inline constexpr bool kIsSerializableToJsonV<std::variant<Types...>> =
    (kIsSerializableToJsonV<Types> && ...);

template <typename T>
struct IsSerializablePairAsObject : std::false_type {};

template <typename K, typename V>
struct IsSerializablePairAsObject<std::pair<K, V>>
    : std::bool_constant<(std::same_as<K, std::string> || std::same_as<K, std::string_view> ||
                          std::convertible_to<K, std::string>) &&
                         kIsSerializableToJsonV<V>> {};

template <typename T>
inline constexpr bool kIsSerializablePairAsObject = IsSerializablePairAsObject<T>::value;

template <typename T>
inline constexpr bool kIsSerializableToJsonV<std::vector<T>> =
    kIsSerializableToJsonV<T> || kIsSerializablePairAsObject<T>;

template <typename T>
inline constexpr bool kIsSerializableToJsonV<std::list<T>> =
    kIsSerializableToJsonV<T> || kIsSerializablePairAsObject<T>;

template <typename T>
inline constexpr bool kIsSerializableToJsonV<std::deque<T>> =
    kIsSerializableToJsonV<T> || kIsSerializablePairAsObject<T>;

template <typename T>
inline constexpr bool kIsSerializableToJsonV<std::forward_list<T>> =
    kIsSerializableToJsonV<T> || kIsSerializablePairAsObject<T>;

template <typename T, size_t N>
inline constexpr bool kIsSerializableToJsonV<std::array<T, N>> =
    kIsSerializableToJsonV<T> || kIsSerializablePairAsObject<T>;

template <typename K, typename V>
inline constexpr bool kIsSerializableToJsonV<std::map<K, V>> =
    (std::same_as<K, std::string> || std::same_as<K, std::string_view> ||
     std::convertible_to<K, std::string>) &&
    kIsSerializableToJsonV<V>;

template <typename K, typename V>
inline constexpr bool kIsSerializableToJsonV<std::unordered_map<K, V>> =
    (std::same_as<K, std::string> || std::same_as<K, std::string_view> ||
     std::convertible_to<K, std::string>) &&
    kIsSerializableToJsonV<V>;

template <typename T, size_t N>
inline constexpr bool kIsSerializableToJsonV<T[N]> = kIsSerializableToJsonV<T>;

template <typename T, size_t N, size_t M>
inline constexpr bool kIsSerializableToJsonV<T[N][M]> = kIsSerializableToJsonV<T>;

template <typename T>
concept SerializableToJson = kIsSerializableToJsonV<T>;
