/*
 * Copyright (c) 2015-2023 The Khronos Group Inc.
 * Copyright (c) 2015-2023 Valve Corporation
 * Copyright (c) 2015-2023 LunarG, Inc.
 * Copyright (c) 2015-2023 Google, Inc.
 * Modifications Copyright (C) 2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include "../framework/layer_validation_tests.h"

class NegativePipelineLayout : public VkLayerTest {};

TEST_F(NegativePipelineLayout, ExceedsSetLimit) {
    TEST_DESCRIPTION("Attempt to create a pipeline layout using more than the physical limit of SetLayouts.");

    ASSERT_NO_FATAL_FAILURE(Init());

    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = 0;
    layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layout_binding.descriptorCount = 1;
    layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_binding.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo ds_layout_ci = LvlInitStruct<VkDescriptorSetLayoutCreateInfo>();
    ds_layout_ci.bindingCount = 1;
    ds_layout_ci.pBindings = &layout_binding;
    vk_testing::DescriptorSetLayout ds_layout(*m_device, ds_layout_ci);

    // Create an array of DSLs, one larger than the physical limit
    const auto excess_layouts = 1 + m_device->phy().properties().limits.maxBoundDescriptorSets;
    std::vector<VkDescriptorSetLayout> dsl_array(excess_layouts, ds_layout.handle());

    VkPipelineLayoutCreateInfo pipeline_layout_ci = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci.setLayoutCount = excess_layouts;
    pipeline_layout_ci.pSetLayouts = dsl_array.data();

    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-setLayoutCount-00286");
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
}

TEST_F(NegativePipelineLayout, ExcessSubsampledPerStageDescriptors) {
    TEST_DESCRIPTION("Attempt to create a pipeline layout where total subsampled descriptors exceed limits");

    AddRequiredExtensions(VK_EXT_FRAGMENT_DENSITY_MAP_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported.";
    }

    auto density_map2_properties = LvlInitStruct<VkPhysicalDeviceFragmentDensityMap2PropertiesEXT>();
    auto properties2 = GetPhysicalDeviceProperties2(density_map2_properties);

    ASSERT_NO_FATAL_FAILURE(InitState());

    uint32_t max_subsampled_samplers = density_map2_properties.maxDescriptorSetSubsampledSamplers;

    // Note: Adding this check in case mock ICDs don't initialize min-max values correctly
    if (max_subsampled_samplers == 0) {
        GTEST_SKIP() << "axDescriptorSetSubsampledSamplers limit (" << max_subsampled_samplers
                     << ") must be greater than 0. Skipping.";
    }

    if (max_subsampled_samplers >= properties2.properties.limits.maxDescriptorSetSamplers) {
        GTEST_SKIP() << "test assumes maxDescriptorSetSubsampledSamplers limit (" << max_subsampled_samplers
                     << ") is less than overall sampler limit (" << properties2.properties.limits.maxDescriptorSetSamplers
                     << "). Skipping.";
    }

    VkDescriptorSetLayoutBinding dslb = {};
    std::vector<VkDescriptorSetLayoutBinding> dslb_vec = {};
    VkDescriptorSetLayoutCreateInfo ds_layout_ci = LvlInitStruct<VkDescriptorSetLayoutCreateInfo>();

    VkSamplerCreateInfo sampler_info = SafeSaneSamplerCreateInfo();
    sampler_info.maxLod = 0.f;
    sampler_info.flags |= VK_SAMPLER_CREATE_SUBSAMPLED_BIT_EXT;
    vk_testing::Sampler sampler(*m_device, sampler_info);
    ASSERT_TRUE(sampler.initialized());

    // just make all the immutable samplers point to the same sampler
    std::vector<VkSampler> immutableSamplers;
    immutableSamplers.resize(max_subsampled_samplers);
    for (uint32_t sampler_idx = 0; sampler_idx < max_subsampled_samplers; sampler_idx++) {
        immutableSamplers[sampler_idx] = sampler.handle();
    }

    // VU 03566 - too many subsampled sampler type descriptors across stages
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    dslb.descriptorCount = max_subsampled_samplers;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    dslb.pImmutableSamplers = &immutableSamplers[0];
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    dslb.descriptorCount = max_subsampled_samplers;
    dslb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    vk_testing::DescriptorSetLayout ds_layout(*m_device, ds_layout_ci);
    ASSERT_TRUE(ds_layout.initialized());

    VkPipelineLayoutCreateInfo pipeline_layout_ci = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci.setLayoutCount = 1;
    pipeline_layout_ci.pSetLayouts = &ds_layout.handle();
    const char *max_sampler_vuid = "VUID-VkPipelineLayoutCreateInfo-pImmutableSamplers-03566";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_sampler_vuid);
    vk_testing::PipelineLayout pipeline_layout(*m_device, pipeline_layout_ci, {&ds_layout});
    m_errorMonitor->VerifyFound();
}

TEST_F(VkLayerTest, CreatePipelineLayoutExcessPerStageDescriptors) {
    TEST_DESCRIPTION("Attempt to create a pipeline layout where total descriptors exceed per-stage limits");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddOptionalExtensions(VK_KHR_MAINTENANCE_3_EXTENSION_NAME);
    AddOptionalExtensions(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }
    ASSERT_NO_FATAL_FAILURE(InitState());
    bool descriptor_indexing =
        IsExtensionsEnabled(VK_KHR_MAINTENANCE_3_EXTENSION_NAME) && IsExtensionsEnabled(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    uint32_t max_uniform_buffers = m_device->phy().properties().limits.maxPerStageDescriptorUniformBuffers;
    uint32_t max_storage_buffers = m_device->phy().properties().limits.maxPerStageDescriptorStorageBuffers;
    uint32_t max_sampled_images = m_device->phy().properties().limits.maxPerStageDescriptorSampledImages;
    uint32_t max_storage_images = m_device->phy().properties().limits.maxPerStageDescriptorStorageImages;
    uint32_t max_samplers = m_device->phy().properties().limits.maxPerStageDescriptorSamplers;
    uint32_t max_combined = std::min(max_samplers, max_sampled_images);
    uint32_t max_input_attachments = m_device->phy().properties().limits.maxPerStageDescriptorInputAttachments;

    uint32_t sum_dyn_uniform_buffers = m_device->phy().properties().limits.maxDescriptorSetUniformBuffersDynamic;
    uint32_t sum_uniform_buffers = m_device->phy().properties().limits.maxDescriptorSetUniformBuffers;
    uint32_t sum_dyn_storage_buffers = m_device->phy().properties().limits.maxDescriptorSetStorageBuffersDynamic;
    uint32_t sum_storage_buffers = m_device->phy().properties().limits.maxDescriptorSetStorageBuffers;
    uint32_t sum_sampled_images = m_device->phy().properties().limits.maxDescriptorSetSampledImages;
    uint32_t sum_storage_images = m_device->phy().properties().limits.maxDescriptorSetStorageImages;
    uint32_t sum_samplers = m_device->phy().properties().limits.maxDescriptorSetSamplers;
    uint32_t sum_input_attachments = m_device->phy().properties().limits.maxDescriptorSetInputAttachments;

    VkPhysicalDeviceDescriptorIndexingProperties descriptor_indexing_properties =
        LvlInitStruct<VkPhysicalDeviceDescriptorIndexingProperties>();
    if (descriptor_indexing) {
        GetPhysicalDeviceProperties2(descriptor_indexing_properties);
    }

    // Devices that report UINT32_MAX for any of these limits can't run this test
    if (vvl::kU32Max ==
        std::max({max_uniform_buffers, max_storage_buffers, max_sampled_images, max_storage_images, max_samplers})) {
        GTEST_SKIP() << "Physical device limits report as UINT32_MAX";
    }

    VkDescriptorSetLayoutBinding dslb = {};
    std::vector<VkDescriptorSetLayoutBinding> dslb_vec = {};
    VkDescriptorSetLayout ds_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayoutCreateInfo ds_layout_ci = LvlInitStruct<VkDescriptorSetLayoutCreateInfo>();
    VkPipelineLayoutCreateInfo pipeline_layout_ci = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci.setLayoutCount = 1;
    pipeline_layout_ci.pSetLayouts = &ds_layout;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    // VU 0fe0023e - too many sampler type descriptors in fragment stage
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    dslb.descriptorCount = max_samplers;
    dslb.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb.descriptorCount = max_combined;
    dslb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    VkResult err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_sampler_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03016"
                                                         : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00287";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_sampler_vuid);
    if ((max_samplers + max_combined) > sum_samplers) {
        const char *max_all_sampler_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03028"
                                                                 : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01677";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_sampler_vuid);  // expect all-stages sum too
    }
    if (max_combined > sum_sampled_images) {
        const char *max_all_sampled_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03033"
                                                                       : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01682";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_sampled_image_vuid);  // expect all-stages sum too
    }
    if (descriptor_indexing) {
        if ((max_samplers + max_combined) > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSamplers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03036");
        }
        if ((max_samplers + max_combined) > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSamplers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03022");
        }
        if (max_combined > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03041");
        }
        if (max_combined > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSampledImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03025");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00240 - too many uniform buffer type descriptors in vertex stage
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb.descriptorCount = max_uniform_buffers + 1;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_uniform_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03017"
                                                         : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00288";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_uniform_vuid);
    if (dslb.descriptorCount > sum_uniform_buffers) {
        const char *max_all_uniform_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03029"
                                                                 : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01678";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_uniform_vuid);  // expect all-stages sum too
    }
    if (dslb.descriptorCount > sum_dyn_uniform_buffers) {
        const char *max_all_uniform_dynamic_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03030"
                                                                         : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01679";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_uniform_dynamic_vuid);  // expect all-stages sum too
    }
    if (descriptor_indexing) {
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03038");
        }
        if ((dslb.descriptorCount * 2) > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03023");
        }
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindUniformBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03037");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00242 - too many storage buffer type descriptors in compute stage
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dslb.descriptorCount = max_storage_buffers + 1;
    dslb.stageFlags = VK_SHADER_STAGE_ALL;
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    dslb_vec.push_back(dslb);
    dslb.binding = 2;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dslb.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_storage_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03018"
                                                         : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00289";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_storage_vuid);
    if (dslb.descriptorCount > sum_dyn_storage_buffers) {
        const char *max_all_storage_dynamic_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03032"
                                                                         : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01681";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_storage_dynamic_vuid);  // expect all-stages sum too
    }
    const uint32_t storage_buffer_count = dslb_vec[0].descriptorCount + dslb_vec[2].descriptorCount;
    if (storage_buffer_count > sum_storage_buffers) {
        const char *max_all_storage_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03031"
                                                                 : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01680";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_storage_vuid);  // expect all-stages sum too
    }
    if (descriptor_indexing) {
        if (storage_buffer_count > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindStorageBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03039");
        }
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03040");
        }
        if ((dslb.descriptorCount * 3) > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03024");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00244 - too many sampled image type descriptors in multiple stages
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    dslb.descriptorCount = max_sampled_images;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    dslb.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    dslb_vec.push_back(dslb);
    dslb.binding = 2;
    dslb.descriptorCount = max_combined;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_sample_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03019"
                                                              : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00290";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_sample_image_vuid);
    const uint32_t sampled_image_count = max_combined + 2 * max_sampled_images;
    if (sampled_image_count > sum_sampled_images) {
        const char *max_all_sampled_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03033"
                                                                       : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01682";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_sampled_image_vuid);  // expect all-stages sum too
    }
    if (max_combined > sum_samplers) {
        const char *max_all_sampler_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03028"
                                                                 : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01677";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_sampler_vuid);  // expect all-stages sum too
    }
    if (descriptor_indexing) {
        if (sampled_image_count > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03041");
        }
        if (sampled_image_count > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSampledImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03025");
        }
        if (max_combined > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSamplers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03036");
        }
        if (max_combined > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSamplers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03022");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00246 - too many storage image type descriptors in fragment stage
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    dslb.descriptorCount = 1 + (max_storage_images / 2);
    dslb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_storage_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03020"
                                                               : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00291";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_storage_image_vuid);
    const uint32_t storage_image_count = 2 * dslb.descriptorCount;
    if (storage_image_count > sum_storage_images) {
        const char *max_all_storage_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03034"
                                                                       : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01683";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_storage_image_vuid);  // expect all-stages sum too
    }
    if (descriptor_indexing) {
        if (storage_image_count > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindStorageImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03042");
        }
        if (storage_image_count > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindStorageImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03026");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00d18 - too many input attachments in fragment stage
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    dslb.descriptorCount = 1 + max_input_attachments;
    dslb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_input_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03021"
                                                       : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01676";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_input_vuid);
    if (dslb.descriptorCount > sum_input_attachments) {
        const char *max_all_input_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03035"
                                                               : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01684";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_input_vuid);  // expect all-stages sum too
    }
    if (descriptor_indexing) {
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindInputAttachments) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03043");
        }
        if (dslb.descriptorCount > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindInputAttachments) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03027");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);
}

TEST_F(VkLayerTest, CreatePipelineLayoutExcessDescriptorsOverall) {
    TEST_DESCRIPTION("Attempt to create a pipeline layout where total descriptors exceed limits");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_MAINTENANCE_3_EXTENSION_NAME);
    AddOptionalExtensions(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }
    ASSERT_NO_FATAL_FAILURE(InitState());
    const bool descriptor_indexing = IsExtensionsEnabled(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    uint32_t max_uniform_buffers = m_device->phy().properties().limits.maxPerStageDescriptorUniformBuffers;
    uint32_t max_storage_buffers = m_device->phy().properties().limits.maxPerStageDescriptorStorageBuffers;
    uint32_t max_sampled_images = m_device->phy().properties().limits.maxPerStageDescriptorSampledImages;
    uint32_t max_storage_images = m_device->phy().properties().limits.maxPerStageDescriptorStorageImages;
    uint32_t max_samplers = m_device->phy().properties().limits.maxPerStageDescriptorSamplers;
    uint32_t max_input_attachments = m_device->phy().properties().limits.maxPerStageDescriptorInputAttachments;

    uint32_t sum_dyn_uniform_buffers = m_device->phy().properties().limits.maxDescriptorSetUniformBuffersDynamic;
    uint32_t sum_uniform_buffers = m_device->phy().properties().limits.maxDescriptorSetUniformBuffers;
    uint32_t sum_dyn_storage_buffers = m_device->phy().properties().limits.maxDescriptorSetStorageBuffersDynamic;
    uint32_t sum_storage_buffers = m_device->phy().properties().limits.maxDescriptorSetStorageBuffers;
    uint32_t sum_sampled_images = m_device->phy().properties().limits.maxDescriptorSetSampledImages;
    uint32_t sum_storage_images = m_device->phy().properties().limits.maxDescriptorSetStorageImages;
    uint32_t sum_samplers = m_device->phy().properties().limits.maxDescriptorSetSamplers;
    uint32_t sum_input_attachments = m_device->phy().properties().limits.maxDescriptorSetInputAttachments;

    VkPhysicalDeviceDescriptorIndexingProperties descriptor_indexing_properties =
        LvlInitStruct<VkPhysicalDeviceDescriptorIndexingProperties>();
    if (descriptor_indexing) {
        GetPhysicalDeviceProperties2(descriptor_indexing_properties);
    }

    // Devices that report UINT32_MAX for any of these limits can't run this test
    if (vvl::kU32Max == std::max({sum_dyn_uniform_buffers, sum_uniform_buffers, sum_dyn_storage_buffers, sum_storage_buffers,
                                  sum_sampled_images, sum_storage_images, sum_samplers, sum_input_attachments})) {
        GTEST_SKIP() << "Physical device limits report as 2^32-1";
    }

    VkDescriptorSetLayoutBinding dslb = {};
    std::vector<VkDescriptorSetLayoutBinding> dslb_vec = {};
    VkDescriptorSetLayout ds_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayoutCreateInfo ds_layout_ci = LvlInitStruct<VkDescriptorSetLayoutCreateInfo>();
    VkPipelineLayoutCreateInfo pipeline_layout_ci = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci.setLayoutCount = 1;
    pipeline_layout_ci.pSetLayouts = &ds_layout;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    // VU 0fe00d1a - too many sampler type descriptors overall
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    dslb.descriptorCount = sum_samplers / 2;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb.descriptorCount = sum_samplers - dslb.descriptorCount + 1;
    dslb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    VkResult err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_all_sampler_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03028"
                                                             : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01677";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_sampler_vuid);
    if (dslb.descriptorCount > max_samplers) {
        const char *max_sampler_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03016"
                                                             : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00287";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_sampler_vuid);  // Expect max-per-stage samplers exceeds limits
    }
    if (dslb.descriptorCount > sum_sampled_images) {
        const char *max_all_sampled_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03033"
                                                                       : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01682";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit,
                                             max_all_sampled_image_vuid);  // Expect max overall sampled image count exceeds limits
    }
    if (dslb.descriptorCount > max_sampled_images) {
        const char *max_sample_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03019"
                                                                  : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00290";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit,
                                             max_sample_image_vuid);  // Expect max per-stage sampled image count exceeds limits
    }
    if (descriptor_indexing) {
        if ((sum_samplers + 1) > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSamplers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03036");
        }
        if (std::max(dslb_vec[0].descriptorCount, dslb_vec[1].descriptorCount) >
            descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSamplers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03022");
        }
        if (dslb_vec[1].descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03041");
        }
        if (dslb_vec[1].descriptorCount > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSampledImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03025");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00d1c - too many uniform buffer type descriptors overall
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dslb.descriptorCount = sum_uniform_buffers + 1;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_all_uniform_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03029"
                                                             : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01678";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_uniform_vuid);
    if (dslb.descriptorCount > max_uniform_buffers) {
        const char *max_uniform_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03017"
                                                             : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00288";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_uniform_vuid);  // expect max-per-stage too
    }
    if (descriptor_indexing) {
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindUniformBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03037");
        }
        if (dslb.descriptorCount > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03023");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00d1e - too many dynamic uniform buffer type descriptors overall
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    dslb.descriptorCount = sum_dyn_uniform_buffers + 1;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_all_uniform_dynamic_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03030"
                                                                     : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01679";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_uniform_dynamic_vuid);
    if (dslb.descriptorCount > max_uniform_buffers) {
        const char *max_uniform_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03017"
                                                             : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00288";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_uniform_vuid);  // expect max-per-stage too
    }
    if (descriptor_indexing) {
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindUniformBuffersDynamic) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03038");
        }
        if (dslb.descriptorCount > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindUniformBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03023");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00d20 - too many storage buffer type descriptors overall
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dslb.descriptorCount = sum_storage_buffers + 1;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_all_storage_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03031"
                                                             : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01680";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_storage_vuid);
    if (dslb.descriptorCount > max_storage_buffers) {
        const char *max_storage_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03018"
                                                             : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00289";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_storage_vuid);  // expect max-per-stage too
    }
    if (descriptor_indexing) {
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindStorageBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03039");
        }
        if (dslb.descriptorCount > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03024");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00d22 - too many dynamic storage buffer type descriptors overall
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    dslb.descriptorCount = sum_dyn_storage_buffers + 1;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_all_storage_dynamic_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03032"
                                                                     : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01681";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_storage_dynamic_vuid);
    if (dslb.descriptorCount > max_storage_buffers) {
        const char *max_storage_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03018"
                                                             : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00289";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_storage_vuid);  // expect max-per-stage too
    }
    if (descriptor_indexing) {
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindStorageBuffersDynamic) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03040");
        }
        if (dslb.descriptorCount > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindStorageBuffers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03024");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00d24 - too many sampled image type descriptors overall
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    dslb.descriptorCount = max_samplers;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    // revisit: not robust to odd limits.
    uint32_t remaining = (max_samplers > sum_sampled_images ? 0 : (sum_sampled_images - max_samplers) / 2);
    dslb.descriptorCount = 1 + remaining;
    dslb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);
    dslb.binding = 2;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    dslb.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_all_sampled_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03033"
                                                                   : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01682";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_sampled_image_vuid);
    // Takes max since VUID only checks per shader stage
    if (std::max(dslb_vec[0].descriptorCount, dslb_vec[1].descriptorCount) > max_sampled_images) {
        const char *max_sample_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03019"
                                                                  : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00290";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit,
                                             max_sample_image_vuid);  // Expect max-per-stage sampled images to exceed limits
    }
    if (descriptor_indexing) {
        if (max_samplers > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSamplers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03036");
        }
        if (max_samplers > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSamplers) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03022");
        }
        if ((dslb_vec[0].descriptorCount + dslb_vec[1].descriptorCount) >
            descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindSampledImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03041");
        }
        if (std::max(dslb_vec[0].descriptorCount, dslb_vec[1].descriptorCount) >
            descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindSampledImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03025");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00d26 - too many storage image type descriptors overall
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    dslb.descriptorCount = sum_storage_images / 2;
    dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);
    dslb.binding = 1;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    dslb.descriptorCount = sum_storage_images - dslb.descriptorCount + 1;
    dslb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_all_storage_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03034"
                                                                   : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01683";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_storage_image_vuid);
    if (dslb.descriptorCount > max_storage_images) {
        const char *max_storage_image_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03020"
                                                                   : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00291";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_storage_image_vuid);  // expect max-per-stage too
    }
    if (descriptor_indexing) {
        if ((sum_storage_images + 1) > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindStorageImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03042");
        }
        if (std::max(dslb_vec[0].descriptorCount, dslb_vec[1].descriptorCount) >
            descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindStorageImages) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03026");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);

    // VU 0fe00d28 - too many input attachment type descriptors overall
    dslb_vec.clear();
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    dslb.descriptorCount = sum_input_attachments + 1;
    dslb.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dslb.pImmutableSamplers = NULL;
    dslb_vec.push_back(dslb);

    ds_layout_ci.bindingCount = dslb_vec.size();
    ds_layout_ci.pBindings = dslb_vec.data();
    err = vk::CreateDescriptorSetLayout(m_device->device(), &ds_layout_ci, NULL, &ds_layout);
    ASSERT_VK_SUCCESS(err);

    const char *max_all_input_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03035"
                                                           : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01684";
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_all_input_vuid);
    if (dslb.descriptorCount > max_input_attachments) {
        const char *max_input_vuid = (descriptor_indexing) ? "VUID-VkPipelineLayoutCreateInfo-descriptorType-03021"
                                                           : "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-01676";
        m_errorMonitor->SetDesiredFailureMsg(kErrorBit, max_input_vuid);  // expect max-per-stage too
    }
    if (descriptor_indexing) {
        if (dslb.descriptorCount > descriptor_indexing_properties.maxDescriptorSetUpdateAfterBindInputAttachments) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-03043");
        }
        if (dslb.descriptorCount > descriptor_indexing_properties.maxPerStageDescriptorUpdateAfterBindInputAttachments) {
            m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-descriptorType-03027");
        }
    }
    err = vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
    vk::DestroyPipelineLayout(m_device->device(), pipeline_layout, NULL);  // Unnecessary but harmless if test passed
    pipeline_layout = VK_NULL_HANDLE;
    vk::DestroyDescriptorSetLayout(m_device->device(), ds_layout, NULL);
}

TEST_F(NegativePipelineLayout, DescriptorTypeMismatch) {
    TEST_DESCRIPTION("Challenge core_validation with shader validation issues related to vkCreateGraphicsPipelines.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    OneOffDescriptorSet descriptor_set(m_device, {
                                                     {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr},
                                                 });

    char const *vsSource = R"glsl(
        #version 450
        layout (std140, set = 0, binding = 0) uniform buf {
            mat4 mvp;
        } ubuf;
        void main(){
           gl_Position = ubuf.mvp * vec4(1);
        }
    )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), pipe.fs_->GetStageCreateInfo()};
    pipe.InitState();
    pipe.pipeline_layout_ = VkPipelineLayoutObj(m_device, {&descriptor_set.layout_});

    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkGraphicsPipelineCreateInfo-layout-07989");
    pipe.CreateGraphicsPipeline();
    m_errorMonitor->VerifyFound();
}

TEST_F(NegativePipelineLayout, DescriptorTypeMismatchCompute) {
    TEST_DESCRIPTION("Test that an error is produced for a pipeline consuming a descriptor-backed resource of a mismatched type");

    ASSERT_NO_FATAL_FAILURE(Init());

    char const *csSource = R"glsl(
        #version 450
        layout(local_size_x=1) in;
        layout(set=0, binding=0) buffer block { vec4 x; };
        void main() {
           x.x = 1.0f;
        }
    )glsl";

    const auto set_info = [&](CreateComputePipelineHelper &helper) {
        helper.cs_.reset(new VkShaderObj(this, csSource, VK_SHADER_STAGE_COMPUTE_BIT));
        helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
    };
    CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit, "VUID-VkComputePipelineCreateInfo-layout-07989");
}

TEST_F(NegativePipelineLayout, DescriptorNotAccessible) {
    TEST_DESCRIPTION(
        "Create a pipeline in which a descriptor used by a shader stage does not include that stage in its stageFlags.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    OneOffDescriptorSet ds(m_device, {
                                         {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT /*!*/, nullptr},
                                     });

    char const *vsSource = R"glsl(
        #version 450
        layout (std140, set = 0, binding = 0) uniform buf {
            mat4 mvp;
        } ubuf;
        void main(){
           gl_Position = ubuf.mvp * vec4(1);
        }
    )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), pipe.fs_->GetStageCreateInfo()};
    pipe.InitState();
    pipe.pipeline_layout_ = VkPipelineLayoutObj(m_device, {&ds.layout_});

    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkGraphicsPipelineCreateInfo-layout-07988");
    pipe.CreateGraphicsPipeline();
    m_errorMonitor->VerifyFound();
}

