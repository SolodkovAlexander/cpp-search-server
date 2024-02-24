#pragma once

#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "document.h"
#include "string_processing.h"

using namespace std::literals;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& text);

public:
    void SetStopWords(const std::string& text);

    void AddDocument(int document_id, 
                     const std::string& raw_document, 
                     DocumentStatus status, 
                     const std::vector<int>& ratings);

    template <typename DocumentFilter>
    std::vector<Document> FindTopDocuments(const std::string& raw_query, 
                                           DocumentFilter document_filter) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query, 
                                           DocumentStatus document_status = DocumentStatus::ACTUAL) const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, 
                                                                       int document_id) const;

    int GetDocumentCount() const;

    int GetDocumentId(int index) const;

private:
    class DocumentInfo {
        public:
            DocumentInfo(DocumentStatus status, 
                         const std::vector<int>& ratings) :
                status_(status),
                rating_(ComputeAverageRating(ratings)) {
            }

            DocumentStatus GetStatus() const {
                return status_;
            }

            int GetRating() const {
                return rating_;
            }

        private: 
            DocumentStatus status_;
            int rating_;

        static int ComputeAverageRating(const std::vector<int>& ratings) {
            const int average_rating = (!ratings.empty() 
                                        ? accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size())
                                        : 0);
            return average_rating;
        }
    };
    std::map<int, DocumentInfo> documents_;

    std::vector<int> document_ids_;
    static constexpr double kDocumentRelevanceDelta = 1e-6;
    static constexpr int kMaxResultDocumentCount = 5;

    std::map<std::string, std::map<int, double>> word_to_frequency_in_document_;
    std::set<std::string> stop_words_;

    enum class WordType {
        kPlus = 0,
        kMinus,
        kStop,
        kInvalid
    };
    struct QueryContent {
        std::map<WordType, std::set<std::string>> words_by_type;
    };

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    bool IsStopWord(const std::string& word) const;

    QueryContent ParseQuery(const std::string& text) const;

    static std::vector<std::string> SplitIntoWords(const std::string& text);

    template <typename WordsContainer>
    static bool CheckWordsAreCorrect(const WordsContainer& words);

    WordType GetQueryWordType(const std::string& query_word) const;

    template <typename DocumentFilter>
    std::vector<Document> FindAllDocuments(const QueryContent& query_content, 
                                           DocumentFilter document_filter) const;

    template <typename DocumentFilter>
    std::map<int, double> ComputeDocumentsRelevance(const QueryContent& query_content, 
                                                    DocumentFilter document_filter) const;

    double ComputeWordIDF(const std::string& word) const;

    void FilterDocumentsRelevanceByMinusWords(std::map<int, double>& documents_relevance,
                                              const std::set<std::string>& minus_words) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) {
    if (!CheckWordsAreCorrect(stop_words)) {
        throw std::invalid_argument("words has invalid symbols"s);
    }
    stop_words_ = std::set(stop_words.begin(), stop_words.end());
}

template <typename DocumentFilter>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, 
                                                     DocumentFilter document_filter) const {
    const auto query_content = ParseQuery(raw_query);

    auto documents = FindAllDocuments(query_content, document_filter);
    sort(documents.begin(), documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < SearchServer::kDocumentRelevanceDelta) {
            return lhs.rating > rhs.rating;
        }
        return lhs.relevance > rhs.relevance;
    });
    if (static_cast<int>(documents.size()) > kMaxResultDocumentCount) {
        documents.resize(kMaxResultDocumentCount);
    }
    return documents;
}

template <typename WordsContainer>
bool SearchServer::CheckWordsAreCorrect(const WordsContainer& words) {
    for (const auto& word : words) {
        for (const auto ch : word) {
            if (ch >= '\000' && ch <= '\037') {
                return false;
            }
        }
    }
    return true;
}

template <typename DocumentFilter>
std::vector<Document> SearchServer::FindAllDocuments(const QueryContent& query_content, 
                                                     DocumentFilter document_filter) const {
    const auto documents_relevance = ComputeDocumentsRelevance(query_content, document_filter);

    std::vector<Document> documents;
    documents.reserve(documents_relevance.size());
    for (const auto& [document_id, document_relevance] : documents_relevance) {
        documents.push_back({document_id, 
                             document_relevance, 
                             documents_.at(document_id).GetRating()});
    }
    return documents;
}

template <typename DocumentFilter>
std::map<int, double> SearchServer::ComputeDocumentsRelevance(const QueryContent& query_content, 
                                                              DocumentFilter document_filter) const {
    if (!query_content.words_by_type.count(WordType::kPlus)
        || query_content.words_by_type.at(WordType::kPlus).empty()) {
        return {};
    }

    std::map<int, double> documents_relevance;
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
