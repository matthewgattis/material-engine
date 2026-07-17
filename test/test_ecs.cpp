#include <glass/entity.hpp>
#include <glass/component_pool.hpp>
#include <glass/world.hpp>

#include <gtest/gtest.h>

#include <unordered_map>

using namespace glass;

// ---- Entity ----

TEST(Entity, SizeIs8Bytes) {
    EXPECT_EQ(sizeof(Entity), 8);
}

TEST(Entity, EqualityOperator) {
    Entity a{0, 0};
    Entity b{0, 0};
    EXPECT_EQ(a, b);
}

TEST(Entity, InequalityByIndex) {
    Entity a{0, 0};
    Entity b{1, 0};
    EXPECT_NE(a, b);
}

TEST(Entity, InequalityByGeneration) {
    Entity a{0, 0};
    Entity b{0, 1};
    EXPECT_NE(a, b);
}

TEST(Entity, NullEntityConstant) {
    EXPECT_EQ(null_entity.index, 0xFFFFFFFF);
    EXPECT_EQ(null_entity.generation, 0u);
}

TEST(Entity, NullEntityEquality) {
    Entity e{0xFFFFFFFF, 0};
    EXPECT_EQ(e, null_entity);
}

TEST(Entity, HashConsistent) {
    Entity e{42, 7};
    auto h1 = std::hash<Entity>{}(e);
    auto h2 = std::hash<Entity>{}(e);
    EXPECT_EQ(h1, h2);
}

TEST(Entity, DifferentEntitiesDifferentHashes) {
    Entity a{0, 0};
    Entity b{1, 0};
    // Not guaranteed, but extremely likely for adjacent indices
    EXPECT_NE(std::hash<Entity>{}(a), std::hash<Entity>{}(b));
}

TEST(Entity, UsableInUnorderedMap) {
    std::unordered_map<Entity, int> map;
    Entity e{5, 3};
    map[e] = 42;
    EXPECT_EQ(map[e], 42);
    EXPECT_EQ(map.size(), 1);
}

// ---- ComponentPool ----

struct Position {
    float x, y, z;
};

struct Name {
    std::string value;
};

TEST(ComponentPool, DefaultConstructionIsEmpty) {
    ComponentPool<Position> pool;
    EXPECT_EQ(pool.size(), 0);
    EXPECT_TRUE(pool.entities().empty());
    EXPECT_TRUE(pool.components().empty());
}

TEST(ComponentPool, AddReturnsReference) {
    ComponentPool<Position> pool;
    Entity e{0, 0};
    auto& pos = pool.add(e, Position{1.0f, 2.0f, 3.0f});
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
    EXPECT_FLOAT_EQ(pos.y, 2.0f);
    EXPECT_FLOAT_EQ(pos.z, 3.0f);
}

TEST(ComponentPool, AddIncreasesSize) {
    ComponentPool<Position> pool;
    pool.add(Entity{0, 0}, Position{});
    EXPECT_EQ(pool.size(), 1);
    pool.add(Entity{1, 0}, Position{});
    EXPECT_EQ(pool.size(), 2);
}

TEST(ComponentPool, HasReturnsFalseInitially) {
    ComponentPool<Position> pool;
    EXPECT_FALSE(pool.has(Entity{0, 0}));
}

TEST(ComponentPool, HasReturnsTrueAfterAdd) {
    ComponentPool<Position> pool;
    Entity e{0, 0};
    pool.add(e, Position{});
    EXPECT_TRUE(pool.has(e));
}

TEST(ComponentPool, HasReturnsFalseForWrongGeneration) {
    ComponentPool<Position> pool;
    Entity e{0, 0};
    pool.add(e, Position{});
    Entity stale{0, 1};
    EXPECT_FALSE(pool.has(stale));
}

TEST(ComponentPool, GetReturnsAddedComponent) {
    ComponentPool<Position> pool;
    Entity e{0, 0};
    pool.add(e, Position{10.0f, 20.0f, 30.0f});
    auto& pos = pool.get(e);
    EXPECT_FLOAT_EQ(pos.x, 10.0f);
    EXPECT_FLOAT_EQ(pos.y, 20.0f);
    EXPECT_FLOAT_EQ(pos.z, 30.0f);
}

TEST(ComponentPool, ConstGetReturnsAddedComponent) {
    ComponentPool<Position> pool;
    Entity e{0, 0};
    pool.add(e, Position{5.0f, 6.0f, 7.0f});
    const auto& cpool = pool;
    auto& pos = cpool.get(e);
    EXPECT_FLOAT_EQ(pos.x, 5.0f);
}

