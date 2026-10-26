#pragma once
// Tiny constructor-injection container (singleton scope).
// A class `Foo(Bar&, Baz&)` is autowired: Bar and Baz are created on demand.
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace appleboot {

class Container {
    std::unordered_map<std::type_index, std::shared_ptr<void>> beans_;
    std::unordered_set<std::type_index> creating_;

    // Converts to any `U&` by resolving U from the container (except T itself,
    // which avoids hijacking the copy constructor).
    template <class Self>
    struct Any {
        Container& c;
        template <class U>
            requires(!std::is_same_v<std::remove_cvref_t<U>, Self>)
        operator U&() const { return c.get<std::remove_cvref_t<U>>(); }
    };

    template <class T>
    std::shared_ptr<T> construct() {
        Any<T> a{*this};
        if constexpr (std::is_constructible_v<T, Any<T>, Any<T>, Any<T>, Any<T>>) return std::make_shared<T>(a, a, a, a);
        else if constexpr (std::is_constructible_v<T, Any<T>, Any<T>, Any<T>>) return std::make_shared<T>(a, a, a);
        else if constexpr (std::is_constructible_v<T, Any<T>, Any<T>>) return std::make_shared<T>(a, a);
        else if constexpr (std::is_constructible_v<T, Any<T>>) return std::make_shared<T>(a);
        else return std::make_shared<T>();
    }

public:
    template <class T>
    T& get() {
        std::type_index key(typeid(T));
        if (auto it = beans_.find(key); it != beans_.end()) return *static_cast<T*>(it->second.get());
        if (!creating_.insert(key).second)
            throw std::runtime_error(std::string("circular dependency while creating ") + typeid(T).name());
        auto p = construct<T>();
        creating_.erase(key);
        T& ref = *p;
        beans_[key] = std::move(p);
        return ref;
    }
};

}  // namespace appleboot