TEST_F(NegativePipelineLayout, UniformBlockNotProvided) {
    TEST_DESCRIPTION(
        "Test that an error is produced for a shader consuming a uniform block which has no corresponding binding in the pipeline "
        "layout");
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "not declared in pipeline layout");

    ASSERT_NO_FATAL_FAILURE(Init());

    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragUniformShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineObj pipe(m_device);
    pipe.AddShader(&vs);
    pipe.AddShader(&fs);

    /* set up CB 0; type is UNORM by default */
    pipe.AddDefaultColorAttachment();
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    VkDescriptorSetObj descriptorSet(m_device);
    descriptorSet.CreateVKDescriptorSet(m_commandBuffer);

    pipe.CreateVKPipeline(descriptorSet.GetPipelineLayout(), renderPass());

    m_errorMonitor->VerifyFound();
}

TEST_F(NegativePipelineLayout, MissingDescriptor) {
    TEST_DESCRIPTION(
        "Test that an error is produced for a compute pipeline consuming a descriptor which is not provided in the pipeline "
        "layout");

    ASSERT_NO_FATAL_FAILURE(Init());

    char const *csSource = R"glsl(
        #version 450
        layout(local_size_x=1) in;
        layout(set=0, binding=0) buffer block { vec4 x; };
        void main(){
           x = vec4(1);
        }
    )glsl";

    CreateComputePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.cs_.reset(new VkShaderObj(this, csSource, VK_SHADER_STAGE_COMPUTE_BIT));
    pipe.InitState();
    pipe.pipeline_layout_ = VkPipelineLayoutObj(m_device, {});
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkComputePipelineCreateInfo-layout-07988");
    pipe.CreateComputePipeline();
    m_errorMonitor->VerifyFound();
}

