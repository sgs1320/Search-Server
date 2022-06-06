#pragma once

#include <iostream>

struct Document {
    Document();

    Document(int id, double relevance, int rating);

   /* Document(const Document& other) = default;

    Document(Document&& other) noexcept;

    Document& operator=(const Document &rhs) = default;

    Document& operator=(Document&& rhs)  noexcept; */



    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

std::ostream &operator<<(std::ostream &out, const Document &document);
