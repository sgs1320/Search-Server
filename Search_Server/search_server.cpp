#include "search_server.h"

SearchServer::SearchServer(const std::string& stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
// from string container
{
}

SearchServer::SearchServer(std::string_view stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor
// from string container
{
}

[[nodiscard]] vector<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

[[nodiscard]] vector<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

[[nodiscard]] size_t SearchServer::GetDocumentCount() const {
    return documents_.size();
}

[[nodiscard]] const map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<std::string_view, double> result = {};
    result.clear();
    for (const auto& [word, freqs] : word_to_document_freqs_) {
        if (freqs.count(document_id) != 0) {
            result.insert({word, freqs.at(document_id)});
        }
    }
    return result;
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const vector<int> &ratings) {
    if (document_id < 0 || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Not valid document id");
    }
    const vector<std::string_view> words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / static_cast<double>(words.size());
    for (std::string_view word : words) {
        auto it = all_words_.insert(string(word));
        word_to_document_freqs_[*it.first][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.push_back(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    documents_.erase(document_id);

    auto id_to_delete = lower_bound(document_ids_.begin(), document_ids_.end(), document_id);
    if (*id_to_delete == document_id) {
        document_ids_.erase(id_to_delete);
    }

    for (auto& [_, freqs] : word_to_document_freqs_) {
        if (freqs.count(document_id) != 0) {
            freqs.erase(document_id);
        }
    }
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy, int document_id) {
    documents_.erase(document_id);

    auto id_to_delete = lower_bound(document_ids_.begin(), document_ids_.end(), document_id);
    if (*id_to_delete == document_id) {
        document_ids_.erase(id_to_delete);
    }

    std::for_each(
            std::execution::par,
            word_to_document_freqs_.begin(), word_to_document_freqs_.end(),
            [&document_id](auto& element) {
                if (element.second.count(document_id) != 0) {
                    element.second.erase(document_id);
                }
            });
}

[[nodiscard]] std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int /*id*/, DocumentStatus document_status, int /*rating*/) { return document_status == status; });
}

[[nodiscard]] std::vector<Document> SearchServer::FindTopDocuments(execution::sequenced_policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, [status](int /*id*/, DocumentStatus document_status, int /*rating*/) { return document_status == status; });
}

[[nodiscard]] std::vector<Document> SearchServer::FindTopDocuments(execution::parallel_policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::par, raw_query, [status](int /*id*/, DocumentStatus document_status, int /*rating*/) { return document_status == status; });
}

[[nodiscard]] std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

[[nodiscard]] std::vector<Document> SearchServer::FindTopDocuments(execution::sequenced_policy, std::string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

[[nodiscard]] std::vector<Document> SearchServer::FindTopDocuments(execution::parallel_policy, std::string_view raw_query) const {
    return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}

[[nodiscard]] std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    std::vector<std::string_view> matched_words;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

[[nodiscard]] std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy,
                                                                                               std::string_view raw_query,
                                                                                               int document_id) const {
    return MatchDocument(raw_query, document_id);
}

[[nodiscard]] std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy,
                                                                                               std::string_view raw_query,
                                                                                               int document_id) const {
    const auto query = ParseQuery(raw_query);

    mutex m;
    mutex x;

    std::vector<std::string_view> matched_words;
    for_each(
            std::execution::par,
            query.plus_words.begin(), query.plus_words.end(),
            [document_id, &matched_words, this, &m](std::string_view word) {
                lock_guard<mutex> guard(m);
                if (word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.push_back(word);
                }
            });

    for_each(
            std::execution::par,
            query.minus_words.begin(), query.minus_words.end(),
            [document_id, &matched_words, this, &x](std::string_view word) {
                lock_guard<mutex> guard(x);
                if (word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.clear();
                    return;
                }
            });

    return {matched_words, documents_.at(document_id).status};
}

// private =========================================================

bool SearchServer::IsValidWord(std::string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) { return c >= '\0' && c < ' '; });
}

int SearchServer::ComputeAverageRating(const vector<int> &ratings) {
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

[[nodiscard]] bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

[[nodiscard]] std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const { // =================== ??????????????????????????
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + std::string(word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

[[nodiscard]] SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }

    // Check validation
    if (text.empty() || text[0] == '-' || text.back() == '-' || !IsValidWord(text)) {
        throw invalid_argument("Not valid word");
    }

    return { text, is_minus, IsStopWord(text)};
}

[[nodiscard]] SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    Query query;
    for (std::string_view word : SplitIntoWords(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            }
            else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

[[nodiscard]] double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(static_cast<double>(GetDocumentCount()) * 1.0 / static_cast<double>(word_to_document_freqs_.at(word).size()));
}

// func out of search_server ===============================================================================

void PrintDocument(const Document& document) {
    std::cout << "{ "s
              << "document_id = "s << document.id << ", "s
              << "relevance = "s << document.relevance << ", "s
              << "rating = "s << document.rating << " }"s << std::endl;
}

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status) {
    std::cout << "{ "s
              << "document_id = "s << document_id << ", "s
              << "status = "s << static_cast<int>(status) << ", "s
              << "words ="s;
    for (std::string_view word : words) {
        std::cout << ' ' << word;
    }
    std::cout << "}"s << std::endl;
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
                 const std::vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << std::endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query) {
    std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка поиска: "s << e.what() << std::endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const std::string& query) {
    try {
        std::cout << "Матчинг документов по запросу: "s << query << std::endl;
        for (const int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << std::endl;
    }
}