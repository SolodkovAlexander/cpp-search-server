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
    struct QueryResult {
        uint64_t request_time;
        bool is_empty;
    };
    std::deque<QueryResult> requests_;

    static constexpr int kMinInDay = 1440;
    const SearchServer& search_server_;
    uint64_t time_;

    void RefreshRequestsByTime() {
        ++time_;
        while (!requests_.empty() && time_ - requests_.front().request_time >= kMinInDay) {
            requests_.pop_front();
        }
    }
};

template <typename DocumentFilter>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, 
                                                   DocumentFilter document_filter) {
    RefreshRequestsByTime();

    const auto documents = search_server_.FindTopDocuments(raw_query, document_filter);
    requests_.push_back({time_, documents.empty()});
    return documents;
}
