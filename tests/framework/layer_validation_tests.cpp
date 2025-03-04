/*
 * Copyright (c) 2015-2023 The Khronos Group Inc.
 * Copyright (c) 2015-2023 Valve Corporation
 * Copyright (c) 2015-2023 LunarG, Inc.
 * Copyright (c) 2015-2023 Google, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */
#include "utils/cast_utils.h"
#include "generated/enum_flag_bits.h"
#include "layer_validation_tests.h"
#include "utils/vk_layer_utils.h"

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
#include "wayland-client.h"
#endif

#include <array>

// Global list of sType,size identifiers
std::vector<std::pair<uint32_t, uint32_t>> custom_stype_info{};

VkFormat FindSupportedDepthOnlyFormat(VkPhysicalDevice phy) {
    const VkFormat ds_formats[] = {VK_FORMAT_D16_UNORM, VK_FORMAT_X8_D24_UNORM_PACK32, VK_FORMAT_D32_SFLOAT};
    for (uint32_t i = 0; i < size(ds_formats); ++i) {
        VkFormatProperties format_props;
        vk::GetPhysicalDeviceFormatProperties(phy, ds_formats[i], &format_props);

        if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return ds_formats[i];
        }
    }
    assert(false);  // Vulkan drivers are guaranteed to have at least one supported format
    return VK_FORMAT_UNDEFINED;
}

VkFormat FindSupportedStencilOnlyFormat(VkPhysicalDevice phy) {
    const VkFormat ds_formats[] = {VK_FORMAT_S8_UINT};
    for (uint32_t i = 0; i < size(ds_formats); ++i) {
        VkFormatProperties format_props;
        vk::GetPhysicalDeviceFormatProperties(phy, ds_formats[i], &format_props);

        if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return ds_formats[i];
        }
    }
    return VK_FORMAT_UNDEFINED;
}

VkFormat FindSupportedDepthStencilFormat(VkPhysicalDevice phy) {
    const VkFormat ds_formats[] = {VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT};
    for (uint32_t i = 0; i < size(ds_formats); ++i) {
        VkFormatProperties format_props;
        vk::GetPhysicalDeviceFormatProperties(phy, ds_formats[i], &format_props);

        if (format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return ds_formats[i];
        }
    }
    assert(false);  // Vulkan drivers are guaranteed to have at least one supported format
    return VK_FORMAT_UNDEFINED;
}

bool ImageFormatIsSupported(VkPhysicalDevice phy, VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
    VkFormatProperties format_props;
    vk::GetPhysicalDeviceFormatProperties(phy, format, &format_props);
    VkFormatFeatureFlags phy_features =
        (VK_IMAGE_TILING_OPTIMAL == tiling ? format_props.optimalTilingFeatures : format_props.linearTilingFeatures);
    return (0 != (phy_features & features));
}

bool ImageFormatAndFeaturesSupported(VkPhysicalDevice phy, VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features) {
    VkFormatProperties format_props;
    vk::GetPhysicalDeviceFormatProperties(phy, format, &format_props);
    VkFormatFeatureFlags phy_features =
        (VK_IMAGE_TILING_OPTIMAL == tiling ? format_props.optimalTilingFeatures : format_props.linearTilingFeatures);
    return (features == (phy_features & features));
}

bool ImageFormatAndFeaturesSupported(const VkInstance inst, const VkPhysicalDevice phy, const VkImageCreateInfo info,
                                     const VkFormatFeatureFlags features) {
    // Verify physical device support of format features
    if (!ImageFormatAndFeaturesSupported(phy, info.format, info.tiling, features)) {
        return false;
    }

    // Verify that PhysDevImageFormatProp() also claims support for the specific usage
    VkImageFormatProperties props;
    VkResult err =
        vk::GetPhysicalDeviceImageFormatProperties(phy, info.format, info.imageType, info.tiling, info.usage, info.flags, &props);
    if (VK_SUCCESS != err) {
        return false;
    }

#if 0  // Convinced this chunk doesn't currently add any additional info, but leaving in place because it may be
       // necessary with future extensions

    // Verify again using version 2, if supported, which *can* return more property data than the original...
    // (It's not clear that this is any more definitive than using the original version - but no harm)
    PFN_vkGetPhysicalDeviceImageFormatProperties2KHR p_GetPDIFP2KHR =
        (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)vk::GetInstanceProcAddr(inst,
                                                                                "vkGetPhysicalDeviceImageFormatProperties2KHR");
    if (NULL != p_GetPDIFP2KHR) {
        VkPhysicalDeviceImageFormatInfo2KHR fmt_info = LvlInitStruct<VkPhysicalDeviceImageFormatInfo2KHR>();
        fmt_info.format = info.format;
        fmt_info.type = info.imageType;
        fmt_info.tiling = info.tiling;
        fmt_info.usage = info.usage;
        fmt_info.flags = info.flags;

        VkImageFormatProperties2KHR fmt_props = LvlInitStruct<VkImageFormatProperties2KHR>();
        err = p_GetPDIFP2KHR(phy, &fmt_info, &fmt_props);
        if (VK_SUCCESS != err) {
            return false;
        }
    }
#endif

    return true;
}

bool BufferFormatAndFeaturesSupported(VkPhysicalDevice phy, VkFormat format, VkFormatFeatureFlags features) {
    VkFormatProperties format_props;
    vk::GetPhysicalDeviceFormatProperties(phy, format, &format_props);
    VkFormatFeatureFlags phy_features = format_props.bufferFeatures;
    return (features == (phy_features & features));
}

bool operator==(const VkDebugUtilsLabelEXT &rhs, const VkDebugUtilsLabelEXT &lhs) {
    bool is_equal = (rhs.color[0] == lhs.color[0]) && (rhs.color[1] == lhs.color[1]) && (rhs.color[2] == lhs.color[2]) &&
                    (rhs.color[3] == lhs.color[3]);
    if (is_equal) {
        if (rhs.pLabelName && lhs.pLabelName) {
            is_equal = (0 == strcmp(rhs.pLabelName, lhs.pLabelName));
        } else {
            is_equal = (rhs.pLabelName == nullptr) && (lhs.pLabelName == nullptr);
        }
    }
    return is_equal;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                                  const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
    auto *data = reinterpret_cast<DebugUtilsLabelCheckData *>(pUserData);
    data->callback(pCallbackData, data);
    return VK_FALSE;
}

#if GTEST_IS_THREADSAFE
void AddToCommandBuffer(ThreadTestData *data) {
    for (int i = 0; i < 80000; i++) {
        vk::CmdSetEvent(data->commandBuffer, data->event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        if (*data->bailout) {
            break;
        }
    }
}

void UpdateDescriptor(ThreadTestData *data) {
    VkDescriptorBufferInfo buffer_info = {};
    buffer_info.buffer = data->buffer;
    buffer_info.offset = 0;
    buffer_info.range = 1;

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = data->descriptorSet;
    descriptor_write.dstBinding = data->binding;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_write.pBufferInfo = &buffer_info;

    for (int i = 0; i < 80000; i++) {
        vk::UpdateDescriptorSets(data->device, 1, &descriptor_write, 0, NULL);
        if (*data->bailout) {
            break;
        }
    }
}

#endif  // GTEST_IS_THREADSAFE

bool ThreadTimeoutHelper::WaitForThreads(int timeout_in_seconds) {
    std::unique_lock lock(mutex_);
    return cv_.wait_for(lock, std::chrono::seconds{timeout_in_seconds}, [this] { return active_threads_ == 0; });
}

void ThreadTimeoutHelper::OnThreadDone() {
    bool last_worker = false;
    {
        std::lock_guard lock(mutex_);
        active_threads_--;
        assert(active_threads_ >= 0);
        if (!active_threads_) last_worker = true;
    }
    if (last_worker) cv_.notify_one();
}

void ReleaseNullFence(ThreadTestData *data) {
    for (int i = 0; i < 40000; i++) {
        vk::DestroyFence(data->device, VK_NULL_HANDLE, NULL);
        if (*data->bailout) {
            break;
        }
    }
}

void TestRenderPassCreate(ErrorMonitor *error_monitor, const vk_testing::Device &device, const VkRenderPassCreateInfo &create_info,
                          bool rp2_supported, const char *rp1_vuid, const char *rp2_vuid) {
    if (rp1_vuid) {
        // If the second VUID is not provided, set it equal to the first VUID.  In this way,
        // we can check both vkCreateRenderPass and vkCreateRenderPass2 with the same VUID
        // if rp2_supported is true;
        if (rp2_supported && !rp2_vuid) {
            rp2_vuid = rp1_vuid;
        }

        error_monitor->SetDesiredFailureMsg(kErrorBit, rp1_vuid);
        vk_testing::RenderPass rp(device, create_info);
        error_monitor->VerifyFound();
    }

    if (rp2_supported && rp2_vuid) {
        safe_VkRenderPassCreateInfo2 create_info2 = ConvertVkRenderPassCreateInfoToV2KHR(create_info);

        const auto vkCreateRenderPass2KHR =
            reinterpret_cast<PFN_vkCreateRenderPass2KHR>(vk::GetDeviceProcAddr(device, "vkCreateRenderPass2KHR"));
        // For API version < 1.2 where the extension was not enabled
        if (vkCreateRenderPass2KHR) {
            error_monitor->SetDesiredFailureMsg(kErrorBit, rp2_vuid);
            vk_testing::RenderPass rp2_khr(device, *create_info2.ptr(), true);
            error_monitor->VerifyFound();
        }

        const auto vkCreateRenderPass2 =
            reinterpret_cast<PFN_vkCreateRenderPass2>(vk::GetDeviceProcAddr(device, "vkCreateRenderPass2"));
        // For API version >= 1.2, try core entrypoint
        if (vkCreateRenderPass2) {
            error_monitor->SetDesiredFailureMsg(kErrorBit, rp2_vuid);
            vk_testing::RenderPass rp2_core(device, *create_info2.ptr(), false);
            error_monitor->VerifyFound();
        }
    }
}

void PositiveTestRenderPassCreate(ErrorMonitor *error_monitor, const vk_testing::Device &device,
                                  const VkRenderPassCreateInfo &create_info, bool rp2_supported) {
    vk_testing::RenderPass rp(device, create_info);
    if (rp2_supported) {
        vk_testing::RenderPass rp2(device, *ConvertVkRenderPassCreateInfoToV2KHR(create_info).ptr(), true);
    }
}

void PositiveTestRenderPass2KHRCreate(const vk_testing::Device &device, const VkRenderPassCreateInfo2KHR &create_info) {
    vk_testing::RenderPass rp(device, create_info, true);
}

void TestRenderPass2KHRCreate(ErrorMonitor &error_monitor, const vk_testing::Device &device,
                              const VkRenderPassCreateInfo2KHR &create_info, const std::initializer_list<const char *> &vuids) {
    for (auto vuid : vuids) {
        error_monitor.SetDesiredFailureMsg(kErrorBit, vuid);
    }
    vk_testing::RenderPass rp(device, create_info, true);
    error_monitor.VerifyFound();
}

void TestRenderPassBegin(ErrorMonitor *error_monitor, const VkDevice device, const VkCommandBuffer command_buffer,
                         const VkRenderPassBeginInfo *begin_info, bool rp2Supported, const char *rp1_vuid, const char *rp2_vuid) {
    VkCommandBufferBeginInfo cmd_begin_info = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                                               VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr};

    if (rp1_vuid) {
        vk::BeginCommandBuffer(command_buffer, &cmd_begin_info);
        error_monitor->SetDesiredFailureMsg(kErrorBit, rp1_vuid);
        vk::CmdBeginRenderPass(command_buffer, begin_info, VK_SUBPASS_CONTENTS_INLINE);
        error_monitor->VerifyFound();
        vk::ResetCommandBuffer(command_buffer, 0);
    }
    if (rp2Supported && rp2_vuid) {
        VkSubpassBeginInfoKHR subpass_begin_info = {VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO_KHR, nullptr, VK_SUBPASS_CONTENTS_INLINE};
        vk::BeginCommandBuffer(command_buffer, &cmd_begin_info);
        error_monitor->SetDesiredFailureMsg(kErrorBit, rp2_vuid);
        vk::CmdBeginRenderPass2KHR(command_buffer, begin_info, &subpass_begin_info);
        error_monitor->VerifyFound();
        vk::ResetCommandBuffer(command_buffer, 0);

        // For api version >= 1.2, try core entrypoint
        PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2 =
            (PFN_vkCmdBeginRenderPass2KHR)vk::GetDeviceProcAddr(device, "vkCmdBeginRenderPass2");
        if (vkCmdBeginRenderPass2) {
            vk::BeginCommandBuffer(command_buffer, &cmd_begin_info);
            error_monitor->SetDesiredFailureMsg(kErrorBit, rp2_vuid);
            vkCmdBeginRenderPass2(command_buffer, begin_info, &subpass_begin_info);
            error_monitor->VerifyFound();
            vk::ResetCommandBuffer(command_buffer, 0);
        }
    }
}

void ValidOwnershipTransferOp(ErrorMonitor *monitor, VkCommandBufferObj *cb, VkPipelineStageFlags src_stages,
                              VkPipelineStageFlags dst_stages, const VkBufferMemoryBarrier *buf_barrier,
                              const VkImageMemoryBarrier *img_barrier) {
    cb->begin();
    uint32_t num_buf_barrier = (buf_barrier) ? 1 : 0;
    uint32_t num_img_barrier = (img_barrier) ? 1 : 0;
    cb->PipelineBarrier(src_stages, dst_stages, 0, 0, nullptr, num_buf_barrier, buf_barrier, num_img_barrier, img_barrier);
    cb->end();
    cb->QueueCommandBuffer();  // Implicitly waits
}

void ValidOwnershipTransfer(ErrorMonitor *monitor, VkCommandBufferObj *cb_from, VkCommandBufferObj *cb_to,
                            VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages,
                            const VkBufferMemoryBarrier *buf_barrier, const VkImageMemoryBarrier *img_barrier) {
    ValidOwnershipTransferOp(monitor, cb_from, src_stages, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, buf_barrier, img_barrier);
    ValidOwnershipTransferOp(monitor, cb_to, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, dst_stages, buf_barrier, img_barrier);
}

void ValidOwnershipTransferOp(ErrorMonitor *monitor, VkCommandBufferObj *cb, const VkBufferMemoryBarrier2KHR *buf_barrier,
                              const VkImageMemoryBarrier2KHR *img_barrier) {
    cb->begin();
    auto dep_info = LvlInitStruct<VkDependencyInfoKHR>();
    dep_info.bufferMemoryBarrierCount = (buf_barrier) ? 1 : 0;
    dep_info.pBufferMemoryBarriers = buf_barrier;
    dep_info.imageMemoryBarrierCount = (img_barrier) ? 1 : 0;
    dep_info.pImageMemoryBarriers = img_barrier;
    cb->PipelineBarrier2KHR(&dep_info);
    cb->end();
    cb->QueueCommandBuffer();  // Implicitly waits
}

void ValidOwnershipTransfer(ErrorMonitor *monitor, VkCommandBufferObj *cb_from, VkCommandBufferObj *cb_to,
                            const VkBufferMemoryBarrier2KHR *buf_barrier, const VkImageMemoryBarrier2KHR *img_barrier) {
    VkBufferMemoryBarrier2KHR fixup_buf_barrier;
    VkImageMemoryBarrier2KHR fixup_img_barrier;
    if (buf_barrier) {
        fixup_buf_barrier = *buf_barrier;
        fixup_buf_barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
        fixup_buf_barrier.dstAccessMask = 0;
    }
    if (img_barrier) {
        fixup_img_barrier = *img_barrier;
        fixup_img_barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
        fixup_img_barrier.dstAccessMask = 0;
    }

    ValidOwnershipTransferOp(monitor, cb_from, buf_barrier ? &fixup_buf_barrier : nullptr,
                             img_barrier ? &fixup_img_barrier : nullptr);

    if (buf_barrier) {
        fixup_buf_barrier = *buf_barrier;
        fixup_buf_barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
        fixup_buf_barrier.srcAccessMask = 0;
    }
    if (img_barrier) {
        fixup_img_barrier = *img_barrier;
        fixup_img_barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE_KHR;
        fixup_img_barrier.srcAccessMask = 0;
    }
    ValidOwnershipTransferOp(monitor, cb_to, buf_barrier ? &fixup_buf_barrier : nullptr,
                             img_barrier ? &fixup_img_barrier : nullptr);
}

VkResult GPDIFPHelper(VkPhysicalDevice dev, const VkImageCreateInfo *ci, VkImageFormatProperties *limits) {
    VkImageFormatProperties tmp_limits;
    limits = limits ? limits : &tmp_limits;
    return vk::GetPhysicalDeviceImageFormatProperties(dev, ci->format, ci->imageType, ci->tiling, ci->usage, ci->flags, limits);
}

VkFormat FindFormatLinearWithoutMips(VkPhysicalDevice gpu, VkImageCreateInfo image_ci) {
    image_ci.tiling = VK_IMAGE_TILING_LINEAR;

    const VkFormat first_vk_format = static_cast<VkFormat>(1);
    const VkFormat last_vk_format = static_cast<VkFormat>(130);  // avoid compressed/feature protected, otherwise 184

    for (VkFormat format = first_vk_format; format <= last_vk_format; format = static_cast<VkFormat>(format + 1)) {
        image_ci.format = format;

        // WORKAROUND for profile and mock_icd not containing valid format limits yet
        VkFormatProperties format_props;
        vk::GetPhysicalDeviceFormatProperties(gpu, format, &format_props);
        const VkFormatFeatureFlags core_filter = 0x1FFF;
        const auto features = (image_ci.tiling == VK_IMAGE_TILING_LINEAR) ? format_props.linearTilingFeatures & core_filter
                                                                          : format_props.optimalTilingFeatures & core_filter;
        if (!(features & core_filter)) continue;

        VkImageFormatProperties img_limits;
        if (VK_SUCCESS == GPDIFPHelper(gpu, &image_ci, &img_limits) && img_limits.maxMipLevels == 1) return format;
    }

    return VK_FORMAT_UNDEFINED;
}

bool FindFormatWithoutSamples(VkPhysicalDevice gpu, VkImageCreateInfo &image_ci) {
    const VkFormat first_vk_format = static_cast<VkFormat>(1);
    const VkFormat last_vk_format = static_cast<VkFormat>(130);  // avoid compressed/feature protected, otherwise 184

    for (VkFormat format = first_vk_format; format <= last_vk_format; format = static_cast<VkFormat>(format + 1)) {
        image_ci.format = format;

        // WORKAROUND for profile and mock_icd not containing valid format limits yet
        VkFormatProperties format_props;
        vk::GetPhysicalDeviceFormatProperties(gpu, format, &format_props);
        const VkFormatFeatureFlags core_filter = 0x1FFF;
        const auto features = (image_ci.tiling == VK_IMAGE_TILING_LINEAR) ? format_props.linearTilingFeatures & core_filter
                                                                          : format_props.optimalTilingFeatures & core_filter;
        if (!(features & core_filter)) continue;

        for (VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_64_BIT; samples > 0;
             samples = static_cast<VkSampleCountFlagBits>(samples >> 1)) {
            image_ci.samples = samples;
            VkImageFormatProperties img_limits;
            if (VK_SUCCESS == GPDIFPHelper(gpu, &image_ci, &img_limits) && !(img_limits.sampleCounts & samples)) return true;
        }
    }

    return false;
}

