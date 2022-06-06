#pragma once


#include <deque>
#include <vector>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) 
        : search_server_(search_server)
    {
    }

    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(std::string_view raw_query, DocumentPredicate document_predicate) {
        auto result = search_server_.FindTopDocuments(raw_query, document_predicate);
        QueryResult element = { 1, static_cast<int>(result.size()) };
        if (requests_.empty()) {
            requests_.push_back(element);
        }

        if (requests_.size() != kMinutesInDay) {
            element.time = ++requests_.back().time;
            requests_.push_back(element);
        } else {
            if (requests_.back().time == kMinutesInDay) {
                element.time = 1;
            } else {
                element.time = ++requests_.back().time;
            }
            requests_.push_back(element);
            requests_.pop_front();
        }

        return result;
    }

    std::vector<Document> AddFindRequest(std::string_view  raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(std::string_view  raw_query);

    [[nodiscard]] int GetNoResultRequests() const;
private:
    struct QueryResult {
        int time = 0;
        int response_counter = 0;
    };
private:
    static const int kMinutesInDay = 1440;
private:
    const SearchServer& search_server_;
    std::deque<QueryResult> requests_;
};