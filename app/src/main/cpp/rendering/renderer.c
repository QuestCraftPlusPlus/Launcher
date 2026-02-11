//
// Created by maks on 12.12.2024.
//

#include "renderer.h"
#include "asset_buffer_read.h"
#include "ktx_texture.h"
#include "vk_init.h"
#include "../xr/xr_init.h"
#include "../xr/xr_linear_algebra.h"
#include "../xr/xr_input.h"
#include "../main.h"

#include <media/NdkImageReader.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/hardware_buffer.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG __FILE_NAME__
#include "../util/log.h"

render_state_t vk_rs;

static bool createShaderModule(const char* filename, AAssetManager* assetManager, VkShaderModule* outModule) {
    off64_t length;
    void* code = readAssetToBuffer((asset_info_t[]){{assetManager, (char*)filename}}, &length);
    if (!code) {
        LOGE("Failed to read shader asset: %s", filename);
        return false;
    }

    VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = length,
            .pCode = (uint32_t*)code
    };

    VkResult result = vkCreateShaderModule(vkinfo.device, &createInfo, NULL, outModule);
    free(code);

    if (result != VK_SUCCESS) {
        LOGE("Failed to create shader module for %s: %d", filename, result);
        return false;
    }
    return true;
}

static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkBuffer* buffer, VmaAllocation* allocation) {
    VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo allocInfo = {
            .usage = memoryUsage
    };
    if (memoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    vmaCreateBuffer(vkinfo.allocator, &bufferInfo, &allocInfo, buffer, allocation, NULL);
}

static bool createVkModel(vk_model_t* model, void* data, size_t size, bool isDynamic) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (isDynamic) {
        memUsage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    } else {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    createBuffer(size, usage, memUsage, &model->buffer, &model->allocation);
    model->vertexCount = size / (isDynamic ? (6 * sizeof(float)) : (8 * sizeof(float)));

    if (!isDynamic && data) {
        VkBuffer stagingBuffer;
        VmaAllocation stagingAlloc;
        createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY, &stagingBuffer, &stagingAlloc);

        void* mappedData;
        vmaMapMemory(vkinfo.allocator, stagingAlloc, &mappedData);
        memcpy(mappedData, data, size);
        vmaUnmapMemory(vkinfo.allocator, stagingAlloc);

        VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, vkinfo.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(vkinfo.device, &allocInfo, &cmd);
        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, NULL};
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkBufferCopy copyRegion = {0, 0, size};
        vkCmdCopyBuffer(cmd, stagingBuffer, model->buffer, 1, &copyRegion);

        vkEndCommandBuffer(cmd);
        VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VkFence fence;
        vkCreateFence(vkinfo.device, &fenceInfo, NULL, &fence);

        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd, 0, NULL};
        vkQueueSubmit(vkinfo.graphicsQueue, 1, &submitInfo, fence);
        vkWaitForFences(vkinfo.device, 1, &fence, VK_TRUE, UINT64_MAX);

        vkFreeCommandBuffers(vkinfo.device, vkinfo.commandPool, 1, &cmd);

        vkDestroyFence(vkinfo.device, fence, NULL);
        vmaDestroyBuffer(vkinfo.allocator, stagingBuffer, stagingAlloc);
    }

    return true;
}

static bool loadAssetModel(vk_model_t* model, const char* filename, AAssetManager* mgr) {
    off64_t length;
    asset_info_t info = {mgr, (char*)filename};
    void* buffer = readAssetToBuffer(&info, &length);
    if (!buffer) return false;

    bool res = createVkModel(model, buffer, length, false);
    free(buffer);
    return res;
}

static VkPipeline createPipelineHelper(AAssetManager* am, const char* vertName, const char* fragName,
                                       VkPipelineLayout layout,
                                       VkVertexInputBindingDescription bindingDesc,
                                       VkVertexInputAttributeDescription* attribs, uint32_t attribCount,
                                       VkPrimitiveTopology topology, bool depthTest, bool blend, VkCullModeFlagBits cullMode, VkRenderPass renderPass) {

    VkShaderModule vertModule, fragModule;
    if (!createShaderModule(vertName, am, &vertModule)) return VK_NULL_HANDLE;
    if (!createShaderModule(fragName, am, &fragModule)) return VK_NULL_HANDLE;

    VkPipelineShaderStageCreateInfo shaderStages[] = {
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertModule,
                    .pName = "main"
            },
            {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragModule,
                    .pName = "main"
            }
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDesc,
            .vertexAttributeDescriptionCount = attribCount,
            .pVertexAttributeDescriptions = attribs
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = topology,
            .primitiveRestartEnable = VK_FALSE
    };

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = dynamicStates
    };

    VkPipelineViewportStateCreateInfo viewportState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .lineWidth = 1.0f,
            .cullMode = cullMode,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };

    VkPipelineDepthStencilStateCreateInfo depthStencil = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = depthTest ? VK_TRUE : VK_FALSE,
            .depthWriteEnable = depthTest ? VK_TRUE : VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable = blend ? VK_TRUE : VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
    };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = layout,
            .renderPass = renderPass,
            .subpass = 0
    };

    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(vkinfo.device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline) != VK_SUCCESS) {
        LOGE("Failed to create graphics pipeline for %s", vertName);
        pipeline = VK_NULL_HANDLE;
    }

    vkDestroyShaderModule(vkinfo.device, fragModule, NULL);
    vkDestroyShaderModule(vkinfo.device, vertModule, NULL);

    return pipeline;
}