TEST_F(NegativePipelineLayout, MultiplePushDescriptorSets) {
    TEST_DESCRIPTION("Verify an error message for multiple push descriptor sets.");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }
    ASSERT_NO_FATAL_FAILURE(InitState());

    VkDescriptorSetLayoutBinding dsl_binding = {};
    dsl_binding.binding = 0;
    dsl_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dsl_binding.descriptorCount = 1;
    dsl_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    dsl_binding.pImmutableSamplers = NULL;

    const unsigned int descriptor_set_layout_count = 2;
    std::vector<VkDescriptorSetLayoutObj> ds_layouts;
    for (uint32_t i = 0; i < descriptor_set_layout_count; ++i) {
        dsl_binding.binding = i;
        ds_layouts.emplace_back(m_device, std::vector<VkDescriptorSetLayoutBinding>(1, dsl_binding),
                                VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    }
    const auto &ds_vk_layouts = MakeVkHandles<VkDescriptorSetLayout>(ds_layouts);

    VkPipelineLayout pipeline_layout;
    VkPipelineLayoutCreateInfo pipeline_layout_ci = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci.pushConstantRangeCount = 0;
    pipeline_layout_ci.pPushConstantRanges = NULL;
    pipeline_layout_ci.setLayoutCount = ds_vk_layouts.size();
    pipeline_layout_ci.pSetLayouts = ds_vk_layouts.data();

    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-00293");
    vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
}