bool FindUnsupportedImage(VkPhysicalDevice gpu, VkImageCreateInfo &image_ci) {
    const VkFormat first_vk_format = static_cast<VkFormat>(1);
    const VkFormat last_vk_format = static_cast<VkFormat>(130);  // avoid compressed/feature protected, otherwise 184

    constexpr std::array tilings = {VK_IMAGE_TILING_LINEAR, VK_IMAGE_TILING_OPTIMAL};
    for (const auto tiling : tilings) {
        image_ci.tiling = tiling;

        for (VkFormat format = first_vk_format; format <= last_vk_format; format = static_cast<VkFormat>(format + 1)) {
            image_ci.format = format;

            VkFormatProperties format_props;
            vk::GetPhysicalDeviceFormatProperties(gpu, format, &format_props);

            const VkFormatFeatureFlags core_filter = 0x1FFF;
            const auto features = (tiling == VK_IMAGE_TILING_LINEAR) ? format_props.linearTilingFeatures & core_filter
                                                                     : format_props.optimalTilingFeatures & core_filter;
            if (!(features & core_filter)) continue;  // We wand supported by features, but not by ImageFormatProperties

            // get as many usage flags as possible
            image_ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            if (features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) image_ci.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
            if (features & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) image_ci.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
            if (features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) image_ci.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            if (features & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
                image_ci.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

            VkImageFormatProperties img_limits;
            if (VK_ERROR_FORMAT_NOT_SUPPORTED == GPDIFPHelper(gpu, &image_ci, &img_limits)) {
                return true;
            }
        }
    }

    return false;
}

VkFormat FindFormatWithoutFeatures(VkPhysicalDevice gpu, VkImageTiling tiling, VkFormatFeatureFlags undesired_features) {
    const VkFormat first_vk_format = static_cast<VkFormat>(1);
    const VkFormat last_vk_format = static_cast<VkFormat>(130);  // avoid compressed/feature protected, otherwise 184
    VkFormat return_format = VK_FORMAT_UNDEFINED;
    for (VkFormat format = first_vk_format; format <= last_vk_format; format = static_cast<VkFormat>(format + 1)) {
        VkFormatProperties format_props;
        vk::GetPhysicalDeviceFormatProperties(gpu, format, &format_props);

        const auto features =
            (tiling == VK_IMAGE_TILING_LINEAR) ? format_props.linearTilingFeatures : format_props.optimalTilingFeatures;
        if ((features & undesired_features) == 0) {
            return_format = format;
            break;
        }
    }

    return return_format;
}

VkExternalMemoryHandleTypeFlags FindSupportedExternalMemoryHandleTypes(VkPhysicalDevice gpu,
                                                                       const VkBufferCreateInfo &buffer_create_info,
                                                                       VkExternalMemoryFeatureFlags requested_features) {
    auto external_info = LvlInitStruct<VkPhysicalDeviceExternalBufferInfo>();
    external_info.flags = buffer_create_info.flags;
    external_info.usage = buffer_create_info.usage;

    VkExternalMemoryHandleTypeFlags supported_types = 0;
    IterateFlags<VkExternalMemoryHandleTypeFlagBits>(
        AllVkExternalMemoryHandleTypeFlagBits, [&](VkExternalMemoryHandleTypeFlagBits flag) {
            external_info.handleType = flag;
            auto external_properties = LvlInitStruct<VkExternalBufferProperties>();
            vk::GetPhysicalDeviceExternalBufferProperties(gpu, &external_info, &external_properties);
            const auto external_features = external_properties.externalMemoryProperties.externalMemoryFeatures;
            if ((external_features & requested_features) == requested_features) {
                supported_types |= flag;
            }
        });
    return supported_types;
}

bool HandleTypeNeedsDedicatedAllocation(VkPhysicalDevice gpu, const VkBufferCreateInfo &buffer_create_info,
                                        VkExternalMemoryHandleTypeFlagBits handle_type) {
    auto external_info = LvlInitStruct<VkPhysicalDeviceExternalBufferInfo>();
    external_info.flags = buffer_create_info.flags;
    external_info.usage = buffer_create_info.usage;
    external_info.handleType = handle_type;

    auto external_properties = LvlInitStruct<VkExternalBufferProperties>();
    vk::GetPhysicalDeviceExternalBufferProperties(gpu, &external_info, &external_properties);

    const auto external_features = external_properties.externalMemoryProperties.externalMemoryFeatures;
    return (external_features & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;
}

VkExternalMemoryHandleTypeFlags FindSupportedExternalMemoryHandleTypes(VkPhysicalDevice gpu,
                                                                       const VkImageCreateInfo &image_create_info,
                                                                       VkExternalMemoryFeatureFlags requested_features) {
    auto external_info = LvlInitStruct<VkPhysicalDeviceExternalImageFormatInfo>();
    auto image_info = LvlInitStruct<VkPhysicalDeviceImageFormatInfo2>(&external_info);
    image_info.format = image_create_info.format;
    image_info.type = image_create_info.imageType;
    image_info.tiling = image_create_info.tiling;
    image_info.usage = image_create_info.usage;
    image_info.flags = image_create_info.flags;

    VkExternalMemoryHandleTypeFlags supported_types = 0;
    IterateFlags<VkExternalMemoryHandleTypeFlagBits>(
        AllVkExternalMemoryHandleTypeFlagBits, [&](VkExternalMemoryHandleTypeFlagBits flag) {
            external_info.handleType = flag;
            auto external_properties = LvlInitStruct<VkExternalImageFormatProperties>();
            auto image_properties = LvlInitStruct<VkImageFormatProperties2>(&external_properties);
            VkResult result = vk::GetPhysicalDeviceImageFormatProperties2(gpu, &image_info, &image_properties);
            const auto external_features = external_properties.externalMemoryProperties.externalMemoryFeatures;
            if (result == VK_SUCCESS && (external_features & requested_features) == requested_features) {
                supported_types |= flag;
            }
        });
    return supported_types;
}

bool HandleTypeNeedsDedicatedAllocation(VkPhysicalDevice gpu, const VkImageCreateInfo &image_create_info,
                                        VkExternalMemoryHandleTypeFlagBits handle_type) {
    auto external_info = LvlInitStruct<VkPhysicalDeviceExternalImageFormatInfo>();
    external_info.handleType = handle_type;
    auto image_info = LvlInitStruct<VkPhysicalDeviceImageFormatInfo2>(&external_info);
    image_info.format = image_create_info.format;
    image_info.type = image_create_info.imageType;
    image_info.tiling = image_create_info.tiling;
    image_info.usage = image_create_info.usage;
    image_info.flags = image_create_info.flags;

    auto external_properties = LvlInitStruct<VkExternalImageFormatProperties>();
    auto image_properties = LvlInitStruct<VkImageFormatProperties2>(&external_properties);
    VkResult result = vk::GetPhysicalDeviceImageFormatProperties2(gpu, &image_info, &image_properties);
    if (result != VK_SUCCESS) return false;

    const auto external_features = external_properties.externalMemoryProperties.externalMemoryFeatures;
    return (external_features & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) != 0;
}

VkExternalMemoryHandleTypeFlagsNV FindSupportedExternalMemoryHandleTypesNV(const VkLayerTest &test,
                                                                           const VkImageCreateInfo &image_create_info,
                                                                           VkExternalMemoryFeatureFlagsNV requested_features) {
    auto vkGetPhysicalDeviceExternalImageFormatPropertiesNV =
        test.GetInstanceProcAddr<PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV>(
            "vkGetPhysicalDeviceExternalImageFormatPropertiesNV");

    VkExternalMemoryHandleTypeFlagsNV supported_types = 0;
    IterateFlags<VkExternalMemoryHandleTypeFlagBitsNV>(
        AllVkExternalMemoryHandleTypeFlagBitsNV, [&](VkExternalMemoryHandleTypeFlagBitsNV flag) {
            VkExternalImageFormatPropertiesNV external_properties = {};
            VkResult result = vkGetPhysicalDeviceExternalImageFormatPropertiesNV(
                test.gpu(), image_create_info.format, image_create_info.imageType, image_create_info.tiling,
                image_create_info.usage, image_create_info.flags, flag, &external_properties);
            const auto external_features = external_properties.externalMemoryFeatures;
            if (result == VK_SUCCESS && (external_features & requested_features) == requested_features) {
                supported_types |= flag;
            }
        });
    return supported_types;
}

VkExternalFenceHandleTypeFlags FindSupportedExternalFenceHandleTypes(VkPhysicalDevice gpu,
                                                                     VkExternalFenceFeatureFlags requested_features) {
    VkExternalFenceHandleTypeFlags supported_types = 0;
    IterateFlags<VkExternalFenceHandleTypeFlagBits>(
        AllVkExternalFenceHandleTypeFlagBits, [&](VkExternalFenceHandleTypeFlagBits flag) {
            auto external_info = LvlInitStruct<VkPhysicalDeviceExternalFenceInfo>();
            external_info.handleType = flag;
            auto external_properties = LvlInitStruct<VkExternalFenceProperties>();
            vk::GetPhysicalDeviceExternalFenceProperties(gpu, &external_info, &external_properties);
            if ((external_properties.externalFenceFeatures & requested_features) == requested_features) {
                supported_types |= flag;
            }
        });
    return supported_types;
}

VkExternalSemaphoreHandleTypeFlags FindSupportedExternalSemaphoreHandleTypes(VkPhysicalDevice gpu,
                                                                             VkExternalSemaphoreFeatureFlags requested_features) {
    VkExternalSemaphoreHandleTypeFlags supported_types = 0;
    IterateFlags<VkExternalSemaphoreHandleTypeFlagBits>(
        AllVkExternalSemaphoreHandleTypeFlagBits, [&](VkExternalSemaphoreHandleTypeFlagBits flag) {
            auto external_info = LvlInitStruct<VkPhysicalDeviceExternalSemaphoreInfo>();
            external_info.handleType = flag;
            auto external_properties = LvlInitStruct<VkExternalSemaphoreProperties>();
            vk::GetPhysicalDeviceExternalSemaphoreProperties(gpu, &external_info, &external_properties);
            if ((external_properties.externalSemaphoreFeatures & requested_features) == requested_features) {
                supported_types |= flag;
            }
        });
    return supported_types;
}

VkExternalMemoryHandleTypeFlags GetCompatibleHandleTypes(VkPhysicalDevice gpu, const VkBufferCreateInfo &buffer_create_info,
                                                         VkExternalMemoryHandleTypeFlagBits handle_type) {
    auto external_info = LvlInitStruct<VkPhysicalDeviceExternalBufferInfo>();
    external_info.flags = buffer_create_info.flags;
    external_info.usage = buffer_create_info.usage;
    external_info.handleType = handle_type;
    auto external_buffer_properties = LvlInitStruct<VkExternalBufferProperties>();
    vk::GetPhysicalDeviceExternalBufferProperties(gpu, &external_info, &external_buffer_properties);
    return external_buffer_properties.externalMemoryProperties.compatibleHandleTypes;
}

VkExternalMemoryHandleTypeFlags GetCompatibleHandleTypes(VkPhysicalDevice gpu, const VkImageCreateInfo &image_create_info,
                                                         VkExternalMemoryHandleTypeFlagBits handle_type) {
    auto external_info = LvlInitStruct<VkPhysicalDeviceExternalImageFormatInfo>();
    external_info.handleType = handle_type;
    auto image_info = LvlInitStruct<VkPhysicalDeviceImageFormatInfo2>(&external_info);
    image_info.format = image_create_info.format;
    image_info.type = image_create_info.imageType;
    image_info.tiling = image_create_info.tiling;
    image_info.usage = image_create_info.usage;
    image_info.flags = image_create_info.flags;
    auto external_properties = LvlInitStruct<VkExternalImageFormatProperties>();
    auto image_properties = LvlInitStruct<VkImageFormatProperties2>(&external_properties);
    if (vk::GetPhysicalDeviceImageFormatProperties2(gpu, &image_info, &image_properties) != VK_SUCCESS) return 0;
    return external_properties.externalMemoryProperties.compatibleHandleTypes;
}

VkExternalFenceHandleTypeFlags GetCompatibleHandleTypes(VkPhysicalDevice gpu, VkExternalFenceHandleTypeFlagBits handle_type) {
    auto external_info = LvlInitStruct<VkPhysicalDeviceExternalFenceInfo>();
    external_info.handleType = handle_type;
    auto external_properties = LvlInitStruct<VkExternalFenceProperties>();
    vk::GetPhysicalDeviceExternalFenceProperties(gpu, &external_info, &external_properties);
    return external_properties.compatibleHandleTypes;
}

VkExternalSemaphoreHandleTypeFlags GetCompatibleHandleTypes(VkPhysicalDevice gpu,
                                                            VkExternalSemaphoreHandleTypeFlagBits handle_type) {
    auto external_info = LvlInitStruct<VkPhysicalDeviceExternalSemaphoreInfo>();
    external_info.handleType = handle_type;
    auto external_properties = LvlInitStruct<VkExternalSemaphoreProperties>();
    vk::GetPhysicalDeviceExternalSemaphoreProperties(gpu, &external_info, &external_properties);
    return external_properties.compatibleHandleTypes;
}

void AllocateDisjointMemory(VkDeviceObj *device, PFN_vkGetImageMemoryRequirements2KHR fp, VkImage mp_image,
                            VkDeviceMemory *mp_image_mem, VkImageAspectFlagBits plane) {
    VkImagePlaneMemoryRequirementsInfo image_plane_req = LvlInitStruct<VkImagePlaneMemoryRequirementsInfo>();
    image_plane_req.planeAspect = plane;

    VkImageMemoryRequirementsInfo2 mem_req_info2 = LvlInitStruct<VkImageMemoryRequirementsInfo2>(&image_plane_req);
    mem_req_info2.image = mp_image;

    VkMemoryRequirements2 mp_image_mem_reqs2 = LvlInitStruct<VkMemoryRequirements2>();

    fp(device->device(), &mem_req_info2, &mp_image_mem_reqs2);

    VkMemoryAllocateInfo mp_image_alloc_info = LvlInitStruct<VkMemoryAllocateInfo>();
    mp_image_alloc_info.allocationSize = mp_image_mem_reqs2.memoryRequirements.size;
    ASSERT_TRUE(device->phy().set_memory_type(mp_image_mem_reqs2.memoryRequirements.memoryTypeBits, &mp_image_alloc_info, 0));
    ASSERT_VK_SUCCESS(vk::AllocateMemory(device->device(), &mp_image_alloc_info, NULL, mp_image_mem));
}

void NegHeightViewportTests(VkDeviceObj *m_device, VkCommandBufferObj *m_commandBuffer, ErrorMonitor *m_errorMonitor) {
    const auto &limits = m_device->props.limits;

    m_commandBuffer->begin();

    using std::vector;
    struct TestCase {
        VkViewport vp;
        vector<std::string> vuids;
    };

    // not necessarily boundary values (unspecified cast rounding), but guaranteed to be over limit
    const auto one_before_min_h = NearestSmaller(-static_cast<float>(limits.maxViewportDimensions[1]));
    const auto one_past_max_h = NearestGreater(static_cast<float>(limits.maxViewportDimensions[1]));

    const auto min_bound = limits.viewportBoundsRange[0];
    const auto max_bound = limits.viewportBoundsRange[1];
    const auto one_before_min_bound = NearestSmaller(min_bound);
    const auto one_past_max_bound = NearestGreater(max_bound);

    const vector<TestCase> test_cases = {{{0.0, 0.0, 64.0, one_before_min_h, 0.0, 1.0}, {"VUID-VkViewport-height-01773"}},
                                         {{0.0, 0.0, 64.0, one_past_max_h, 0.0, 1.0}, {"VUID-VkViewport-height-01773"}},
                                         {{0.0, 0.0, 64.0, NAN, 0.0, 1.0}, {"VUID-VkViewport-height-01773"}},
                                         {{0.0, one_before_min_bound, 64.0, 1.0, 0.0, 1.0}, {"VUID-VkViewport-y-01775"}},
                                         {{0.0, one_past_max_bound, 64.0, -1.0, 0.0, 1.0}, {"VUID-VkViewport-y-01776"}},
                                         {{0.0, min_bound, 64.0, -1.0, 0.0, 1.0}, {"VUID-VkViewport-y-01777"}},
                                         {{0.0, max_bound, 64.0, 1.0, 0.0, 1.0}, {"VUID-VkViewport-y-01233"}}};

    for (const auto &test_case : test_cases) {
        for (const auto &vuid : test_case.vuids) {
            if (vuid == "VUID-Undefined")
                m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "is less than VkPhysicalDeviceLimits::viewportBoundsRange[0]");
            else
                m_errorMonitor->SetDesiredFailureMsg(kErrorBit, vuid);
        }
        vk::CmdSetViewport(m_commandBuffer->handle(), 0, 1, &test_case.vp);
        m_errorMonitor->VerifyFound();
    }
}

void CreateSamplerTest(VkLayerTest &test, const VkSamplerCreateInfo *create_info, const std::string &code) {
    if (code.length()) {
        test.Monitor().SetDesiredFailureMsg(kErrorBit | kWarningBit, code);
    }

    vk_testing::Sampler sampler(*test.DeviceObj(), *create_info);

    if (code.length()) {
        test.Monitor().VerifyFound();
    }
}

void CreateBufferTest(VkLayerTest &test, const VkBufferCreateInfo *create_info, const std::string &code) {
    if (code.length()) {
        test.Monitor().SetDesiredFailureMsg(kErrorBit, code);
    }
    vk_testing::Buffer buffer(*test.DeviceObj(), *create_info, vk_testing::no_mem);
    if (code.length()) {
        test.Monitor().VerifyFound();
    }
}

void CreateImageTest(VkLayerTest &test, const VkImageCreateInfo *create_info, const std::string &code) {
    if (code.length()) {
        test.Monitor().SetDesiredFailureMsg(kErrorBit, code);
    }
    vk_testing::Image image(*test.DeviceObj(), *create_info, vk_testing::no_mem);
    if (code.length()) {
        test.Monitor().VerifyFound();
    }
}

void CreateBufferViewTest(VkLayerTest &test, const VkBufferViewCreateInfo *create_info, const std::vector<std::string> &codes) {
    if (codes.size()) {
        std::for_each(codes.begin(), codes.end(), [&](const std::string &s) { test.Monitor().SetDesiredFailureMsg(kErrorBit, s); });
    }
    vk_testing::BufferView view(*test.DeviceObj(), *create_info);
    if (codes.size()) {
        test.Monitor().VerifyFound();
    }
}

void CreateImageViewTest(VkLayerTest &test, const VkImageViewCreateInfo *create_info, const std::string &code) {
    if (code.length()) {
        test.Monitor().SetDesiredFailureMsg(kErrorBit, code);
    }
    vk_testing::ImageView view(*test.DeviceObj(), *create_info);
    if (code.length()) {
        test.Monitor().VerifyFound();
    }
}

VkSamplerCreateInfo SafeSaneSamplerCreateInfo() {
    VkSamplerCreateInfo sampler_create_info = LvlInitStruct<VkSamplerCreateInfo>();
    sampler_create_info.magFilter = VK_FILTER_NEAREST;
    sampler_create_info.minFilter = VK_FILTER_NEAREST;
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_create_info.mipLodBias = 0.0;
    sampler_create_info.anisotropyEnable = VK_FALSE;
    sampler_create_info.maxAnisotropy = 1.0;
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_NEVER;
    sampler_create_info.minLod = 0.0;
    sampler_create_info.maxLod = 16.0;
    sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler_create_info.unnormalizedCoordinates = VK_FALSE;

    return sampler_create_info;
}

VkImageViewCreateInfo SafeSaneImageViewCreateInfo(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask) {
    VkImageViewCreateInfo image_view_create_info = LvlInitStruct<VkImageViewCreateInfo>();
    image_view_create_info.image = image;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = format;
    image_view_create_info.subresourceRange.layerCount = 1;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.aspectMask = aspect_mask;

    return image_view_create_info;
}

VkImageViewCreateInfo SafeSaneImageViewCreateInfo(const VkImageObj &image, VkFormat format, VkImageAspectFlags aspect_mask) {
    return SafeSaneImageViewCreateInfo(image.handle(), format, aspect_mask);
}

bool CheckSynchronization2SupportAndInitState(VkRenderFramework *framework) {
    PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2 =
        (PFN_vkGetPhysicalDeviceFeatures2)vk::GetInstanceProcAddr(framework->instance(), "vkGetPhysicalDeviceFeatures2");

    {
        auto sync2_features = LvlInitStruct<VkPhysicalDeviceSynchronization2FeaturesKHR>();
        auto features2 = LvlInitStruct<VkPhysicalDeviceFeatures2>(&sync2_features);
        vkGetPhysicalDeviceFeatures2(framework->gpu(), &features2);
        if (!sync2_features.synchronization2) {
            return false;
        }
    }

    auto sync2_features = LvlInitStruct<VkPhysicalDeviceSynchronization2FeaturesKHR>();
    sync2_features.synchronization2 = VK_TRUE;
    auto features2 = LvlInitStruct<VkPhysicalDeviceFeatures2>(&sync2_features);
    framework->InitState(nullptr, &features2, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    return true;
}

void VkLayerTest::VKTriangleTest(BsoFailSelect failCase) {
    ASSERT_TRUE(m_device && m_device->initialized());  // VKTriangleTest assumes Init() has finished

    ASSERT_NO_FATAL_FAILURE(InitViewport());

    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj ps(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineObj pipelineobj(m_device);
    pipelineobj.AddDefaultColorAttachment();
    pipelineobj.AddShader(&vs);
    pipelineobj.AddShader(&ps);

    bool failcase_needs_depth = false;  // to mark cases that need depth attachment

    VkBufferObj index_buffer;

    switch (failCase) {
        case BsoFailLineWidth: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_LINE_WIDTH);
            VkPipelineInputAssemblyStateCreateInfo ia_state = LvlInitStruct<VkPipelineInputAssemblyStateCreateInfo>();
            ia_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            pipelineobj.SetInputAssembly(&ia_state);
            break;
        }
        case BsoFailLineStipple: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_LINE_STIPPLE_EXT);
            VkPipelineInputAssemblyStateCreateInfo ia_state = LvlInitStruct<VkPipelineInputAssemblyStateCreateInfo>();
            ia_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            pipelineobj.SetInputAssembly(&ia_state);

            VkPipelineRasterizationLineStateCreateInfoEXT line_state =
                LvlInitStruct<VkPipelineRasterizationLineStateCreateInfoEXT>();
            line_state.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_BRESENHAM_EXT;
            line_state.stippledLineEnable = VK_TRUE;
            line_state.lineStippleFactor = 1;
            line_state.lineStipplePattern = 0;
            pipelineobj.SetLineState(&line_state);
            break;
        }
        case BsoFailDepthBias: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_DEPTH_BIAS);
            VkPipelineRasterizationStateCreateInfo rs_state = LvlInitStruct<VkPipelineRasterizationStateCreateInfo>();
            rs_state.depthBiasEnable = VK_TRUE;
            rs_state.lineWidth = 1.0f;
            pipelineobj.SetRasterization(&rs_state);
            break;
        }
        case BsoFailViewport: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_VIEWPORT);
            break;
        }
        case BsoFailScissor: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_SCISSOR);
            break;
        }
        case BsoFailBlend: {
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
            VkPipelineColorBlendAttachmentState att_state = {};
            att_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
            att_state.blendEnable = VK_TRUE;
            pipelineobj.AddColorAttachment(0, att_state);
            break;
        }
        case BsoFailDepthBounds: {
            failcase_needs_depth = true;
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
            break;
        }
        case BsoFailStencilReadMask: {
            failcase_needs_depth = true;
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
            break;
        }
        case BsoFailStencilWriteMask: {
            failcase_needs_depth = true;
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
            break;
        }
        case BsoFailStencilReference: {
            failcase_needs_depth = true;
            pipelineobj.MakeDynamic(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
            break;
        }

        case BsoFailIndexBuffer:
            break;
        case BsoFailIndexBufferBadSize:
        case BsoFailIndexBufferBadOffset:
        case BsoFailIndexBufferBadMapSize:
        case BsoFailIndexBufferBadMapOffset: {
            // Create an index buffer for these tests.
            // There is no need to populate it because we should bail before trying to draw.
            uint32_t const indices[] = {0};
            VkBufferCreateInfo buffer_info = LvlInitStruct<VkBufferCreateInfo>();
            buffer_info.size = 1024;
            buffer_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            buffer_info.queueFamilyIndexCount = 1;
            buffer_info.pQueueFamilyIndices = indices;
            index_buffer.init(*m_device, buffer_info, (VkMemoryPropertyFlags)VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        } break;
        case BsoFailCmdClearAttachments:
            break;
        case BsoFailNone:
            break;
        default:
            break;
    }

    VkDescriptorSetObj descriptorSet(m_device);

    VkImageView *depth_attachment = nullptr;
    if (failcase_needs_depth) {
        m_depth_stencil_fmt = FindSupportedDepthStencilFormat(gpu());

        m_depthStencil->Init(m_device, m_width, m_height, m_depth_stencil_fmt,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        depth_attachment = m_depthStencil->BindInfo();
    }

    ASSERT_NO_FATAL_FAILURE(InitRenderTarget(1, depth_attachment));
    m_commandBuffer->begin();

    GenericDrawPreparation(m_commandBuffer, pipelineobj, descriptorSet, failCase);

    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);

    // render triangle
    if (failCase == BsoFailIndexBuffer) {
        // Use DrawIndexed w/o an index buffer bound
        m_commandBuffer->DrawIndexed(3, 1, 0, 0, 0);
    } else if (failCase == BsoFailIndexBufferBadSize) {
        // Bind the index buffer and draw one too many indices
        m_commandBuffer->BindIndexBuffer(&index_buffer, 0, VK_INDEX_TYPE_UINT16);
        m_commandBuffer->DrawIndexed(513, 1, 0, 0, 0);
    } else if (failCase == BsoFailIndexBufferBadOffset) {
        // Bind the index buffer and draw one past the end of the buffer using the offset
        m_commandBuffer->BindIndexBuffer(&index_buffer, 0, VK_INDEX_TYPE_UINT16);
        m_commandBuffer->DrawIndexed(512, 1, 1, 0, 0);
    } else if (failCase == BsoFailIndexBufferBadMapSize) {
        // Bind the index buffer at the middle point and draw one too many indices
        m_commandBuffer->BindIndexBuffer(&index_buffer, 512, VK_INDEX_TYPE_UINT16);
        m_commandBuffer->DrawIndexed(257, 1, 0, 0, 0);
    } else if (failCase == BsoFailIndexBufferBadMapOffset) {
        // Bind the index buffer at the middle point and draw one past the end of the buffer
        m_commandBuffer->BindIndexBuffer(&index_buffer, 512, VK_INDEX_TYPE_UINT16);
        m_commandBuffer->DrawIndexed(256, 1, 1, 0, 0);
    } else {
        m_commandBuffer->Draw(3, 1, 0, 0);
    }

    if (failCase == BsoFailCmdClearAttachments) {
        VkClearAttachment color_attachment = {};
        color_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        color_attachment.colorAttachment = 2000000000;  // Someone who knew what they were doing would use 0 for the index;
        VkClearRect clear_rect = {{{0, 0}, {m_width, m_height}}, 0, 1};

        vk::CmdClearAttachments(m_commandBuffer->handle(), 1, &color_attachment, 1, &clear_rect);
    }

    // finalize recording of the command buffer
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();
    m_commandBuffer->QueueCommandBuffer(true);
    DestroyRenderTarget();
}