static void createPipelines(AAssetManager* am, VkRenderPass renderPass) {
    VkDescriptorSetLayoutBinding uboBinding = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, NULL};
    VkDescriptorSetLayoutCreateInfo layoutInfo0 = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &uboBinding
    };
    vkCreateDescriptorSetLayout(vkinfo.device, &layoutInfo0, NULL, &vk_rs.set0Layout);

    VkDescriptorSetLayoutBinding texBindings[] = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
            {
                    .binding = 2,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .pImmutableSamplers = &vk_rs.surfaceSampler
            }
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo1 = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 3,
            .pBindings = texBindings
    };
    vkCreateDescriptorSetLayout(vkinfo.device, &layoutInfo1, NULL, &vk_rs.set1Layout);

    VkDescriptorSetLayout layouts[] = {vk_rs.set0Layout, vk_rs.set1Layout };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = layouts
    };
    vkCreatePipelineLayout(vkinfo.device, &pipelineLayoutInfo, NULL, &vk_rs.pipelineLayout);

    VkDescriptorSetLayoutBinding gltfTexBindings[] = {
            { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
            { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL },
    };
    VkDescriptorSetLayoutCreateInfo gltfDescriptorLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 2,
            .pBindings = gltfTexBindings
    };
    vkCreateDescriptorSetLayout(vkinfo.device, &gltfDescriptorLayoutInfo, NULL, &vk_rs.gltfDescriptorSetLayout);

    VkDescriptorSetLayout gltfLayouts[] = {
            vk_rs.set0Layout,
            vk_rs.gltfDescriptorSetLayout
    };
    VkPipelineLayoutCreateInfo gltfPipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = gltfLayouts
    };
    vkCreatePipelineLayout(vkinfo.device, &gltfPipelineLayoutInfo, NULL, &vk_rs.gltfPipelineLayout);

    VkVertexInputBindingDescription gltfBinding = {0, 8 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription gltfAttribs[] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},                  // Position (Offset 0)
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)},  // Normal (Offset 12)
            {2, 0, VK_FORMAT_R32G32_SFLOAT, 6 * sizeof(float)}      // UV (Offset 24)
    };
    vk_rs.gltfPipeline = createPipelineHelper(am, "gltf.vert.spv", "gltf.frag.spv",
                                              vk_rs.gltfPipelineLayout,
                                              gltfBinding, gltfAttribs, 3,
                                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true, false, VK_CULL_MODE_BACK_BIT, renderPass);

    VkVertexInputBindingDescription lineBinding = {0, 6 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription lineAttribs[] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},               // Position (Offset 0)
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)}    // Color (Offset 12)
    };
    vk_rs.linePipeline = createPipelineHelper(am, "single_color.vert.spv", "single_color.frag.spv",
                                                       vk_rs.pipelineLayout,
                                                       lineBinding, lineAttribs, 2,
                                                       VK_PRIMITIVE_TOPOLOGY_LINE_LIST, true, true, VK_CULL_MODE_NONE, renderPass);

    VkVertexInputBindingDescription blitBinding = { 0, 5 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription blitAttribs[] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
            {1, 0, VK_FORMAT_R32G32_SFLOAT, 3 * sizeof(float)}
    };

    vk_rs.blitPipeline = createPipelineHelper(am, "blit.vert.spv", "blit.frag.spv",
                                                       vk_rs.pipelineLayout,
                                                       blitBinding, blitAttribs, 2,
                                                       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false, true, VK_CULL_MODE_FRONT_BIT, renderPass);
    LOGI("Pipelines initialized");
}

static void createDepthBuffer(VkExtent2D extent) {
    VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent.width = extent.width,
            .extent.height = extent.height,
            .extent.depth = 1,
            .mipLevels = 1,
            .arrayLayers = xrinfo.nViews,
            .format = VK_FORMAT_D16_UNORM,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo allocInfo = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
    vmaCreateImage(vkinfo.allocator, &imageInfo, &allocInfo, &vk_rs.depthImage, &vk_rs.depthAlloc, NULL);

    VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vk_rs.depthImage,
            .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format = VK_FORMAT_D16_UNORM,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = xrinfo.nViews
    };
    vkCreateImageView(vkinfo.device, &viewInfo, NULL, &vk_rs.depthImageView);
    vk_rs.depthSize = extent;
}

static void destroyFramebuffers() {
    if (vk_rs.framebuffers) {
        for (uint32_t i = 0; i < vk_rs.framebufferCount; i++) {
            if (vk_rs.framebuffers[i] != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(vkinfo.device, vk_rs.framebuffers[i], NULL);
            }
            if (vk_rs.swapchainImageViews[i] != VK_NULL_HANDLE) {
                vkDestroyImageView(vkinfo.device, vk_rs.swapchainImageViews[i], NULL);
            }
        }
        free(vk_rs.framebuffers);
        free(vk_rs.swapchainImageViews);
        vk_rs.framebuffers = NULL;
        vk_rs.swapchainImageViews = NULL;
        vk_rs.framebufferCount = 0;
    }

    if (vk_rs.depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(vkinfo.device, vk_rs.depthImageView, NULL);
        vmaDestroyImage(vkinfo.allocator, vk_rs.depthImage, vk_rs.depthAlloc);
        vk_rs.depthImageView = VK_NULL_HANDLE;
        vk_rs.depthImage = VK_NULL_HANDLE;
    }
}

