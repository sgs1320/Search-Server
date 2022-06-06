#include "process_queries.h"

#include <execution>
#include <utility>

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> result(queries.size());
    transform(
            execution::par,
            queries.begin(), queries.end(),
            result.begin(),
            [&search_server](const std::string& query) { return search_server.FindTopDocuments(query); }
            );

    return result;
}

std::vector<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries) {

    std::vector<Document> result;
    for (vector<Document>& documents : ProcessQueries(search_server, queries)) {
        transform(
                documents.begin(), documents.end(),
                back_inserter(result),
                [](Document& document) { return document; }
                );
    }

    return result;
}