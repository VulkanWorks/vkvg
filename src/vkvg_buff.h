/*
 * Copyright (c) 2018 Jean-Philippe Bruyère <jp_bruyere@hotmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef VKVG_BUFF_H
#define VKVG_BUFF_H

#include <vulkan/vulkan.h>
#include "vkvg.h"
#include "vk_mem_alloc.h"

typedef struct vkvg_buff_t {
	VkvgDevice			pDev;
	VkBuffer			buffer;
	VmaAllocation		alloc;
	VmaAllocationInfo	allocInfo;
	VkDescriptorBufferInfo descriptor;
}vkvg_buff;

void vkvg_buffer_create			(VkvgDevice pDev, VkBufferUsageFlags usage,
									VmaMemoryUsage memoryPropertyFlags, VkDeviceSize size, vkvg_buff *buff);
void vkvg_buffer_destroy		(vkvg_buff* buff);

void vkvg_buffer_flush			(vkvg_buff* buff);
#endif
