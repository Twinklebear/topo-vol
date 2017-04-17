#pragma once

#include <vector>
#include <map>
#include <iterator>
#include "gl_core_4_5.h"

namespace glt {
	class Buffer;
	class BufferAllocator;
}

std::ostream& operator<<(std::ostream &os, const glt::Buffer &b);
std::ostream& operator<<(std::ostream &os, const glt::BufferAllocator &b);

namespace glt {
// An allocated sub buffer within some large buffer
struct SubBuffer {
	size_t offset, size;
	GLuint buffer;

	SubBuffer(size_t offset = 0, size_t size = 0, GLuint buf = 0);
	// Map this sub buffer and return a pointer to the mapped range
	void* map(GLenum target, GLenum access);
	// Unamp this sub buffer
	void unmap(GLenum target);
};

// Map multiple subbuffers, coalescing the mappings for subbuffers in the
// same buffer. Returns vector of pointers to the start of each mapped
// buffer, note that some mappings may overlap so you should stay within
// each subbuffer's size
// WARNING: You probably should NOT use GL_MAP_INVALIDATE_RANGE_BIT with this mapping
// since you might invalidate other buffers that are between the one's you've requested
std::vector<char*> map_multiple(const std::vector<SubBuffer> &buffers, GLenum access);
void unmap_multiple(const std::vector<SubBuffer> &buffers);

// A block of memory in the buffer
struct Block {
	size_t offset, size;
};

// A large buffer that can hand out sub buffers to satisfy allocation requests
class Buffer {
	size_t size;
	GLuint buffer;
	std::map<size_t, Block> freeb, used;

	friend std::ostream& ::operator<<(std::ostream &os, const glt::Buffer &b);
public:
	// Allocate a buffer with some capacity
	Buffer(size_t size);
	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;
	Buffer(Buffer &&b);
	~Buffer();
	// Check if the sub buffer was allocated from this block's data store
	bool contains(SubBuffer &b) const;
	// Allocate a sub buffer with some capacity, returns
	// true if the buffer was able to satisfy the request
	bool alloc(size_t sz, SubBuffer &buf, size_t align = 1);
	// Reallocate a sub buffer to some new (larger) capacity, returns true if the
	// buffer was able to meet the request. The buffer be realloc'd should
	// be one allocated in this buffer
	bool realloc(SubBuffer &b, size_t new_sz, size_t align = 1);
	// Free the block used by the sub buffer and merge and neighboring free blocks
	void free(SubBuffer &buf);
};

enum class BufAlignment {
	UNIFORM_BUFFER,
	TEXTURE_BUFFER,
	SHADER_STORAGE_BUFFER,
};

// A buffer allocator that will use Buffers to meet allocation requests. If the allocator
// runs out of free space in its buffers it will allocate another to meet demand
class BufferAllocator {
	size_t capacity;
	std::vector<Buffer> buffers;
	GLint ubo_alignment, tbo_alignment, ssbo_alignment;

	friend std::ostream& ::operator<<(std::ostream &os, const glt::BufferAllocator &b);
public:
	// Create a buffer allocator which will allocate memory in chunks of `capacity`
	BufferAllocator(size_t capacity);
	BufferAllocator(const BufferAllocator&) = delete;
	BufferAllocator& operator=(const BufferAllocator&) = delete;
	// Allocate a sub buffer of some size within some free space in the allocator's buffers
	SubBuffer alloc(size_t sz, size_t align = 1);
	// Allocate a sub buffer of some size using the GL buffer alignemnt
	SubBuffer alloc(size_t sz, const BufAlignment align);
	// Reallocate a sub buffer to some new (larger) capacity. If there's enough room after
	// the buffer in the parent it will simply be expanded otherwise the data
	// may be moved within the parent or to a new buffer in the allocator
	void realloc(SubBuffer &b, size_t new_sz, size_t align = 1);
	void realloc(SubBuffer &b, size_t new_sz, const BufAlignment align);
	// Free the sub buffer so that the used space may be re-used
	void free(SubBuffer &buf);
};
}
std::ostream& operator<<(std::ostream &os, const glt::SubBuffer &b);
std::ostream& operator<<(std::ostream &os, const glt::Block &b);