void VkLayerTest::GenericDrawPreparation(VkCommandBufferObj *commandBuffer, VkPipelineObj &pipelineobj,
                                         VkDescriptorSetObj &descriptorSet, BsoFailSelect failCase) {
    commandBuffer->ClearAllBuffers(m_renderTargets, m_clear_color, m_depthStencil, m_depth_clear_color, m_stencil_clear_color);

    commandBuffer->PrepareAttachments(m_renderTargets, m_depthStencil);
    // Make sure depthWriteEnable is set so that Depth fail test will work
    // correctly
    // Make sure stencilTestEnable is set so that Stencil fail test will work
    // correctly
    VkStencilOpState stencil = {};
    stencil.failOp = VK_STENCIL_OP_KEEP;
    stencil.passOp = VK_STENCIL_OP_KEEP;
    stencil.depthFailOp = VK_STENCIL_OP_KEEP;
    stencil.compareOp = VK_COMPARE_OP_NEVER;

    VkPipelineDepthStencilStateCreateInfo ds_ci = LvlInitStruct<VkPipelineDepthStencilStateCreateInfo>();
    ds_ci.depthTestEnable = VK_FALSE;
    ds_ci.depthWriteEnable = VK_TRUE;
    ds_ci.depthCompareOp = VK_COMPARE_OP_NEVER;
    ds_ci.depthBoundsTestEnable = VK_FALSE;
    if (failCase == BsoFailDepthBounds) {
        ds_ci.depthBoundsTestEnable = VK_TRUE;
        ds_ci.maxDepthBounds = 0.0f;
        ds_ci.minDepthBounds = 0.0f;
    }
    ds_ci.stencilTestEnable = VK_TRUE;
    ds_ci.front = stencil;
    ds_ci.back = stencil;

    pipelineobj.SetDepthStencil(&ds_ci);
    pipelineobj.SetViewport(m_viewports);
    pipelineobj.SetScissor(m_scissors);
    descriptorSet.CreateVKDescriptorSet(commandBuffer);
    VkResult err = pipelineobj.CreateVKPipeline(descriptorSet.GetPipelineLayout(), renderPass());
    ASSERT_VK_SUCCESS(err);
    vk::CmdBindPipeline(commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineobj.handle());
    commandBuffer->BindDescriptorSet(descriptorSet);
}

void VkLayerTest::Init(VkPhysicalDeviceFeatures *features, VkPhysicalDeviceFeatures2 *features2,
                       const VkCommandPoolCreateFlags flags, void *instance_pnext) {
    InitFramework(m_errorMonitor, instance_pnext);
    InitState(features, features2, flags);
}

VkCommandBufferObj *VkLayerTest::CommandBuffer() { return m_commandBuffer; }

VkLayerTest::VkLayerTest() {
#if !defined(VK_USE_PLATFORM_ANDROID_KHR)
    m_instance_extension_names.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#else
    m_instance_extension_names.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

    instance_layers_.push_back(kValidationLayerName);

    if (InstanceLayerSupported("VK_LAYER_LUNARG_device_profile_api")) {
        instance_layers_.push_back("VK_LAYER_LUNARG_device_profile_api");
    }

    if (InstanceLayerSupported(kSynchronization2LayerName)) {
        instance_layers_.push_back(kSynchronization2LayerName);
    }

    app_info_ = LvlInitStruct<VkApplicationInfo>();
    app_info_.pApplicationName = "layer_tests";
    app_info_.applicationVersion = 1;
    app_info_.pEngineName = "unittest";
    app_info_.engineVersion = 1;
    app_info_.apiVersion = VK_API_VERSION_1_0;

    // Find out what version the instance supports and record the default target instance
    auto enumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)vk::GetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
    if (enumerateInstanceVersion) {
        enumerateInstanceVersion(&m_instance_api_version);
    } else {
        m_instance_api_version = VK_API_VERSION_1_0;
    }
    m_target_api_version = app_info_.apiVersion;
}