static void ensureRenderResources(VkExtent2D extent) {
    if (vk_rs.framebuffers &&
        vk_rs.depthSize.width == extent.width &&
        vk_rs.depthSize.height == extent.height) {
        return;
    }

    vkDeviceWaitIdle(vkinfo.device);
    destroyFramebuffers();

    LOGI("Creating Framebuffers for extent: %dx%d", extent.width, extent.height);

    createDepthBuffer(extent);

    uint32_t count = xrinfo.renderTarget.swapchainImageCount;
    vk_rs.framebufferCount = count;
    vk_rs.framebuffers = malloc(sizeof(VkFramebuffer) * count);
    vk_rs.swapchainImageViews = malloc(sizeof(VkImageView) * count);

    for (uint32_t i = 0; i < count; i++) {
        VkImageViewCreateInfo viewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = xrinfo.renderTarget.swapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                .format = VK_FORMAT_R8G8B8A8_SRGB, // Must match swapchain format
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, xrinfo.nViews}
        };
        vkCreateImageView(vkinfo.device, &viewInfo, NULL, &vk_rs.swapchainImageViews[i]);

        VkImageView attachments[] = {vk_rs.swapchainImageViews[i], vk_rs.depthImageView };
        VkFramebufferCreateInfo fbInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = vk_rs.renderPass,
                .attachmentCount = 2,
                .pAttachments = attachments,
                .width = extent.width,
                .height = extent.height,
                .layers = 1
        };
        vkCreateFramebuffer(vkinfo.device, &fbInfo, NULL, &vk_rs.framebuffers[i]);
    }
}

static void createRenderPass(VkFormat colorFormat) {
    VkAttachmentDescription attachments[2] = {0};

    attachments[0].format = colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    attachments[1].format = VK_FORMAT_D16_UNORM;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorRef,
            .pDepthStencilAttachment = &depthRef
    };

    uint32_t viewMask = (1 << xrinfo.nViews) - 1;

    VkRenderPassMultiviewCreateInfo multiviewInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
            .subpassCount = 1,
            .pViewMasks = &viewMask,
            .correlationMaskCount = 0,
            .pCorrelationMasks = NULL
    };

    VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = &multiviewInfo,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .subpassCount = 1,
            .pSubpasses = &subpass
    };

    assert(vkCreateRenderPass(vkinfo.device, &renderPassInfo, NULL, &vk_rs.renderPass) == VK_SUCCESS);
}

static void createDescriptorPools() {
    uint32_t imgCount = xrinfo.renderTarget.swapchainImageCount;

    uint32_t totalSets = imgCount * 2;

    VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, imgCount},         // 1 per set0
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imgCount * 3} // (Binding 0, 1, AND 2) per set1
    };

    VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = totalSets,
            .poolSizeCount = 2,
            .pPoolSizes = poolSizes
    };
    vkCreateDescriptorPool(vkinfo.device, &poolInfo, NULL, &vk_rs.descriptorPool);

    VkDescriptorSetLayout* layouts = malloc(totalSets * sizeof(VkDescriptorSetLayout));
    for (uint32_t i = 0; i < imgCount; i++) {
        layouts[i * 2 + 0] = vk_rs.set0Layout;
        layouts[i * 2 + 1] = vk_rs.set1Layout;
    }

    vk_rs.descriptorSets = malloc(totalSets * sizeof(VkDescriptorSet));

    VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vk_rs.descriptorPool,
            .descriptorSetCount = totalSets,
            .pSetLayouts = layouts
    };
    vkAllocateDescriptorSets(vkinfo.device, &allocInfo, vk_rs.descriptorSets);
    free(layouts);

    for (uint32_t i = 0; i < imgCount; i++) {
        VkDescriptorSet set0 = vk_rs.descriptorSets[i * 2 + 0];
        VkDescriptorSet set1 = vk_rs.descriptorSets[i * 2 + 1];

        VkDescriptorBufferInfo bufferInfo = {vk_rs.uniformBuffer, 0, sizeof(UboViewData) };

        VkDescriptorImageInfo imageInfos[1] = {
                { .sampler = vk_rs.surfaceSampler, .imageView = vk_rs.surfaceTextures[i].imageView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
        };

        VkWriteDescriptorSet writes[2];
        writes[0] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set0, .dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .pBufferInfo = &bufferInfo };
        writes[1] = (VkWriteDescriptorSet){
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set1,
                .dstBinding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &imageInfos[0]
        };

        vkUpdateDescriptorSets(vkinfo.device, 2, writes, 0, NULL);
    }
}

