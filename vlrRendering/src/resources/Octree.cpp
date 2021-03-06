#include "resources/Octree.h"

#include "maths/Colour.h"
#include "maths/Normal.h"
#include "rendering/child_desc.h"
#include "util/CUDAUtil.h"

#include <stdio.h>
#include <queue>
#include <vector>
#include <map>
#include <stdint.h>

#include <glm/glm.hpp>

namespace vlr
{
	namespace rendering
	{
		// Sort two child descriptor blocks by depth
		bool sortByDepth(child_desc_builder_block& lhs, child_desc_builder_block& rhs)
		{
			return lhs.depth < rhs.depth;
		}

		// Generate a node of tree
		void genNode(std::vector<child_desc_builder_block>&,
			point_test_func&,
			glm::vec3 min, glm::vec3 max, int32_t depth,
			int32_t max_depth, int32_t block_id, int32_t idx,
			int& child_count);

		int32_t genOctree(int32_t** ret, int32_t max_depth,
			point_test_func& test_point_func,
			const glm::vec3& min, const glm::vec3& max)
		{
			const int invalid_block_id = ((1 << 24) - 1);
			
			const int raw_attachment_size_ints = 3;
			const int raw_attachment_size = raw_attachment_size_ints * sizeof(int32_t);

			// Check for existence of root and immediately return if not
			raw_attachment_uncompressed shading_attributes;
			if (!test_point_func(min, max, shading_attributes))
			{
				*ret = nullptr;
				return 0;
			}

			// Generated blocks of child descriptors
			std::vector<child_desc_builder_block> child_desc_builder_blocks;

			// Generate tree
			// Create root child descriptor block
			child_desc_builder_block root;
			root.id = 0;
			root.parent_block_id = invalid_block_id;
			root.count = 1;
			root.depth = 0;
			root.child_desc_builder[0].shading_attributes[0] = shading_attributes;

			// Add to collection
			child_desc_builder_blocks.push_back(root);

			int child_count = 0;

			// Start generation at root
			genNode(child_desc_builder_blocks, test_point_func, min, max,
				0, max_depth, 0, 0, child_count);
			
			// Sort tree by depth
			std::sort(child_desc_builder_blocks.begin(), child_desc_builder_blocks.end(), sortByDepth);

			// A map of (id -> pointer) for child descriptor blocks
			std::map<int32_t, pointer_desc> child_desc_builder_map;

			// A map of (freespace block -> first free space)
			std::map<int32_t, int32_t> first_freespace;

			// Iterate through and calculate pointers
			// Amount of space to reserve for far pointers (in child descriptors, not bytes)
			const int32_t chunk_size = 0x4000;
			const int32_t reserved_size = 0x2000;
			const int32_t remaining_size = chunk_size - reserved_size;

			// Initial pointer += 1 to leave space for info block pointer
			uintptr_t size = 0;

			std::vector<uintptr_t> skipped_descs;

			for (auto it = child_desc_builder_blocks.begin(); it != child_desc_builder_blocks.end(); ++it)
			{
				pointer_desc pointer;
				pointer.far = false;

				// If this is in the space reserved for far pointers, skip to next chunk
				if ((size + it->count) % chunk_size >= remaining_size)
				{
					size = chunk_size * (size / chunk_size + 1);
				}

				// Skip a slot for the info block pointer if this is at an 8kB boundary
				uintptr_t boundary_pos = (size * child_desc_size) / 0x2000;
				uintptr_t boundary_pos_next = ((size + it->count) * child_desc_size) / 0x2000;

				if ((size * child_desc_size) % 0x2000 == 0 || boundary_pos < boundary_pos_next)
				{
					skipped_descs.push_back(boundary_pos_next * 0x2000);
					size = ((boundary_pos_next * 0x2000) / child_desc_size) + 1;
				}

				uintptr_t ptr = size;

				// If this isn't the root
				if (it != child_desc_builder_blocks.begin())
				{
					// TODO: use parent pointer instead of parent block pointer
					uintptr_t parent_ptr = child_desc_builder_map[it->parent_block_id].ptr;
					uintptr_t relative_ptr = ptr - parent_ptr;

					if (relative_ptr >= chunk_size)
					{
						uintptr_t chunk_id = parent_ptr / chunk_size;
						uintptr_t freespace_ptr = chunk_id * chunk_size + remaining_size;

						// Get first free space in parent chunk free space block
						if (first_freespace.count(chunk_id) == 0)
							first_freespace[chunk_id] = 0;

						int32_t freespace_idx = first_freespace[chunk_id]++;
						freespace_ptr += freespace_idx;

						assert(freespace_idx < reserved_size);

						pointer.far = true;
						pointer.far_ptr = freespace_ptr;
					}
				}

				pointer.ptr = ptr;

				child_desc_builder_map[it->id] = pointer;

				size += it->count;
			}
			
			// Skip to start of next chunk
			size = chunk_size * (size / chunk_size + 1);

			// Allocate memory for tree
			uintptr_t child_descs_size = size * child_desc_size;
			uintptr_t info_section_size = sizeof(info_section);
			uintptr_t raw_attachment_lookup_size = size * sizeof(uint32_t);

			uintptr_t info_section_loc = child_descs_size;
			uintptr_t raw_attachment_lookup_loc = info_section_loc + info_section_size;
			uintptr_t raw_attachments_loc = raw_attachment_lookup_loc + raw_attachment_lookup_size;

			uintptr_t data_size = child_descs_size +
								  info_section_size +
								  raw_attachment_lookup_size;

			std::vector<raw_attachment> raw_attachments;

			int32_t* data = (int32_t*)malloc(data_size);
			
			// Iterate through blocks a second time and write data
			for (auto it = child_desc_builder_blocks.begin(); it != child_desc_builder_blocks.end(); ++it)
			{
				uintptr_t cur_ptr = child_desc_builder_map[it->id].ptr;

				// For each child descriptor in block
				// Write it in reverse to match raycast
				for (int32_t i = it->count-1; i >= 0; --i)
				{
					int child_count = numberOfSetBits(it->child_desc_builder[i].child_mask);

					int32_t* cur_descriptor = data + cur_ptr * child_desc_size_ints;

					// Initialise new child descriptor
					int32_t desc = 0;
					
					// Get child pointer if this is a non-leaf
					uintptr_t child_ptr = 0;
					
					bool far_needed = false;
					uintptr_t far_ptr = 0;

					if ((it->child_desc_builder[i].non_leaf_mask & (1 << (7 - i))) != 0)
					{
						int32_t child_id = it->child_desc_builder[i].child_id;

						child_ptr = child_desc_builder_map[child_id].ptr - cur_ptr;

						if (child_desc_builder_map[child_id].far)
						{
							// Indicate that this is a far pointer
							far_needed = true;

							// Write the far pointer
							uintptr_t far_ptr_abs = child_desc_builder_map[child_id].far_ptr *
								child_desc_size_ints;
							far_ptr = child_desc_builder_map[child_id].far_ptr - cur_ptr;
							data[far_ptr_abs] = child_ptr;
						}
					}

					// Check child pointer is < 15 bits
					// (this should be ensured in the pointer mapping stage)
					if (!far_needed)
					{
						assert((uint32_t)child_ptr < chunk_size);
					
						// Write child pointer to descriptor
						// Child pointer is the relative pointer
						// between the parent (cur_ptr) and the child
						desc ^= child_ptr << 17;
					}
					else
					{
						assert((uint32_t)far_ptr < chunk_size);
					
						// Write child pointer to descriptor
						// Child pointer is the relative pointer
						// between the parent (cur_ptr) and the child
						desc ^= far_ptr << 17;

						int32_t test = ((uint32_t)desc) >> 17;

						// Set far pointer flag
						desc ^= 1 << 16;
					}

					// Write child mask and non-leaf mask to descriptor
					desc ^= it->child_desc_builder[i].child_mask << 8;
					desc ^= it->child_desc_builder[i].non_leaf_mask;

					// Write data
					cur_descriptor[0] = desc;

					// Write shading data for children
					size_t raw_attachment_offset = raw_attachments.size();

					raw_attachment_lookup* raw_attribute_lookup =
						(raw_attachment_lookup*)((int)data + raw_attachment_lookup_loc) + cur_ptr;

					raw_attribute_lookup->ptr = raw_attachments_loc + raw_attachment_offset * sizeof(raw_attachment);

					for (int j = 0; j < 8; ++j)
					{
						if ((it->child_desc_builder[i].child_mask & (1 << j)) == 0)
							continue;

						// Create raw attachment
						raw_attachment raw_attachment;

						pack_raw_attachment(it->child_desc_builder[i].shading_attributes[j], raw_attachment);

						// Push back raw attachments
						raw_attachments.push_back(raw_attachment);
					}

					// Increment pointer
					cur_ptr += 1;
				}
			}

			// Write info section pointers throughout child descriptors
			for (auto it = skipped_descs.begin(); it != skipped_descs.end(); ++it)
			{
				uintptr_t offset = *it;

				int32_t* info_ptr_loc = (int32_t*)((uintptr_t)data + offset);

				*info_ptr_loc = info_section_loc;
			}

			// Write info section
			info_section* info_sec = (info_section*)((uintptr_t)data + info_section_loc);
			info_sec->raw_lookup = raw_attachment_lookup_loc;

			// Combine raw attachments and data
			uintptr_t raw_attachments_size = raw_attachments.size() * sizeof(raw_attachment);
			uintptr_t total_size = data_size + raw_attachments_size;
			data = (int32_t*)realloc(data, total_size);

			memcpy((void*)((uintptr_t)data + raw_attachments_loc), raw_attachments.data(), raw_attachments_size);

			// Return pointer to head of tree
			*ret = data;

			// Return data size in bytes
			return total_size;
		}