void VkLayerTest::AddSurfaceExtension() {
    AddRequiredExtensions(VK_KHR_SURFACE_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    AddWsiExtensions(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    AddWsiExtensions(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    AddWsiExtensions(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    AddWsiExtensions(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
    AddWsiExtensions(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
}

void VkLayerTest::SetTargetApiVersion(uint32_t target_api_version) {
    if (target_api_version == 0) target_api_version = VK_API_VERSION_1_0;
    m_attempted_api_version = target_api_version;  // used to know if request failed

    m_target_api_version = std::min(target_api_version, m_instance_api_version);
    app_info_.apiVersion = m_target_api_version;
}

uint32_t VkLayerTest::DeviceValidationVersion() const {
    // The validation layers assume the version we are validating to is the apiVersion unless the device apiVersion is lower
    return std::min(m_target_api_version, physDevProps().apiVersion);
}

template <>
VkPhysicalDeviceFeatures2 VkLayerTest::GetPhysicalDeviceFeatures2(VkPhysicalDeviceFeatures2 &features2) {
    if (DeviceValidationVersion() >= VK_API_VERSION_1_1) {
        vk::GetPhysicalDeviceFeatures2(gpu(), &features2);
    } else {
        auto vkGetPhysicalDeviceFeatures2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(
            vk::GetInstanceProcAddr(instance(), "vkGetPhysicalDeviceFeatures2KHR"));
        assert(vkGetPhysicalDeviceFeatures2KHR);
        vkGetPhysicalDeviceFeatures2KHR(gpu(), &features2);
    }
    return features2;
}

template <>
VkPhysicalDeviceProperties2 VkLayerTest::GetPhysicalDeviceProperties2(VkPhysicalDeviceProperties2 &props2) {
    if (DeviceValidationVersion() >= VK_API_VERSION_1_1) {
        vk::GetPhysicalDeviceProperties2(gpu(), &props2);
    } else {
        auto vkGetPhysicalDeviceProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(
            vk::GetInstanceProcAddr(instance(), "vkGetPhysicalDeviceProperties2KHR"));
        assert(vkGetPhysicalDeviceProperties2KHR);
        vkGetPhysicalDeviceProperties2KHR(gpu(), &props2);
    }
    return props2;
}

bool VkLayerTest::IsDriver(VkDriverId driver_id) {
    if (VkRenderFramework::IgnoreDisableChecks()) {
        return false;
    } else {
        auto driver_properties = LvlInitStruct<VkPhysicalDeviceDriverProperties>();
        auto physical_device_properties2 = LvlInitStruct<VkPhysicalDeviceProperties2>(&driver_properties);
        GetPhysicalDeviceProperties2(physical_device_properties2);
        return (driver_properties.driverID == driver_id);
    }
}

bool VkLayerTest::LoadDeviceProfileLayer(
    PFN_vkSetPhysicalDeviceFormatPropertiesEXT &fpvkSetPhysicalDeviceFormatPropertiesEXT,
    PFN_vkGetOriginalPhysicalDeviceFormatPropertiesEXT &fpvkGetOriginalPhysicalDeviceFormatPropertiesEXT) {
    if (IsPlatform(kMockICD)) {
        printf("Device Profile layer is for real GPU, if using MockICD with profiles, just adjust the profile json file instead\n");
        return false;
    }

    // Load required functions
    fpvkSetPhysicalDeviceFormatPropertiesEXT =
        (PFN_vkSetPhysicalDeviceFormatPropertiesEXT)vk::GetInstanceProcAddr(instance(), "vkSetPhysicalDeviceFormatPropertiesEXT");
    fpvkGetOriginalPhysicalDeviceFormatPropertiesEXT = (PFN_vkGetOriginalPhysicalDeviceFormatPropertiesEXT)vk::GetInstanceProcAddr(
        instance(), "vkGetOriginalPhysicalDeviceFormatPropertiesEXT");

    if (!(fpvkSetPhysicalDeviceFormatPropertiesEXT) || !(fpvkGetOriginalPhysicalDeviceFormatPropertiesEXT)) {
        printf(
            "Can't find device_profile_api functions; make sure VK_LAYER_PATH is set correctly to where the validation layers "
            "are built, the device profile layer should be in the same directory.\n");
        return false;
    }

    return true;
}

bool VkLayerTest::LoadDeviceProfileLayer(
    PFN_vkSetPhysicalDeviceFormatProperties2EXT &fpvkSetPhysicalDeviceFormatProperties2EXT,
    PFN_vkGetOriginalPhysicalDeviceFormatProperties2EXT &fpvkGetOriginalPhysicalDeviceFormatProperties2EXT) {
    if (IsPlatform(kMockICD)) {
        printf("Device Profile layer is for real GPU, if using MockICD with profiles, just adjust the profile json file instead\n");
        return false;
    }

    // Load required functions
    fpvkSetPhysicalDeviceFormatProperties2EXT =
        (PFN_vkSetPhysicalDeviceFormatProperties2EXT)vk::GetInstanceProcAddr(instance(), "vkSetPhysicalDeviceFormatProperties2EXT");
    fpvkGetOriginalPhysicalDeviceFormatProperties2EXT =
        (PFN_vkGetOriginalPhysicalDeviceFormatProperties2EXT)vk::GetInstanceProcAddr(
            instance(), "vkGetOriginalPhysicalDeviceFormatProperties2EXT");

    if (!(fpvkSetPhysicalDeviceFormatProperties2EXT) || !(fpvkGetOriginalPhysicalDeviceFormatProperties2EXT)) {
        printf(
            "Can't find device_profile_api functions; make sure VK_LAYER_PATH is set correctly to where the validation layers "
            "are built, the device profile layer should be in the same directory.\n");
        return false;
    }

    return true;
}

bool VkLayerTest::LoadDeviceProfileLayer(PFN_vkSetPhysicalDeviceLimitsEXT &fpvkSetPhysicalDeviceLimitsEXT,
                                         PFN_vkGetOriginalPhysicalDeviceLimitsEXT &fpvkGetOriginalPhysicalDeviceLimitsEXT) {
    if (IsPlatform(kMockICD)) {
        printf("Device Profile layer is for real GPU, if using MockICD with profiles, just adjust the profile json file instead\n");
        return false;
    }

    // Load required functions
    fpvkSetPhysicalDeviceLimitsEXT =
        (PFN_vkSetPhysicalDeviceLimitsEXT)vk::GetInstanceProcAddr(instance(), "vkSetPhysicalDeviceLimitsEXT");
    fpvkGetOriginalPhysicalDeviceLimitsEXT =
        (PFN_vkGetOriginalPhysicalDeviceLimitsEXT)vk::GetInstanceProcAddr(instance(), "vkGetOriginalPhysicalDeviceLimitsEXT");

    if (!(fpvkSetPhysicalDeviceLimitsEXT) || !(fpvkGetOriginalPhysicalDeviceLimitsEXT)) {
        printf(
            "Can't find device_profile_api functions; make sure VK_LAYER_PATH is set correctly to where the validation layers "
            "are built, the device profile layer should be in the same directory.\n");
        return false;
    }

    return true;
}

bool VkLayerTest::LoadDeviceProfileLayer(PFN_vkSetPhysicalDeviceFeaturesEXT &fpvkSetPhysicalDeviceFeaturesEXT,
                                         PFN_vkGetOriginalPhysicalDeviceFeaturesEXT &fpvkGetOriginalPhysicalDeviceFeaturesEXT) {
    if (IsPlatform(kMockICD)) {
        printf("Device Profile layer is for real GPU, if using MockICD with profiles, just adjust the profile json file instead\n");
        return false;
    }

    // Load required functions
    fpvkSetPhysicalDeviceFeaturesEXT =
        (PFN_vkSetPhysicalDeviceFeaturesEXT)vk::GetInstanceProcAddr(instance(), "vkSetPhysicalDeviceFeaturesEXT");
    fpvkGetOriginalPhysicalDeviceFeaturesEXT =
        (PFN_vkGetOriginalPhysicalDeviceFeaturesEXT)vk::GetInstanceProcAddr(instance(), "vkGetOriginalPhysicalDeviceFeaturesEXT");

    if (!(fpvkSetPhysicalDeviceFeaturesEXT) || !(fpvkGetOriginalPhysicalDeviceFeaturesEXT)) {
        printf(
            "Can't find device_profile_api functions; make sure VK_LAYER_PATH is set correctly to where the validation layers "
            "are built, the device profile layer should be in the same directory.\n");
        return false;
    }

    return true;
}

bool VkLayerTest::LoadDeviceProfileLayer(PFN_VkSetPhysicalDeviceProperties2EXT &fpvkSetPhysicalDeviceProperties2EXT) {
    if (IsPlatform(kMockICD)) {
        printf("Device Profile layer is for real GPU, if using MockICD with profiles, just adjust the profile json file instead\n");
        return false;
    }

    // Load required functions
    fpvkSetPhysicalDeviceProperties2EXT =
        (PFN_VkSetPhysicalDeviceProperties2EXT)vk::GetInstanceProcAddr(instance(), "vkSetPhysicalDeviceProperties2EXT");

    if (!fpvkSetPhysicalDeviceProperties2EXT) {
        printf(
            "Can't find device_profile_api functions; make sure VK_LAYER_PATH is set correctly to where the validation layers "
            "are built, the device profile layer should be in the same directory.\n");
        return false;
    }

    return true;
}

bool VkBufferTest::GetTestConditionValid(VkDeviceObj *aVulkanDevice, eTestEnFlags aTestFlag, VkBufferUsageFlags aBufferUsage) {
    if (eInvalidDeviceOffset != aTestFlag && eInvalidMemoryOffset != aTestFlag) {
        return true;
    }
    VkDeviceSize offset_limit = 0;
    if (eInvalidMemoryOffset == aTestFlag) {
        VkBuffer vulkanBuffer;
        VkBufferCreateInfo buffer_create_info = LvlInitStruct<VkBufferCreateInfo>();
        buffer_create_info.size = 32;
        buffer_create_info.usage = aBufferUsage;

        vk::CreateBuffer(aVulkanDevice->device(), &buffer_create_info, nullptr, &vulkanBuffer);
        VkMemoryRequirements memory_reqs = {};

        vk::GetBufferMemoryRequirements(aVulkanDevice->device(), vulkanBuffer, &memory_reqs);
        vk::DestroyBuffer(aVulkanDevice->device(), vulkanBuffer, nullptr);
        offset_limit = memory_reqs.alignment;
    } else if ((VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT) & aBufferUsage) {
        offset_limit = aVulkanDevice->props.limits.minTexelBufferOffsetAlignment;
    } else if (VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT & aBufferUsage) {
        offset_limit = aVulkanDevice->props.limits.minUniformBufferOffsetAlignment;
    } else if (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT & aBufferUsage) {
        offset_limit = aVulkanDevice->props.limits.minStorageBufferOffsetAlignment;
    }
    return eOffsetAlignment < offset_limit;
}

VkBufferTest::VkBufferTest(VkDeviceObj *aVulkanDevice, VkBufferUsageFlags aBufferUsage, eTestEnFlags aTestFlag)
    : AllocateCurrent(true),
      BoundCurrent(false),
      CreateCurrent(false),
      InvalidDeleteEn(false),
      VulkanDevice(aVulkanDevice->device()) {
    if (eBindNullBuffer == aTestFlag || eBindFakeBuffer == aTestFlag) {
        VkMemoryAllocateInfo memory_allocate_info = LvlInitStruct<VkMemoryAllocateInfo>();
        memory_allocate_info.allocationSize = 1;   // fake size -- shouldn't matter for the test
        memory_allocate_info.memoryTypeIndex = 0;  // fake type -- shouldn't matter for the test
        vk::AllocateMemory(VulkanDevice, &memory_allocate_info, nullptr, &VulkanMemory);

        VulkanBuffer = (aTestFlag == eBindNullBuffer) ? VK_NULL_HANDLE : (VkBuffer)0xCDCDCDCDCDCDCDCD;

        vk::BindBufferMemory(VulkanDevice, VulkanBuffer, VulkanMemory, 0);
    } else {
        VkBufferCreateInfo buffer_create_info = LvlInitStruct<VkBufferCreateInfo>();
        buffer_create_info.size = 32;
        buffer_create_info.usage = aBufferUsage;

        vk::CreateBuffer(VulkanDevice, &buffer_create_info, nullptr, &VulkanBuffer);

        CreateCurrent = true;

        VkMemoryRequirements memory_requirements;
        vk::GetBufferMemoryRequirements(VulkanDevice, VulkanBuffer, &memory_requirements);

        VkMemoryAllocateInfo memory_allocate_info = LvlInitStruct<VkMemoryAllocateInfo>();
        memory_allocate_info.allocationSize = memory_requirements.size + eOffsetAlignment;
        bool pass = aVulkanDevice->phy().set_memory_type(memory_requirements.memoryTypeBits, &memory_allocate_info,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        if (!pass) {
            CreateCurrent = false;
            vk::DestroyBuffer(VulkanDevice, VulkanBuffer, nullptr);
            return;
        }

        vk::AllocateMemory(VulkanDevice, &memory_allocate_info, NULL, &VulkanMemory);
        // NB: 1 is intentionally an invalid offset value
        const bool offset_en = eInvalidDeviceOffset == aTestFlag || eInvalidMemoryOffset == aTestFlag;
        vk::BindBufferMemory(VulkanDevice, VulkanBuffer, VulkanMemory, offset_en ? eOffsetAlignment : 0);
        BoundCurrent = true;

        InvalidDeleteEn = (eFreeInvalidHandle == aTestFlag);
    }
}

VkBufferTest::~VkBufferTest() {
    if (CreateCurrent) {
        vk::DestroyBuffer(VulkanDevice, VulkanBuffer, nullptr);
    }
    if (AllocateCurrent) {
        if (InvalidDeleteEn) {
            auto bad_memory = CastFromUint64<VkDeviceMemory>(CastToUint64(VulkanMemory) + 1);
            vk::FreeMemory(VulkanDevice, bad_memory, nullptr);
        }
        vk::FreeMemory(VulkanDevice, VulkanMemory, nullptr);
    }
}

void SetImageLayout(VkDeviceObj *device, VkImageAspectFlags aspect, VkImage image, VkImageLayout image_layout) {
    VkCommandPoolObj pool(device, device->graphics_queue_node_index_);
    VkCommandBufferObj cmd_buf(device, &pool);

    cmd_buf.begin();
    VkImageMemoryBarrier layout_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                        nullptr,
                                        0,
                                        VK_ACCESS_MEMORY_READ_BIT,
                                        VK_IMAGE_LAYOUT_UNDEFINED,
                                        image_layout,
                                        VK_QUEUE_FAMILY_IGNORED,
                                        VK_QUEUE_FAMILY_IGNORED,
                                        image,
                                        {aspect, 0, 1, 0, 1}};

    cmd_buf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
                            &layout_barrier);
    cmd_buf.end();

    cmd_buf.QueueCommandBuffer();
}

std::unique_ptr<VkImageObj> VkArmBestPracticesLayerTest::CreateImage(VkFormat format, const uint32_t width, const uint32_t height,
                                                                     VkImageUsageFlags attachment_usage) {
    auto img = std::unique_ptr<VkImageObj>(new VkImageObj(m_device));
    img->Init(width, height, 1, format,
              VK_IMAGE_USAGE_SAMPLED_BIT | attachment_usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
              VK_IMAGE_TILING_OPTIMAL);
    return img;
}

VkRenderPass VkArmBestPracticesLayerTest::CreateRenderPass(VkFormat format, VkAttachmentLoadOp load_op,
                                                           VkAttachmentStoreOp store_op) {
    VkRenderPass renderpass{VK_NULL_HANDLE};

    // Create renderpass
    VkAttachmentDescription attachment = {};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = load_op;
    attachment.storeOp = store_op;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkAttachmentReference attachment_reference = {};
    attachment_reference.attachment = 0;
    attachment_reference.layout = VK_IMAGE_LAYOUT_GENERAL;

    VkSubpassDescription subpass = {};
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachment_reference;

    VkRenderPassCreateInfo rpinf = LvlInitStruct<VkRenderPassCreateInfo>();
    rpinf.attachmentCount = 1;
    rpinf.pAttachments = &attachment;
    rpinf.subpassCount = 1;
    rpinf.pSubpasses = &subpass;
    rpinf.dependencyCount = 0;
    rpinf.pDependencies = nullptr;

    VkResult result = vk::CreateRenderPass(m_device->handle(), &rpinf, nullptr, &renderpass);
    assert(result == VK_SUCCESS);
    (void)result;

    return renderpass;
}

VkFramebuffer VkArmBestPracticesLayerTest::CreateFramebuffer(const uint32_t width, const uint32_t height, VkImageView image_view,
                                                             VkRenderPass renderpass) {
    VkFramebuffer framebuffer{VK_NULL_HANDLE};

    VkFramebufferCreateInfo framebuffer_create_info = LvlInitStruct<VkFramebufferCreateInfo>();
    framebuffer_create_info.renderPass = renderpass;
    framebuffer_create_info.attachmentCount = 1;
    framebuffer_create_info.pAttachments = &image_view;
    framebuffer_create_info.width = width;
    framebuffer_create_info.height = height;
    framebuffer_create_info.layers = 1;

    VkResult result = vk::CreateFramebuffer(m_device->handle(), &framebuffer_create_info, nullptr, &framebuffer);
    assert(result == VK_SUCCESS);
    (void)result;

    return framebuffer;
}

VkSampler VkArmBestPracticesLayerTest::CreateDefaultSampler() {
    VkSampler sampler{VK_NULL_HANDLE};

    VkSamplerCreateInfo sampler_create_info = LvlInitStruct<VkSamplerCreateInfo>();
    sampler_create_info.magFilter = VK_FILTER_NEAREST;
    sampler_create_info.minFilter = VK_FILTER_NEAREST;
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler_create_info.maxLod = VK_LOD_CLAMP_NONE;

    VkResult result = vk::CreateSampler(m_device->handle(), &sampler_create_info, nullptr, &sampler);
    assert(result == VK_SUCCESS);
    (void)result;

    return sampler;
}

bool VkBufferTest::GetBufferCurrent() { return AllocateCurrent && BoundCurrent && CreateCurrent; }

const VkBuffer &VkBufferTest::GetBuffer() { return VulkanBuffer; }

void VkBufferTest::TestDoubleDestroy() {
    // Destroy the buffer but leave the flag set, which will cause
    // the buffer to be destroyed again in the destructor.
    vk::DestroyBuffer(VulkanDevice, VulkanBuffer, nullptr);
}

OneOffDescriptorSet::OneOffDescriptorSet(VkDeviceObj *device, const Bindings &bindings,
                                         VkDescriptorSetLayoutCreateFlags layout_flags, void *layout_pnext,
                                         VkDescriptorPoolCreateFlags poolFlags, void *allocate_pnext, int buffer_info_size,
                                         int image_info_size, int buffer_view_size)
    : device_{device}, pool_{}, layout_(device, bindings, layout_flags, layout_pnext), set_(VK_NULL_HANDLE) {
    VkResult err;
    buffer_infos.reserve(buffer_info_size);
    image_infos.reserve(image_info_size);
    buffer_views.reserve(buffer_view_size);
    std::vector<VkDescriptorPoolSize> sizes;
    for (const auto &b : bindings) sizes.push_back({b.descriptorType, std::max(1u, b.descriptorCount)});

    VkDescriptorPoolCreateInfo dspci = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, poolFlags, 1, uint32_t(sizes.size()), sizes.data()};
    err = vk::CreateDescriptorPool(device_->handle(), &dspci, nullptr, &pool_);
    if (err != VK_SUCCESS) return;

    if ((layout_flags & VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR) == 0) {
        VkDescriptorSetAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, allocate_pnext, pool_, 1,
                                                  &layout_.handle()};
        err = vk::AllocateDescriptorSets(device_->handle(), &alloc_info, &set_);
    }
}

OneOffDescriptorSet::~OneOffDescriptorSet() {
    // No need to destroy set-- it's going away with the pool.
    vk::DestroyDescriptorPool(device_->handle(), pool_, nullptr);
}

bool OneOffDescriptorSet::Initialized() { return pool_ != VK_NULL_HANDLE && layout_.initialized() && set_ != VK_NULL_HANDLE; }

void OneOffDescriptorSet::Clear() {
    buffer_infos.clear();
    image_infos.clear();
    buffer_views.clear();
    descriptor_writes.clear();
}

void OneOffDescriptorSet::WriteDescriptorBufferInfo(int binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range,
                                                    VkDescriptorType descriptorType, uint32_t arrayElement, uint32_t count) {
    const auto index = buffer_infos.size();

    VkDescriptorBufferInfo buffer_info = {};
    buffer_info.buffer = buffer;
    buffer_info.offset = offset;
    buffer_info.range = range;

    for (uint32_t i = 0; i < count; ++i) {
        buffer_infos.emplace_back(buffer_info);
    }

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = set_;
    descriptor_write.dstBinding = binding;
    descriptor_write.dstArrayElement = arrayElement;
    descriptor_write.descriptorCount = count;
    descriptor_write.descriptorType = descriptorType;
    descriptor_write.pBufferInfo = &buffer_infos[index];
    descriptor_write.pImageInfo = nullptr;
    descriptor_write.pTexelBufferView = nullptr;

    descriptor_writes.emplace_back(descriptor_write);
}

void OneOffDescriptorSet::WriteDescriptorBufferView(int binding, VkBufferView buffer_view, VkDescriptorType descriptorType,
                                                    uint32_t arrayElement, uint32_t count) {
    const auto index = buffer_views.size();

    for (uint32_t i = 0; i < count; ++i) {
        buffer_views.emplace_back(buffer_view);
    }

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = set_;
    descriptor_write.dstBinding = binding;
    descriptor_write.dstArrayElement = arrayElement;
    descriptor_write.descriptorCount = count;
    descriptor_write.descriptorType = descriptorType;
    descriptor_write.pTexelBufferView = &buffer_views[index];
    descriptor_write.pImageInfo = nullptr;
    descriptor_write.pBufferInfo = nullptr;

    descriptor_writes.emplace_back(descriptor_write);
}

void OneOffDescriptorSet::WriteDescriptorImageInfo(int binding, VkImageView image_view, VkSampler sampler,
                                                   VkDescriptorType descriptorType, VkImageLayout imageLayout,
                                                   uint32_t arrayElement, uint32_t count) {
    const auto index = image_infos.size();

    VkDescriptorImageInfo image_info = {};
    image_info.imageView = image_view;
    image_info.sampler = sampler;
    image_info.imageLayout = imageLayout;

    for (uint32_t i = 0; i < count; ++i) {
        image_infos.emplace_back(image_info);
    }

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = set_;
    descriptor_write.dstBinding = binding;
    descriptor_write.dstArrayElement = arrayElement;
    descriptor_write.descriptorCount = count;
    descriptor_write.descriptorType = descriptorType;
    descriptor_write.pImageInfo = &image_infos[index];
    descriptor_write.pBufferInfo = nullptr;
    descriptor_write.pTexelBufferView = nullptr;

    descriptor_writes.emplace_back(descriptor_write);
}

void OneOffDescriptorSet::UpdateDescriptorSets() {
    vk::UpdateDescriptorSets(device_->handle(), descriptor_writes.size(), descriptor_writes.data(), 0, NULL);
}

CreatePipelineHelper::CreatePipelineHelper(VkLayerTest &test, uint32_t color_attachments_count)
    : cb_attachments_(color_attachments_count), layer_test_(test) {}

CreatePipelineHelper::~CreatePipelineHelper() {
    VkDevice device = layer_test_.device();
    vk::DestroyPipelineCache(device, pipeline_cache_, nullptr);
    vk::DestroyPipeline(device, pipeline_, nullptr);
}

void CreatePipelineHelper::InitDescriptorSetInfo() {
    dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
}

void CreatePipelineHelper::InitInputAndVertexInfo() {
    vi_ci_ = LvlInitStruct<VkPipelineVertexInputStateCreateInfo>();

    ia_ci_ = LvlInitStruct<VkPipelineInputAssemblyStateCreateInfo>();
    ia_ci_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
}

void CreatePipelineHelper::InitMultisampleInfo() {
    pipe_ms_state_ci_ = LvlInitStruct<VkPipelineMultisampleStateCreateInfo>();
    pipe_ms_state_ci_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    pipe_ms_state_ci_.sampleShadingEnable = VK_FALSE;
    pipe_ms_state_ci_.minSampleShading = 1.0;
    pipe_ms_state_ci_.pSampleMask = NULL;
}

void CreatePipelineHelper::InitPipelineLayoutInfo() {
    pipeline_layout_ci_ = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci_.setLayoutCount = 1;     // Not really changeable because InitState() sets exactly one pSetLayout
    pipeline_layout_ci_.pSetLayouts = nullptr;  // must bound after it is created
}

void CreatePipelineHelper::InitViewportInfo() {
    viewport_ = {0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 1.0f};
    scissor_ = {{0, 0}, {64, 64}};

    vp_state_ci_ = LvlInitStruct<VkPipelineViewportStateCreateInfo>();
    vp_state_ci_.viewportCount = 1;
    vp_state_ci_.pViewports = &viewport_;  // ignored if dynamic
    vp_state_ci_.scissorCount = 1;
    vp_state_ci_.pScissors = &scissor_;  // ignored if dynamic
}

void CreatePipelineHelper::InitDynamicStateInfo() {
    // Use a "validity" check on the {} initialized structure to detect initialization
    // during late bind
}

void CreatePipelineHelper::InitShaderInfo() { ResetShaderInfo(bindStateVertShaderText, bindStateFragShaderText); }

void CreatePipelineHelper::ResetShaderInfo(const char *vertex_shader_text, const char *fragment_shader_text) {
    vs_.reset(new VkShaderObj(&layer_test_, vertex_shader_text, VK_SHADER_STAGE_VERTEX_BIT));
    fs_.reset(new VkShaderObj(&layer_test_, fragment_shader_text, VK_SHADER_STAGE_FRAGMENT_BIT));
    // We shouldn't need a fragment shader but add it to be able to run on more devices
    shader_stages_ = {vs_->GetStageCreateInfo(), fs_->GetStageCreateInfo()};
}

void CreatePipelineHelper::InitRasterizationInfo() {
    rs_state_ci_ = LvlInitStruct<VkPipelineRasterizationStateCreateInfo>(&line_state_ci_);
    rs_state_ci_.flags = 0;
    rs_state_ci_.depthClampEnable = VK_FALSE;
    rs_state_ci_.rasterizerDiscardEnable = VK_FALSE;
    rs_state_ci_.polygonMode = VK_POLYGON_MODE_FILL;
    rs_state_ci_.cullMode = VK_CULL_MODE_BACK_BIT;
    rs_state_ci_.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rs_state_ci_.depthBiasEnable = VK_FALSE;
    rs_state_ci_.lineWidth = 1.0F;
}

void CreatePipelineHelper::InitLineRasterizationInfo() {
    line_state_ci_ = LvlInitStruct<VkPipelineRasterizationLineStateCreateInfoEXT>();
    line_state_ci_.lineRasterizationMode = VK_LINE_RASTERIZATION_MODE_DEFAULT_EXT;
    line_state_ci_.stippledLineEnable = VK_FALSE;
    line_state_ci_.lineStippleFactor = 0;
    line_state_ci_.lineStipplePattern = 0;
}

void CreatePipelineHelper::InitBlendStateInfo() {
    cb_ci_ = LvlInitStruct<VkPipelineColorBlendStateCreateInfo>();
    cb_ci_.logicOpEnable = VK_FALSE;
    cb_ci_.logicOp = VK_LOGIC_OP_COPY;  // ignored if enable is VK_FALSE above
    cb_ci_.attachmentCount = cb_attachments_.size();
    ASSERT_TRUE(IsValidVkStruct(layer_test_.RenderPassInfo()));
    cb_ci_.pAttachments = cb_attachments_.data();
    for (int i = 0; i < 4; i++) {
        cb_ci_.blendConstants[0] = 1.0F;
    }
}

void CreatePipelineHelper::InitGraphicsPipelineInfo() {
    // Color-only rendering in a subpass with no depth/stencil attachment
    // Active Pipeline Shader Stages
    //    Vertex Shader
    //    Fragment Shader
    // Required: Fixed-Function Pipeline Stages
    //    VkPipelineVertexInputStateCreateInfo
    //    VkPipelineInputAssemblyStateCreateInfo
    //    VkPipelineViewportStateCreateInfo
    //    VkPipelineRasterizationStateCreateInfo
    //    VkPipelineMultisampleStateCreateInfo
    //    VkPipelineColorBlendStateCreateInfo
    gp_ci_ = LvlInitStruct<VkGraphicsPipelineCreateInfo>();
    gp_ci_.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
    gp_ci_.pVertexInputState = &vi_ci_;
    gp_ci_.pInputAssemblyState = &ia_ci_;
    gp_ci_.pTessellationState = nullptr;
    gp_ci_.pViewportState = &vp_state_ci_;
    gp_ci_.pMultisampleState = &pipe_ms_state_ci_;
    gp_ci_.pRasterizationState = &rs_state_ci_;
    gp_ci_.pDepthStencilState = nullptr;
    gp_ci_.pColorBlendState = &cb_ci_;
    gp_ci_.pDynamicState = nullptr;
    gp_ci_.renderPass = layer_test_.renderPass();
}

void CreatePipelineHelper::InitPipelineCacheInfo() {
    pc_ci_ = LvlInitStruct<VkPipelineCacheCreateInfo>();
    pc_ci_.flags = 0;
    pc_ci_.initialDataSize = 0;
    pc_ci_.pInitialData = nullptr;
}

void CreatePipelineHelper::InitTesselationState() {
    // TBD -- add shaders and create_info
}

void CreatePipelineHelper::InitInfo() {
    InitDescriptorSetInfo();
    InitInputAndVertexInfo();
    InitMultisampleInfo();
    InitPipelineLayoutInfo();
    InitViewportInfo();
    InitDynamicStateInfo();
    InitShaderInfo();
    InitRasterizationInfo();
    InitLineRasterizationInfo();
    InitBlendStateInfo();
    InitGraphicsPipelineInfo();
    InitPipelineCacheInfo();
}

void CreatePipelineHelper::InitVertexInputLibInfo(void *p_next) {
    InitDescriptorSetInfo();
    InitInputAndVertexInfo();
    InitMultisampleInfo();
    InitPipelineLayoutInfo();
    InitViewportInfo();
    InitDynamicStateInfo();
    InitRasterizationInfo();
    InitLineRasterizationInfo();
    InitBlendStateInfo();

    gpl_info.emplace(LvlInitStruct<VkGraphicsPipelineLibraryCreateInfoEXT>(p_next));
    gpl_info->flags = VK_GRAPHICS_PIPELINE_LIBRARY_VERTEX_INPUT_INTERFACE_BIT_EXT;

    gp_ci_ = LvlInitStruct<VkGraphicsPipelineCreateInfo>(&gpl_info);
    gp_ci_.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    gp_ci_.pVertexInputState = &vi_ci_;
    gp_ci_.pInputAssemblyState = &ia_ci_;

    InitPipelineCacheInfo();
}

void CreatePipelineHelper::InitPreRasterLibInfo(uint32_t count, const VkPipelineShaderStageCreateInfo *info, void *p_next) {
    InitDescriptorSetInfo();
    InitInputAndVertexInfo();
    InitMultisampleInfo();
    InitPipelineLayoutInfo();
    InitViewportInfo();
    InitDynamicStateInfo();
    InitRasterizationInfo();
    InitLineRasterizationInfo();
    InitBlendStateInfo();

    gpl_info.emplace(LvlInitStruct<VkGraphicsPipelineLibraryCreateInfoEXT>(p_next));
    gpl_info->flags = VK_GRAPHICS_PIPELINE_LIBRARY_PRE_RASTERIZATION_SHADERS_BIT_EXT;

    gp_ci_ = LvlInitStruct<VkGraphicsPipelineCreateInfo>(&gpl_info);
    gp_ci_.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    gp_ci_.pViewportState = &vp_state_ci_;
    gp_ci_.pRasterizationState = &rs_state_ci_;

    // If using Dynamic Rendering, will need to be set to null
    // otherwise needs to be shared across libraries in the same executable pipeline
    gp_ci_.renderPass = layer_test_.renderPass();
    gp_ci_.subpass = 0;

    gp_ci_.stageCount = count;
    gp_ci_.pStages = info;

    InitPipelineCacheInfo();
}

void CreatePipelineHelper::InitFragmentLibInfo(uint32_t count, const VkPipelineShaderStageCreateInfo *info, void *p_next) {
    InitDescriptorSetInfo();
    InitInputAndVertexInfo();
    InitMultisampleInfo();
    InitPipelineLayoutInfo();
    InitViewportInfo();
    InitDynamicStateInfo();
    InitRasterizationInfo();
    InitLineRasterizationInfo();
    InitBlendStateInfo();

    gpl_info.emplace(LvlInitStruct<VkGraphicsPipelineLibraryCreateInfoEXT>(p_next));
    gpl_info->flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_SHADER_BIT_EXT;

    gp_ci_ = LvlInitStruct<VkGraphicsPipelineCreateInfo>(&gpl_info);
    gp_ci_.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR;
    //  gp_ci_.pTessellationState = nullptr; // TODO
    gp_ci_.pViewportState = &vp_state_ci_;
    gp_ci_.pRasterizationState = &rs_state_ci_;

    // If using Dynamic Rendering, will need to be set to null
    // otherwise needs to be shared across libraries in the same executable pipeline
    gp_ci_.renderPass = layer_test_.renderPass();
    gp_ci_.subpass = 0;

    // TODO if renderPass is null, MS info is not needed
    gp_ci_.pMultisampleState = &pipe_ms_state_ci_;

    gp_ci_.stageCount = count;
    gp_ci_.pStages = info;

    InitPipelineCacheInfo();
}

void CreatePipelineHelper::InitFragmentOutputLibInfo(void *p_next) {
    InitDescriptorSetInfo();
    InitInputAndVertexInfo();
    InitMultisampleInfo();
    InitPipelineLayoutInfo();
    InitViewportInfo();
    InitDynamicStateInfo();
    InitRasterizationInfo();
    InitLineRasterizationInfo();
    InitBlendStateInfo();

    gpl_info.emplace(LvlInitStruct<VkGraphicsPipelineLibraryCreateInfoEXT>(p_next));
    gpl_info->flags = VK_GRAPHICS_PIPELINE_LIBRARY_FRAGMENT_OUTPUT_INTERFACE_BIT_EXT;

    gp_ci_ = LvlInitStruct<VkGraphicsPipelineCreateInfo>(&gpl_info);
    gp_ci_.flags = VK_PIPELINE_CREATE_LIBRARY_BIT_KHR | VK_PIPELINE_CREATE_RETAIN_LINK_TIME_OPTIMIZATION_INFO_BIT_EXT;
    gp_ci_.pColorBlendState = &cb_ci_;
    gp_ci_.pMultisampleState = &pipe_ms_state_ci_;
    gp_ci_.pRasterizationState = &rs_state_ci_;

    // If using Dynamic Rendering, will need to be set to null
    // otherwise needs to be shared across libraries in the same executable pipeline
    gp_ci_.renderPass = layer_test_.renderPass();
    gp_ci_.subpass = 0;

    InitPipelineCacheInfo();
}

void CreatePipelineHelper::InitState() {
    descriptor_set_.reset(new OneOffDescriptorSet(layer_test_.DeviceObj(), dsl_bindings_));
    ASSERT_TRUE(descriptor_set_->Initialized());

    const std::vector<VkPushConstantRange> push_ranges(
        pipeline_layout_ci_.pPushConstantRanges,
        pipeline_layout_ci_.pPushConstantRanges + pipeline_layout_ci_.pushConstantRangeCount);
    pipeline_layout_ =
        VkPipelineLayoutObj(layer_test_.DeviceObj(), {&descriptor_set_->layout_}, push_ranges, pipeline_layout_ci_.flags);

    InitPipelineCache();
}

void CreatePipelineHelper::InitPipelineCache() {
    if (pipeline_cache_ != VK_NULL_HANDLE) {
        vk::DestroyPipelineCache(layer_test_.device(), pipeline_cache_, nullptr);
    }
    VkResult err = vk::CreatePipelineCache(layer_test_.device(), &pc_ci_, NULL, &pipeline_cache_);
    ASSERT_VK_SUCCESS(err);
}

void CreatePipelineHelper::LateBindPipelineInfo() {
    // By value or dynamically located items must be late bound
    gp_ci_.layout = pipeline_layout_.handle();
    if (gp_ci_.stageCount == 0) {
        gp_ci_.stageCount = shader_stages_.size();
        gp_ci_.pStages = shader_stages_.data();
    }
    if ((gp_ci_.pTessellationState == nullptr) && IsValidVkStruct(tess_ci_)) {
        gp_ci_.pTessellationState = &tess_ci_;
    }
    if ((gp_ci_.pDynamicState == nullptr) && IsValidVkStruct(dyn_state_ci_)) {
        gp_ci_.pDynamicState = &dyn_state_ci_;
    }
    if ((gp_ci_.pDepthStencilState == nullptr) && IsValidVkStruct(ds_ci_)) {
        gp_ci_.pDepthStencilState = &ds_ci_;
    }
}

VkResult CreatePipelineHelper::CreateGraphicsPipeline(bool implicit_destroy, bool do_late_bind) {
    VkResult err;
    if (do_late_bind) {
        LateBindPipelineInfo();
    }
    if (implicit_destroy && (pipeline_ != VK_NULL_HANDLE)) {
        vk::DestroyPipeline(layer_test_.device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    err = vk::CreateGraphicsPipelines(layer_test_.device(), pipeline_cache_, 1, &gp_ci_, NULL, &pipeline_);
    return err;
}

CreateComputePipelineHelper::CreateComputePipelineHelper(VkLayerTest &test) : layer_test_(test) {}

CreateComputePipelineHelper::~CreateComputePipelineHelper() {
    VkDevice device = layer_test_.device();
    vk::DestroyPipelineCache(device, pipeline_cache_, nullptr);
    vk::DestroyPipeline(device, pipeline_, nullptr);
}

void CreateComputePipelineHelper::InitDescriptorSetInfo() {
    dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
}

void CreateComputePipelineHelper::InitPipelineLayoutInfo() {
    pipeline_layout_ci_ = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci_.setLayoutCount = 1;     // Not really changeable because InitState() sets exactly one pSetLayout
    pipeline_layout_ci_.pSetLayouts = nullptr;  // must bound after it is created
}

void CreateComputePipelineHelper::InitShaderInfo() {
    cs_.reset(new VkShaderObj(&layer_test_, bindStateMinimalShaderText, VK_SHADER_STAGE_COMPUTE_BIT));
    // We shouldn't need a fragment shader but add it to be able to run on more devices
}

void CreateComputePipelineHelper::InitComputePipelineInfo() {
    cp_ci_ = LvlInitStruct<VkComputePipelineCreateInfo>();
    cp_ci_.flags = 0;
}

void CreateComputePipelineHelper::InitPipelineCacheInfo() {
    pc_ci_ = LvlInitStruct<VkPipelineCacheCreateInfo>();
    pc_ci_.flags = 0;
    pc_ci_.initialDataSize = 0;
    pc_ci_.pInitialData = nullptr;
}

void CreateComputePipelineHelper::InitInfo() {
    InitDescriptorSetInfo();
    InitPipelineLayoutInfo();
    InitShaderInfo();
    InitComputePipelineInfo();
    InitPipelineCacheInfo();
}

void CreateComputePipelineHelper::InitState() {
    descriptor_set_.reset(new OneOffDescriptorSet(layer_test_.DeviceObj(), dsl_bindings_));
    ASSERT_TRUE(descriptor_set_->Initialized());

    const std::vector<VkPushConstantRange> push_ranges(
        pipeline_layout_ci_.pPushConstantRanges,
        pipeline_layout_ci_.pPushConstantRanges + pipeline_layout_ci_.pushConstantRangeCount);
    pipeline_layout_ = VkPipelineLayoutObj(layer_test_.DeviceObj(), {&descriptor_set_->layout_}, push_ranges);

    InitPipelineCache();
}

void CreateComputePipelineHelper::InitPipelineCache() {
    if (pipeline_cache_ != VK_NULL_HANDLE) {
        vk::DestroyPipelineCache(layer_test_.device(), pipeline_cache_, nullptr);
    }
    VkResult err = vk::CreatePipelineCache(layer_test_.device(), &pc_ci_, NULL, &pipeline_cache_);
    ASSERT_VK_SUCCESS(err);
}

void CreateComputePipelineHelper::LateBindPipelineInfo() {
    // By value or dynamically located items must be late bound
    cp_ci_.layout = pipeline_layout_.handle();
    cp_ci_.stage = cs_.get()->GetStageCreateInfo();
}

VkResult CreateComputePipelineHelper::CreateComputePipeline(bool implicit_destroy, bool do_late_bind) {
    VkResult err;
    if (do_late_bind) {
        LateBindPipelineInfo();
    }
    if (implicit_destroy && (pipeline_ != VK_NULL_HANDLE)) {
        vk::DestroyPipeline(layer_test_.device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    err = vk::CreateComputePipelines(layer_test_.device(), pipeline_cache_, 1, &cp_ci_, NULL, &pipeline_);
    return err;
}

CreateNVRayTracingPipelineHelper::CreateNVRayTracingPipelineHelper(VkLayerTest &test) : layer_test_(test) {}
CreateNVRayTracingPipelineHelper::~CreateNVRayTracingPipelineHelper() {
    VkDevice device = layer_test_.device();
    vk::DestroyPipelineCache(device, pipeline_cache_, nullptr);
    vk::DestroyPipeline(device, pipeline_, nullptr);
}

void CreateNVRayTracingPipelineHelper::InitShaderGroups() {
    {
        VkRayTracingShaderGroupCreateInfoNV group = LvlInitStruct<VkRayTracingShaderGroupCreateInfoNV>();
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        group.generalShader = 0;
        group.closestHitShader = VK_SHADER_UNUSED_NV;
        group.anyHitShader = VK_SHADER_UNUSED_NV;
        group.intersectionShader = VK_SHADER_UNUSED_NV;
        groups_.push_back(group);
    }
    {
        VkRayTracingShaderGroupCreateInfoNV group = LvlInitStruct<VkRayTracingShaderGroupCreateInfoNV>();
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
        group.generalShader = VK_SHADER_UNUSED_NV;
        group.closestHitShader = 1;
        group.anyHitShader = VK_SHADER_UNUSED_NV;
        group.intersectionShader = VK_SHADER_UNUSED_NV;
        groups_.push_back(group);
    }
    {
        VkRayTracingShaderGroupCreateInfoNV group = LvlInitStruct<VkRayTracingShaderGroupCreateInfoNV>();
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        group.generalShader = 2;
        group.closestHitShader = VK_SHADER_UNUSED_NV;
        group.anyHitShader = VK_SHADER_UNUSED_NV;
        group.intersectionShader = VK_SHADER_UNUSED_NV;
        groups_.push_back(group);
    }
}

void CreateNVRayTracingPipelineHelper::InitShaderGroupsKHR() {
    {
        VkRayTracingShaderGroupCreateInfoKHR group = LvlInitStruct<VkRayTracingShaderGroupCreateInfoKHR>();
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = 0;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups_KHR_.push_back(group);
    }
    {
        VkRayTracingShaderGroupCreateInfoKHR group = LvlInitStruct<VkRayTracingShaderGroupCreateInfoKHR>();
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        group.generalShader = VK_SHADER_UNUSED_KHR;
        group.closestHitShader = 1;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups_KHR_.push_back(group);
    }
    {
        VkRayTracingShaderGroupCreateInfoKHR group = LvlInitStruct<VkRayTracingShaderGroupCreateInfoKHR>();
        group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        group.generalShader = 2;
        group.closestHitShader = VK_SHADER_UNUSED_KHR;
        group.anyHitShader = VK_SHADER_UNUSED_KHR;
        group.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups_KHR_.push_back(group);
    }
}
void CreateNVRayTracingPipelineHelper::InitDescriptorSetInfo() {
    dsl_bindings_ = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV, nullptr},
        {1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1, VK_SHADER_STAGE_RAYGEN_BIT_NV, nullptr},
    };
}

void CreateNVRayTracingPipelineHelper::InitDescriptorSetInfoKHR() {
    dsl_bindings_ = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
    };
}

void CreateNVRayTracingPipelineHelper::InitPipelineLayoutInfo() {
    pipeline_layout_ci_ = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci_.setLayoutCount = 1;     // Not really changeable because InitState() sets exactly one pSetLayout
    pipeline_layout_ci_.pSetLayouts = nullptr;  // must bound after it is created
}

void CreateNVRayTracingPipelineHelper::InitShaderInfoKHR() {
    static const char rayGenShaderText[] = R"glsl(
        #version 460 core
        #extension GL_EXT_ray_tracing : enable
        layout(set = 0, binding = 0, rgba8) uniform image2D image;
        layout(set = 0, binding = 1) uniform accelerationStructureEXT as;

        layout(location = 0) rayPayloadEXT float payload;

        void main()
        {
           vec4 col = vec4(0, 0, 0, 1);

           vec3 origin = vec3(float(gl_LaunchIDEXT.x)/float(gl_LaunchSizeEXT.x), float(gl_LaunchIDEXT.y)/float(gl_LaunchSizeEXT.y), 1.0);
           vec3 dir = vec3(0.0, 0.0, -1.0);

           payload = 0.5;
           traceRayEXT(as, gl_RayFlagsCullBackFacingTrianglesEXT, 0xff, 0, 1, 0, origin, 0.0, dir, 1000.0, 0);

           col.y = payload;

           imageStore(image, ivec2(gl_LaunchIDEXT.xy), col);
        }
    )glsl";

    static char const closestHitShaderText[] = R"glsl(
        #version 460
        #extension GL_EXT_ray_tracing : enable
        layout(location = 0) rayPayloadInEXT float hitValue;

        void main() {
            hitValue = 1.0;
        }
    )glsl";

    static char const missShaderText[] = R"glsl(
        #version 460 core
        #extension GL_EXT_ray_tracing : enable
        layout(location = 0) rayPayloadInEXT float hitValue;

        void main() {
            hitValue = 0.0;
        }
    )glsl";

    rgs_.reset(new VkShaderObj(&layer_test_, rayGenShaderText, VK_SHADER_STAGE_RAYGEN_BIT_KHR, SPV_ENV_VULKAN_1_2));
    chs_.reset(new VkShaderObj(&layer_test_, closestHitShaderText, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, SPV_ENV_VULKAN_1_2));
    mis_.reset(new VkShaderObj(&layer_test_, missShaderText, VK_SHADER_STAGE_MISS_BIT_KHR, SPV_ENV_VULKAN_1_2));

    shader_stages_ = {rgs_->GetStageCreateInfo(), chs_->GetStageCreateInfo(), mis_->GetStageCreateInfo()};
}

void CreateNVRayTracingPipelineHelper::InitShaderInfo() {  // DONE
    static const char rayGenShaderText[] = R"glsl(
        #version 460 core
        #extension GL_NV_ray_tracing : require
        layout(set = 0, binding = 0, rgba8) uniform image2D image;
        layout(set = 0, binding = 1) uniform accelerationStructureNV as;

        layout(location = 0) rayPayloadNV float payload;

        void main()
        {
           vec4 col = vec4(0, 0, 0, 1);

           vec3 origin = vec3(float(gl_LaunchIDNV.x)/float(gl_LaunchSizeNV.x), float(gl_LaunchIDNV.y)/float(gl_LaunchSizeNV.y), 1.0);
           vec3 dir = vec3(0.0, 0.0, -1.0);

           payload = 0.5;
           traceNV(as, gl_RayFlagsCullBackFacingTrianglesNV, 0xff, 0, 1, 0, origin, 0.0, dir, 1000.0, 0);

           col.y = payload;

           imageStore(image, ivec2(gl_LaunchIDNV.xy), col);
        }
    )glsl";

    static char const closestHitShaderText[] = R"glsl(
        #version 460 core
        #extension GL_NV_ray_tracing : require
        layout(location = 0) rayPayloadInNV float hitValue;

        void main() {
            hitValue = 1.0;
        }
    )glsl";

    static char const missShaderText[] = R"glsl(
        #version 460 core
        #extension GL_NV_ray_tracing : require
        layout(location = 0) rayPayloadInNV float hitValue;

        void main() {
            hitValue = 0.0;
        }
    )glsl";

    rgs_.reset(new VkShaderObj(&layer_test_, rayGenShaderText, VK_SHADER_STAGE_RAYGEN_BIT_NV));
    chs_.reset(new VkShaderObj(&layer_test_, closestHitShaderText, VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV));
    mis_.reset(new VkShaderObj(&layer_test_, missShaderText, VK_SHADER_STAGE_MISS_BIT_NV));

    shader_stages_ = {rgs_->GetStageCreateInfo(), chs_->GetStageCreateInfo(), mis_->GetStageCreateInfo()};
}

