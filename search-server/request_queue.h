#pragma once

#include <deque>
#include <stdint.h>
#include <string>
#include <vector>

#include "document.h"
#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentFilter>
    std::vector<Document> AddFindRequest(const std::string& raw_query, 
                                         DocumentFilter document_filter);

    std::vector<Document> AddFindRequest(const std::string& raw_query, 
                                         DocumentStatus document_status = DocumentStatus::ACTUAL);

    int GetNoResultRequests() const;
private:
    const SearchServer& search_server_;
    struct QueryResult {
        bool is_empty;
    };
    std::deque<QueryResult> requests_;
    
    static constexpr int kMinInDay = 1440;

    void RemoveOldRequests();
};

template <typename DocumentFilter>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, 
                                                   DocumentFilter document_filter) {
    RemoveOldRequests();

    const auto documents = search_server_.FindTopDocuments(raw_query, document_filter);
    requests_.push_back({documents.empty()});
    return documents;
}
