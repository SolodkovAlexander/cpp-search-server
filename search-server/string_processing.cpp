#include "string_processing.h"

using namespace std;

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
