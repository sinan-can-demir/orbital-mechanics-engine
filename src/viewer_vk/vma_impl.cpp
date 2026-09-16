// VMA ships as a single header; exactly one translation unit must define
// VMA_IMPLEMENTATION before including it, which compiles the library body
// here. Every other file just includes <vk_mem_alloc.h> normally to get the
// declarations.
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
