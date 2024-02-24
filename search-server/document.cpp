#include "document.h"

using namespace std;

Document::Document(int _id, 
                   double _relevance, 
                   int _rating) : 
    id(_id), 
    relevance(_relevance),
    rating(_rating) { 
}

ostream& operator<<(ostream& output, 
                    const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s;
    return output;
}

void PrintDocument(const Document& document) {
    cout << document << endl;
}
