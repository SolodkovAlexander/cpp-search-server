#include <cmath>

#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& text) : 
    SearchServer(::SplitIntoWords(text)) {
}

void SearchServer::SetStopWords(const string& text) {
    const auto stop_words = SplitIntoWords(text);
    stop_words_ = set(stop_words.begin(), stop_words.end());
}

void SearchServer::AddDocument(int document_id, 
                               const string& raw_document, 
                               DocumentStatus status, 
                               const vector<int>& ratings) {
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

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, 
                                                DocumentStatus document_status) const {
    return FindTopDocuments(raw_query, 
                            [document_status](int, DocumentStatus status, int) -> bool { return status == document_status; });
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, 
                                                                  int document_id) const {
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

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

int SearchServer::GetDocumentId(int index) const {
    if (index < 0 || index >= GetDocumentCount()) {
        throw out_of_range("document index is invalid"s);
    }

    return document_ids_.at(index);
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const auto& word : SplitIntoWords(text)) {
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

SearchServer::QueryContent SearchServer::ParseQuery(const string& text) const {
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

vector<string> SearchServer::SplitIntoWords(const string& text) {
    const auto words = ::SplitIntoWords(text);
    if (!CheckWordsAreCorrect(words)) {
        throw invalid_argument("text has words with invalid symbols"s);
    }
    return words;
}

SearchServer::WordType SearchServer::GetQueryWordType(const string& query_word) const {
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

double SearchServer::ComputeWordIDF(const string& word) const {
    const double word_idf = (word_to_frequency_in_document_.count(word) && !word_to_frequency_in_document_.at(word).empty()
                             ? log(static_cast<double>(documents_.size()) / static_cast<double>(word_to_frequency_in_document_.at(word).size()))
                             : 0.0);
    return word_idf;
}

void SearchServer::FilterDocumentsRelevanceByMinusWords(map<int, double>& documents_relevance,
                                                        const set<string>& minus_words) const {
    for (const auto& [word, word_document_info] : word_to_frequency_in_document_) {
        if (!minus_words.count(word)) continue;

        for (const auto& [document_id, word_document_tf] : word_document_info) {
            documents_relevance.erase(document_id);
        }
    }
}
