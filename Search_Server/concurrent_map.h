#pragma once

#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <string>
#include <vector>
#include <mutex>
#include <execution>

template <typename Key, typename Value>
class ConcurrentMap {
public:
    typedef std::pair<std::mutex, std::map<Key, Value>> MutexToMap;

    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        explicit Access(std::mutex &m, Value& value)
                : n(m)
                , ref_to_value(value) {
        }

        ~Access() {
            n.unlock();
        }

        std::mutex& n;
        Value& ref_to_value;
    };

    explicit ConcurrentMap(size_t bucket_count) : split_map_in_vec_(bucket_count) {}

    Access operator[](const Key& key) {
        uint64_t index = static_cast<uint64_t>(key) % split_map_in_vec_.size();
        split_map_in_vec_[index].first.lock();
        return Access(split_map_in_vec_[index].first, split_map_in_vec_[index].second[key]);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, dictionary] : split_map_in_vec_) {
            std::lock_guard<std::mutex> guard(mutex);
            result.insert(dictionary.begin(), dictionary.end());
        }
        return result;
    }

private:
    std::vector<MutexToMap> split_map_in_vec_;
};

//==========================================================================================================================
/*template <typename ExecutionPolicy, typename ForwardRange, typename Function>
void ForEach(const ExecutionPolicy& policy, ForwardRange& range, Function function) {
    if constexpr (
            std::is_same_v<ExecutionPolicy, std::execution::sequenced_policy>
                                       || std::is_same_v<
                                          typename std::iterator_traits<typename ForwardRange::iterator>::iterator_category,
                    std::random_access_iterator_tag
                    >
            ) {

        for_each(policy, range.begin(), range.end(), function);

    } else {

        static constexpr int PART_COUNT = 4;
        const auto part_length = size(range) / PART_COUNT;
        auto part_begin = range.begin();
        auto part_end = next(part_begin, part_length);

        std::vector<std::future<void>> futures;
        for (
                int i = 0;
                i < PART_COUNT;
                ++i,
                        part_begin = part_end,
                        part_end = (i == PART_COUNT - 1
                                    ? range.end()
                                    : next(part_begin, part_length))
                ) {
            futures.push_back(async([function, part_begin, part_end] { for_each(part_begin, part_end, function); }));
        }

    }
}

template <typename ForwardRange, typename Function>
void ForEach(ForwardRange& range, Function function) {
    ForEach(std::execution::seq, range, function);
}*/
