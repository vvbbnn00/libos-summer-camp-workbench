#include "util.h"
#include "mem.h"
#include "fences.h"
#include "string.h"

struct page_pool* root_page_pool;
extern uint8_t __mem_vm_begin, __mem_vm_end;
struct shared_memory_device* shared_mem;

// 初始化共享内存
void shared_memory_init() {
    shared_mem = (struct shared_memory_device*)mem_alloc_page(NUM_PAGES(sizeof(struct shared_memory_device)), false);
    shared_mem->va = SHARED_MEM_BASE;
    shared_mem->size = SHARED_MEM_SIZE;
    
    // 将物理内存映射到虚拟地址空间
    shared_mem->pa = (paddr_t)mem_alloc_page(NUM_PAGES(shared_mem->size), false);
    if (shared_mem->pa == 0) {
        ERROR("Failed to allocate shared memory.");
        return;
    }

    // 测试用
    memcpy((char*)shared_mem->pa, "Ciallo!\0", 8);

    INFO("Shared memory initialized: va=0x%x, pa=0x%x, size=0x%x", shared_mem->va, shared_mem->pa, shared_mem->size);
}

bool root_pool_set_up_bitmap(struct page_pool *root_pool) {
    size_t bitmap_nr_pages,         // 用于记录内存位图所需的页面数 
           bitmap_base, pageoff;    // 用于记录位图的基地址和页偏移量
    struct ppages bitmap_pp;
    bitmap_t* root_bitmap;

    // 计算位图所需的页面数
    bitmap_nr_pages = root_pool->nr_pages / (8 * PAGE_SIZE) +
                      ((root_pool->nr_pages % (8 * PAGE_SIZE) != 0) ? 1 : 0);
    INFO("Bitmap pages required: 0x%x, 0x%x, 0x%x", root_pool->nr_pages, 8 * PAGE_SIZE, bitmap_nr_pages);

    // 检查页面池是否足够大以容纳位图
    if (root_pool->nr_pages <= bitmap_nr_pages) {
        ERROR("Not enough pages in root pool to accommodate bitmap.");
        return false;
    }

    // 设置位图基地址
    // BUG: 由于bitmap_base在虚拟机的最后开始分配内存，因此，必须为其预留空间
    // 以防止覆盖虚拟机的内存，如果不预留空间，会出现预料之外的问题（例如输出乱码）
    size_t bitmap_size = bitmap_nr_pages * PAGE_SIZE;
    bitmap_base = (size_t) &__mem_vm_end - bitmap_size;
    INFO("Bitmap base address: 0x%x, reserved size: 0x%x", bitmap_base, bitmap_size);

    // 获取位图的页面
    bitmap_pp = mem_ppages_get(bitmap_base, bitmap_nr_pages);
    if (bitmap_pp.base == 0) {
        // 内存分配失败，返回 false
        ERROR("Failed to allocate pages for bitmap.");
        return false;
    }
    INFO("Bitmap pages allocated: base=0x%x, nr_pages=0x%x", (void*)bitmap_pp.base, bitmap_pp.nr_pages);

    // 初始化位图
    root_bitmap = (bitmap_t*)bitmap_pp.base;
    root_pool->bitmap = root_bitmap;

    INFO("Memset: address=0x%x, size=0x%x", (void*)root_pool->bitmap, (bitmap_nr_pages) * PAGE_SIZE);
    memset((void*)root_pool->bitmap, 0, (bitmap_nr_pages) * PAGE_SIZE);
    INFO("Memset completed.");

    // 计算页偏移量并设置位图
    pageoff = NUM_PAGES(bitmap_pp.base - root_pool->base);
    INFO("Page offset: 0x%x", pageoff);

    bitmap_set_consecutive(root_pool->bitmap, pageoff, bitmap_pp.nr_pages);
    root_pool->free -= bitmap_pp.nr_pages;

    return true;
}

