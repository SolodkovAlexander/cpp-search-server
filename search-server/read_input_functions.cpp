#include <iostream>

#include "read_input_functions.h"

using namespace std;

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
