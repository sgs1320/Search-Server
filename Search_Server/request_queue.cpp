#include "request_queue.h"

std::vector<Document> RequestQueue::AddFindRequest(std::string_view  raw_query, DocumentStatus status) {
    return AddFindRequest(raw_query,
                          [status](int /*document_id*/, DocumentStatus document_status, int /*rating*/)
                          { return document_status == status; } );
}

std::vector<Document> RequestQueue::AddFindRequest(std::string_view  raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}

[[nodiscard]] int RequestQueue::GetNoResultRequests() const {
    return count_if(requests_.begin(), requests_.end(), [](QueryResult element) { return element.response_counter == 0; });
}