static void createSurface() {
    AImageReader_newWithUsage(SURFACE_WIDTH, SURFACE_HEIGHT, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE, (int)xrinfo.renderTarget.swapchainImageCount, &vk_rs.surfaceReader);

    JNIEnv* env = getJniEnv();

    ANativeWindow *window;
    AImageReader_getWindow(vk_rs.surfaceReader, &window);
    jobject surface = ANativeWindow_toSurface(env, window);
    setVulkanSurface(env, surface);

    vk_rs.surfaceTextures = malloc(xrinfo.renderTarget.swapchainImageCount * sizeof(native_surface_texture_t));
    for (int i = 0; i < xrinfo.renderTarget.swapchainImageCount; i++) {
        VkImageCreateInfo imageInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .extent = { SURFACE_WIDTH, SURFACE_HEIGHT, 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .samples = VK_SAMPLE_COUNT_1_BIT
        };

        VmaAllocationCreateInfo allocInfo = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };

        vmaCreateImage(vkinfo.allocator, &imageInfo, &allocInfo,
                       &vk_rs.surfaceTextures[i].image,
                       &vk_rs.surfaceTextures[i].allocation, NULL);

        VkImageViewCreateInfo viewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = vk_rs.surfaceTextures[i].image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        vkCreateImageView(vkinfo.device, &viewInfo, NULL, &vk_rs.surfaceTextures[i].imageView);
    }

    VkSamplerCreateInfo samplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .mipLodBias = 0.0f,
            .minLod = 0.0f,
            .maxLod = 1.0f,
    };

    vkCreateSampler(vkinfo.device, &samplerInfo, NULL, &vk_rs.surfaceSampler);
}

PFN_vkGetAndroidHardwareBufferPropertiesANDROID pfnGetAndroidHardwareBufferPropertiesANDROID = NULL;

static void importSurfaceData(frame_begin_end_state_t* state) {
    AImage* image = NULL;
    if (AImageReader_acquireLatestImage(vk_rs.surfaceReader, &image) != AMEDIA_OK || !image) {
        return;
    }

    AHardwareBuffer* buffer = NULL;
    AImage_getHardwareBuffer(image, &buffer);
    AHardwareBuffer_acquire(buffer); // incrs refcount

    VkExternalMemoryImageCreateInfo extImageInfo = {
            .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
            .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID
    };

    VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = &extImageInfo,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = { SURFACE_WIDTH, SURFACE_HEIGHT, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkImage tempImage;
    vkCreateImage(vkinfo.device, &imageInfo, NULL, &tempImage);

    VkAndroidHardwareBufferPropertiesANDROID ahbProps = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID
    };

    if (!pfnGetAndroidHardwareBufferPropertiesANDROID) {
        pfnGetAndroidHardwareBufferPropertiesANDROID = (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)
                vkGetDeviceProcAddr(vkinfo.device, "vkGetAndroidHardwareBufferPropertiesANDROID");
    }

    if (!pfnGetAndroidHardwareBufferPropertiesANDROID) {
        LOGE("Failed to load vkGetAndroidHardwareBufferPropertiesANDROID! "
             "Ensure VK_ANDROID_external_memory_android_hardware_buffer is enabled.");
    }

    pfnGetAndroidHardwareBufferPropertiesANDROID(vkinfo.device, buffer, &ahbProps);

    VkMemoryDedicatedAllocateInfo dedicatedInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
            .pNext = NULL,
            .image = tempImage,
            .buffer = VK_NULL_HANDLE
    };

    VkImportAndroidHardwareBufferInfoANDROID importInfo = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
            .pNext = &dedicatedInfo,
            .buffer = buffer
    };

    VkMemoryAllocateInfo memAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &importInfo,
            .allocationSize = ahbProps.allocationSize,
            .memoryTypeIndex = findMemoryType(ahbProps.memoryTypeBits, 0)
    };

    VkDeviceMemory tempMem;
    vkAllocateMemory(vkinfo.device, &memAllocInfo, NULL, &tempMem);
    vkBindImageMemory(vkinfo.device, tempImage, tempMem, 0);

    VkCommandBufferAllocateInfo cmdAlloc = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = vkinfo.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(vkinfo.device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo begin = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT };
    vkBeginCommandBuffer(cmd, &begin);

    VkImageMemoryBarrier barrier1 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .image = tempImage,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT
    };

    VkImage targetImage = vk_rs.surfaceTextures[state->frame.imageIndex].image;
    VkImageMemoryBarrier barrier2 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = targetImage,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT
    };

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier1);
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier2);

    VkImageCopy copyRegion = {
            .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            .extent = { SURFACE_WIDTH, SURFACE_HEIGHT, 1 }
    };
    vkCmdCopyImage(cmd, tempImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    VkImageMemoryBarrier barrier3 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .image = targetImage,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier3);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd };
    vkQueueSubmit(vkinfo.graphicsQueue, 1, &submit, NULL);
    vkQueueWaitIdle(vkinfo.graphicsQueue);
    vkFreeCommandBuffers(vkinfo.device, vkinfo.commandPool, 1, &cmd);

    vkDestroyImage(vkinfo.device, tempImage, NULL);
    vkFreeMemory(vkinfo.device, tempMem, NULL);

    AHardwareBuffer_release(buffer);
    AImage_delete(image);
}

static void createAttachmentImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, vk_texture_t* out) {
    VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .extent = {width, height, 1},
            .mipLevels = 1,
            .arrayLayers = xrinfo.nViews,
            .format = format,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo allocInfo = { .usage = VMA_MEMORY_USAGE_GPU_ONLY };
    vmaCreateImage(vkinfo.allocator, &imageInfo, &allocInfo, &out->image, &out->allocation, NULL);

    VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = out->image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format = format,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, xrinfo.nViews}
    };
    vkCreateImageView(vkinfo.device, &viewInfo, NULL, &out->view);
}

