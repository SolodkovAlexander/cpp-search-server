#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
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
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const auto& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        const auto words = SplitIntoWordsNoStop(document);
        for (const auto& word : words) {
            if (!documents_[word].count(document_id)) {
                documents_[word][document_id] = ComputeWordTF(word, words);
            }
        }
        ++document_count_;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const auto query_content = ParseQuery(raw_query);

        auto documents = FindAllDocuments(query_content);
        sort(documents.begin(), documents.end(), [](const Document& lhs, const Document& rhs) {
            return lhs.relevance > rhs.relevance;
        });
        if (documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return documents;
    }

private:
    int document_count_ = 0;
    map<string, map<int, double>> documents_;
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

    vector<Document> FindAllDocuments(const QueryContent& query_content) const {
        const auto documents_relevance = ComputeDocumentsRelevance(query_content);

        vector<Document> documents;
        documents.reserve(documents_relevance.size());
        for (const auto [document_id, document_relevance] : documents_relevance) {
            documents.push_back({document_id, document_relevance});
        }
        return documents;
    }

    map<int, double> ComputeDocumentsRelevance(const QueryContent& query_content) const {
        if (!query_content.words_by_type.count(WordType::kPlus)
                || query_content.words_by_type.at(WordType::kPlus).empty()) {
            return {};
        }

        map<int, double> documents_relevance;
        for (const auto& word : query_content.words_by_type.at(WordType::kPlus)) {
            if (!documents_.count(word)) continue;

            const auto word_idf = ComputeWordIDF(word);
            for (const auto& [document_id, word_document_tf] : documents_.at(word)) {
                documents_relevance[document_id] += word_document_tf * word_idf;
            }
        }
        if (query_content.words_by_type.count(WordType::kMinus)) {
            FilterDocumentsRelevanceByMinusWords(documents_relevance,
                                                 query_content.words_by_type.at(WordType::kMinus));
        }
        return documents_relevance;
    }

    double ComputeWordIDF(const string& word) const {
        if (!documents_.count(word)
                || documents_.at(word).empty()) {
            return 0.0;
        }
        return log(static_cast<double>(document_count_) / documents_.at(word).size());
    }

    double static ComputeWordTF(const string& word, const vector<string>& words) {
        if (words.empty()) {
            return 0.0;
        }
        return count(words.begin(), words.end(), word) / static_cast<double>(words.size());
    }

    void FilterDocumentsRelevanceByMinusWords(map<int, double>& documents_relevance,
                                              const set<string>& minus_words) const {
        for (const auto& [word, word_document_info] : documents_) {
            if (!minus_words.count(word)) continue;

            for (const auto [document_id, word_document_tf] : word_document_info) {
                documents_relevance.erase(document_id);
            }
        }
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const auto document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const auto query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}
