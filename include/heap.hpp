/*
 * MIT License

 * Copyright (c) 2016 Thomas Prescher

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <new>
#include <cstdio>
#include <algorithm>

class memory
{
public:
    virtual size_t base() = 0;
    virtual size_t size() = 0;
    virtual size_t end() = 0;
    virtual size_t alignment() = 0;
};

class fixed_memory : public memory
{
private:
    size_t base_;
    size_t size_;
    size_t alignment_;
public:
    fixed_memory(size_t base, size_t size, size_t alignment)
        : base_(base)
        , size_(size)
        , alignment_(alignment)
    {
    }

    virtual size_t base() { return base_; }
    virtual size_t size() { return size_; };
    virtual size_t end()  { return base_ + size_; }
    virtual size_t alignment() { return alignment_; }
};

class first_fit_heap
{
private:
    class header_free;
    class header_used;

    class footer {
    public:
        size_t size() const { return s; }

        void size(size_t size) { s = size; }
        header_free* header() { return reinterpret_cast<header_free*>(reinterpret_cast<char *>(this) + sizeof(footer) - size() - sizeof(header_used)); }

    private:
        size_t s;
    };

    class header_used
    {
    private:
        enum {
            PREV_FREE_MASK = ~(1ul << (sizeof(size_t) * 8 - 1)),
            THIS_FREE_MASK = ~(1ul << (sizeof(size_t) * 8 - 2)),
            SIZE_MASK      = (~PREV_FREE_MASK) | (~THIS_FREE_MASK),
            CANARY_VALUE   = 0x1337133713371337ul,
        };

        size_t raw;
        volatile size_t canary {CANARY_VALUE};

    public:
        header_used(const size_t size_) { size(size_); }

        size_t size() const { return raw & ~SIZE_MASK; }

        void size(size_t s)
        {
            assert((s & ~SIZE_MASK) == s);
            raw &= SIZE_MASK;
            raw |= s & ~SIZE_MASK;
        }

        bool prev_free() const { return raw & ~PREV_FREE_MASK; }

        void prev_free(bool val)
        {
            raw &= PREV_FREE_MASK;
            raw |= (~PREV_FREE_MASK) * val;
        }

        bool is_free() const { return raw & ~THIS_FREE_MASK; }

        void is_free(bool val)
        {
            raw &= THIS_FREE_MASK;
            raw |= (~THIS_FREE_MASK) * val;
        }

        bool canary_alive() { return canary == CANARY_VALUE; }

        header_used *following_block()
        {
            //XXX: sanity checking
            return reinterpret_cast<header_used *>(reinterpret_cast<char *>(this) + sizeof(header_used) + size());
        }

        header_used *preceding_block()
        {
            if (not prev_free()) {
                return nullptr;
            }

            //XXX: sanity checking
            footer *prev_footer = reinterpret_cast<footer *>(reinterpret_cast<char *>(this) - sizeof(footer));

            return prev_footer->header();
        }

        void *data_ptr() { return this+1; }
    };

    class header_free : public header_used
    {
    public:
        header_free(const size_t size) : header_used(size)
        {
            update_footer();
            is_free(true);
        }

        header_free *next() const { return next_; }
        void         next(header_free *val) { next_ = val; }

        footer *get_footer()
        {
            return reinterpret_cast<footer *>(reinterpret_cast<char *>(this) + sizeof(header_used) + size() - sizeof(footer));
        }

        void update_footer()
        {
            get_footer()->size(size());
        }

        header_free *next_ {nullptr};
    };

    class free_list_container
    {
    public:
        free_list_container(memory &mem_, header_free *root) : mem(mem_), list(root)
        {
            //assert(is_power_of_two(mem.alignment());
            assert(mem.size() != 0);
            assert((mem.base() & (mem.alignment() - 1)) == 0);
            assert((mem.base() + mem.size()) > mem.base());
        }

        class iterator
        {
        public:
            iterator(header_free *block_ = nullptr) : block(block_) {}

            iterator &operator++()
            {
                block = block->next();
                return *this;
            }

            header_free *operator*() const
            {
                return block;
            }

            bool operator!=(const iterator &other) const
            {
                return block != other.block;
            }

            bool operator==(const iterator &other) const
            {
                return not operator!=(other);
            }

        private:
            header_free *block;
        };

        iterator begin() const { return iterator(list); }
        iterator end()   const { return iterator(); }

    private:
        iterator position_for(header_free *val)
        {
            for (auto elem : *this) {
                if (not elem->next() or (elem->next() > val)) {
                    return elem < val ? iterator(elem) : iterator();
                }
            }

            return end();
        }

        iterator insert_after(header_free *val, iterator other)
        {
            // printf("insert %p at %p next: %p\n", val, *other, *other ? (*other)->next() : (header_free*)0xaffe);
            if (not val) {
                return {};
            }

            assert(val > *other);

            if (other == end()) {
                // insert at list head
                // printf("here\n");
                val->next(list);
                val->is_free(true);
                val->update_footer();
                list = val;
            } else {
                // insert block into chain
                auto *tmp = (*other)->next();
                val->next(tmp);
                val->is_free(true);
                val->update_footer();

                assert(val != *other);
                (*other)->next(val);
            }

            // update meta data of surrounding blocks
            auto       *following = val->following_block();
            const auto *preceding = val->preceding_block();

            if (following) {
                following->prev_free(true);
            }

            if (preceding and preceding->is_free()) {
                val->prev_free(true);
            }

            return {val};
        }

        iterator try_merge_back(iterator it)
        {
            auto *following = (*it)->following_block();
            if (following and following->is_free()) {
                auto *following_free = static_cast<header_free*>(following);
                (*it)->next(following_free->next());
                (*it)->size((*it)->size() + following_free->size() + sizeof(header_used));
                (*it)->update_footer();
            }
            return it;
        }

        iterator try_merge_front(iterator it)
        {
            if (not (*it)->prev_free()) {
                // printf("  %p prev not free\n", (*it)->preceding_block());
                return it;
            }

            auto *preceding = static_cast<header_free *>((*it)->preceding_block());
            if (not preceding) {
                // printf("  no preceding block\n");
                return it;
            }

            // printf("  merging %p with %p\n", preceding, this);

            assert(preceding->is_free());
            return try_merge_back({preceding});
        }

        const size_t min_block_size = sizeof(header_free) - sizeof(header_used) + sizeof(footer) ;

        size_t align(size_t size)
        {
            size_t real_size {(size + mem.alignment() - 1) & ~(mem.alignment() - 1)};
            return std::max(min_block_size, real_size);
        }

        bool fits(header_free &block, size_t size)
        {
            assert(size >= min_block_size);
            return block.size() >= size;
        }

        iterator first_free(size_t size, iterator &before)
        {
            iterator before_ = end();

            for (auto elem : *this) {
                assert(elem->canary_alive());

                if (fits(*elem, size)) {
                    before = before_;
                    return {elem};
                }
                before_ = iterator(elem);
            }

            return {};
        }

    public:
        iterator insert(header_free *val)
        {
            auto pos  = position_for(val);
            auto elem = insert_after(val, pos);
            return try_merge_front(try_merge_back(elem));
        }


        header_used *alloc(size_t size)
        {
            if (size == 0) {
                return nullptr;
            }

            size = align(size);

            iterator prev;
            auto it = first_free(size, prev);

            if (it == end()) {
                return nullptr;
            }

            auto  &block          = **it;
            size_t size_remaining = block.size() - size;

            if (size_remaining < (sizeof(header_free) + sizeof(footer))) {
                // remaining size cannot hold another block, use entire space
                size += size_remaining;
            } else {
                // split block into two
                block.size(size);
                block.update_footer();
                auto *new_block = new (block.following_block()) header_free(size_remaining - sizeof(header_used));
                new_block->next(block.next());
                new_block->prev_free(true);
                block.next(new_block);
            }

            if (*prev) {
                (*prev)->next(block.next());
            } else {
                list = block.next();
            }

            auto *following = block.following_block();
            if (following) {
                following->prev_free(false);
            }

            block.is_free(false);
            return &block;
        }

    private:
        memory &mem;
        header_free *list;
    };

private:
    free_list_container free_list;

public:
    first_fit_heap(memory &mem_) : free_list(mem_, new(reinterpret_cast<void *>(mem_.base())) header_free(mem_.size() - sizeof(header_used)))
    {
    }

    void dump()
    {
        for (auto block : free_list) {
            printf("[%p, %p) -> ", block, block->following_block());
        }
        printf("\n");
    }

    void *alloc(size_t size)
    {
        auto *block = free_list.alloc(size);
        return block ? block->data_ptr() : nullptr;
    }

    void free(void *p)
    {
        header_free *header {reinterpret_cast<header_free *>(reinterpret_cast<char *>(p) - sizeof(header_used))};

        assert(header->canary_alive());

        free_list.insert(header);
    }

    size_t num_blocks() const
    {
        size_t cnt {0};

        for (auto __attribute__((unused)) elem : free_list) {
            cnt++;
        }

        return cnt;
    }

    size_t free_mem() const
    {
        size_t size {0};

        for (auto  elem : free_list) {
            size += elem->size();
        }

        return size;
    }
};

