#include "commons.h"
#include "concurrent_hash_map.h"

#include <string>
#include <unordered_set>
#include <ranges>
#include <stdexcept>

#include <catch2/catch_test_macros.hpp>

static constexpr auto kSeed = 723'834u;

size_t PairHash(const std::pair<int, int>& x) {
    return 17239u * x.first + x.second;
}

TEST_CASE("Constructors") {
    ConcurrentHashMap<int, int> first_table;
    ConcurrentHashMap<std::string, std::string> second_table(5, 16);
    ConcurrentHashMap<int, int64_t> third_table(17);

    ConcurrentHashMap<std::pair<int, int>, int, decltype(&PairHash)> pair_table(PairHash);
    REQUIRE(pair_table.Insert({1, 1}, 2));
    REQUIRE(pair_table.Size() == 1);

    auto lambda = [](const std::pair<int, int>& x) { return PairHash(x); };
    ConcurrentHashMap<std::pair<int, int>, std::string, decltype(lambda)> new_pair_table(100, 1,
                                                                                         lambda);
    REQUIRE(new_pair_table.Insert({1, 2}, "string"));
    REQUIRE(new_pair_table.Size() == 1u);
}

TEST_CASE("Operations") {
    ConcurrentHashMap<int, int> table;
    REQUIRE(table.Insert(3, 1));
    REQUIRE(table.Insert(2, 2));
    REQUIRE_FALSE(table.Insert(2, 1));
    REQUIRE(table.Find(2).second == 2);
    REQUIRE_FALSE(table.Find(5).first);
    REQUIRE(table.Size() == 2);
    table.Insert(5, 5);
    REQUIRE(table.Find(5).second == 5);

    REQUIRE(table.Erase(5));
    REQUIRE(table.Size() == 2);
    auto tmp = table.Find(2);
    REQUIRE(tmp.first);
    auto some_search = table.Find(5);
    REQUIRE_FALSE(some_search.first);

    table.Clear();
    REQUIRE(table.Size() == 0);
    table.Insert(7, 3);
    REQUIRE(table.Find(7) == std::pair{true, 3});
}

TEST_CASE("Constness") {
    ConcurrentHashMap<int, int> table;
    table.Insert(1, 1);
    table.Insert(2, 2);
    table.Insert(3, 3);

    REQUIRE(table.Find(3) == std::pair{true, 3});
    REQUIRE_FALSE(table.Find(4).first);
    REQUIRE(table.At(1) == 1);
    REQUIRE_THROWS_AS(table.At(0), std::out_of_range);
}

TEST_CASE("Clear") {
    ConcurrentHashMap<int, int> table;
    table.Insert(1, 1);
    table.Clear();
    REQUIRE(table.Insert(1, 1));
}

TEST_CASE("Insertions") {
    Tester<true> tester;
    for (auto i = 0; i < 4; ++i) {
        tester.AddTask<QueryType::INSERT>(Random{kSeed + i});
    }
}

TEST_CASE("Searching") {
    Tester<true> tester;
    tester.AddTask<QueryType::INSERT>(Random{kSeed - 1});
    tester.AddTask<QueryType::FIND>(Random{kSeed - 2});
    tester.AddTask<QueryType::INSERT>(Increment{-1});
    tester.AddTask<QueryType::FIND>(Increment{0});
}

TEST_CASE("Erasing") {
    Tester tester{1'000};
    tester.AddTask<QueryType::INSERT>(Random{kSeed + 100});
    tester.AddTask<QueryType::INSERT>(Random{kSeed + 200});
    tester.AddTask<QueryType::ERASE>(Random{kSeed + 100});
    tester.AddTask<QueryType::ERASE>(Random{kSeed + 200});
    tester.AddTask<QueryType::INSERT>(Increment{100});
    tester.AddTask<QueryType::ERASE>(Increment{-100});
    tester.AddTask<QueryType::FIND>(Random{19, 100, 200});
    tester.AddTask<QueryType::FIND>(Random{19, 100, 200});
}

TEST_CASE("InsertAndClear") {
    Tester tester{Tester<>::kMaxIterations / 2};
    tester.AddTask<QueryType::INSERT>(Random{kSeed + 17239});
    tester.AddTask<QueryType::CLEAR>(Increment{0});
}

static void MakeQueriesAndCheck(ConcurrentHashMap<int, int>* table, std::unordered_set<int>* set,
                                int index) {
    static constexpr auto kNumIterations = Tester<>::kMaxIterations;
    static constexpr auto kWindow = 100 * static_cast<int>(kNumIterations);
    static std::mutex mutex;
    auto seed = kSeed + (index + 1) * 10;
    Random random{kSeed - (index + 1) * 100, index * kWindow + 1, (index + 1) * kWindow};

    set->reserve(kNumIterations);
    std::vector<int> inserted;
    inserted.reserve(kNumIterations);
    std::mt19937 gen{seed};
    std::uniform_real_distribution<double> dist;

    for (auto i = 0u; i < kNumIterations; ++i) {
        if (auto prob = dist(gen); prob < 0.6) {
            auto value = random();
            inserted.push_back(value);
            table->Insert(value, 1);
            set->insert(value);
        } else if (prob < 0.8 && !inserted.empty()) {
            std::uniform_int_distribution index{0ul, inserted.size() - 1};
            auto value = inserted[index(gen)];
            table->Erase(value);
            set->erase(value);
        } else if (!inserted.empty()) {
            std::uniform_int_distribution index{0ul, inserted.size() - 1};
            auto value = inserted[index(gen)];
            std::lock_guard lock{mutex};
            REQUIRE(table->Find(value).first == set->contains(value));
        }
    }
}

TEST_CASE("OneThreadInteraction") {
    static constexpr auto kThreadsCount = 4;

    ConcurrentHashMap<int, int> table;
    std::vector<std::unordered_set<int>> sets(kThreadsCount);
    std::vector<std::jthread> threads;
    for (auto i = 0; auto& set : sets) {
        threads.emplace_back(MakeQueriesAndCheck, &table, &set, i++);
    }
    threads.clear();

    size_t total_size = 0;
    for (const auto& set : sets) {
        total_size += set.size();
        for (auto x : set) {
            REQUIRE(table.Find(x).first);
        }
    }
    REQUIRE(table.Size() == total_size);
}
