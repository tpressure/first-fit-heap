#include "test.hpp"
#include <heap.hpp>
#include <vector>
static constexpr size_t PAGE_SIZE {4096};

template<size_t ALIGNMENT = 16>
class test_ctx
{
public:
    test_ctx(size_t size) :
        ptr_raw(new char [size + ALIGNMENT]),
        ptr_align(reinterpret_cast<char *>((reinterpret_cast<size_t>(ptr_raw) + ALIGNMENT - 1) & ~(ALIGNMENT -1))),
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


TEST_SUITE_START

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
    static constexpr size_t alloc_size {16};
    test_ctx<> ctx(PAGE_SIZE);

    std::vector<void *> ptrs;

    size_t init_size {ctx.heap.free_mem()};
    void  *p {nullptr};

    for (unsigned pass = 0; pass < 10; pass++) {
        TRACE("PASS (%d/%d)", pass+1, 10);
        while((p = ctx.alloc(alloc_size))) {
            ptrs.push_back(p);
        }

        ASSERT(ctx.heap.num_blocks() == 0);

        while (ptrs.size() > 0) {
            ctx.free(ptrs.back());
            ptrs.pop_back();
        }

        ASSERT(ctx.heap.free_mem() == init_size);
    }

    return TEST_SUCCESS;
});

TEST_SUITE_END
