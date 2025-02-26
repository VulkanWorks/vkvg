﻿/*
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

#include "vkvg_device_internal.h"
#include "vkh_queue.h"
#include "vkh_phyinfo.h"
#include "vk_mem_alloc.h"

VkvgDevice vkvg_device_create(VkSampleCountFlags samples, bool deferredResolve) {
	const char* enabledExts [10];
	uint32_t enabledExtsCount = 0, phyCount = 0;
	_instance_extensions_check_init ();

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	bool dbgUtilsSupported = _instance_extension_supported("VK_EXT_debug_utils");
	 if (dbgUtilsSupported)
		enabledExts[enabledExtsCount++] = "VK_EXT_debug_utils";
#endif
	if (_instance_extension_supported("VK_KHR_get_physical_device_properties2"))
		enabledExts[enabledExtsCount++] = "VK_KHR_get_physical_device_properties2";

	_instance_extensions_check_release();

	VkhApp app =  vkh_app_create(1, 1, "vkvg", 0, NULL, enabledExtsCount, enabledExts);

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	if (dbgUtilsSupported)
		vkh_app_enable_debug_messenger(app
								, VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
								, VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
								, NULL);
#endif
	VkhPhyInfo* phys = vkh_app_get_phyinfos (app, &phyCount, VK_NULL_HANDLE);
	if (phyCount == 0) {
		vkh_app_destroy (app);
		return NULL;
	}

	VkhPhyInfo pi = 0;
	if (!_try_get_phyinfo(phys, phyCount, VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, &pi))
		if (!_try_get_phyinfo(phys, phyCount, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, &pi))
			pi = phys[0];

	uint32_t qCount = 0;
	float qPriorities[] = {0.0};
	VkDeviceQueueCreateInfo pQueueInfos[] = { {0},{0},{0} };

	if (vkh_phyinfo_create_queues (pi, pi->gQueue, 1, qPriorities, &pQueueInfos[qCount]))
		qCount++;

	VkPhysicalDeviceFeatures enabledFeatures = { 0 
		//.fillModeNonSolid = true
	};

	enabledExtsCount=0;
	//https://vulkan.lunarg.com/doc/view/1.2.162.0/mac/1.2-extensions/vkspec.html#VK_KHR_portability_subset
	if (vkh_phyinfo_try_get_extension_properties(pi, "VK_KHR_portability_subset", NULL))
		enabledExts[enabledExtsCount++] = "VK_KHR_get_physical_device_properties2";

	VkDeviceCreateInfo device_info = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
									   .queueCreateInfoCount = qCount,
									   .pQueueCreateInfos = (VkDeviceQueueCreateInfo*)&pQueueInfos,
									   .enabledExtensionCount = enabledExtsCount,
									   .ppEnabledExtensionNames = enabledExts,
									   .pEnabledFeatures = &enabledFeatures};

	VkhDevice vkhd = vkh_device_create(app, pi, &device_info);

	VkvgDevice vkvgDev = vkvg_device_create_from_vk_multisample (
				vkh_app_get_inst(app),
				vkh_device_get_phy(vkhd),
				vkh_device_get_vkdev(vkhd),
				pi->gQueue, 0,
				samples, deferredResolve);

	vkvgDev->vkhDev = vkhd;

	vkh_app_free_phyinfos (phyCount, phys);

	return vkvgDev;
}
VkvgDevice vkvg_device_create_from_vk(VkInstance inst, VkPhysicalDevice phy, VkDevice vkdev, uint32_t qFamIdx, uint32_t qIndex)
{
	return vkvg_device_create_from_vk_multisample (inst,phy,vkdev,qFamIdx,qIndex, VK_SAMPLE_COUNT_1_BIT, false);
}
VkvgDevice vkvg_device_create_from_vk_multisample(VkInstance inst, VkPhysicalDevice phy, VkDevice vkdev, uint32_t qFamIdx, uint32_t qIndex, VkSampleCountFlags samples, bool deferredResolve)
{
	LOG(VKVG_LOG_INFO, "CREATE Device: qFam = %d; qIdx = %d\n", qFamIdx, qIndex);

	VkvgDevice dev = (vkvg_device*)calloc(1,sizeof(vkvg_device));

	dev->instance = inst;
	dev->hdpi	= 72;
	dev->vdpi	= 72;
	dev->samples= samples;
	dev->deferredResolve = deferredResolve;
	dev->vkDev	= vkdev;
	dev->phy	= phy;
	dev->status = VKVG_STATUS_SUCCESS;

#if VKVG_DBG_STATS
	dev->debug_stats = (vkvg_debug_stats_t) {0};
#endif	

	VkFormat format = FB_COLOR_FORMAT;

	_check_best_image_tiling(dev, format);
	if (dev->status != VKVG_STATUS_SUCCESS)		
		return dev;	

	if (!_init_function_pointers (dev)){
		dev->status = VKVG_STATUS_NULL_POINTER;
		return dev;
	}

	VkhPhyInfo phyInfos = vkh_phyinfo_create (dev->phy, NULL);

	dev->phyMemProps = phyInfos->memProps;
	dev->gQueue = vkh_queue_create ((VkhDevice)dev, qFamIdx, qIndex);
	MUTEX_INIT (&dev->gQMutex);

	vkh_phyinfo_destroy (phyInfos);

	VmaAllocatorCreateInfo allocatorInfo = {
		.physicalDevice = phy,
		.device = vkdev
	};
	vmaCreateAllocator(&allocatorInfo, &dev->allocator);

	dev->lastCtx= NULL;

	dev->cmdPool= vkh_cmd_pool_create		((VkhDevice)dev, dev->gQueue->familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	dev->cmd	= vkh_cmd_buff_create		((VkhDevice)dev, dev->cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	dev->fence	= vkh_fence_create_signaled ((VkhDevice)dev);

	_create_pipeline_cache		(dev);
	_init_fonts_cache			(dev);
	if (dev->deferredResolve || dev->samples == VK_SAMPLE_COUNT_1_BIT){
		dev->renderPass = _createRenderPassNoResolve (dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD);
		dev->renderPass_ClearStencil = _createRenderPassNoResolve (dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR);
		dev->renderPass_ClearAll = _createRenderPassNoResolve (dev, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);
	}else{
		dev->renderPass = _createRenderPassMS (dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD);
		dev->renderPass_ClearStencil = _createRenderPassMS (dev, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_CLEAR);
		dev->renderPass_ClearAll = _createRenderPassMS (dev, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR);
	}
	_createDescriptorSetLayout	(dev);
	_setupPipelines				(dev);

	_create_empty_texture		(dev, format, dev->supportedTiling);

	dev->references = 1;

#if defined(DEBUG) && defined (VKVG_DBG_UTILS)
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)dev->cmdPool, "Device Cmd Pool");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)dev->cmd, "Device Cmd Buff");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_FENCE, (uint64_t)dev->fence, "Device Fence");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)dev->renderPass, "RP load img/stencil");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)dev->renderPass_ClearStencil, "RP clear stencil");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_RENDER_PASS, (uint64_t)dev->renderPass_ClearAll, "RP clear all");

	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)dev->dslSrc, "DSLayout SOURCE");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)dev->dslFont, "DSLayout FONT");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)dev->dslGrad, "DSLayout GRADIENT");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)dev->pipelineLayout, "PLLayout dev");

#ifndef __APPLE__
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipelinePolyFill, "PL Poly fill");
#endif
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipelineClipping, "PL Clipping");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipe_OVER, "PL draw Over");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipe_SUB, "PL draw Substract");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_PIPELINE, (uint64_t)dev->pipe_CLEAR, "PL draw Clear");

	vkh_image_set_name(dev->emptyImg, "empty IMG");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)vkh_image_get_view(dev->emptyImg), "empty IMG VIEW");
	vkh_device_set_object_name((VkhDevice)dev, VK_OBJECT_TYPE_SAMPLER, (uint64_t)vkh_image_get_sampler(dev->emptyImg), "empty IMG SAMPLER");
#endif
	return dev;
}

void vkvg_device_destroy (VkvgDevice dev)
{
	dev->references--;
	if (dev->references > 0)
		return;

	LOG(VKVG_LOG_INFO, "DESTROY Device\n");

	vkh_image_destroy				(dev->emptyImg);

	vkDestroyDescriptorSetLayout	(dev->vkDev, dev->dslGrad,NULL);
	vkDestroyDescriptorSetLayout	(dev->vkDev, dev->dslFont,NULL);
	vkDestroyDescriptorSetLayout	(dev->vkDev, dev->dslSrc, NULL);
#ifndef __APPLE__
	vkDestroyPipeline				(dev->vkDev, dev->pipelinePolyFill, NULL);
#endif
	vkDestroyPipeline				(dev->vkDev, dev->pipelineClipping, NULL);

	vkDestroyPipeline				(dev->vkDev, dev->pipe_OVER,	NULL);
	vkDestroyPipeline				(dev->vkDev, dev->pipe_SUB,		NULL);
	vkDestroyPipeline				(dev->vkDev, dev->pipe_CLEAR,	NULL);

#ifdef VKVG_WIRED_DEBUG
	vkDestroyPipeline				(dev->vkDev, dev->pipelineWired, NULL);
	vkDestroyPipeline				(dev->vkDev, dev->pipelineLineList, NULL);
#endif

	vkDestroyPipelineLayout			(dev->vkDev, dev->pipelineLayout, NULL);
	vkDestroyPipelineCache			(dev->vkDev, dev->pipelineCache, NULL);
	vkDestroyRenderPass				(dev->vkDev, dev->renderPass, NULL);
	vkDestroyRenderPass				(dev->vkDev, dev->renderPass_ClearStencil, NULL);
	vkDestroyRenderPass				(dev->vkDev, dev->renderPass_ClearAll, NULL);

	vkWaitForFences					(dev->vkDev, 1, &dev->fence, VK_TRUE, UINT64_MAX);

	vkDestroyFence					(dev->vkDev, dev->fence,NULL);
	vkFreeCommandBuffers			(dev->vkDev, dev->cmdPool, 1, &dev->cmd);
	vkDestroyCommandPool			(dev->vkDev, dev->cmdPool, NULL);

	vkh_queue_destroy(dev->gQueue);

	_destroy_font_cache(dev);

	vmaDestroyAllocator (dev->allocator);

	MUTEX_DESTROY (&dev->gQMutex);

	if (dev->vkhDev) {
		VkhApp app = vkh_device_get_app (dev->vkhDev);
		vkh_device_destroy (dev->vkhDev);
		vkh_app_destroy (app);
	}

	free(dev);
}

vkvg_status_t vkvg_device_status (VkvgDevice dev) {
	return dev->status;
}
VkvgDevice vkvg_device_reference (VkvgDevice dev) {
	dev->references++;
	return dev;
}
uint32_t vkvg_device_get_reference_count (VkvgDevice dev) {
	return dev->references;
}
void vkvg_device_set_dpy (VkvgDevice dev, int hdpy, int vdpy) {
	dev->hdpi = hdpy;
	dev->vdpi = vdpy;

	//TODO: reset font cache
}
void vkvg_device_get_dpy (VkvgDevice dev, int* hdpy, int* vdpy) {
	*hdpy = dev->hdpi;
	*vdpy = dev->vdpi;
}
#if VKVG_DBG_STATS
vkvg_debug_stats_t vkvg_device_get_stats (VkvgDevice dev) {
	return dev->debug_stats;
}
vkvg_debug_stats_t vkvg_device_reset_stats (VkvgDevice dev) {
	dev->debug_stats = (vkvg_debug_stats_t) {0};
}
#endif