void CreateNVRayTracingPipelineHelper::InitNVRayTracingPipelineInfo() {
    rp_ci_ = LvlInitStruct<VkRayTracingPipelineCreateInfoNV>();
    rp_ci_.maxRecursionDepth = 0;
    rp_ci_.stageCount = shader_stages_.size();
    rp_ci_.pStages = shader_stages_.data();
    rp_ci_.groupCount = groups_.size();
    rp_ci_.pGroups = groups_.data();
}

void CreateNVRayTracingPipelineHelper::InitKHRRayTracingPipelineInfo() {
    rp_ci_KHR_ = LvlInitStruct<VkRayTracingPipelineCreateInfoKHR>();
    rp_ci_KHR_.maxPipelineRayRecursionDepth = 0;
    rp_ci_KHR_.stageCount = shader_stages_.size();
    rp_ci_KHR_.pStages = shader_stages_.data();
    rp_ci_KHR_.groupCount = groups_KHR_.size();
    rp_ci_KHR_.pGroups = groups_KHR_.data();
}

void CreateNVRayTracingPipelineHelper::InitPipelineCacheInfo() {
    pc_ci_ = LvlInitStruct<VkPipelineCacheCreateInfo>();
    pc_ci_.flags = 0;
    pc_ci_.initialDataSize = 0;
    pc_ci_.pInitialData = nullptr;
}

void CreateNVRayTracingPipelineHelper::InitInfo(bool isKHR) {
    isKHR ? InitShaderGroupsKHR() : InitShaderGroups();
    isKHR ? InitDescriptorSetInfoKHR() : InitDescriptorSetInfo();
    InitPipelineLayoutInfo();
    isKHR ? InitShaderInfoKHR() : InitShaderInfo();
    isKHR ? InitKHRRayTracingPipelineInfo() : InitNVRayTracingPipelineInfo();
    InitPipelineCacheInfo();
}

void CreateNVRayTracingPipelineHelper::InitState() {
    descriptor_set_.reset(new OneOffDescriptorSet(layer_test_.DeviceObj(), dsl_bindings_));
    ASSERT_TRUE(descriptor_set_->Initialized());

    pipeline_layout_ = VkPipelineLayoutObj(layer_test_.DeviceObj(), {&descriptor_set_->layout_});

    InitPipelineCache();
}

void CreateNVRayTracingPipelineHelper::InitPipelineCache() {
    if (pipeline_cache_ != VK_NULL_HANDLE) {
        vk::DestroyPipelineCache(layer_test_.device(), pipeline_cache_, nullptr);
    }
    VkResult err = vk::CreatePipelineCache(layer_test_.device(), &pc_ci_, NULL, &pipeline_cache_);
    ASSERT_VK_SUCCESS(err);
}

void CreateNVRayTracingPipelineHelper::LateBindPipelineInfo(bool isKHR) {
    // By value or dynamically located items must be late bound
    if (isKHR) {
        rp_ci_KHR_.layout = pipeline_layout_.handle();
        rp_ci_KHR_.stageCount = shader_stages_.size();
        rp_ci_KHR_.pStages = shader_stages_.data();
    } else {
        rp_ci_.layout = pipeline_layout_.handle();
        rp_ci_.stageCount = shader_stages_.size();
        rp_ci_.pStages = shader_stages_.data();
    }
}

