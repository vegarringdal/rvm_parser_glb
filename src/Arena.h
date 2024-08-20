/*
 * All code here is trimmed down/modified code from here:
 * https://github.com/cdyk/rvmparser
 */

#pragma once
#include <cstdint>
#include <cassert>
#include <cstring>
#include <stdlib.h>

void *xmalloc(size_t size);

struct Arena
{
    Arena() = default;
    Arena(const Arena &) = delete;
    Arena &operator=(const Arena &) = delete;
    ~Arena();

    uint8_t *first = nullptr;
    uint8_t *curr = nullptr;
    size_t fill = 0;
    size_t size = 0;

    void *alloc(size_t bytes);
    void *dup(const void *src, size_t bytes);
    void clear();

    template <typename T>
    T *alloc() { return new (alloc(sizeof(T))) T(); }
};