TEST(ComponentPool, RemoveDecreasesSize) {
    ComponentPool<Position> pool;
    Entity e{0, 0};
    pool.add(e, Position{});
    EXPECT_EQ(pool.size(), 1);
    pool.remove(e);
    EXPECT_EQ(pool.size(), 0);
}

TEST(ComponentPool, RemoveSetsHasToFalse) {
    ComponentPool<Position> pool;
    Entity e{0, 0};
    pool.add(e, Position{});
    pool.remove(e);
    EXPECT_FALSE(pool.has(e));
}

TEST(ComponentPool, RemoveOfMissingEntityIsNoop) {
    ComponentPool<Position> pool;
    pool.remove(Entity{99, 0}); // should not crash
    EXPECT_EQ(pool.size(), 0);
}

TEST(ComponentPool, SparseGrowsForLargeIndices) {
    ComponentPool<Position> pool;
    Entity e{1000, 0};
    pool.add(e, Position{1.0f, 2.0f, 3.0f});
    EXPECT_TRUE(pool.has(e));
    EXPECT_FLOAT_EQ(pool.get(e).x, 1.0f);
}

TEST(ComponentPool, DensePackingAfterRemove) {
    ComponentPool<Position> pool;
    Entity e0{0, 0};
    Entity e1{1, 0};
    Entity e2{2, 0};
    pool.add(e0, Position{0, 0, 0});
    pool.add(e1, Position{1, 1, 1});
    pool.add(e2, Position{2, 2, 2});

    pool.remove(e0);

    // After removing e0, e2 should have been swapped into its slot
    EXPECT_EQ(pool.size(), 2);
    EXPECT_TRUE(pool.has(e1));
    EXPECT_TRUE(pool.has(e2));
    // Dense array should be contiguous
    EXPECT_EQ(pool.components().size(), 2);
}

TEST(ComponentPool, EntitiesAccessor) {
    ComponentPool<Position> pool;
    Entity e0{0, 0};
    Entity e1{1, 0};
    pool.add(e0, Position{});
    pool.add(e1, Position{});
    auto& entities = pool.entities();
    EXPECT_EQ(entities.size(), 2);
    EXPECT_EQ(entities[0], e0);
    EXPECT_EQ(entities[1], e1);
}

TEST(ComponentPool, ComponentsAccessorMutable) {
    ComponentPool<Position> pool;
    pool.add(Entity{0, 0}, Position{1, 2, 3});
    pool.components()[0].x = 99.0f;
    EXPECT_FLOAT_EQ(pool.get(Entity{0, 0}).x, 99.0f);
}

TEST(ComponentPool, WorksWithMoveOnlyTypes) {
    ComponentPool<Name> pool;
    Entity e{0, 0};
    pool.add(e, Name{"hello"});
    EXPECT_EQ(pool.get(e).value, "hello");
}

// ---- World ----

TEST(World, CreateReturnsValidEntity) {
    World world;
    Entity e = world.create();
    EXPECT_NE(e, null_entity);
}

TEST(World, CreateIncrementingIndices) {
    World world;
    Entity e0 = world.create();
    Entity e1 = world.create();
    Entity e2 = world.create();
    EXPECT_EQ(e0.index, 0u);
    EXPECT_EQ(e1.index, 1u);
    EXPECT_EQ(e2.index, 2u);
}

TEST(World, CreateStartsAtGenerationZero) {
    World world;
    Entity e = world.create();
    EXPECT_EQ(e.generation, 0u);
}

TEST(World, AliveReturnsTrueForNew) {
    World world;
    Entity e = world.create();
    EXPECT_TRUE(world.alive(e));
}

TEST(World, AliveReturnsFalseForNull) {
    World world;
    EXPECT_FALSE(world.alive(null_entity));
}

TEST(World, AliveReturnsFalseForUncreated) {
    World world;
    EXPECT_FALSE(world.alive(Entity{999, 0}));
}

TEST(World, DestroyMarksDead) {
    World world;
    Entity e = world.create();
    world.destroy(e);
    EXPECT_FALSE(world.alive(e));
}

TEST(World, DestroyOfDeadEntityIsNoop) {
    World world;
    Entity e = world.create();
    world.destroy(e);
    world.destroy(e); // should not crash
    EXPECT_FALSE(world.alive(e));
}

TEST(World, ReuseIndexAfterDestroy) {
    World world;
    Entity e0 = world.create();
    uint32_t original_index = e0.index;
    world.destroy(e0);
    Entity e1 = world.create();
    EXPECT_EQ(e1.index, original_index);
}

TEST(World, GenerationIncrementsOnReuse) {
    World world;
    Entity e0 = world.create();
    EXPECT_EQ(e0.generation, 0u);
    world.destroy(e0);
    Entity e1 = world.create();
    EXPECT_EQ(e1.generation, 1u);
}