static void createSMAARenderPasses() {
    uint32_t viewMask = (1 << xrinfo.nViews) - 1;
    VkRenderPassMultiviewCreateInfo multiviewInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
            .subpassCount = 1,
            .pViewMasks = &viewMask
    };

    VkAttachmentDescription offscreenAtt = {
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkAttachmentDescription depthAtt = {
            .format = VK_FORMAT_D16_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkAttachmentReference offscreenRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription offscreenSubpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &offscreenRef,
            .pDepthStencilAttachment = &depthRef
    };

    VkRenderPassCreateInfo offInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = &multiviewInfo,
            .attachmentCount = 2,
            .pAttachments = (VkAttachmentDescription[]){offscreenAtt, depthAtt},
            .subpassCount = 1,
            .pSubpasses = &offscreenSubpass
    };
    vkCreateRenderPass(vkinfo.device, &offInfo, NULL, &vk_rs.smaa.offscreenPass);

    VkAttachmentDescription edgeAtt = {
            .format = VK_FORMAT_R8G8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkAttachmentReference edgeRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription edgeSubpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &edgeRef
    };
    VkRenderPassCreateInfo edgeInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = &multiviewInfo,
            .attachmentCount = 1,
            .pAttachments = &edgeAtt,
            .subpassCount = 1,
            .pSubpasses = &edgeSubpass
    };
    vkCreateRenderPass(vkinfo.device, &edgeInfo, NULL, &vk_rs.smaa.edgePass);

    VkAttachmentDescription weightAtt = {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkAttachmentReference weightRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription weightSubpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &weightRef
    };
    VkRenderPassCreateInfo weightInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext = &multiviewInfo,
            .attachmentCount = 1,
            .pAttachments = &weightAtt,
            .subpassCount = 1,
            .pSubpasses = &weightSubpass
    };
    vkCreateRenderPass(vkinfo.device, &weightInfo, NULL, &vk_rs.smaa.weightPass);
}

bool initSMAA(AAssetManager* am) {
    VkSamplerCreateInfo areaSamplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };

    VkSamplerCreateInfo searchSamplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    };

    loadKtxEx(&(asset_info_t){am, "antialiasing/smaa/area.ktx"}, &vk_rs.smaa.areaTex, areaSamplerInfo);
    loadKtxEx(&(asset_info_t){am, "antialiasing/smaa/search.ktx"}, &vk_rs.smaa.searchTex, searchSamplerInfo);

    createSMAARenderPasses();

    VkDescriptorSetLayoutBinding bindings[] = {
            {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
            {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
            {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
            {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
            {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, NULL},
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 5,
            .pBindings = bindings
    };
    vkCreateDescriptorSetLayout(vkinfo.device, &layoutInfo, NULL, &vk_rs.smaa.descriptorSetLayout);

    VkPushConstantRange pushConstantRange = {
            .size = 4 * sizeof(float),
            .stageFlags = VK_SHADER_STAGE_ALL
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &vk_rs.smaa.descriptorSetLayout,
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &pushConstantRange
    };
    vkCreatePipelineLayout(vkinfo.device, &pipelineLayoutInfo, NULL, &vk_rs.smaa.pipelineLayout);

    VkVertexInputBindingDescription emptyBinding = {0};
    vk_rs.smaa.edgePipeline = createPipelineHelper(am, "antialiasing/smaa/smaa_edge.vert.spv", "antialiasing/smaa/smaa_edge.frag.spv",
                                                  vk_rs.smaa.pipelineLayout, emptyBinding, NULL, 0,
                                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false, false, VK_CULL_MODE_NONE, vk_rs.smaa.edgePass);

    vk_rs.smaa.weightPipeline = createPipelineHelper(am, "antialiasing/smaa/smaa_weight.vert.spv", "antialiasing/smaa/smaa_weight.frag.spv",
                                                    vk_rs.smaa.pipelineLayout, emptyBinding, NULL, 0,
                                                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false, false, VK_CULL_MODE_NONE, vk_rs.smaa.weightPass);

    vk_rs.smaa.blendPipeline = createPipelineHelper(am, "antialiasing/smaa/smaa_blend.vert.spv", "antialiasing/smaa/smaa_blend.frag.spv",
                                                   vk_rs.smaa.pipelineLayout, emptyBinding, NULL, 0,
                                                   VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, false, false, VK_CULL_MODE_NONE, vk_rs.renderPass);

    VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5};
    VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
    };
    vkCreateDescriptorPool(vkinfo.device, &poolInfo, NULL, &vk_rs.smaa.descriptorPool);

    VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vk_rs.smaa.descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &vk_rs.smaa.descriptorSetLayout
    };
    vkAllocateDescriptorSets(vkinfo.device, &allocInfo, &vk_rs.smaa.descriptorSet);

    return true;
}

