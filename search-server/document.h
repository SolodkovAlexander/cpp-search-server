#pragma once

#include <iostream>

using namespace std::literals;

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

struct Document {
    Document() = default;
    Document(int _id, 
             double _relevance, 
             int _rating);
    
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

std::ostream& operator<<(std::ostream& output, 
                         const Document& document);

void PrintDocument(const Document& document);
