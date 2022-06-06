#pragma once

#include <map>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <stdexcept>
#include <execution>

#include "document.h"
#include "string_processing.h"
#include "read_input_functions.h"
#include "concurrent_map.h"

using namespace std::string_literals;
using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(StringContainer stop_words) {
        if (!all_of(stop_words.begin(), stop_words.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid"s);
        }

        for (std::string_view word : stop_words) {
            auto it = all_words_.insert(string(word));
            stop_words_.insert(*it.first);
        }
    }

    explicit SearchServer(const std::string& stop_words_text);

    explicit SearchServer(std::string_view stop_words_text);

    [[nodiscard]] vector<int>::const_iterator begin() const;

    [[nodiscard]] vector<int>::const_iterator end() const;

    [[nodiscard]] size_t GetDocumentCount() const;

    [[nodiscard]] const map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const vector<int>& ratings);

    void RemoveDocument(int document_id);

    void RemoveDocument(std::execution::sequenced_policy, int document_id);

    void RemoveDocument(std::execution::parallel_policy, int document_id);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(), [&](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < kRelevanceAccuracy) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(execution::sequenced_policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
        return FindTopDocuments(raw_query, document_predicate);
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(execution::parallel_policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);
        sort(std::execution::par, matched_documents.begin(), matched_documents.end(), [&](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < kRelevanceAccuracy) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    [[nodiscard]] std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    [[nodiscard]] std::vector<Document> FindTopDocuments(execution::sequenced_policy, std::string_view raw_query, DocumentStatus status) const;

    [[nodiscard]] std::vector<Document> FindTopDocuments(execution::parallel_policy, std::string_view raw_query, DocumentStatus status) const;

    [[nodiscard]] std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    [[nodiscard]] std::vector<Document> FindTopDocuments(execution::sequenced_policy, std::string_view raw_query) const;

    [[nodiscard]] std::vector<Document> FindTopDocuments(execution::parallel_policy, std::string_view raw_query) const;

    [[nodiscard]] std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    [[nodiscard]] std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::sequenced_policy,
                                                                                     std::string_view raw_query,
                                                                                     int document_id) const;

    [[nodiscard]] std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::execution::parallel_policy,
                                                                                     std::string_view raw_query,
                                                                                     int document_id) const;

private:
    struct DocumentData {
        int rating = 0;
        DocumentStatus status = DocumentStatus::ACTUAL;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus = false;
        bool is_stop = false;
    };

    struct Query {
        std::set<string_view> plus_words;
        std::set<string_view> minus_words;
    };

private:
    static constexpr double kNumberForComparisonDots = 1e-6;

private:
    static bool IsValidWord(std::string_view word);

    static int ComputeAverageRating(const vector<int>& ratings);

    [[nodiscard]] bool IsStopWord(std::string_view word) const;

    [[nodiscard]] std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    [[nodiscard]] QueryWord ParseQueryWord(std::string_view text) const;

    [[nodiscard]] Query ParseQuery(std::string_view text) const;

    [[nodiscard]] double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;
        for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        matched_documents.reserve(document_to_relevance.size());
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
        }
        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(execution::sequenced_policy, const Query& query, DocumentPredicate document_predicate) const {
        return FindAllDocuments(query, document_predicate);
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(execution::parallel_policy, const Query& query, DocumentPredicate document_predicate) const {
        ConcurrentMap<int, double> document_to_relevance_concurrent(word_to_document_freqs_.size());
        for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for_each(
                    execution::par,
                    word_to_document_freqs_.at(word).begin(), word_to_document_freqs_.at(word).end(),
                    [&](auto& element) {
                        const auto& document_data = documents_.at(element.first);
                        if (document_predicate(element.first, document_data.status, document_data.rating)) {
                            document_to_relevance_concurrent[element.first].ref_to_value += element.second * inverse_document_freq;
                        }
                    });
        }
        map<int, double> document_to_relevance = document_to_relevance_concurrent.BuildOrdinaryMap();

        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        matched_documents.reserve(document_to_relevance.size());
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
        }
        return matched_documents;
    }

private:
    set<string> all_words_;
    std::set<std::string_view> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_; // map<WORD, map<DOCUMENT_ID, FREQUENCY>>
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;
    double kRelevanceAccuracy = 1e-6;
};

void PrintDocument(const Document& document);

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