static void ensureSMAAResources(VkExtent2D extent) {
    static VkExtent2D currentExtent = {0, 0};
    if (currentExtent.width == extent.width && currentExtent.height == extent.height) return;

    currentExtent = extent;
    vkDeviceWaitIdle(vkinfo.device);

    if (vk_rs.smaa.offscreenFramebuffer) vkDestroyFramebuffer(vkinfo.device, vk_rs.smaa.offscreenFramebuffer, NULL);
    if (vk_rs.smaa.edgeFramebuffer) vkDestroyFramebuffer(vkinfo.device, vk_rs.smaa.edgeFramebuffer, NULL);
    if (vk_rs.smaa.weightFramebuffer) vkDestroyFramebuffer(vkinfo.device, vk_rs.smaa.weightFramebuffer, NULL);
    if (vk_rs.smaa.sceneTargetTexture.view) { vkDestroyImageView(vkinfo.device, vk_rs.smaa.sceneTargetTexture.view, NULL); vmaDestroyImage(vkinfo.allocator, vk_rs.smaa.sceneTargetTexture.image, vk_rs.smaa.sceneTargetTexture.allocation); }
    if (vk_rs.smaa.edgeTexture.view) { vkDestroyImageView(vkinfo.device, vk_rs.smaa.edgeTexture.view, NULL); vmaDestroyImage(vkinfo.allocator, vk_rs.smaa.edgeTexture.image, vk_rs.smaa.edgeTexture.allocation); }
    if (vk_rs.smaa.weightTexture.view) { vkDestroyImageView(vkinfo.device, vk_rs.smaa.weightTexture.view, NULL); vmaDestroyImage(vkinfo.allocator, vk_rs.smaa.weightTexture.image, vk_rs.smaa.weightTexture.allocation); }

    createAttachmentImage(extent.width, extent.height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk_rs.smaa.sceneTargetTexture);
    createAttachmentImage(extent.width, extent.height, VK_FORMAT_R8G8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk_rs.smaa.edgeTexture);
    createAttachmentImage(extent.width, extent.height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk_rs.smaa.weightTexture);

    VkImageView offscreenAtt[] = {vk_rs.smaa.sceneTargetTexture.view, vk_rs.depthImageView};
    VkFramebufferCreateInfo fbInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = vk_rs.smaa.offscreenPass,
            .attachmentCount = 2,
            .pAttachments = offscreenAtt,
            .width = extent.width, .height = extent.height, .layers = 1
    };
    vkCreateFramebuffer(vkinfo.device, &fbInfo, NULL, &vk_rs.smaa.offscreenFramebuffer);

    fbInfo.renderPass = vk_rs.smaa.edgePass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &vk_rs.smaa.edgeTexture.view;
    vkCreateFramebuffer(vkinfo.device, &fbInfo, NULL, &vk_rs.smaa.edgeFramebuffer);

    fbInfo.renderPass = vk_rs.smaa.weightPass;
    fbInfo.pAttachments = &vk_rs.smaa.weightTexture.view;
    vkCreateFramebuffer(vkinfo.device, &fbInfo, NULL, &vk_rs.smaa.weightFramebuffer);

    VkDescriptorImageInfo sceneInfo = {vk_rs.surfaceSampler, vk_rs.smaa.sceneTargetTexture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo edgeInfo = {vk_rs.surfaceSampler, vk_rs.smaa.edgeTexture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo areaInfo = {vk_rs.smaa.areaTex.sampler, vk_rs.smaa.areaTex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo searchInfo = {vk_rs.smaa.searchTex.sampler, vk_rs.smaa.searchTex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkDescriptorImageInfo weightInfo = {vk_rs.surfaceSampler, vk_rs.smaa.weightTexture.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet writes[] = {
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, vk_rs.smaa.descriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sceneInfo, NULL, NULL},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, vk_rs.smaa.descriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &edgeInfo, NULL, NULL},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, vk_rs.smaa.descriptorSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &areaInfo, NULL, NULL},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, vk_rs.smaa.descriptorSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &searchInfo, NULL, NULL},
            {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, vk_rs.smaa.descriptorSet, 4, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &weightInfo, NULL, NULL},
    };
    vkUpdateDescriptorSets(vkinfo.device, 5, writes, 0, NULL);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantParameter"
static bool loadTexture(ktx_texture_t* texture, asset_info_t* uploadInfo) {
    return loadKtx(uploadInfo, texture);
}
#pragma clang diagnostic pop

bool initRenderer(AAssetManager *assetManager) {
    if (!vkinfo.initialized) {
        LOGE("Vulkan must be initialized before Renderer");
        return false;
    }

    createSurface();

    createBuffer(sizeof(UboViewData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, &vk_rs.uniformBuffer, &vk_rs.uniformAlloc);
    vmaMapMemory(vkinfo.allocator, vk_rs.uniformAlloc, &vk_rs.uniformMappedData);

    loadAssetModel(&vk_rs.targetRectModel, "tv.x", assetManager);
    createVkModel(&vk_rs.leftRay, NULL, 12 * sizeof(float), true);
    createVkModel(&vk_rs.rightRay, NULL, 12 * sizeof(float), true);

    createRenderPass(VK_FORMAT_R8G8B8A8_SRGB);

    createPipelines(assetManager, vk_rs.renderPass);
    createDescriptorPools();

    VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = vkinfo.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = xrinfo.renderTarget.swapchainImageCount
    };
    vk_rs.cmdBuffers = malloc(xrinfo.renderTarget.swapchainImageCount * sizeof(VkCommandBuffer));
    vkAllocateCommandBuffers(vkinfo.device, &allocInfo, vk_rs.cmdBuffers);

    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    vk_rs.renderFences = malloc(xrinfo.renderTarget.swapchainImageCount * sizeof(VkFence));
    for (int i = 0; i < xrinfo.renderTarget.swapchainImageCount; i++) {
        vkCreateFence(vkinfo.device, &fenceInfo, NULL, &vk_rs.renderFences[i]);
    }

    initSMAA(assetManager);
    model_load(&(asset_info_t){.path = "scene.glb", .assetManager = assetManager}, &vk_rs.worldModelGltf);

    LOGI("Renderer initialized successfully (Vulkan)");
    return true;
}

static void updateUniforms(frame_begin_end_state_t *state) {
    UboViewData ubo = {0};

    for(uint32_t i = 0; i < xrinfo.renderTarget.swapchainImageCount; i++) {
        XrCompositionLayerProjectionView projectionView = state->projectionViews[i];
        XrMatrix4x4f projection, view;

        XrMatrix4x4f_CreateProjectionFov(&projection, projectionView.fov, 0.1f, 1000.0f);
        XrMatrix4x4f_CreateViewMatrix(&view, &projectionView.pose.position, &projectionView.pose.orientation);
        XrMatrix4x4f_Multiply(&ubo.projectionViews[i], &projection, &view);
    }

    XrMatrix4x4f model_translate, model_rotate;
    XrMatrix4x4f_CreateTranslation(&model_translate, 1.5f, -2, -15.5f);
    XrMatrix4x4f_CreateRotation(&model_rotate, 0, xrInput.worldRotationY, 0);
    XrMatrix4x4f_Multiply(&ubo.modelMatrix, &model_rotate, &model_translate);

    memcpy(vk_rs.uniformMappedData, &ubo, sizeof(UboViewData));
}

static void updateLines(vk_model_t* lineModel, XrVector3f color, XrVector3f start, XrVector3f end) {
    float data[12] = {
            start.x, start.y, start.z, color.x, color.y, color.z,
            end.x, end.y, end.z, color.x, color.y, color.z
    };

    void* ptr;
    vmaMapMemory(vkinfo.allocator, lineModel->allocation, &ptr);
    memcpy(ptr, data, sizeof(data));
    vmaUnmapMemory(vkinfo.allocator, lineModel->allocation);
}

void renderFrame(frame_begin_end_state_t *state) {
    vkWaitForFences(vkinfo.device, 1, &vk_rs.renderFences[state->frame.imageIndex], VK_TRUE, UINT64_MAX);
    vkResetFences(vkinfo.device, 1, &vk_rs.renderFences[state->frame.imageIndex]);

    importSurfaceData(state);

    updateUniforms(state);

    XrMatrix4x4f modelMat = ((UboViewData*)vk_rs.uniformMappedData)->modelMatrix;
    for (int i = 0; i < 2; i++) {
        vk_model_t* line = (i == 0) ? &vk_rs.leftRay : &vk_rs.rightRay;
        XrVector3f start, end;
        getControllerRay(i, modelMat, &start, &end);
        XrVector3f dir;
        XrVector3f_Sub(&dir, &end, &start);
        XrVector3f_Normalize(&dir);
        XrVector3f scaled;
        XrVector3f_Scale(&scaled, &dir, 10.0f);
        XrVector3f newEnd;
        XrVector3f_Add(&newEnd, &start, &scaled);
        updateLines(line, (XrVector3f){1,0,0}, start, newEnd);
    }

    XrExtent2Di rect = state->frame.outputRect.extent;
    ensureRenderResources((VkExtent2D){(uint32_t)rect.width, (uint32_t)rect.height});
    ensureSMAAResources((VkExtent2D){(uint32_t)rect.width, (uint32_t)rect.height});

    uint32_t imgIndex = state->frame.imageIndex;
    VkFramebuffer currentFb = vk_rs.framebuffers[imgIndex];

    VkCommandBuffer cmd = vk_rs.cmdBuffers[imgIndex];

    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue clearValues[2] = {
            {.color = {{0.1f, 0.1f, 0.1f, 1.0f}}},
            {.depthStencil = {1.0f, 0}}
    };

    VkRenderPassBeginInfo rpInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vk_rs.smaa.offscreenPass,
            .framebuffer = vk_rs.smaa.offscreenFramebuffer,
            .renderArea.extent = {rect.width, rect.height},
            .clearValueCount = 2,
            .pClearValues = clearValues
    };

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {0, 0, (float)rect.width, (float)rect.height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {{0,0}, {rect.width, rect.height}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offsets[] = {0};

    VkDescriptorSet sets[] = {
            vk_rs.descriptorSets[state->frame.imageIndex * 2 + 0],
            vk_rs.descriptorSets[state->frame.imageIndex * 2 + 1],
    };

    // World
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.gltfPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            vk_rs.gltfPipelineLayout, 0, 1,
                            &vk_rs.descriptorSets[imgIndex * 2], 0, NULL);
    model_draw(cmd, imgIndex, &vk_rs.worldModelGltf);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.pipelineLayout, 0, 2, sets, 0, NULL);

    // Screen
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.blitPipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_rs.targetRectModel.buffer, offsets);
    vkCmdDraw(cmd, 6, 1, 0, 0);

    // Rays
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.linePipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_rs.leftRay.buffer, offsets);
    vkCmdDraw(cmd, vk_rs.leftRay.vertexCount, 1, 0, 0);
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk_rs.rightRay.buffer, offsets);
    vkCmdDraw(cmd, vk_rs.rightRay.vertexCount, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    float width = (float)state->frame.outputRect.extent.width;
    float height = (float)state->frame.outputRect.extent.height;
    float metrics[4] = {1.0f / width, 1.0f / height, width, height};

    // SMAA edges
    VkClearValue smaaClear = {.color = {{0.0f, 0.0f, 0.0f, 0.0f}}};
    VkRenderPassBeginInfo edgeRp = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vk_rs.smaa.edgePass,
            .framebuffer = vk_rs.smaa.edgeFramebuffer,
            .renderArea.extent = {rect.width, rect.height},
            .clearValueCount = 1, .pClearValues = &smaaClear
    };
    vkCmdBeginRenderPass(cmd, &edgeRp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.smaa.edgePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.smaa.pipelineLayout, 0, 1, &vk_rs.smaa.descriptorSet, 0, NULL);
    vkCmdPushConstants(cmd, vk_rs.smaa.pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(metrics), metrics);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    // SMAA weights
    VkRenderPassBeginInfo weightRp = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vk_rs.smaa.weightPass,
            .framebuffer = vk_rs.smaa.weightFramebuffer,
            .renderArea.extent = {rect.width, rect.height},
            .clearValueCount = 1, .pClearValues = &smaaClear
    };
    vkCmdBeginRenderPass(cmd, &weightRp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.smaa.weightPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.smaa.pipelineLayout, 0, 1, &vk_rs.smaa.descriptorSet, 0, NULL);
    vkCmdPushConstants(cmd, vk_rs.smaa.pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(metrics), metrics);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    // SMAA neighborhood blending (final)
    VkRenderPassBeginInfo finalRp = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vk_rs.renderPass,
            .framebuffer = vk_rs.framebuffers[imgIndex],
            .renderArea.extent = {rect.width, rect.height},
            .clearValueCount = 2,
            .pClearValues = clearValues
    };
    vkCmdBeginRenderPass(cmd, &finalRp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.smaa.blendPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.smaa.pipelineLayout, 0, 1, &vk_rs.smaa.descriptorSet, 0, NULL);
    vkCmdPushConstants(cmd, vk_rs.smaa.pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(metrics), metrics);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd
    };
    vkQueueSubmit(vkinfo.graphicsQueue, 1, &submitInfo, vk_rs.renderFences[imgIndex]);
}

