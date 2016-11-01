#include "test.hpp"
#include <heap.hpp>

static constexpr size_t PAGE_SIZE {4096};

class TestCtx
{
public:
    TestCtx(size_t size, size_t alignment) :
        ptr_raw(new char [size + alignment]),
        ptr_align(reinterpret_cast<char *>((reinterpret_cast<size_t>(ptr_raw) + alignment - 1) & ~(alignment -1))),
        mem(reinterpret_cast<size_t>(ptr_align), size, alignment),
        heap(mem)
    {
    }

    ~TestCtx() { delete[] ptr_raw; }
    
    void *alloc(size_t size) { return heap.alloc(size); };
    void free(void *p) { heap.free(p); }


private:
    char *ptr_raw;
    char *ptr_align;
    Fixed_Memory mem;
public:
    FirstFirtHeap heap;

};

TEST_SUITE_START

TEST(simple_alloc_and_free,
{
    TestCtx ctx(PAGE_SIZE, 16);
    
    ctx.free(ctx.alloc(10));
    ASSERT(ctx.heap.num_blocks() == 1);

    return TEST_SUCCESS;
});

TEST(heap_alignment,
{
    size_t align_test[] = {16, 32, 64, 128, 1024};
    for (auto alignment : align_test) {
        TRACE("alignment: %ld", alignment);

        TestCtx ctx(4 * PAGE_SIZE, alignment);
        
        void *ptr = ctx.alloc(alignment);
        size_t addr = reinterpret_cast<size_t>(ptr);
        
        ASSERT((addr & (alignment - 1)) == 0);
    }

    return TEST_SUCCESS;
});

TEST_SUITE_END
