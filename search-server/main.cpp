#include <algorithm>
#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string line;
    getline(cin, line);
    return line;
}

int ReadLineWithNumber() {
    int number = 0;
    cin >> number;
    ReadLine();
    return number;
}

vector<string> SplitIntoWords(const string& text) {
    string word;
    vector<string> words;
    for (const auto ch : text) {
        if (ch == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += ch;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
    
    Document() = default;

    Document(int _id, double _relevance, int _rating) : 
    id(_id), 
    relevance(_relevance), 
    rating(_rating) {}
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

/* Реализация класса SearchServer */
class SearchServer {

public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) {
        if (!CheckWordsAreCorrect(stop_words)) {
            throw invalid_argument("words has invalid symbols"s);
        }
        stop_words_ = set(stop_words.begin(), stop_words.end());
    }

    explicit SearchServer(const string& text): 
    SearchServer(::SplitIntoWords(text)) {}

public:
    void SetStopWords(const string& text) {
        const auto stop_words = SplitIntoWords(text);
        stop_words_ = set(stop_words.begin(), stop_words.end());
    }

    void AddDocument(int document_id, const string& raw_document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("document id is invalid"s);
        }
        if (documents_.count(document_id)) {
            throw invalid_argument("document with this id has already been added"s);
        }

        const auto words = SplitIntoWordsNoStop(raw_document);
        const double word_frequency = 1.0 / static_cast<double>(words.size());
        for (const auto& word : words) {
            word_to_frequency_in_document_[word][document_id] += word_frequency;
        }
        documents_.emplace(document_id, DocumentInfo(status, ratings));
        document_ids_.push_back(document_id);
    }

    template <typename DocumentFilter>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentFilter document_filter) const {
        const auto query_content = ParseQuery(raw_query);

        auto documents = FindAllDocuments(query_content, document_filter);
        sort(documents.begin(), documents.end(), [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < SearchServer::kDocumentRelevanceDelta) {
                return lhs.rating > rhs.rating;
            }
            return lhs.relevance > rhs.relevance;
        });
        if (static_cast<int>(documents.size()) > MAX_RESULT_DOCUMENT_COUNT) {
            documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus document_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(
            raw_query, 
            [document_status](int, DocumentStatus status, int) -> bool { return status == document_status; }
        );
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const auto query_content = ParseQuery(raw_query);

        bool minus_word_found = false;
        if (query_content.words_by_type.count(WordType::kMinus)) {
            for (const auto& word : query_content.words_by_type.at(WordType::kMinus)) {
                if (word_to_frequency_in_document_.count(word) && word_to_frequency_in_document_.at(word).count(document_id)) {
                    minus_word_found = true;
                    break;
                }
            }
        }
        if (minus_word_found || !query_content.words_by_type.count(WordType::kPlus)) {
            return {vector<string>(), documents_.at(document_id).GetStatus()};
        }

        vector<string> matched_words;
        for (const auto& word : query_content.words_by_type.at(WordType::kPlus)) {
            if (word_to_frequency_in_document_.count(word) && word_to_frequency_in_document_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }

        return {matched_words, documents_.at(document_id).GetStatus()};
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    int GetDocumentId(int index) const {
        if (index < 0 || index >= GetDocumentCount()) {
            throw out_of_range("document index is invalid"s);
        }

        return document_ids_.at(index);
    }

private:
    class DocumentInfo {
        public:
            DocumentInfo(DocumentStatus status, const vector<int>& ratings) :
                status_(status),
                rating_(ComputeAverageRating(ratings)) {}

            DocumentStatus GetStatus() const {
                return status_;
            }

            int GetRating() const {
                return rating_;
            }

        private: 
            DocumentStatus status_;
            int rating_;

        static int ComputeAverageRating(const vector<int>& ratings) {
            const int average_rating = (!ratings.empty() 
                                        ? accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size())
                                        : 0);
            return average_rating;
        }
    };
    map<int, DocumentInfo> documents_;
    vector<int> document_ids_;
    static constexpr double kDocumentRelevanceDelta = 1e-6;

    map<string, map<int, double>> word_to_frequency_in_document_;
    set<string> stop_words_;

    enum class WordType {
        kPlus = 0,
        kMinus,
        kStop,
        kInvalid
    };

    struct QueryContent {
        map<WordType, set<string>> words_by_type;
    };

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const auto& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    QueryContent ParseQuery(const string& text) const {
        auto words = SplitIntoWords(text);
        QueryContent query_content;
        for (auto& word : words) {
            const auto query_word_type = GetQueryWordType(word);
            switch (query_word_type) {
                case WordType::kMinus: {
                    word = word.substr(1);
                    [[fallthrough]];
                }
                case WordType::kPlus: {
                    query_content.words_by_type[query_word_type].insert(word);
                    break;
                }
                case WordType::kInvalid: {
                    throw invalid_argument("text has invalid words"s);
                }
                default: {
                    break;
                }
            }
        }
        return query_content;
    }

    static vector<string> SplitIntoWords(const string& text) {
        const auto words = ::SplitIntoWords(text);
        if (!CheckWordsAreCorrect(words)) {
            throw invalid_argument("text has words with invalid symbols"s);
        }
        return words;
    }

    template <typename WordsContainer>
    static bool CheckWordsAreCorrect(const WordsContainer& words) {
        for (const auto& word : words) {
            for (const auto ch : word) {
                if (ch >= '\000' && ch <= '\037') {
                    return false;
                }
            }
        }
        return true;
    }

    WordType GetQueryWordType(const string& query_word) const {
        if (IsStopWord(query_word)) {
            return WordType::kStop;
        }
        if (query_word[0] == '-') {
            if (query_word.size() == 1 || query_word[1] == '-') {
                return WordType::kInvalid;
            }
            return WordType::kMinus;
        }
        return WordType::kPlus;
    }

    template <typename DocumentFilter>
    vector<Document> FindAllDocuments(const QueryContent& query_content, DocumentFilter document_filter) const {
        const auto documents_relevance = ComputeDocumentsRelevance(query_content, document_filter);

        vector<Document> documents;
        documents.reserve(documents_relevance.size());
        for (const auto& [document_id, document_relevance] : documents_relevance) {
            documents.push_back({document_id, document_relevance, documents_.at(document_id).GetRating()});
        }
        return documents;
    }

    template <typename DocumentFilter>
    map<int, double> ComputeDocumentsRelevance(const QueryContent& query_content, DocumentFilter document_filter) const {
        if (!query_content.words_by_type.count(WordType::kPlus)
            || query_content.words_by_type.at(WordType::kPlus).empty()) {
            return {};
        }

        map<int, double> documents_relevance;
        for (const auto& word : query_content.words_by_type.at(WordType::kPlus)) {
            if (!word_to_frequency_in_document_.count(word)) continue;

            const auto word_idf = ComputeWordIDF(word);
            for (const auto& [document_id, word_document_tf] : word_to_frequency_in_document_.at(word)) {
                if (document_filter(document_id, 
                                    documents_.at(document_id).GetStatus(), 
                                    documents_.at(document_id).GetRating())) {
                    documents_relevance[document_id] += word_document_tf * word_idf;
                }
            }
        }
        if (query_content.words_by_type.count(WordType::kMinus)) {
            FilterDocumentsRelevanceByMinusWords(documents_relevance,
                                                 query_content.words_by_type.at(WordType::kMinus));
        }
        return documents_relevance;
    }

    double ComputeWordIDF(const string& word) const {
        const double word_idf = (word_to_frequency_in_document_.count(word) && !word_to_frequency_in_document_.at(word).empty()
                                 ? log(static_cast<double>(documents_.size()) / static_cast<double>(word_to_frequency_in_document_.at(word).size()))
                                 : 0.0);
        return word_idf;
    }

    void FilterDocumentsRelevanceByMinusWords(map<int, double>& documents_relevance,
                                              const set<string>& minus_words) const {
        for (const auto& [word, word_document_info] : word_to_frequency_in_document_) {
            if (!minus_words.count(word)) continue;

            for (const auto& [document_id, word_document_tf] : word_document_info) {
                documents_relevance.erase(document_id);
            }
        }
    }
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin_range, Iterator end_range, size_t page_size) {        
        auto start_page = begin_range;
        while (start_page < end_range) {
            auto end_page = start_page + page_size;
            if (end_page > end_range) {
                end_page = end_range;
            };
            pages_.push_back({start_page, end_page});
            start_page += page_size;
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    auto size() const {
        return pages_.size();
    }

private:
    vector<pair<Iterator, Iterator>> pages_;
}; 

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template <typename Iterator>
ostream& operator<<(ostream& output, const pair<Iterator, Iterator>& page) {
    for (auto it = page.first; it != page.second; ++it) {
        cout << *it;
    }
    return output;
} 

ostream& operator<<(ostream& output, const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s;
    return output;
}

void PrintDocument(const Document& document) {
    cout << document << endl;
}

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server) : search_server_{search_server}, time_{0} {}

    template <typename DocumentFilter>
    vector<Document> AddFindRequest(const string& raw_query, DocumentFilter document_filter) {
        RefreshRequestsByTime();

        const auto documents = search_server_.FindTopDocuments(raw_query, document_filter);
        requests_.push_back({time_, documents.empty()});
        return documents;
    }
    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus document_status = DocumentStatus::ACTUAL) {
        return AddFindRequest(
            raw_query, 
            [document_status](int, DocumentStatus status, int) -> bool { return status == document_status; }
        );
    }
    int GetNoResultRequests() const {
        return count_if(requests_.begin(), requests_.end(), [](const auto& query_result) -> bool {
            return query_result.is_empty;
        });
    }
private:
    struct QueryResult {
        uint64_t request_time;
        bool is_empty;
    };
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    uint64_t time_;

    void RefreshRequestsByTime() {
        ++time_;
        while (!requests_.empty() && time_ - requests_.front().request_time >= min_in_day_) {
            requests_.pop_front();
        }
    }
};

int main() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog"s);
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar"s);
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow"s);
    cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << endl;
    return 0;
} 