TEST(World, StaleHandleIsNotAlive) {
    World world;
    Entity e0 = world.create();
    world.destroy(e0);
    // e0 still has generation 0, but the slot is now generation 1
    EXPECT_FALSE(world.alive(e0));
}

TEST(World, AddAndGetComponent) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{1.0f, 2.0f, 3.0f});
    auto& pos = world.get<Position>(e);
    EXPECT_FLOAT_EQ(pos.x, 1.0f);
}

TEST(World, HasComponent) {
    World world;
    Entity e = world.create();
    EXPECT_FALSE(world.has<Position>(e));
    world.add<Position>(e, Position{});
    EXPECT_TRUE(world.has<Position>(e));
}

TEST(World, RemoveComponent) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{});
    world.remove<Position>(e);
    EXPECT_FALSE(world.has<Position>(e));
}

TEST(World, RemoveNonexistentComponentIsNoop) {
    World world;
    Entity e = world.create();
    world.remove<Position>(e); // no pool exists yet, should not crash
}

TEST(World, MultipleComponentTypes) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{1, 2, 3});
    world.add<Name>(e, Name{"test"});
    EXPECT_TRUE(world.has<Position>(e));
    EXPECT_TRUE(world.has<Name>(e));
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 1.0f);
    EXPECT_EQ(world.get<Name>(e).value, "test");
}

TEST(World, DestroyRemovesAllComponents) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{});
    world.add<Name>(e, Name{"foo"});
    world.destroy(e);
    // After destroy, pools still exist but entity's components are removed
    Entity e2 = world.create(); // reuses index
    EXPECT_FALSE(world.has<Position>(e2));
    EXPECT_FALSE(world.has<Name>(e2));
}

TEST(World, DestroyCallsCallback) {
    World world;
    bool called = false;
    Entity captured = null_entity;
    world.on_entity_destroy([&](World&, Entity e) {
        called = true;
        captured = e;
    });
    Entity e = world.create();
    world.destroy(e);
    EXPECT_TRUE(called);
    EXPECT_EQ(captured, e);
}

TEST(World, DestroyCallbackFiresBeforeComponentRemoval) {
    World world;
    bool had_component = false;
    world.on_entity_destroy([&](World& w, Entity e) {
        had_component = w.has<Position>(e);
    });
    Entity e = world.create();
    world.add<Position>(e, Position{42, 0, 0});
    world.destroy(e);
    EXPECT_TRUE(had_component);
}

TEST(World, MultipleEntityDestroyListeners) {
    World world;
    int calls = 0;
    world.on_entity_destroy([&](World&, Entity) { calls++; });
    world.on_entity_destroy([&](World&, Entity) { calls++; });
    world.destroy(world.create());
    EXPECT_EQ(calls, 2);
}

TEST(World, OnConstructFiresOnAdd) {
    World world;
    Entity captured = null_entity;
    float captured_x = 0.0f;
    world.on_construct<Position>([&](Entity e, Position& p) {
        captured = e;
        captured_x = p.x;
    });
    Entity e = world.create();
    world.add<Position>(e, Position{7, 0, 0});
    EXPECT_EQ(captured, e);
    EXPECT_FLOAT_EQ(captured_x, 7.0f);
}

TEST(World, OnDestroyFiresOnComponentRemove) {
    World world;
    int calls = 0;
    world.on_destroy<Position>([&](Entity, Position&) { calls++; });
    Entity e = world.create();
    world.add<Position>(e, Position{});
    world.remove<Position>(e);
    EXPECT_EQ(calls, 1);
}

TEST(World, OnDestroyFiresOnEntityDestroy) {
    World world;
    int calls = 0;
    world.on_destroy<Position>([&](Entity, Position& p) {
        calls++;
        EXPECT_FLOAT_EQ(p.x, 3.0f); // component still intact
    });
    Entity e = world.create();
    world.add<Position>(e, Position{3, 0, 0});
    world.destroy(e);
    EXPECT_EQ(calls, 1);
}

TEST(World, OnDestroyCanSalvageMoveOnlyComponent) {
    World world;
    std::string salvaged;
    world.on_destroy<Name>([&](Entity, Name& n) { salvaged = std::move(n.value); });
    Entity e = world.create();
    world.add<Name>(e, Name{"precious"});
    world.destroy(e);
    EXPECT_EQ(salvaged, "precious");
}

TEST(World, SignalsDoNotFireForOtherComponentTypes) {
    World world;
    int calls = 0;
    world.on_destroy<Name>([&](Entity, Name&) { calls++; });
    Entity e = world.create();
    world.add<Position>(e, Position{});
    world.destroy(e);
    EXPECT_EQ(calls, 0);
}

