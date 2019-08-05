#include "test.hpp"
#include <heap.hpp>
#include <vector>
#include <string.h>
static constexpr size_t PAGE_SIZE {4096};

template<size_t ALIGNMENT = 16>
class test_ctx
{
public:
    test_ctx(size_t size) :
        ptr_raw(new char [size + ALIGNMENT]),
        ptr_align(reinterpret_cast<char *>((reinterpret_cast<size_t>(ptr_raw) + ALIGNMENT - 1) & ~(ALIGNMENT - 1))),
        mem(reinterpret_cast<size_t>(ptr_align), size),
        heap(mem)
    {
    }

    ~test_ctx() { delete[] ptr_raw; }

    void *alloc(size_t size) { return heap.alloc(size); };
    void free(void *p) { heap.free(p); }

private:
    char *ptr_raw;
    char *ptr_align;
public:
    fixed_memory mem;
    first_fit_heap<ALIGNMENT> heap;

};

template<class CTX>
static bool check_alignment()
{
    CTX ctx(PAGE_SIZE);
    const size_t alignment {ctx.heap.alignment()};
    TRACE("alignment: %ld", alignment);

    void *ptr = ctx.alloc(alignment);
    size_t addr = reinterpret_cast<size_t>(ptr);

    return ((addr & (alignment - 1)) == 0);
}

template<size_t ALIGNMENT, class ON_PTR_ALLOC_FN, class ON_PTR_FREE_FN>
bool generic_alloc_and_free(ON_PTR_ALLOC_FN ptr_alloc_fn,
                            ON_PTR_FREE_FN ptr_free_fn,
                            size_t alloc_size = ALIGNMENT
                            )
{
    test_ctx<> ctx(32 * PAGE_SIZE);

    std::vector<void *> ptrs;

    size_t init_size {ctx.heap.free_mem()};
    void  *p {nullptr};

    for (unsigned pass = 0; pass < 10; pass++) {
        while((p = ctx.alloc(alloc_size))) {
            ptrs.push_back(p);
            if (TEST_FAILED == ptr_alloc_fn(p, alloc_size)) {
                return TEST_FAILED;
            }
        }

        if (ctx.heap.num_blocks() > 1) { return TEST_FAILED; }

        while (ptrs.size() > 0) {
            if (TEST_FAILED == ptr_free_fn(ptrs.back(), alloc_size)) {
                return TEST_FAILED;
            }

            ctx.free(ptrs.back());
            ptrs.pop_back();
        }

        if (ctx.heap.free_mem() != init_size) {
            return TEST_FAILED;
        }
    }

    return TEST_SUCCESS;
}

TEST_SUITE_START

TEST(zero_alloc_should_not_return_nullptr,
{
    test_ctx<> ctx(PAGE_SIZE);

    void *ptr = ctx.alloc(0);
    ASSERT(ptr != nullptr);

    return TEST_SUCCESS;
});

TEST(simple_alloc_and_free,
{
    test_ctx<> ctx(PAGE_SIZE);

    ctx.free(ctx.alloc(10));
    ASSERT(ctx.heap.num_blocks() == 1);

    return TEST_SUCCESS;
});

TEST(heap_alignment,
{
    ASSERT(check_alignment<test_ctx<16>>());
    ASSERT(check_alignment<test_ctx<32>>());
    ASSERT(check_alignment<test_ctx<64>>());
    ASSERT(check_alignment<test_ctx<128>>());
    ASSERT(check_alignment<test_ctx<256>>());
    ASSERT(check_alignment<test_ctx<1024>>());
    return TEST_SUCCESS;
});

TEST(linear_alloc_and_free,
{
    auto after_alloc = [](void *p, size_t size) {
        memset(p, 0xf, size);
        return TEST_SUCCESS;
    };

    auto before_free = [](void *p, size_t size) {
        char *ptr = reinterpret_cast<char *>(p);
        for (size_t i = 0; i < size; i++) {
            if (ptr[i] != 0xf) {
                TRACE("pointer mismatch at %zd: %c vs %c", i, ptr[i], 0xf);
                return TEST_FAILED;
            }
        }

        return TEST_SUCCESS;
    };

    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free));
    ASSERT(generic_alloc_and_free<32>(after_alloc, before_free));
    ASSERT(generic_alloc_and_free<64>(after_alloc, before_free));
    ASSERT(generic_alloc_and_free<256>(after_alloc, before_free));
    ASSERT(generic_alloc_and_free<1024>(after_alloc, before_free));
    ASSERT(generic_alloc_and_free<2048>(after_alloc, before_free));
    ASSERT(generic_alloc_and_free<4096>(after_alloc, before_free));

    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free, 32));
    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free, 64));
    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free, 128));
    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free, 256));

    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free, 31));
    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free, 60));
    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free, 129));
    ASSERT(generic_alloc_and_free<16>(after_alloc, before_free, 277));

    return TEST_SUCCESS;
});

TEST(heap_has_valid_default_alignment,
{
    __attribute__((aligned(HEAP_MIN_ALIGNMENT))) char buffer[1024];

    fixed_memory mem(size_t(buffer), 1024);
    first_fit_heap<> heap(mem);
    ASSERT(heap.alignment() == HEAP_MIN_ALIGNMENT);
    return TEST_SUCCESS;
});

TEST(integrity_check,
{
    __attribute__((aligned(HEAP_MIN_ALIGNMENT))) char buffer[1024];

    fixed_memory mem(size_t(buffer), 1024);
    first_fit_heap<> heap(mem);

    HEAP_UNUSED auto* p1 = heap.alloc(16);
    auto* p2 = heap.alloc(16);
    HEAP_UNUSED auto* p3 = heap.alloc(16);
    auto* p4 = heap.alloc(16);
    HEAP_UNUSED auto* p5 = heap.alloc(16);
    *((char*)p3 - 8) = 5;

    heap.free(p2);
    heap.free(p4);

    try {
        heap.check_integrity();
    } catch (std::exception&) {
        return TEST_SUCCESS;
    }
    return TEST_FAILED;
});

TEST(merging_works_without_losing_memory,
{
    __attribute__((aligned(HEAP_MIN_ALIGNMENT))) char buffer[1024];

    fixed_memory mem(size_t(buffer), 1024);
    first_fit_heap<> heap(mem);

    auto free_mem_begin {heap.free_mem()};
    ASSERT(heap.num_blocks() == 1);

    auto* p1 = heap.alloc(16);
    auto* p2 = heap.alloc(16);
    auto* p3 = heap.alloc(16);

    ASSERT(heap.num_blocks() == 1);

    heap.free(p2);
    ASSERT(heap.num_blocks() == 2);

    heap.free(p1);
    ASSERT(heap.num_blocks() == 2);

    heap.free(p3);
    ASSERT(heap.num_blocks() == 1);

    ASSERT(heap.free_mem() == free_mem_begin);

    return TEST_SUCCESS;
});


TEST_SUITE_END
