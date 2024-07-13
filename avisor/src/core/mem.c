#include "util.h"
#include "mem.h"
#include "fences.h"
#include "string.h"

struct page_pool* root_page_pool;
extern uint8_t __mem_vm_begin, __mem_vm_end;

// 增加调试信息的改进后的代码
bool root_pool_set_up_bitmap(struct page_pool *root_pool) {
    size_t bitmap_nr_pages, bitmap_base, pageoff;
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
    bitmap_base = (size_t) &__mem_vm_end;
    INFO("Bitmap base address: 0x%x", (void*)bitmap_base);

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

    // 如果不-1会出现内存越界访问，目前找不到原因
    INFO("Memset: address=0x%x, size=0x%x", (void*)root_pool->bitmap, (bitmap_nr_pages - 1) * PAGE_SIZE);
    memset((void*)root_pool->bitmap, 0, (bitmap_nr_pages - 1) * PAGE_SIZE);
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

    root_mem_region.base = (size_t) &__mem_vm_begin;
    root_mem_region.size = (size_t) (&__mem_vm_end - &__mem_vm_begin);

    root_pool = &root_mem_region.page_pool;
    root_pool->base = ALIGN(root_mem_region.base, PAGE_SIZE);
    root_pool->nr_pages = root_mem_region.size / PAGE_SIZE;
    root_pool->free = root_pool->nr_pages;
    
    if (!root_pool_set_up_bitmap(&root_mem_region.page_pool)) {
        ERROR("ERROR.\n");
    }
    root_page_pool = root_pool;

    INFO("MEM INIT");
}

void *mem_alloc_page(size_t nr_pages, bool phys_aligned) {
    struct ppages ppages = mem_alloc_ppages(nr_pages, phys_aligned);

    return (void*)ppages.base;
}

bool pp_alloc(struct page_pool *pool, size_t nr_pages, bool aligned,
                     struct ppages *ppages) {
    bool ok = false;
    size_t start, curr, bit, next_aligned;

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