TEST(World, AddStampsVersion) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{});
    EXPECT_GT(world.version<Position>(e), 0u);
}

TEST(World, PatchAdvancesVersion) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{});
    auto v0 = world.version<Position>(e);
    world.patch<Position>(e).x = 5.0f;
    EXPECT_GT(world.version<Position>(e), v0);
    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 5.0f);
}

TEST(World, GetDoesNotAdvanceVersion) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{});
    auto v0 = world.version<Position>(e);
    world.get<Position>(e).x = 5.0f;
    EXPECT_EQ(world.version<Position>(e), v0);
}

TEST(World, VersionsSurviveSwapRemove) {
    World world;
    Entity e0 = world.create();
    Entity e1 = world.create();
    world.add<Position>(e0, Position{});
    world.add<Position>(e1, Position{});
    world.patch<Position>(e1);
    auto v1 = world.version<Position>(e1);

    // Removing e0 swaps e1 into its dense slot; its version must follow.
    world.remove<Position>(e0);
    EXPECT_EQ(world.version<Position>(e1), v1);
}

TEST(World, VersionsAreIndependentPerPool) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{});
    world.add<Name>(e, Name{"a"});
    auto name_v = world.version<Name>(e);
    world.patch<Position>(e);
    EXPECT_EQ(world.version<Name>(e), name_v);
}

TEST(World, PoolReturnsNullptrForUnusedType) {
    World world;
    EXPECT_EQ(world.pool<Position>(), nullptr);
}

TEST(World, PoolReturnsNonNullAfterAdd) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{});
    EXPECT_NE(world.pool<Position>(), nullptr);
}

// ---- View ----

TEST(View, SingleComponentIteration) {
    World world;
    Entity e0 = world.create();
    Entity e1 = world.create();
    world.add<Position>(e0, Position{1, 0, 0});
    world.add<Position>(e1, Position{2, 0, 0});

    int count = 0;
    world.view<Position>().each([&](Entity, Position&) {
        count++;
    });
    EXPECT_EQ(count, 2);
}

TEST(View, IterationCallbackReceivesCorrectValues) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{10, 20, 30});

    world.view<Position>().each([&](Entity entity, Position& pos) {
        EXPECT_EQ(entity, e);
        EXPECT_FLOAT_EQ(pos.x, 10.0f);
        EXPECT_FLOAT_EQ(pos.y, 20.0f);
        EXPECT_FLOAT_EQ(pos.z, 30.0f);
    });
}

TEST(View, MultiComponentIteration) {
    World world;
    Entity e0 = world.create();
    Entity e1 = world.create();
    Entity e2 = world.create();
    world.add<Position>(e0, Position{1, 0, 0});
    world.add<Position>(e1, Position{2, 0, 0});
    world.add<Position>(e2, Position{3, 0, 0});
    // Only e1 has both Position and Name
    world.add<Name>(e1, Name{"tagged"});

    int count = 0;
    world.view<Position, Name>().each([&](Entity e, Position& pos, Name& name) {
        EXPECT_EQ(e, e1);
        EXPECT_FLOAT_EQ(pos.x, 2.0f);
        EXPECT_EQ(name.value, "tagged");
        count++;
    });
    EXPECT_EQ(count, 1);
}

TEST(View, EmptyPoolIsNoop) {
    World world;
    int count = 0;
    world.view<Position>().each([&](Entity, Position&) {
        count++;
    });
    EXPECT_EQ(count, 0);
}

TEST(View, MissingSecondPoolIsNoop) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{});
    // Name pool doesn't exist

    int count = 0;
    world.view<Position, Name>().each([&](Entity, Position&, Name&) {
        count++;
    });
    EXPECT_EQ(count, 0);
}

TEST(View, IterationCanModifyComponent) {
    World world;
    Entity e = world.create();
    world.add<Position>(e, Position{1, 2, 3});

    world.view<Position>().each([](Entity, Position& pos) {
        pos.x = 99.0f;
    });

    EXPECT_FLOAT_EQ(world.get<Position>(e).x, 99.0f);
}

TEST(View, IteratesSmallestPool) {
    World world;
    // Create many entities with Position, only one with Name
    for (int i = 0; i < 100; i++) {
        Entity e = world.create();
        world.add<Position>(e, Position{static_cast<float>(i), 0, 0});
    }
    Entity tagged = world.create();
    world.add<Position>(tagged, Position{999, 0, 0});
    world.add<Name>(tagged, Name{"sole"});

    int count = 0;
    world.view<Position, Name>().each([&](Entity, Position& pos, Name& name) {
        EXPECT_FLOAT_EQ(pos.x, 999.0f);
        EXPECT_EQ(name.value, "sole");
        count++;
    });
    EXPECT_EQ(count, 1);
}