static void destroyVkModel(vk_model_t* model) {
    if (model->buffer) {
        vmaDestroyBuffer(vkinfo.allocator, model->buffer, model->allocation);
        model->buffer = VK_NULL_HANDLE;
    }
}

static void destroyKtxTexture(ktx_texture_t* tex) {
    if (tex->view) vkDestroyImageView(vkinfo.device, tex->view, NULL);
    if (tex->image) vmaDestroyImage(vkinfo.allocator, tex->image, tex->allocation);
    if (tex->sampler) vkDestroySampler(vkinfo.device, tex->sampler, NULL);
}

void cleanupRenderer() {
    if (vkinfo.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vkinfo.device);
    }

    if (vk_rs.renderFences) {
        for (int i = 0; i < xrinfo.renderTarget.swapchainImageCount; i++) {
            vkDestroyFence(vkinfo.device, vk_rs.renderFences[i], NULL);
        }
        free(vk_rs.renderFences);
    }
    if (vk_rs.cmdBuffers) {
        vkFreeCommandBuffers(vkinfo.device, vkinfo.commandPool, xrinfo.renderTarget.swapchainImageCount, vk_rs.cmdBuffers);
        free(vk_rs.cmdBuffers);
    }

    vkDestroyPipeline(vkinfo.device, vk_rs.linePipeline, NULL);
    vkDestroyPipeline(vkinfo.device, vk_rs.blitPipeline, NULL);
    vkDestroyPipelineLayout(vkinfo.device, vk_rs.pipelineLayout, NULL);

    if (vk_rs.descriptorPool) {
        vkDestroyDescriptorPool(vkinfo.device, vk_rs.descriptorPool, NULL);
        free(vk_rs.descriptorSets);
    }
    vkDestroyDescriptorSetLayout(vkinfo.device, vk_rs.set0Layout, NULL);
    vkDestroyDescriptorSetLayout(vkinfo.device, vk_rs.set1Layout, NULL);

    destroyVkModel(&vk_rs.targetRectModel);
    destroyVkModel(&vk_rs.leftRay);
    destroyVkModel(&vk_rs.rightRay);

    model_free(&vk_rs.worldModelGltf);

    if (vk_rs.surfaceSampler) {
        vkDestroySampler(vkinfo.device, vk_rs.surfaceSampler, NULL);
    }
    if (vk_rs.surfaceTextures) {
        for (int i = 0; i < xrinfo.renderTarget.swapchainImageCount; i++) {
            vkDestroyImageView(vkinfo.device, vk_rs.surfaceTextures[i].imageView, NULL);
            vmaDestroyImage(vkinfo.allocator, vk_rs.surfaceTextures[i].image, vk_rs.surfaceTextures[i].allocation);
        }
        free(vk_rs.surfaceTextures);
    }
    if (vk_rs.surfaceReader) {
        AImageReader_delete(vk_rs.surfaceReader);
    }

    if (vk_rs.uniformMappedData) {
        vmaUnmapMemory(vkinfo.allocator, vk_rs.uniformAlloc);
    }
    vmaDestroyBuffer(vkinfo.allocator, vk_rs.uniformBuffer, vk_rs.uniformAlloc);

    destroyFramebuffers();
    vkDestroyRenderPass(vkinfo.device, vk_rs.renderPass, NULL);

    LOGI("Renderer cleanup complete.");
}