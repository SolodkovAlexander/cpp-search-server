#include <algorithm>

#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server) : 
    search_server_(search_server) {
}

vector<Document> RequestQueue::AddFindRequest(const string& raw_query, 
                                              DocumentStatus document_status) {
    return AddFindRequest(raw_query, 
                          [document_status](int, DocumentStatus status, int) -> bool { return status == document_status; });
}

int RequestQueue::GetNoResultRequests() const {
    return count_if(requests_.begin(), 
                    requests_.end(), 
                    [](const auto& query_result) -> bool { return query_result.is_empty; });
}

void RequestQueue::RemoveOldRequests() {
    if (requests_.size() == kMinInDay) {
        requests_.pop_front();
    }
}