VkResult CreateNVRayTracingPipelineHelper::CreateNVRayTracingPipeline(bool implicit_destroy, bool do_late_bind) {
    VkResult err;
    if (do_late_bind) {
        LateBindPipelineInfo();
    }
    if (implicit_destroy && (pipeline_ != VK_NULL_HANDLE)) {
        vk::DestroyPipeline(layer_test_.device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }

    PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV =
        (PFN_vkCreateRayTracingPipelinesNV)vk::GetInstanceProcAddr(layer_test_.instance(), "vkCreateRayTracingPipelinesNV");
    err = vkCreateRayTracingPipelinesNV(layer_test_.device(), pipeline_cache_, 1, &rp_ci_, nullptr, &pipeline_);
    return err;
}

VkResult CreateNVRayTracingPipelineHelper::CreateKHRRayTracingPipeline(bool implicit_destroy, bool do_late_bind) {
    VkResult err;
    if (do_late_bind) {
        LateBindPipelineInfo(true /*isKHR*/);
    }
    if (implicit_destroy && (pipeline_ != VK_NULL_HANDLE)) {
        vk::DestroyPipeline(layer_test_.device(), pipeline_, nullptr);
        pipeline_ = VK_NULL_HANDLE;
    }
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR =
        (PFN_vkCreateRayTracingPipelinesKHR)vk::GetInstanceProcAddr(layer_test_.instance(), "vkCreateRayTracingPipelinesKHR");
    err = vkCreateRayTracingPipelinesKHR(layer_test_.device(), 0, pipeline_cache_, 1, &rp_ci_KHR_, nullptr, &pipeline_);
    return err;
}

BarrierQueueFamilyBase::QueueFamilyObjs::~QueueFamilyObjs() {
    delete command_buffer2;
    delete command_buffer;
    delete command_pool;
    delete queue;
}

void BarrierQueueFamilyBase::QueueFamilyObjs::Init(VkDeviceObj *device, uint32_t qf_index, VkQueue qf_queue,
                                                   VkCommandPoolCreateFlags cp_flags) {
    index = qf_index;
    queue = new VkQueueObj(qf_queue, qf_index);
    command_pool = new VkCommandPoolObj(device, qf_index, cp_flags);
    command_buffer = new VkCommandBufferObj(device, command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, queue);
    command_buffer2 = new VkCommandBufferObj(device, command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, queue);
};

BarrierQueueFamilyBase::Context::Context(VkLayerTest *test, const std::vector<uint32_t> &queue_family_indices) : layer_test(test) {
    if (0 == queue_family_indices.size()) {
        return;  // This is invalid
    }
    VkDeviceObj *device_obj = layer_test->DeviceObj();
    queue_families.reserve(queue_family_indices.size());
    default_index = queue_family_indices[0];
    for (auto qfi : queue_family_indices) {
        VkQueue queue = device_obj->queue_family_queues(qfi)[0]->handle();
        queue_families.emplace(std::make_pair(qfi, QueueFamilyObjs()));
        queue_families[qfi].Init(device_obj, qfi, queue, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    }
    Reset();
}

void BarrierQueueFamilyBase::Context::Reset() {
    layer_test->DeviceObj()->wait();
    for (auto &qf : queue_families) {
        vk::ResetCommandPool(layer_test->device(), qf.second.command_pool->handle(), 0);
    }
}

void BarrierQueueFamilyTestHelper::Init(std::vector<uint32_t> *families, bool image_memory, bool buffer_memory) {
    VkDeviceObj *device_obj = context_->layer_test->DeviceObj();

    image_.Init(32, 32, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_TILING_OPTIMAL, 0, families,
                image_memory);

    ASSERT_TRUE(image_.initialized());

    image_barrier_ = image_.image_memory_barrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, image_.Layout(),
                                                 image_.Layout(), image_.subresource_range(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));

    VkMemoryPropertyFlags mem_prop = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    buffer_.init_as_src_and_dst(*device_obj, 256, mem_prop, families, buffer_memory);
    ASSERT_TRUE(buffer_.initialized());
    buffer_barrier_ = buffer_.buffer_memory_barrier(VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0, VK_WHOLE_SIZE);
}

void Barrier2QueueFamilyTestHelper::Init(std::vector<uint32_t> *families, bool image_memory, bool buffer_memory) {
    VkDeviceObj *device_obj = context_->layer_test->DeviceObj();

    image_.Init(32, 32, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_TILING_OPTIMAL, 0, families,
                image_memory);

    ASSERT_TRUE(image_.initialized());

    image_barrier_ = image_.image_memory_barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                 VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, image_.Layout(),
                                                 image_.Layout(), image_.subresource_range(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1));

    VkMemoryPropertyFlags mem_prop = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    buffer_.init_as_src_and_dst(*device_obj, 256, mem_prop, families, buffer_memory);
    ASSERT_TRUE(buffer_.initialized());
    buffer_barrier_ = buffer_.buffer_memory_barrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                                    VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT, 0, VK_WHOLE_SIZE);
}

BarrierQueueFamilyBase::QueueFamilyObjs *BarrierQueueFamilyBase::GetQueueFamilyInfo(Context *context, uint32_t qfi) {
    QueueFamilyObjs *qf;

    auto qf_it = context->queue_families.find(qfi);
    if (qf_it != context->queue_families.end()) {
        qf = &(qf_it->second);
    } else {
        qf = &(context->queue_families[context->default_index]);
    }
    return qf;
}

void BarrierQueueFamilyTestHelper::operator()(const std::string &img_err, const std::string &buf_err, uint32_t src, uint32_t dst,
                                              uint32_t queue_family_index, Modifier mod) {
    auto &monitor = context_->layer_test->Monitor();
    const bool has_img_err = img_err.size() > 0;
    const bool has_buf_err = buf_err.size() > 0;
    bool positive = !has_img_err && !has_buf_err;
    if (has_img_err) monitor.SetDesiredFailureMsg(kErrorBit | kWarningBit, img_err);
    if (has_buf_err) monitor.SetDesiredFailureMsg(kErrorBit | kWarningBit, buf_err);

    image_barrier_.srcQueueFamilyIndex = src;
    image_barrier_.dstQueueFamilyIndex = dst;
    buffer_barrier_.srcQueueFamilyIndex = src;
    buffer_barrier_.dstQueueFamilyIndex = dst;

    QueueFamilyObjs *qf = GetQueueFamilyInfo(context_, queue_family_index);

    VkCommandBufferObj *command_buffer = qf->command_buffer;
    for (int cb_repeat = 0; cb_repeat < (mod == Modifier::DOUBLE_COMMAND_BUFFER ? 2 : 1); cb_repeat++) {
        command_buffer->begin();
        for (int repeat = 0; repeat < (mod == Modifier::DOUBLE_RECORD ? 2 : 1); repeat++) {
            vk::CmdPipelineBarrier(command_buffer->handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                   VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &buffer_barrier_, 1, &image_barrier_);
        }
        command_buffer->end();
        command_buffer = qf->command_buffer2;  // Second pass (if any) goes to the secondary command_buffer.
    }

    if (queue_family_index != kInvalidQueueFamily) {
        if (mod == Modifier::DOUBLE_COMMAND_BUFFER) {
            // the Fence resolves to VK_NULL_HANLE... i.e. no fence
            qf->queue->submit({{qf->command_buffer, qf->command_buffer2}}, vk_testing::Fence(), positive);
        } else {
            qf->command_buffer->QueueCommandBuffer(positive);  // Check for success on positive tests only
        }
    }

    if (!positive) {
        monitor.VerifyFound();
    }
    context_->Reset();
};

void Barrier2QueueFamilyTestHelper::operator()(const std::string &img_err, const std::string &buf_err, uint32_t src, uint32_t dst,
                                               uint32_t queue_family_index, Modifier mod) {
    auto &monitor = context_->layer_test->Monitor();
    bool positive = true;
    if (img_err.length()) {
        monitor.SetDesiredFailureMsg(kErrorBit | kWarningBit, img_err);
        positive = false;
    }
    if (buf_err.length()) {
        monitor.SetDesiredFailureMsg(kErrorBit | kWarningBit, buf_err);
        positive = false;
    }

    image_barrier_.srcQueueFamilyIndex = src;
    image_barrier_.dstQueueFamilyIndex = dst;
    buffer_barrier_.srcQueueFamilyIndex = src;
    buffer_barrier_.dstQueueFamilyIndex = dst;

    auto dep_info = LvlInitStruct<VkDependencyInfoKHR>();
    dep_info.bufferMemoryBarrierCount = 1;
    dep_info.pBufferMemoryBarriers = &buffer_barrier_;
    dep_info.imageMemoryBarrierCount = 1;
    dep_info.pImageMemoryBarriers = &image_barrier_;

    QueueFamilyObjs *qf = GetQueueFamilyInfo(context_, queue_family_index);

    VkCommandBufferObj *command_buffer = qf->command_buffer;
    for (int cb_repeat = 0; cb_repeat < (mod == Modifier::DOUBLE_COMMAND_BUFFER ? 2 : 1); cb_repeat++) {
        command_buffer->begin();
        for (int repeat = 0; repeat < (mod == Modifier::DOUBLE_RECORD ? 2 : 1); repeat++) {
            command_buffer->PipelineBarrier2KHR(&dep_info);
        }
        command_buffer->end();
        command_buffer = qf->command_buffer2;  // Second pass (if any) goes to the secondary command_buffer.
    }

    if (queue_family_index != kInvalidQueueFamily) {
        if (mod == Modifier::DOUBLE_COMMAND_BUFFER) {
            // the Fence resolves to VK_NULL_HANLE... i.e. no fence
            qf->queue->submit({{qf->command_buffer, qf->command_buffer2}}, vk_testing::Fence(), positive);
        } else {
            qf->command_buffer->QueueCommandBuffer(positive);  // Check for success on positive tests only
        }
    }

    if (!positive) {
        monitor.VerifyFound();
    }
    context_->Reset();
};

