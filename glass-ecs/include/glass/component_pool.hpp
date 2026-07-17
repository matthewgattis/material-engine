#pragma once

#include <glass/entity.hpp>

#include <cassert>
#include <functional>
#include <optional>
#include <vector>

namespace glass {

class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void remove(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
};

template<typename T>
class ComponentPool : public IComponentPool {
public:
    using Signal = std::function<void(Entity, T&)>;

    T& add(Entity e, T&& component) {
        ensure_sparse(e.index);
        dense_.push_back(std::move(component));
        dense_entities_.push_back(e);
        versions_.push_back(++version_counter_);
        sparse_[e.index] = dense_.size() - 1;
        for (auto& fn : on_construct_) {
            fn(e, dense_[sparse_[e.index].value()]);
        }
        return dense_[sparse_[e.index].value()];
    }

    void remove(Entity e) override {
        if (!has(e)) {
            return;
        }
        // Fire before removal so listeners can salvage the component
        // (e.g. hand GPU resources to a deferred-destroy queue).
        for (auto& fn : on_destroy_) {
            fn(e, dense_[sparse_[e.index].value()]);
        }
        auto dense_index = sparse_[e.index].value();
        auto last_index = dense_.size() - 1;

        if (dense_index != last_index) {
            dense_[dense_index] = std::move(dense_[last_index]);
            dense_entities_[dense_index] = dense_entities_[last_index];
            versions_[dense_index] = versions_[last_index];
            sparse_[dense_entities_[dense_index].index] = dense_index;
        }

        dense_.pop_back();
        dense_entities_.pop_back();
        versions_.pop_back();
        sparse_[e.index].reset();
    }

    T& get(Entity e) {
        assert(has(e));
        return dense_[sparse_[e.index].value()];
    }

    const T& get(Entity e) const {
        assert(has(e));
        return dense_[sparse_[e.index].value()];
    }

    bool has(Entity e) const override {
        if (e.index >= sparse_.size()) {
            return false;
        }
        if (!sparse_[e.index].has_value()) {
            return false;
        }
        auto dense_index = sparse_[e.index].value();
        return dense_entities_[dense_index] == e;
    }

    size_t size() const { return dense_.size(); }

    const std::vector<Entity>& entities() const { return dense_entities_; }
    std::vector<T>& components() { return dense_; }
    const std::vector<T>& components() const { return dense_; }

    void on_construct(Signal fn) { on_construct_.push_back(std::move(fn)); }
    void on_destroy(Signal fn) { on_destroy_.push_back(std::move(fn)); }

    // Monotonic per-pool write versions. add() stamps a fresh version and
    // bump() advances it; get() does not, so tracked writes must go through
    // World::patch<T>(). Consumers (e.g. replication) compare against the
    // version they last observed.
    uint64_t version(Entity e) const {
        assert(has(e));
        return versions_[sparse_[e.index].value()];
    }

    T& bump(Entity e) {
        assert(has(e));
        auto dense_index = sparse_[e.index].value();
        versions_[dense_index] = ++version_counter_;
        return dense_[dense_index];
    }

private:
    std::vector<T> dense_;
    std::vector<Entity> dense_entities_;
    std::vector<std::optional<size_t>> sparse_;
    std::vector<uint64_t> versions_;
    uint64_t version_counter_{0};
    std::vector<Signal> on_construct_;
    std::vector<Signal> on_destroy_;

    void ensure_sparse(uint32_t index) {
        if (index >= sparse_.size()) {
            sparse_.resize(index + 1);
        }
    }
};

} // namespace glass
