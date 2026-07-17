#pragma once

#include <glass/component_pool.hpp>
#include <glass/view.hpp>

#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace glass {

class World {
public:
    using DestroyCallback = std::function<void(World&, Entity)>;

    Entity create();
    void destroy(Entity e);
    bool alive(Entity e) const;

    // Entity-level destroy listeners fire before any component is removed,
    // so they can still inspect the dying entity's components.
    void on_entity_destroy(DestroyCallback callback) {
        entity_destroy_listeners_.push_back(std::move(callback));
    }

    // Component-level signals fire on add<T>() and on any removal path
    // (remove<T>() or entity destroy), with the component still intact.
    template<typename T>
    void on_construct(typename ComponentPool<T>::Signal fn) {
        ensure_pool<T>().on_construct(std::move(fn));
    }

    template<typename T>
    void on_destroy(typename ComponentPool<T>::Signal fn) {
        ensure_pool<T>().on_destroy(std::move(fn));
    }

    template<typename T>
    T& add(Entity e, T&& component) {
        return ensure_pool<T>().add(e, std::move(component));
    }

    template<typename T>
    void remove(Entity e) {
        auto* pool = try_pool<T>();
        if (pool) {
            pool->remove(e);
        }
    }

    template<typename T>
    T& get(Entity e) {
        return ensure_pool<T>().get(e);
    }

    template<typename T>
    const T& get(Entity e) const {
        auto* pool = try_pool<T>();
        assert(pool);
        return pool->get(e);
    }

    template<typename T>
    bool has(Entity e) const {
        auto* pool = try_pool<T>();
        return pool && pool->has(e);
    }

    template<typename... Ts>
    View<Ts...> view() {
        return View<Ts...>(*this);
    }

    // Pool access for View iteration
    template<typename T>
    ComponentPool<T>* pool() {
        return try_pool<T>();
    }

private:
    std::vector<uint32_t> generations_;
    std::vector<uint32_t> free_list_;
    uint32_t next_index_{0};

    std::unordered_map<std::type_index, std::unique_ptr<IComponentPool>> pools_;
    std::vector<DestroyCallback> entity_destroy_listeners_;

    template<typename T>
    ComponentPool<T>& ensure_pool() {
        auto key = std::type_index(typeid(T));
        auto it = pools_.find(key);
        if (it == pools_.end()) {
            auto [inserted, _] = pools_.emplace(key, std::make_unique<ComponentPool<T>>());
            return static_cast<ComponentPool<T>&>(*inserted->second);
        }
        return static_cast<ComponentPool<T>&>(*it->second);
    }

    template<typename T>
    ComponentPool<T>* try_pool() {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) {
            return nullptr;
        }
        return static_cast<ComponentPool<T>*>(it->second.get());
    }

    template<typename T>
    const ComponentPool<T>* try_pool() const {
        auto it = pools_.find(std::type_index(typeid(T)));
        if (it == pools_.end()) {
            return nullptr;
        }
        return static_cast<const ComponentPool<T>*>(it->second.get());
    }
};

// View implementation - needs full World definition
template<typename... Ts>
void View<Ts...>::each(auto&& fn) {
    using FirstType = std::tuple_element_t<0, std::tuple<Ts...>>;
    auto* first_pool = world_.template pool<FirstType>();
    if (!first_pool) {
        return;
    }

    // Find the smallest pool among all requested types
    size_t min_size = first_pool->size();
    const std::vector<Entity>* smallest_entities = &first_pool->entities();

    auto check_pool = [&]<typename U>() {
        auto* p = world_.template pool<U>();
        if (!p) {
            min_size = 0;
            return;
        }
        if (p->size() < min_size) {
            min_size = p->size();
            smallest_entities = &p->entities();
        }
    };
    (check_pool.template operator()<Ts>(), ...);

    if (min_size == 0) {
        return;
    }

    // Copy entities to avoid issues if components are modified during iteration
    auto entities_copy = *smallest_entities;
    for (auto e : entities_copy) {
        if ((world_.template has<Ts>(e) && ...)) {
            fn(e, world_.template get<Ts>(e)...);
        }
    }
}

} // namespace glass
