#pragma once

#include <cstddef>
#include <iterator>
#include <iostream>

template <typename Iterator>
void AdvanceImpl(Iterator& itr, ptrdiff_t n, std::input_iterator_tag) {
    // std::cout << "InputIterator used" << '\n';
    if (n >= 0) {
        while (n--) {
            ++itr;
        }
    }
}

template <typename Iterator>
void AdvanceImpl(Iterator& itr, ptrdiff_t n, std::forward_iterator_tag) {
    // std::cout << "ForwardIterator used" << '\n';
    if (n >= 0) {
        while (n--) {
            ++itr;
        }
    }
}

template <typename Iterator>
void AdvanceImpl(Iterator& itr, ptrdiff_t n, std::output_iterator_tag) {
    // std::cout << "OutputIterator used" << '\n';
    if (n >= 0) {
        while (n--) {
            ++itr;
        }
    }
}

template <typename Iterator>
void AdvanceImpl(Iterator& itr, ptrdiff_t n, std::bidirectional_iterator_tag) {
    // std::cout << "BidirectionalIterator used" << '\n';
    if (n >= 0) {
        while (n--) {
            ++itr;
        }
    } else {
        while (n++) {
            --itr;
        }
    }
}

template <typename Iterator>
void AdvanceImpl(Iterator& itr, ptrdiff_t n, std::random_access_iterator_tag) {
    // std::cout << "RandomAccessIterator used" << '\n';
    itr += n;
}

template <class Iterator>
void Advance(Iterator& iterator, ptrdiff_t n) {
    typename std::iterator_traits<Iterator>::iterator_category category;
    AdvanceImpl(iterator, n, category);
}