void mem_init() {
    static struct mem_region root_mem_region;
    struct page_pool *root_pool;

    INFO("mem_vm_begin: 0x%x, mem_vm_end: 0x%x, size= %u MB", &__mem_vm_begin, &__mem_vm_end, (&__mem_vm_end - &__mem_vm_begin) / 1024 / 1024);

    root_mem_region.base = (size_t) &__mem_vm_begin;
    root_mem_region.size = (size_t) (&__mem_vm_end - &__mem_vm_begin);

    root_pool = &root_mem_region.page_pool;
    root_pool->base = ALIGN(root_mem_region.base, PAGE_SIZE);
    root_pool->nr_pages = root_mem_region.size / PAGE_SIZE;
    root_pool->free = root_pool->nr_pages;

    INFO("Root pool: base=0x%x, nr_pages=0x%x", root_pool->base, root_pool->nr_pages);
    
    if (!root_pool_set_up_bitmap(&root_mem_region.page_pool)) {
        ERROR("ERROR.\n");
    }
    root_page_pool = root_pool;

    shared_memory_init();

    INFO("MEM INIT");
}

void *mem_alloc_page(size_t nr_pages, bool phys_aligned) {
    struct ppages ppages = mem_alloc_ppages(nr_pages, phys_aligned);

    return (void*)ppages.base;
}

bool pp_alloc(struct page_pool *pool, size_t nr_pages, bool aligned,
                     struct ppages *ppages) {
    bool ok = false;
    size_t start, curr, next_aligned;
    int bit; // bug: bit 若为 size_t 类型，会导致 bitmap_find_consec 函数返回值为 -1 时，无法进入 if 语句

    ppages->nr_pages = 0;
    if (nr_pages == 0) {
        return true;
    }

    spin_lock(&pool->lock);

    /**
     *  If we need a contigous segment aligned to its size, lets start
     * at an already aligned index.
     */
    start = aligned ? pool->base / PAGE_SIZE % nr_pages : 0;
    curr = pool->last + ((pool->last + start) % nr_pages);

    /**
     * Lets make two searches:
     *  - one starting from the last known free index.
     *  - in case this does not work, start from index 0.
     */
    for (size_t i = 0; i < 2 && !ok; i++) {
        while (pool->free != 0) {
            bit = bitmap_find_consec(pool->bitmap, pool->nr_pages, curr, nr_pages, false);

            if (bit < 0) {
                /**
                 * No num_page page sement was found. If this is the first 
                 * iteration set position to 0 to start next search from index 0.
                 */
                next_aligned = (nr_pages - ((pool->base / PAGE_SIZE) % nr_pages)) % nr_pages;
                curr = aligned ? next_aligned : 0;

                break;
            } else if (aligned && (((bit + start) % nr_pages) != 0)) {
                /**
                 *  If we're looking for an aligned segment and the found
                 * contigous segment is not aligned, start the search again
                 * from the last aligned index
                 */
                curr = bit + ((bit + start) % nr_pages);
            } else {
                /**
                 * We've found our pages. Fill output argument info, mark
                 * them as allocated, and update page pool bookkeeping.
                 */
                ppages->base = pool->base + (bit * PAGE_SIZE);
                ppages->nr_pages = nr_pages;
                bitmap_set_consecutive(pool->bitmap, bit, nr_pages);
                pool->free -= nr_pages;
                pool->last = bit + nr_pages;
                ok = true;

                break;
            }
        }
    }
    spin_unlock(&pool->lock);

    return ok;
}

struct ppages mem_alloc_ppages(size_t nr_pages, bool aligned) {
    struct ppages pages = {.nr_pages = 0};

    if (!pp_alloc(root_page_pool, nr_pages, aligned, &pages)) {
        ERROR("not enough ppages");
    }

    return pages;
}

bool mem_free_page(void *page, size_t nr_pages) {
    struct page_pool *pool = root_page_pool;
    size_t bit;

    // 检查输入参数
    if (!page || nr_pages == 0) {
        ERROR("Invalid arguments to mem_free_page.");
        return false;
    }

    spin_lock(&pool->lock);

    // 计算页面在位图中的偏移量
    bit = ((size_t)page - pool->base) / PAGE_SIZE;

    // 检查页面是否在合法范围内
    if (bit >= pool->nr_pages) {
        ERROR("Page address out of range.");
        spin_unlock(&pool->lock);
        return false;
    }

    // 释放连续的页面
    bitmap_clear_consecutive(pool->bitmap, bit, nr_pages);
    pool->free += nr_pages;

    spin_unlock(&pool->lock);

    INFO("Freed %u pages at address %x.", nr_pages, page);

    return true;
}

size_t mem_get_free_pages() {
    return root_page_pool->free;
}