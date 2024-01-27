#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
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
    vector<string> words;
    string word;
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
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const auto& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int id, const string& raw_content, DocumentStatus status, const vector<int>& ratings) {
        const auto words = SplitIntoWordsNoStop(raw_content);

        const double word_frequency = 1.0 / static_cast<double>(words.size());
        for (const auto& word : words) {
            word_to_frequency_in_document_[word][id] += word_frequency;
        }
        documents_.emplace(id, DocumentInfo(status, ratings));
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
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
            return {vector<string>(), documents_.at(document_id).getStatus()};
        }

        vector<string> matched_words;
        for (const auto& word : query_content.words_by_type.at(WordType::kPlus)) {
            if (word_to_frequency_in_document_.count(word) && word_to_frequency_in_document_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        return {matched_words, documents_.at(document_id).getStatus()};
    }

    template <typename DocumentFilter>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentFilter document_filter) const {
        const auto query_content = ParseQuery(raw_query);

        auto documents_data = FindAllDocuments(query_content, document_filter);
        sort(documents_data.begin(), documents_data.end(), [](const Document& lhs, const Document& rhs) {
            if (abs(lhs.relevance - rhs.relevance) < SearchServer::kDocumentRelevanceDelta) {
                return lhs.rating > rhs.rating;
            }
            return lhs.relevance > rhs.relevance;
        });
        if (static_cast<int>(documents_data.size()) > MAX_RESULT_DOCUMENT_COUNT) {
            documents_data.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return documents_data;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus document_status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [document_status](int /*id*/, DocumentStatus status, int /*rating*/) -> bool {
            return status == document_status;
        });
    }

private:
    class DocumentInfo {
        public:
            DocumentInfo(DocumentStatus status, const vector<int>& ratings) :
                status_(status),
                rating_(ComputeAverageRating(ratings)) {
            }

            DocumentStatus getStatus() const {
                return status_;
            }

            int getRating() const {
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
    static constexpr double kDocumentRelevanceDelta = 1e-6;

    map<string, map<int, double>> word_to_frequency_in_document_;
    set<string> stop_words_;

    enum class WordType {
        kPlus = 0,
        kMinus,
        kStop
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
                default: {
                    break;
                }
            }
        }
        return query_content;
    }

    WordType GetQueryWordType(const string& query_word) const {
        if (IsStopWord(query_word)) {
            return WordType::kStop;
        }
        if (query_word[0] == '-') {
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
            documents.push_back({document_id, document_relevance, documents_.at(document_id).getRating()});
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
                                    documents_.at(document_id).getStatus(), 
                                    documents_.at(document_id).getRating())) {
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

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus /*status*/, int /*rating*/) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}