		void genNode(std::vector<child_desc_builder_block>& child_desc_builder_blocks,
			point_test_func& test_point_func,
			glm::vec3 min, glm::vec3 max, int32_t depth, int32_t max_depth,
			int32_t block_id, int32_t idx, int& child_count)
		{
			child_desc_builder_blocks[block_id].child_desc_builder[idx].child_mask = 0;
			child_desc_builder_blocks[block_id].child_desc_builder[idx].non_leaf_mask = 0;

			glm::vec3 half = 0.5f * (max - min);

			int32_t nonleaf_count = 0;

			int child_id = 0;
			for (int32_t i = 0; i < 8; ++i)
			{
				bool leaf = (depth + 1) >= max_depth;

				int32_t x = (i & 1) >> 0;
				int32_t y = (i & 2) >> 1;
				int32_t z = (i & 4) >> 2;

				glm::vec3 new_min = min + glm::vec3(half.x * (float)x,
					half.y * (float)y, half.z * (float)z);
				glm::vec3 new_max = new_min + half;

				// Continue loop if this node is empty
				raw_attachment_uncompressed shading_attributes;
				if (!test_point_func(min, max, shading_attributes))
					continue;

				// Set colour
				child_desc_builder_blocks[block_id].child_desc_builder[idx].shading_attributes[i] = shading_attributes;

				// Set child bit
				child_desc_builder_blocks[block_id].child_desc_builder[idx].child_mask ^= 1 << (7 - i);

				// Continue with loop if this is a leaf
				if (leaf)
					continue;

				nonleaf_count++;

				// Set non-leaf bit
				child_desc_builder_blocks[block_id].child_desc_builder[idx].non_leaf_mask ^= 1 << (7 - i);

				// Increment child id
				child_id++;
			}

			// If there are non-leaves, set child ptr
			// and generate children
			if (child_desc_builder_blocks[block_id].child_desc_builder[idx].non_leaf_mask != 0)
			{
				child_desc_builder_block child_block;
				child_block.id = child_desc_builder_blocks.size();
				child_block.count = nonleaf_count;
				child_block.depth = depth + 1;
				
				child_block.parent_id = idx;
				child_block.parent_block_id = block_id;

				child_desc_builder_blocks[block_id].child_desc_builder[idx].child_id = child_block.id;

				// Warning: this can invalidate desc reference
				// We re-set it since it is still needed
				child_desc_builder_blocks.push_back(child_block);
				//desc = child_desc_builder_blocks[block_id].child_desc_builder[idx];

				int32_t nonleaves = 0;
				for (int32_t i = 0; i < 8; ++i)
				{
					assert(nonleaves <= nonleaf_count);

					// If this is a leaf, continue
					if ((child_desc_builder_blocks[block_id].child_desc_builder[idx].non_leaf_mask & (1 << (7 - i))) == 0)
						continue;

					nonleaves++;

					int32_t x = i & 1;
					int32_t y = (i & 2) >> 1;
					int32_t z = (i & 4) >> 2;

					glm::vec3 newMin = min + glm::vec3(half.x * (float)x,
						half.y * (float)y, half.z * (float)z);
					glm::vec3 newMax = newMin + half;

					genNode(child_desc_builder_blocks,
						test_point_func,
						newMin, newMax,
						depth + 1, max_depth,
						child_block.id, i, child_count);
				}
			}
		}
	}
}