bool InitFrameworkForRayTracingTest(VkRenderFramework *framework, bool is_khr, VkPhysicalDeviceFeatures2KHR *features2,
                                    VkValidationFeaturesEXT *enabled_features) {
    framework->AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    framework->AddRequiredExtensions(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    if (is_khr) {
        framework->AddRequiredExtensions(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        framework->AddRequiredExtensions(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        framework->AddRequiredExtensions(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        framework->AddRequiredExtensions(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
        framework->AddRequiredExtensions(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        framework->AddRequiredExtensions(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        framework->AddRequiredExtensions(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        framework->AddRequiredExtensions(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME);
    } else {
        framework->AddRequiredExtensions(VK_NV_RAY_TRACING_EXTENSION_NAME);
    }

    framework->InitFramework(&framework->Monitor(), enabled_features);
    if (!framework->AreRequiredExtensionsEnabled()) {
        printf("%s device extension not supported, skipping test\n", framework->RequiredExtensionsNotSupported().c_str());
        return false;
    }

    if (features2) {
        // extension enabled as dependency of RT extension
        auto vkGetPhysicalDeviceFeatures2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(
            vk::GetInstanceProcAddr(framework->instance(), "vkGetPhysicalDeviceFeatures2KHR"));
        assert(vkGetPhysicalDeviceFeatures2KHR);
        vkGetPhysicalDeviceFeatures2KHR(framework->gpu(), features2);
    }
    return true;
}

void GetSimpleGeometryForAccelerationStructureTests(const VkDeviceObj &device, VkBufferObj *vbo, VkBufferObj *ibo,
                                                    VkGeometryNV *geometry, VkDeviceSize offset, bool buffer_device_address) {
    VkBufferUsageFlags usage =
        VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    auto alloc_flags = LvlInitStruct<VkMemoryAllocateFlagsInfo>();
    void *alloc_pnext = nullptr;
    if (buffer_device_address) {
        usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        alloc_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        alloc_pnext = &alloc_flags;
    }
    vbo->init(device, 1024, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, usage, alloc_pnext);
    ibo->init(device, 1024, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, usage, alloc_pnext);

    constexpr std::array vertices = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f};
    constexpr std::array<uint32_t, 3> indicies = {{0, 1, 2}};

    uint8_t *mapped_vbo_buffer_data = (uint8_t *)vbo->memory().map();
    std::memcpy(mapped_vbo_buffer_data + offset, (uint8_t *)vertices.data(), sizeof(float) * vertices.size());
    vbo->memory().unmap();

    uint8_t *mapped_ibo_buffer_data = (uint8_t *)ibo->memory().map();
    std::memcpy(mapped_ibo_buffer_data + offset, (uint8_t *)indicies.data(), sizeof(uint32_t) * indicies.size());
    ibo->memory().unmap();

    *geometry = {};
    geometry->sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
    geometry->geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;
    geometry->geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
    geometry->geometry.triangles.vertexData = vbo->handle();
    geometry->geometry.triangles.vertexOffset = 0;
    geometry->geometry.triangles.vertexCount = 3;
    geometry->geometry.triangles.vertexStride = 12;
    geometry->geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry->geometry.triangles.indexData = ibo->handle();
    geometry->geometry.triangles.indexOffset = 0;
    geometry->geometry.triangles.indexCount = 3;
    geometry->geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry->geometry.triangles.transformData = VK_NULL_HANDLE;
    geometry->geometry.triangles.transformOffset = 0;
    geometry->geometry.aabbs = {};
    geometry->geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
}

std::pair<VkBufferObj &&, VkAccelerationStructureGeometryKHR> GetSimpleAABB(const VkDeviceObj &device,
                                                                            uint32_t vk_api_version /*= VK_API_VERSION_1_2*/) {
    const std::array<VkAabbPositionsKHR, 1> aabbs = {{{-1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f}}};

    const VkDeviceSize aabb_buffer_size = sizeof(aabbs[0]) * aabbs.size();
    VkBufferObj aabb_buffer;
    auto alloc_flags = LvlInitStruct<VkMemoryAllocateFlagsInfo>();
    alloc_flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

    aabb_buffer.init(
        device, aabb_buffer_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        &alloc_flags);

    uint8_t *mapped_aabb_buffer_data = (uint8_t *)aabb_buffer.memory().map();
    std::memcpy(mapped_aabb_buffer_data, (uint8_t *)aabbs.data(), static_cast<std::size_t>(aabb_buffer_size));
    aabb_buffer.memory().unmap();

    VkDeviceOrHostAddressConstKHR address;
    address.deviceAddress = aabb_buffer.address(vk_api_version);

    auto as_geom = LvlInitStruct<VkAccelerationStructureGeometryKHR>();
    as_geom.geometryType = VK_GEOMETRY_TYPE_AABBS_KHR;
    as_geom.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
    as_geom.geometry.aabbs.data = address;
    as_geom.geometry.aabbs.stride = static_cast<VkDeviceSize>(sizeof(aabbs[0]));

    return {std::move(aabb_buffer), as_geom};
}

void VkLayerTest::OOBRayTracingShadersTestBody(bool gpu_assisted) {
    SetTargetApiVersion(VK_API_VERSION_1_1);
    AddRequiredExtensions(VK_NV_RAY_TRACING_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    AddOptionalExtensions(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    VkValidationFeatureEnableEXT validation_feature_enables[] = {VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT};
    VkValidationFeatureDisableEXT validation_feature_disables[] = {
        VK_VALIDATION_FEATURE_DISABLE_THREAD_SAFETY_EXT, VK_VALIDATION_FEATURE_DISABLE_API_PARAMETERS_EXT,
        VK_VALIDATION_FEATURE_DISABLE_OBJECT_LIFETIMES_EXT, VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT};
    VkValidationFeaturesEXT validation_features = {};
    validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validation_features.enabledValidationFeatureCount = 1;
    validation_features.pEnabledValidationFeatures = validation_feature_enables;
    validation_features.disabledValidationFeatureCount = 4;
    validation_features.pDisabledValidationFeatures = validation_feature_disables;
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor, gpu_assisted ? &validation_features : nullptr));
    bool descriptor_indexing = IsExtensionsEnabled(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    if (gpu_assisted && IsPlatform(kMockICD)) {
        GTEST_SKIP() << "GPU-AV can't run on MockICD";
    }

    VkPhysicalDeviceFeatures2KHR features2 = LvlInitStruct<VkPhysicalDeviceFeatures2KHR>();
    auto indexing_features = LvlInitStruct<VkPhysicalDeviceDescriptorIndexingFeaturesEXT>();
    if (descriptor_indexing) {
        features2 = GetPhysicalDeviceFeatures2(indexing_features);
        if (!indexing_features.runtimeDescriptorArray || !indexing_features.descriptorBindingPartiallyBound ||
            !indexing_features.descriptorBindingSampledImageUpdateAfterBind ||
            !indexing_features.descriptorBindingVariableDescriptorCount) {
            printf("Not all descriptor indexing features supported, skipping descriptor indexing tests\n");
            descriptor_indexing = false;
        }
    }

    VkCommandPoolCreateFlags pool_flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2, pool_flags));

    PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR =
        (PFN_vkGetPhysicalDeviceProperties2KHR)vk::GetInstanceProcAddr(instance(), "vkGetPhysicalDeviceProperties2KHR");
    ASSERT_TRUE(vkGetPhysicalDeviceProperties2KHR != nullptr);

    auto ray_tracing_properties = LvlInitStruct<VkPhysicalDeviceRayTracingPropertiesNV>();
    auto properties2 = LvlInitStruct<VkPhysicalDeviceProperties2KHR>(&ray_tracing_properties);
    vkGetPhysicalDeviceProperties2KHR(gpu(), &properties2);
    if (ray_tracing_properties.maxTriangleCount == 0) {
        GTEST_SKIP() << "Did not find required ray tracing properties";
    }

    VkQueue ray_tracing_queue = m_device->m_queue;
    uint32_t ray_tracing_queue_family_index = 0;

    // If supported, run on the compute only queue.
    const std::optional<uint32_t> compute_only_queue_family_index =
        m_device->QueueFamilyMatching(VK_QUEUE_COMPUTE_BIT, VK_QUEUE_GRAPHICS_BIT);
    if (compute_only_queue_family_index) {
        const auto &compute_only_queues = m_device->queue_family_queues(compute_only_queue_family_index.value());
        if (!compute_only_queues.empty()) {
            ray_tracing_queue = compute_only_queues[0]->handle();
            ray_tracing_queue_family_index = compute_only_queue_family_index.value();
        }
    }

    VkCommandPoolObj ray_tracing_command_pool(m_device, ray_tracing_queue_family_index,
                                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBufferObj ray_tracing_command_buffer(m_device, &ray_tracing_command_pool);

    constexpr std::array<VkAabbPositionsKHR, 1> aabbs = {{{-1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f}}};

    struct VkGeometryInstanceNV {
        float transform[12];
        uint32_t instanceCustomIndex : 24;
        uint32_t mask : 8;
        uint32_t instanceOffset : 24;
        uint32_t flags : 8;
        uint64_t accelerationStructureHandle;
    };

    const VkDeviceSize aabb_buffer_size = sizeof(VkAabbPositionsKHR) * aabbs.size();
    VkBufferObj aabb_buffer;
    aabb_buffer.init(*m_device, aabb_buffer_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, {ray_tracing_queue_family_index});

    uint8_t *mapped_aabb_buffer_data = (uint8_t *)aabb_buffer.memory().map();
    std::memcpy(mapped_aabb_buffer_data, (uint8_t *)aabbs.data(), static_cast<std::size_t>(aabb_buffer_size));
    aabb_buffer.memory().unmap();

    VkGeometryNV geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
    geometry.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
    geometry.geometry.triangles = {};
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
    geometry.geometry.aabbs = {};
    geometry.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;
    geometry.geometry.aabbs.aabbData = aabb_buffer.handle();
    geometry.geometry.aabbs.numAABBs = static_cast<uint32_t>(aabbs.size());
    geometry.geometry.aabbs.offset = 0;
    geometry.geometry.aabbs.stride = static_cast<VkDeviceSize>(sizeof(VkAabbPositionsKHR));
    geometry.flags = 0;

    VkAccelerationStructureInfoNV bot_level_as_info = LvlInitStruct<VkAccelerationStructureInfoNV>();
    bot_level_as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;
    bot_level_as_info.instanceCount = 0;
    bot_level_as_info.geometryCount = 1;
    bot_level_as_info.pGeometries = &geometry;

    VkAccelerationStructureCreateInfoNV bot_level_as_create_info = LvlInitStruct<VkAccelerationStructureCreateInfoNV>();
    bot_level_as_create_info.info = bot_level_as_info;

    VkAccelerationStructureObj bot_level_as(*m_device, bot_level_as_create_info);

    const std::array instances = {
        VkGeometryInstanceNV{
            {
                // clang-format off
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                // clang-format on
            },
            0,
            0xFF,
            0,
            VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV,
            bot_level_as.opaque_handle(),
        },
    };

    VkDeviceSize instance_buffer_size = sizeof(instances[0]) * instances.size();
    VkBufferObj instance_buffer;
    instance_buffer.init(*m_device, instance_buffer_size,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, {ray_tracing_queue_family_index});

    uint8_t *mapped_instance_buffer_data = (uint8_t *)instance_buffer.memory().map();
    std::memcpy(mapped_instance_buffer_data, (uint8_t *)instances.data(), static_cast<std::size_t>(instance_buffer_size));
    instance_buffer.memory().unmap();

    VkAccelerationStructureInfoNV top_level_as_info = LvlInitStruct<VkAccelerationStructureInfoNV>();
    top_level_as_info.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
    top_level_as_info.instanceCount = 1;
    top_level_as_info.geometryCount = 0;

    VkAccelerationStructureCreateInfoNV top_level_as_create_info = LvlInitStruct<VkAccelerationStructureCreateInfoNV>();
    top_level_as_create_info.info = top_level_as_info;

    VkAccelerationStructureObj top_level_as(*m_device, top_level_as_create_info);

    VkDeviceSize scratch_buffer_size = std::max(bot_level_as.build_scratch_memory_requirements().memoryRequirements.size,
                                                top_level_as.build_scratch_memory_requirements().memoryRequirements.size);
    VkBufferObj scratch_buffer;
    scratch_buffer.init(*m_device, scratch_buffer_size, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_BUFFER_USAGE_RAY_TRACING_BIT_NV);

    ray_tracing_command_buffer.begin();

    // Build bot level acceleration structure
    ray_tracing_command_buffer.BuildAccelerationStructure(&bot_level_as, scratch_buffer.handle());

    // Barrier to prevent using scratch buffer for top level build before bottom level build finishes
    VkMemoryBarrier memory_barrier = LvlInitStruct<VkMemoryBarrier>();
    memory_barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
    memory_barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV;
    ray_tracing_command_buffer.PipelineBarrier(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
                                               VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV, 0, 1, &memory_barrier, 0,
                                               nullptr, 0, nullptr);

    // Build top level acceleration structure
    ray_tracing_command_buffer.BuildAccelerationStructure(&top_level_as, scratch_buffer.handle(), instance_buffer.handle());

    ray_tracing_command_buffer.end();

    VkSubmitInfo submit_info = LvlInitStruct<VkSubmitInfo>();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &ray_tracing_command_buffer.handle();
    vk::QueueSubmit(ray_tracing_queue, 1, &submit_info, VK_NULL_HANDLE);
    vk::QueueWaitIdle(ray_tracing_queue);

    VkTextureObj texture(m_device, nullptr);
    VkSamplerObj sampler(m_device);

    VkDeviceSize storage_buffer_size = 1024;
    VkBufferObj storage_buffer;
    storage_buffer.init(*m_device, storage_buffer_size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, {ray_tracing_queue_family_index});

    VkDeviceSize shader_binding_table_buffer_size = ray_tracing_properties.shaderGroupBaseAlignment * 4ull;
    VkBufferObj shader_binding_table_buffer;
    shader_binding_table_buffer.init(*m_device, shader_binding_table_buffer_size,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     VK_BUFFER_USAGE_RAY_TRACING_BIT_NV, {ray_tracing_queue_family_index});

    // Setup descriptors!
    const VkShaderStageFlags kAllRayTracingStages = VK_SHADER_STAGE_RAYGEN_BIT_NV | VK_SHADER_STAGE_ANY_HIT_BIT_NV |
                                                    VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_MISS_BIT_NV |
                                                    VK_SHADER_STAGE_INTERSECTION_BIT_NV | VK_SHADER_STAGE_CALLABLE_BIT_NV;

    void *layout_pnext = nullptr;
    void *allocate_pnext = nullptr;
    VkDescriptorPoolCreateFlags pool_create_flags = 0;
    VkDescriptorSetLayoutCreateFlags layout_create_flags = 0;
    VkDescriptorBindingFlagsEXT ds_binding_flags[3] = {};
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT layout_createinfo_binding_flags[1] = {};
    if (descriptor_indexing) {
        ds_binding_flags[0] = 0;
        ds_binding_flags[1] = 0;
        ds_binding_flags[2] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;

        layout_createinfo_binding_flags[0] = LvlInitStruct<VkDescriptorSetLayoutBindingFlagsCreateInfoEXT>();
        layout_createinfo_binding_flags[0].bindingCount = 3;
        layout_createinfo_binding_flags[0].pBindingFlags = ds_binding_flags;
        layout_create_flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
        pool_create_flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
        layout_pnext = layout_createinfo_binding_flags;
    }

    // Prepare descriptors
    OneOffDescriptorSet ds(m_device,
                           {
                               {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1, kAllRayTracingStages, nullptr},
                               {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kAllRayTracingStages, nullptr},
                               {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6, kAllRayTracingStages, nullptr},
                           },
                           layout_create_flags, layout_pnext, pool_create_flags);

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variable_count =
        LvlInitStruct<VkDescriptorSetVariableDescriptorCountAllocateInfoEXT>();
    uint32_t desc_counts;
    if (descriptor_indexing) {
        layout_create_flags = 0;
        pool_create_flags = 0;
        ds_binding_flags[2] =
            VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
        desc_counts = 6;  // We'll reserve 8 spaces in the layout, but the descriptor will only use 6
        variable_count.descriptorSetCount = 1;
        variable_count.pDescriptorCounts = &desc_counts;
        allocate_pnext = &variable_count;
    }

    OneOffDescriptorSet ds_variable(m_device,
                                    {
                                        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV, 1, kAllRayTracingStages, nullptr},
                                        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kAllRayTracingStages, nullptr},
                                        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8, kAllRayTracingStages, nullptr},
                                    },
                                    layout_create_flags, layout_pnext, pool_create_flags, allocate_pnext);

    VkAccelerationStructureNV top_level_as_handle = top_level_as.handle();
    VkWriteDescriptorSetAccelerationStructureNV write_descript_set_as =
        LvlInitStruct<VkWriteDescriptorSetAccelerationStructureNV>();
    write_descript_set_as.accelerationStructureCount = 1;
    write_descript_set_as.pAccelerationStructures = &top_level_as_handle;

    VkDescriptorBufferInfo descriptor_buffer_info = {};
    descriptor_buffer_info.buffer = storage_buffer.handle();
    descriptor_buffer_info.offset = 0;
    descriptor_buffer_info.range = storage_buffer_size;

    VkDescriptorImageInfo descriptor_image_infos[6] = {};
    for (int i = 0; i < 6; i++) {
        descriptor_image_infos[i] = texture.DescriptorImageInfo();
        descriptor_image_infos[i].sampler = sampler.handle();
        descriptor_image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet descriptor_writes[3] = {};
    descriptor_writes[0] = LvlInitStruct<VkWriteDescriptorSet>(&write_descript_set_as);
    descriptor_writes[0].dstSet = ds.set_;
    descriptor_writes[0].dstBinding = 0;
    descriptor_writes[0].descriptorCount = 1;
    descriptor_writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

    descriptor_writes[1] = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_writes[1].dstSet = ds.set_;
    descriptor_writes[1].dstBinding = 1;
    descriptor_writes[1].descriptorCount = 1;
    descriptor_writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_writes[1].pBufferInfo = &descriptor_buffer_info;

    descriptor_writes[2] = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_writes[2].dstSet = ds.set_;
    descriptor_writes[2].dstBinding = 2;
    if (descriptor_indexing) {
        descriptor_writes[2].descriptorCount = 5;  // Intentionally don't write index 5
    } else {
        descriptor_writes[2].descriptorCount = 6;
    }
    descriptor_writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_writes[2].pImageInfo = descriptor_image_infos;
    vk::UpdateDescriptorSets(m_device->device(), 3, descriptor_writes, 0, NULL);
    if (descriptor_indexing) {
        descriptor_writes[0].dstSet = ds_variable.set_;
        descriptor_writes[1].dstSet = ds_variable.set_;
        descriptor_writes[2].dstSet = ds_variable.set_;
        vk::UpdateDescriptorSets(m_device->device(), 3, descriptor_writes, 0, NULL);
    }

    const VkPipelineLayoutObj pipeline_layout(m_device, {&ds.layout_});
    const VkPipelineLayoutObj pipeline_layout_variable(m_device, {&ds_variable.layout_});

    const auto SetImagesArrayLength = [](const std::string &shader_template, const std::string &length_str) {
        const std::string to_replace = "IMAGES_ARRAY_LENGTH";

        std::string result = shader_template;
        auto position = result.find(to_replace);
        assert(position != std::string::npos);
        result.replace(position, to_replace.length(), length_str);
        return result;
    };

    const std::string rgen_source_template = R"(#version 460
        #extension GL_EXT_nonuniform_qualifier : require
        #extension GL_EXT_samplerless_texture_functions : require
        #extension GL_NV_ray_tracing : require

        layout(set = 0, binding = 0) uniform accelerationStructureNV topLevelAS;
        layout(set = 0, binding = 1, std430) buffer RayTracingSbo {
	        uint rgen_index;
	        uint ahit_index;
	        uint chit_index;
	        uint miss_index;
	        uint intr_index;
	        uint call_index;

	        uint rgen_ran;
	        uint ahit_ran;
	        uint chit_ran;
	        uint miss_ran;
	        uint intr_ran;
	        uint call_ran;

	        float result1;
	        float result2;
	        float result3;
        } sbo;
        layout(set = 0, binding = 2) uniform texture2D textures[IMAGES_ARRAY_LENGTH];

        layout(location = 0) rayPayloadNV vec3 payload;
        layout(location = 3) callableDataNV vec3 callableData;

        void main() {
            sbo.rgen_ran = 1;

	        executeCallableNV(0, 3);
	        sbo.result1 = callableData.x;

	        vec3 origin = vec3(0.0f, 0.0f, -2.0f);
	        vec3 direction = vec3(0.0f, 0.0f, 1.0f);

	        traceNV(topLevelAS, gl_RayFlagsNoneNV, 0xFF, 0, 1, 0, origin, 0.001, direction, 10000.0, 0);
	        sbo.result2 = payload.x;

	        traceNV(topLevelAS, gl_RayFlagsNoneNV, 0xFF, 0, 1, 0, origin, 0.001, -direction, 10000.0, 0);
	        sbo.result3 = payload.x;

            if (sbo.rgen_index > 0) {
                // OOB here:
                sbo.result3 = texelFetch(textures[sbo.rgen_index], ivec2(0, 0), 0).x;
            }
        }
        )";

    const std::string rgen_source = SetImagesArrayLength(rgen_source_template, "6");
    const std::string rgen_source_runtime = SetImagesArrayLength(rgen_source_template, "");

    const std::string ahit_source_template = R"(#version 460
        #extension GL_EXT_nonuniform_qualifier : require
        #extension GL_EXT_samplerless_texture_functions : require
        #extension GL_NV_ray_tracing : require

        layout(set = 0, binding = 1, std430) buffer StorageBuffer {
	        uint rgen_index;
	        uint ahit_index;
	        uint chit_index;
	        uint miss_index;
	        uint intr_index;
	        uint call_index;

	        uint rgen_ran;
	        uint ahit_ran;
	        uint chit_ran;
	        uint miss_ran;
	        uint intr_ran;
	        uint call_ran;

	        float result1;
	        float result2;
	        float result3;
        } sbo;
        layout(set = 0, binding = 2) uniform texture2D textures[IMAGES_ARRAY_LENGTH];

        hitAttributeNV vec3 hitValue;

        layout(location = 0) rayPayloadInNV vec3 payload;

        void main() {
	        sbo.ahit_ran = 2;

	        payload = vec3(0.1234f);

            if (sbo.ahit_index > 0) {
                // OOB here:
                payload.x = texelFetch(textures[sbo.ahit_index], ivec2(0, 0), 0).x;
            }
        }
    )";
    const std::string ahit_source = SetImagesArrayLength(ahit_source_template, "6");
    const std::string ahit_source_runtime = SetImagesArrayLength(ahit_source_template, "");

    const std::string chit_source_template = R"(#version 460
        #extension GL_EXT_nonuniform_qualifier : require
        #extension GL_EXT_samplerless_texture_functions : require
        #extension GL_NV_ray_tracing : require

        layout(set = 0, binding = 1, std430) buffer RayTracingSbo {
	        uint rgen_index;
	        uint ahit_index;
	        uint chit_index;
	        uint miss_index;
	        uint intr_index;
	        uint call_index;

	        uint rgen_ran;
	        uint ahit_ran;
	        uint chit_ran;
	        uint miss_ran;
	        uint intr_ran;
	        uint call_ran;

	        float result1;
	        float result2;
	        float result3;
        } sbo;
        layout(set = 0, binding = 2) uniform texture2D textures[IMAGES_ARRAY_LENGTH];

        layout(location = 0) rayPayloadInNV vec3 payload;

        hitAttributeNV vec3 attribs;

        void main() {
            sbo.chit_ran = 3;

            payload = attribs;
            if (sbo.chit_index > 0) {
                // OOB here:
                payload.x = texelFetch(textures[sbo.chit_index], ivec2(0, 0), 0).x;
            }
        }
        )";
    const std::string chit_source = SetImagesArrayLength(chit_source_template, "6");
    const std::string chit_source_runtime = SetImagesArrayLength(chit_source_template, "");

    const std::string miss_source_template = R"(#version 460
        #extension GL_EXT_nonuniform_qualifier : enable
        #extension GL_EXT_samplerless_texture_functions : require
        #extension GL_NV_ray_tracing : require

        layout(set = 0, binding = 1, std430) buffer RayTracingSbo {
	        uint rgen_index;
	        uint ahit_index;
	        uint chit_index;
	        uint miss_index;
	        uint intr_index;
	        uint call_index;

	        uint rgen_ran;
	        uint ahit_ran;
	        uint chit_ran;
	        uint miss_ran;
	        uint intr_ran;
	        uint call_ran;

	        float result1;
	        float result2;
	        float result3;
        } sbo;
        layout(set = 0, binding = 2) uniform texture2D textures[IMAGES_ARRAY_LENGTH];

        layout(location = 0) rayPayloadInNV vec3 payload;

        void main() {
            sbo.miss_ran = 4;

            payload = vec3(1.0, 0.0, 0.0);

            if (sbo.miss_index > 0) {
                // OOB here:
                payload.x = texelFetch(textures[sbo.miss_index], ivec2(0, 0), 0).x;
            }
        }
    )";
    const std::string miss_source = SetImagesArrayLength(miss_source_template, "6");
    const std::string miss_source_runtime = SetImagesArrayLength(miss_source_template, "");

    const std::string intr_source_template = R"(#version 460
        #extension GL_EXT_nonuniform_qualifier : require
        #extension GL_EXT_samplerless_texture_functions : require
        #extension GL_NV_ray_tracing : require

        layout(set = 0, binding = 1, std430) buffer StorageBuffer {
	        uint rgen_index;
	        uint ahit_index;
	        uint chit_index;
	        uint miss_index;
	        uint intr_index;
	        uint call_index;

	        uint rgen_ran;
	        uint ahit_ran;
	        uint chit_ran;
	        uint miss_ran;
	        uint intr_ran;
	        uint call_ran;

	        float result1;
	        float result2;
	        float result3;
        } sbo;
        layout(set = 0, binding = 2) uniform texture2D textures[IMAGES_ARRAY_LENGTH];

        hitAttributeNV vec3 hitValue;

        void main() {
	        sbo.intr_ran = 5;

	        hitValue = vec3(0.0f, 0.5f, 0.0f);

	        reportIntersectionNV(1.0f, 0);

            if (sbo.intr_index > 0) {
                // OOB here:
                hitValue.x = texelFetch(textures[sbo.intr_index], ivec2(0, 0), 0).x;
            }
        }
    )";
    const std::string intr_source = SetImagesArrayLength(intr_source_template, "6");
    const std::string intr_source_runtime = SetImagesArrayLength(intr_source_template, "");

    const std::string call_source_template = R"(#version 460
        #extension GL_EXT_nonuniform_qualifier : require
        #extension GL_EXT_samplerless_texture_functions : require
        #extension GL_NV_ray_tracing : require

        layout(set = 0, binding = 1, std430) buffer StorageBuffer {
	        uint rgen_index;
	        uint ahit_index;
	        uint chit_index;
	        uint miss_index;
	        uint intr_index;
	        uint call_index;

	        uint rgen_ran;
	        uint ahit_ran;
	        uint chit_ran;
	        uint miss_ran;
	        uint intr_ran;
	        uint call_ran;

	        float result1;
	        float result2;
	        float result3;
        } sbo;
        layout(set = 0, binding = 2) uniform texture2D textures[IMAGES_ARRAY_LENGTH];

        layout(location = 3) callableDataInNV vec3 callableData;

        void main() {
	        sbo.call_ran = 6;

	        callableData = vec3(0.1234f);

            if (sbo.call_index > 0) {
                // OOB here:
                callableData.x = texelFetch(textures[sbo.call_index], ivec2(0, 0), 0).x;
            }
        }
    )";
    const std::string call_source = SetImagesArrayLength(call_source_template, "6");
    const std::string call_source_runtime = SetImagesArrayLength(call_source_template, "");

    struct TestCase {
        const std::string &rgen_shader_source;
        const std::string &ahit_shader_source;
        const std::string &chit_shader_source;
        const std::string &miss_shader_source;
        const std::string &intr_shader_source;
        const std::string &call_shader_source;
        bool variable_length;
        uint32_t rgen_index;
        uint32_t ahit_index;
        uint32_t chit_index;
        uint32_t miss_index;
        uint32_t intr_index;
        uint32_t call_index;
        const char *expected_error;
    };

    std::vector<TestCase> tests;
    tests.push_back({rgen_source, ahit_source, chit_source, miss_source, intr_source, call_source, false, 25, 0, 0, 0, 0, 0,
                     "Index of 25 used to index descriptor array of length 6."});
    tests.push_back({rgen_source, ahit_source, chit_source, miss_source, intr_source, call_source, false, 0, 25, 0, 0, 0, 0,
                     "Index of 25 used to index descriptor array of length 6."});
    tests.push_back({rgen_source, ahit_source, chit_source, miss_source, intr_source, call_source, false, 0, 0, 25, 0, 0, 0,
                     "Index of 25 used to index descriptor array of length 6."});
    tests.push_back({rgen_source, ahit_source, chit_source, miss_source, intr_source, call_source, false, 0, 0, 0, 25, 0, 0,
                     "Index of 25 used to index descriptor array of length 6."});
    tests.push_back({rgen_source, ahit_source, chit_source, miss_source, intr_source, call_source, false, 0, 0, 0, 0, 25, 0,
                     "Index of 25 used to index descriptor array of length 6."});
    tests.push_back({rgen_source, ahit_source, chit_source, miss_source, intr_source, call_source, false, 0, 0, 0, 0, 0, 25,
                     "Index of 25 used to index descriptor array of length 6."});

    if (descriptor_indexing) {
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 25, 0, 0, 0, 0, 0, "Index of 25 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 25, 0, 0, 0, 0, "Index of 25 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 25, 0, 0, 0, "Index of 25 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 25, 0, 0, "Index of 25 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 0, 25, 0, "Index of 25 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 0, 0, 25, "Index of 25 used to index descriptor array of length 6."});

        // For this group, 6 is less than max specified (max specified is 8) but more than actual specified (actual specified is 5)
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 6, 0, 0, 0, 0, 0, "Index of 6 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 6, 0, 0, 0, 0, "Index of 6 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 6, 0, 0, 0, "Index of 6 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 6, 0, 0, "Index of 6 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 0, 6, 0, "Index of 6 used to index descriptor array of length 6."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 0, 0, 6, "Index of 6 used to index descriptor array of length 6."});

        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 5, 0, 0, 0, 0, 0, "Descriptor index 5 is uninitialized."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 5, 0, 0, 0, 0, "Descriptor index 5 is uninitialized."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 5, 0, 0, 0, "Descriptor index 5 is uninitialized."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 5, 0, 0, "Descriptor index 5 is uninitialized."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 0, 5, 0, "Descriptor index 5 is uninitialized."});
        tests.push_back({rgen_source_runtime, ahit_source_runtime, chit_source_runtime, miss_source_runtime, intr_source_runtime,
                         call_source_runtime, true, 0, 0, 0, 0, 0, 5, "Descriptor index 5 is uninitialized."});
    }

    PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV = reinterpret_cast<PFN_vkCreateRayTracingPipelinesNV>(
        vk::GetDeviceProcAddr(m_device->handle(), "vkCreateRayTracingPipelinesNV"));
    ASSERT_TRUE(vkCreateRayTracingPipelinesNV != nullptr);

    PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV =
        reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesNV>(
            vk::GetDeviceProcAddr(m_device->handle(), "vkGetRayTracingShaderGroupHandlesNV"));
    ASSERT_TRUE(vkGetRayTracingShaderGroupHandlesNV != nullptr);

    PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV =
        reinterpret_cast<PFN_vkCmdTraceRaysNV>(vk::GetDeviceProcAddr(m_device->handle(), "vkCmdTraceRaysNV"));
    ASSERT_TRUE(vkCmdTraceRaysNV != nullptr);

    // Iteration 0 tests with no descriptor set bound (to sanity test "draw" validation). Iteration 1
    // tests what's in the test case vector.
    for (const auto &test : tests) {
        if (gpu_assisted) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, test.expected_error);
        }

        VkShaderObj rgen_shader(this, test.rgen_shader_source.c_str(), VK_SHADER_STAGE_RAYGEN_BIT_NV);
        VkShaderObj ahit_shader(this, test.ahit_shader_source.c_str(), VK_SHADER_STAGE_ANY_HIT_BIT_NV);
        VkShaderObj chit_shader(this, test.chit_shader_source.c_str(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV);
        VkShaderObj miss_shader(this, test.miss_shader_source.c_str(), VK_SHADER_STAGE_MISS_BIT_NV);
        VkShaderObj intr_shader(this, test.intr_shader_source.c_str(), VK_SHADER_STAGE_INTERSECTION_BIT_NV);
        VkShaderObj call_shader(this, test.call_shader_source.c_str(), VK_SHADER_STAGE_CALLABLE_BIT_NV);

        VkPipelineShaderStageCreateInfo stage_create_infos[6] = {};
        stage_create_infos[0] = LvlInitStruct<VkPipelineShaderStageCreateInfo>();
        stage_create_infos[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_NV;
        stage_create_infos[0].module = rgen_shader.handle();
        stage_create_infos[0].pName = "main";

        stage_create_infos[1] = LvlInitStruct<VkPipelineShaderStageCreateInfo>();
        stage_create_infos[1].stage = VK_SHADER_STAGE_ANY_HIT_BIT_NV;
        stage_create_infos[1].module = ahit_shader.handle();
        stage_create_infos[1].pName = "main";

        stage_create_infos[2] = LvlInitStruct<VkPipelineShaderStageCreateInfo>();
        stage_create_infos[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
        stage_create_infos[2].module = chit_shader.handle();
        stage_create_infos[2].pName = "main";

        stage_create_infos[3] = LvlInitStruct<VkPipelineShaderStageCreateInfo>();
        stage_create_infos[3].stage = VK_SHADER_STAGE_MISS_BIT_NV;
        stage_create_infos[3].module = miss_shader.handle();
        stage_create_infos[3].pName = "main";

        stage_create_infos[4] = LvlInitStruct<VkPipelineShaderStageCreateInfo>();
        stage_create_infos[4].stage = VK_SHADER_STAGE_INTERSECTION_BIT_NV;
        stage_create_infos[4].module = intr_shader.handle();
        stage_create_infos[4].pName = "main";

        stage_create_infos[5] = LvlInitStruct<VkPipelineShaderStageCreateInfo>();
        stage_create_infos[5].stage = VK_SHADER_STAGE_CALLABLE_BIT_NV;
        stage_create_infos[5].module = call_shader.handle();
        stage_create_infos[5].pName = "main";

        VkRayTracingShaderGroupCreateInfoNV group_create_infos[4] = {};
        group_create_infos[0] = LvlInitStruct<VkRayTracingShaderGroupCreateInfoNV>();
        group_create_infos[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        group_create_infos[0].generalShader = 0;  // rgen
        group_create_infos[0].closestHitShader = VK_SHADER_UNUSED_NV;
        group_create_infos[0].anyHitShader = VK_SHADER_UNUSED_NV;
        group_create_infos[0].intersectionShader = VK_SHADER_UNUSED_NV;

        group_create_infos[1] = LvlInitStruct<VkRayTracingShaderGroupCreateInfoNV>();
        group_create_infos[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        group_create_infos[1].generalShader = 3;  // miss
        group_create_infos[1].closestHitShader = VK_SHADER_UNUSED_NV;
        group_create_infos[1].anyHitShader = VK_SHADER_UNUSED_NV;
        group_create_infos[1].intersectionShader = VK_SHADER_UNUSED_NV;

        group_create_infos[2] = LvlInitStruct<VkRayTracingShaderGroupCreateInfoNV>();
        group_create_infos[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_NV;
        group_create_infos[2].generalShader = VK_SHADER_UNUSED_NV;
        group_create_infos[2].closestHitShader = 2;
        group_create_infos[2].anyHitShader = 1;
        group_create_infos[2].intersectionShader = 4;

        group_create_infos[3] = LvlInitStruct<VkRayTracingShaderGroupCreateInfoNV>();
        group_create_infos[3].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
        group_create_infos[3].generalShader = 5;  // call
        group_create_infos[3].closestHitShader = VK_SHADER_UNUSED_NV;
        group_create_infos[3].anyHitShader = VK_SHADER_UNUSED_NV;
        group_create_infos[3].intersectionShader = VK_SHADER_UNUSED_NV;

        VkRayTracingPipelineCreateInfoNV pipeline_ci = LvlInitStruct<VkRayTracingPipelineCreateInfoNV>();
        pipeline_ci.stageCount = 6;
        pipeline_ci.pStages = stage_create_infos;
        pipeline_ci.groupCount = 4;
        pipeline_ci.pGroups = group_create_infos;
        pipeline_ci.maxRecursionDepth = 2;
        pipeline_ci.layout = test.variable_length ? pipeline_layout_variable.handle() : pipeline_layout.handle();

        VkPipeline pipeline = VK_NULL_HANDLE;
        ASSERT_VK_SUCCESS(vkCreateRayTracingPipelinesNV(m_device->handle(), VK_NULL_HANDLE, 1, &pipeline_ci, nullptr, &pipeline));

        std::vector<uint8_t> shader_binding_table_data;
        shader_binding_table_data.resize(static_cast<std::size_t>(shader_binding_table_buffer_size), 0);
        ASSERT_VK_SUCCESS(vkGetRayTracingShaderGroupHandlesNV(m_device->handle(), pipeline, 0, 4,
                                                              static_cast<std::size_t>(shader_binding_table_buffer_size),
                                                              shader_binding_table_data.data()));

        uint8_t *mapped_shader_binding_table_data = (uint8_t *)shader_binding_table_buffer.memory().map();
        std::memcpy(mapped_shader_binding_table_data, shader_binding_table_data.data(), shader_binding_table_data.size());
        shader_binding_table_buffer.memory().unmap();

        ray_tracing_command_buffer.begin();

        vk::CmdBindPipeline(ray_tracing_command_buffer.handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipeline);

        if (gpu_assisted) {
            vk::CmdBindDescriptorSets(ray_tracing_command_buffer.handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
                                      test.variable_length ? pipeline_layout_variable.handle() : pipeline_layout.handle(), 0, 1,
                                      test.variable_length ? &ds_variable.set_ : &ds.set_, 0, nullptr);
        } else {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-None-02697");
        }

        if (gpu_assisted) {
            m_errorMonitor->SetUnexpectedError("VUID-vkCmdTraceRaysNV-callableShaderBindingOffset-02462");
            m_errorMonitor->SetUnexpectedError("VUID-vkCmdTraceRaysNV-missShaderBindingOffset-02458");
            // Need these values to pass mapped storage buffer checks
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupHandleSize * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupHandleSize * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupHandleSize * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupHandleSize * 3ull, ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
        } else {
            // offset shall be multiple of shaderGroupBaseAlignment and stride of shaderGroupHandleSize
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 3ull, ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
        }

        ray_tracing_command_buffer.end();
        // Update the index of the texture that the shaders should read
        uint32_t *mapped_storage_buffer_data = (uint32_t *)storage_buffer.memory().map();
        mapped_storage_buffer_data[0] = test.rgen_index;
        mapped_storage_buffer_data[1] = test.ahit_index;
        mapped_storage_buffer_data[2] = test.chit_index;
        mapped_storage_buffer_data[3] = test.miss_index;
        mapped_storage_buffer_data[4] = test.intr_index;
        mapped_storage_buffer_data[5] = test.call_index;
        mapped_storage_buffer_data[6] = 0;
        mapped_storage_buffer_data[7] = 0;
        mapped_storage_buffer_data[8] = 0;
        mapped_storage_buffer_data[9] = 0;
        mapped_storage_buffer_data[10] = 0;
        mapped_storage_buffer_data[11] = 0;
        storage_buffer.memory().unmap();

        vk::QueueSubmit(ray_tracing_queue, 1, &submit_info, VK_NULL_HANDLE);
        vk::QueueWaitIdle(ray_tracing_queue);
        m_errorMonitor->VerifyFound();

        if (gpu_assisted) {
            mapped_storage_buffer_data = (uint32_t *)storage_buffer.memory().map();
            ASSERT_TRUE(mapped_storage_buffer_data[6] == 1);
            ASSERT_TRUE(mapped_storage_buffer_data[7] == 2);
            ASSERT_TRUE(mapped_storage_buffer_data[8] == 3);
            ASSERT_TRUE(mapped_storage_buffer_data[9] == 4);
            ASSERT_TRUE(mapped_storage_buffer_data[10] == 5);
            ASSERT_TRUE(mapped_storage_buffer_data[11] == 6);
            storage_buffer.memory().unmap();
        } else {
            ray_tracing_command_buffer.begin();
            vk::CmdBindPipeline(ray_tracing_command_buffer.handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, pipeline);
            vk::CmdBindDescriptorSets(ray_tracing_command_buffer.handle(), VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
                                      test.variable_length ? pipeline_layout_variable.handle() : pipeline_layout.handle(), 0, 1,
                                      test.variable_length ? &ds_variable.set_ : &ds.set_, 0, nullptr);

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-callableShaderBindingOffset-02462");
            VkDeviceSize stride_align = ray_tracing_properties.shaderGroupHandleSize;
            VkDeviceSize invalid_max_stride = ray_tracing_properties.maxShaderGroupStride +
                                              (stride_align - (ray_tracing_properties.maxShaderGroupStride %
                                                               stride_align));  // should be less than maxShaderGroupStride
            VkDeviceSize invalid_stride =
                ray_tracing_properties.shaderGroupHandleSize >> 1;  // should  be multiple of shaderGroupHandleSize
            VkDeviceSize invalid_offset =
                ray_tracing_properties.shaderGroupBaseAlignment >> 1;  // should be multiple of shaderGroupBaseAlignment

            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(), invalid_offset,
                             ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-callableShaderBindingStride-02465");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment, invalid_stride,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-callableShaderBindingStride-02468");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment, invalid_max_stride,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            // hit shader
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-hitShaderBindingOffset-02460");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), invalid_offset, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment,
                             ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-hitShaderBindingStride-02464");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             invalid_stride, shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment,
                             ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-hitShaderBindingStride-02467");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             invalid_max_stride, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment, ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            // miss shader
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-missShaderBindingOffset-02458");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             invalid_offset, ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 2ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment,
                             ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-missShaderBindingStride-02463");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, invalid_stride,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment, ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-missShaderBindingStride-02466");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, invalid_max_stride,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment, ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            // raygenshader
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-raygenShaderBindingOffset-02456");
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(), invalid_offset,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 1ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 2ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment,
                             ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/1);

            m_errorMonitor->VerifyFound();
            const auto &limits = m_device->props.limits;

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-width-02469");
            uint32_t invalid_width = limits.maxComputeWorkGroupCount[0] + 1;
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment, ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/invalid_width, /*height=*/1, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-height-02470");
            uint32_t invalid_height = limits.maxComputeWorkGroupCount[1] + 1;
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment, ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/invalid_height, /*depth=*/1);
            m_errorMonitor->VerifyFound();

            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-vkCmdTraceRaysNV-depth-02471");
            uint32_t invalid_depth = limits.maxComputeWorkGroupCount[2] + 1;
            vkCmdTraceRaysNV(ray_tracing_command_buffer.handle(), shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 0ull, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment * 1ull, ray_tracing_properties.shaderGroupHandleSize,
                             shader_binding_table_buffer.handle(), ray_tracing_properties.shaderGroupBaseAlignment * 2ull,
                             ray_tracing_properties.shaderGroupHandleSize, shader_binding_table_buffer.handle(),
                             ray_tracing_properties.shaderGroupBaseAlignment, ray_tracing_properties.shaderGroupHandleSize,
                             /*width=*/1, /*height=*/1, /*depth=*/invalid_depth);
            m_errorMonitor->VerifyFound();

            ray_tracing_command_buffer.end();
        }
        vk::DestroyPipeline(m_device->handle(), pipeline, nullptr);
    }
}