TEST_F(NegativePipelineLayout, SetLayoutFlags) {
    TEST_DESCRIPTION("Validate setLayout flags in create pipeline layout.");

    AddRequiredExtensions(VK_EXT_MUTABLE_DESCRIPTOR_TYPE_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto mut_features = LvlInitStruct<VkPhysicalDeviceMutableDescriptorTypeFeaturesEXT>();
    GetPhysicalDeviceFeatures2(mut_features);
    if (!mut_features.mutableDescriptorType) {
        GTEST_SKIP() << "mutableDescriptorType not supported.";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &mut_features));

    VkDescriptorSetLayoutBinding layout_binding = {};
    layout_binding.binding = 0;
    layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layout_binding.descriptorCount = 1;
    layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    layout_binding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo ds_layout_ci = LvlInitStruct<VkDescriptorSetLayoutCreateInfo>();
    ds_layout_ci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_HOST_ONLY_POOL_BIT_EXT;
    ds_layout_ci.bindingCount = 1;
    ds_layout_ci.pBindings = &layout_binding;

    vk_testing::DescriptorSetLayout ds_layout;
    ds_layout.init(*m_device, ds_layout_ci);
    VkDescriptorSetLayout ds_layout_handle = ds_layout.handle();

    VkPipelineLayoutCreateInfo pipeline_layout_ci = LvlInitStruct<VkPipelineLayoutCreateInfo>();
    pipeline_layout_ci.setLayoutCount = 1;
    pipeline_layout_ci.pSetLayouts = &ds_layout_handle;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    m_errorMonitor->SetDesiredFailureMsg(kErrorBit, "VUID-VkPipelineLayoutCreateInfo-pSetLayouts-04606");
    vk::CreatePipelineLayout(m_device->device(), &pipeline_layout_ci, NULL, &pipeline_layout);
    m_errorMonitor->VerifyFound();
}
