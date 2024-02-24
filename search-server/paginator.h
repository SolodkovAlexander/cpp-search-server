#pragma once

#include <iostream>
#include <utility>
#include <vector>

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin_range, 
              Iterator end_range, 
              size_t page_size) {        
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
    std::vector<std::pair<Iterator, Iterator>> pages_;
}; 

template <typename Container>
auto Paginate(const Container& container, 
              size_t page_size) {
    return Paginator(container.begin(), container.end(), page_size);
}

template <typename Iterator>
std::ostream& operator<<(std::ostream& output, 
                         const std::pair<Iterator, Iterator>& page) {
    for (auto it = page.first; it != page.second; ++it) {
        std::cout << *it;
    }
    return output;
}