void VkSyncValTest::InitSyncValFramework(bool enable_queue_submit_validation) {
    // Enable synchronization validation

    // Optional feature definition, add if requested (but they can't be defined at the conditional scope)
    const char *kEnableQueuSubmitSyncValidation = "VALIDATION_CHECK_ENABLE_SYNCHRONIZATION_VALIDATION_QUEUE_SUBMIT";
    VkLayerSettingValueDataEXT qs_setting_string_value{};
    qs_setting_string_value.arrayString.pCharArray = kEnableQueuSubmitSyncValidation;
    qs_setting_string_value.arrayString.count = strlen(qs_setting_string_value.arrayString.pCharArray);
    VkLayerSettingValueEXT qs_enable_setting_val = {"enables", VK_LAYER_SETTING_VALUE_TYPE_STRING_ARRAY_EXT,
                                                    qs_setting_string_value};
    VkLayerSettingsEXT qs_settings{VK_STRUCTURE_TYPE_INSTANCE_LAYER_SETTINGS_EXT, nullptr, 1, &qs_enable_setting_val};

    if (enable_queue_submit_validation) {
        features_.pNext = &qs_settings;
    }
    InitFramework(m_errorMonitor, &features_);
}

void print_android(const char *c) {
#ifdef VK_USE_PLATFORM_ANDROID_KHR
    __android_log_print(ANDROID_LOG_INFO, "VulkanLayerValidationTests", "%s", c);
#endif  // VK_USE_PLATFORM_ANDROID_KHR
}

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
const char *appTag = "VulkanLayerValidationTests";
static bool initialized = false;
static bool active = false;

// Convert Intents to argv
// Ported from Hologram sample, only difference is flexible key
std::vector<std::string> get_args(android_app &app, const char *intent_extra_data_key) {
    std::vector<std::string> args;
    JavaVM &vm = *app.activity->vm;
    JNIEnv *p_env;
    if (vm.AttachCurrentThread(&p_env, nullptr) != JNI_OK) return args;

    JNIEnv &env = *p_env;
    jobject activity = app.activity->clazz;
    jmethodID get_intent_method = env.GetMethodID(env.GetObjectClass(activity), "getIntent", "()Landroid/content/Intent;");
    jobject intent = env.CallObjectMethod(activity, get_intent_method);
    jmethodID get_string_extra_method =
        env.GetMethodID(env.GetObjectClass(intent), "getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;");
    jvalue get_string_extra_args;
    get_string_extra_args.l = env.NewStringUTF(intent_extra_data_key);
    jstring extra_str = static_cast<jstring>(env.CallObjectMethodA(intent, get_string_extra_method, &get_string_extra_args));

    std::string args_str;
    if (extra_str) {
        const char *extra_utf = env.GetStringUTFChars(extra_str, nullptr);
        args_str = extra_utf;
        env.ReleaseStringUTFChars(extra_str, extra_utf);
        env.DeleteLocalRef(extra_str);
    }

    env.DeleteLocalRef(get_string_extra_args.l);
    env.DeleteLocalRef(intent);
    vm.DetachCurrentThread();

    // split args_str
    std::stringstream ss(args_str);
    std::string arg;
    while (std::getline(ss, arg, ' ')) {
        if (!arg.empty()) args.push_back(arg);
    }

    return args;
}

void addFullTestCommentIfPresent(const ::testing::TestInfo &test_info, std::string &error_message) {
    const char *const type_param = test_info.type_param();
    const char *const value_param = test_info.value_param();

    if (type_param != NULL || value_param != NULL) {
        error_message.append(", where ");
        if (type_param != NULL) {
            error_message.append("TypeParam = ").append(type_param);
            if (value_param != NULL) error_message.append(" and ");
        }
        if (value_param != NULL) {
            error_message.append("GetParam() = ").append(value_param);
        }
    }
}

class LogcatPrinter : public ::testing::EmptyTestEventListener {
    // Called before a test starts.
    virtual void OnTestStart(const ::testing::TestInfo &test_info) {
        __android_log_print(ANDROID_LOG_INFO, appTag, "[ RUN      ] %s.%s", test_info.test_case_name(), test_info.name());
    }

    // Called after a failed assertion or a SUCCEED() invocation.
    virtual void OnTestPartResult(const ::testing::TestPartResult &result) {
        // If the test part succeeded, we don't need to do anything.
        if (result.type() == ::testing::TestPartResult::kSuccess) return;

        __android_log_print(ANDROID_LOG_INFO, appTag, "%s in %s:%d %s", result.failed() ? "*** Failure" : "Success",
                            result.file_name(), result.line_number(), result.summary());
    }

    // Called after a test ends.
    virtual void OnTestEnd(const ::testing::TestInfo &info) {
        std::string result;
        if (info.result()->Passed()) {
            result.append("[       OK ]");
        } else if (info.result()->Skipped()) {
            result.append("[  SKIPPED ]");
        } else {
            result.append("[  FAILED  ]");
        }
        result.append(info.test_case_name()).append(".").append(info.name());
        if (info.result()->Failed()) addFullTestCommentIfPresent(info, result);

        if (::testing::GTEST_FLAG(print_time)) {
            std::ostringstream os;
            os << info.result()->elapsed_time();
            result.append(" (").append(os.str()).append(" ms)");
        }

        __android_log_print(ANDROID_LOG_INFO, appTag, "%s", result.c_str());
    };
};

static int32_t processInput(struct android_app *app, AInputEvent *event) { return 0; }

static void processCommand(struct android_app *app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            if (app->window) {
                initialized = true;
                VkTestFramework::window = app->window;
            }
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            active = true;
            break;
        }
        case APP_CMD_LOST_FOCUS: {
            active = false;
            break;
        }
    }
}

static void destroyActivity(struct android_app *app) {
    ANativeActivity_finish(app->activity);

    // Wait for APP_CMD_DESTROY
    while (app->destroyRequested == 0) {
        struct android_poll_source *source = nullptr;
        int events = 0;
        int result = ALooper_pollAll(-1, nullptr, &events, reinterpret_cast<void **>(&source));

        if ((result >= 0) && (source)) {
            source->process(app, source);
        } else {
            break;
        }
    }
}

void android_main(struct android_app *app) {
    app->onAppCmd = processCommand;
    app->onInputEvent = processInput;

    while (1) {
        int events;
        struct android_poll_source *source;
        while (ALooper_pollAll(active ? 0 : -1, NULL, &events, (void **)&source) >= 0) {
            if (source) {
                source->process(app, source);
            }

            if (app->destroyRequested != 0) {
                VkTestFramework::Finish();
                return;
            }
        }

        if (initialized && active) {
            // Use the following key to send arguments to gtest, i.e.
            // --es args "--gtest_filter=-VkLayerTest.foo"
            const char key[] = "args";
            std::vector<std::string> args = get_args(*app, key);

            std::string filter = "";
            if (args.size() > 0) {
                __android_log_print(ANDROID_LOG_INFO, appTag, "Intent args = %s", args[0].c_str());
                filter += args[0];
            } else {
                __android_log_print(ANDROID_LOG_INFO, appTag, "No Intent args detected");
            }

            int argc = 2;
            char *argv[] = {(char *)"foo", (char *)filter.c_str()};
            __android_log_print(ANDROID_LOG_DEBUG, appTag, "filter = %s", argv[1]);

            // Route output to files until we can override the gtest output
            freopen("/sdcard/Android/data/com.example.VulkanLayerValidationTests/files/out.txt", "w", stdout);
            freopen("/sdcard/Android/data/com.example.VulkanLayerValidationTests/files/err.txt", "w", stderr);

            ::testing::InitGoogleTest(&argc, argv);

            ::testing::TestEventListeners &listeners = ::testing::UnitTest::GetInstance()->listeners();
            listeners.Append(new LogcatPrinter);

            VkTestFramework::InitArgs(&argc, argv);
            ::testing::AddGlobalTestEnvironment(new TestEnvironment);

            int result = RUN_ALL_TESTS();

            if (result != 0) {
                __android_log_print(ANDROID_LOG_INFO, appTag, "==== Tests FAILED ====");
            } else {
                __android_log_print(ANDROID_LOG_INFO, appTag, "==== Tests PASSED ====");
            }

            VkTestFramework::Finish();

            fclose(stdout);
            fclose(stderr);

            destroyActivity(app);
            raise(SIGTERM);
            return;
        }
    }
}
#endif

#if defined(_WIN32) && !defined(NDEBUG)
#include <crtdbg.h>
#endif

int main(int argc, char **argv) {
    int result;

#if defined(_WIN32)
#if !defined(NDEBUG)
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    // Avoid "Abort, Retry, Ignore" dialog boxes
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

    ::testing::InitGoogleTest(&argc, argv);
    VkTestFramework::InitArgs(&argc, argv);

    ::testing::AddGlobalTestEnvironment(new TestEnvironment);

    result = RUN_ALL_TESTS();

    VkTestFramework::Finish();
    return result;
}
