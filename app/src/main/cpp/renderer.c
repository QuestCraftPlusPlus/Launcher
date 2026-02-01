//
// Created by maks on 12.12.2024.
//

#include "renderer.h"
#include "asset_buffer_read.h"
#include "ktx_texture.h"
#include "vk_init.h"
#include "xr_init.h"
#include "xr_linear_algebra.h"
#include "xr_input.h"
#include "renderer_types.h"
#include "main.h"

#include <media/NdkImageReader.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/hardware_buffer.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"

typedef struct {
    XrMatrix4x4f projectionViews[2]; // Max 2 views usually (apparently the fucking varjo has 4???)
    XrMatrix4x4f modelMatrix;
} UboViewData;

typedef struct {
    AHardwareBuffer* hb;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
    uint32_t last_frame_used;
} surface_buffer_t;

typedef struct {
    ANativeWindow* window;
    AImageReader* reader;

    surface_buffer_t* buffers;
    VkSampler sampler;
    VkSamplerYcbcrConversion conversion;

    int width;
    int height;
    bool frameReady;
} native_surface_t;

struct {
    VkPipelineLayout pipelineLayout;
    VkPipeline worldPipeline;
    VkPipeline linePipeline;
    VkPipeline blitPipeline;

    VkRenderPass renderPass;
    VkExtent2D depthSize;
    VkImage depthImage;
    VmaAllocation depthAlloc;
    VkImageView depthImageView;

    VkFramebuffer* framebuffers;
    uint32_t framebufferCount;
    VkImageView* swapchainImageViews;

    vk_model_t worldModel;
    vk_model_t targetRectModel;
    vk_model_t leftRay;
    vk_model_t rightRay;

    vk_texture_t atlas;
    vk_texture_t light;

    native_surface_t surface;

    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet* descriptorSets;

    VkDescriptorSetLayout set0Layout;
    VkDescriptorSetLayout set1Layout;

    VkBuffer uniformBuffer;
    VmaAllocation uniformAlloc;
    void* uniformMappedData;

    VkCommandBuffer* cmdBuffers;
    VkFence* renderFences;
} vk_rs;

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
                                       VkVertexInputBindingDescription bindingDesc,
                                       VkVertexInputAttributeDescription* attribs, uint32_t attribCount,
                                       VkPrimitiveTopology topology, bool depthTest, bool blend, VkRenderPass renderPass) {

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
            .cullMode = VK_CULL_MODE_BACK_BIT,
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
            .layout = vk_rs.pipelineLayout,
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
                    .pImmutableSamplers = &vk_rs.surface.sampler
            }
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo1 = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 3,
            .pBindings = texBindings
    };
    vkCreateDescriptorSetLayout(vkinfo.device, &layoutInfo1, NULL, &vk_rs.set1Layout);

    VkDescriptorSetLayout layouts[] = { vk_rs.set0Layout, vk_rs.set1Layout };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 2,
            .pSetLayouts = layouts
    };
    vkCreatePipelineLayout(vkinfo.device, &pipelineLayoutInfo, NULL, &vk_rs.pipelineLayout);

    VkVertexInputBindingDescription worldBinding = {0, 8 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription worldAttribs[] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},               // Position (Offset 0)
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 3 * sizeof(float)} // Tex Light (Offset 12)
    };
    vk_rs.worldPipeline = createPipelineHelper(am, "lightmap.vert.spv", "lightmap.frag.spv",
                                               worldBinding, worldAttribs, 2,
                                               VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true, false, renderPass);

    VkVertexInputBindingDescription lineBinding = {0, 6 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription lineAttribs[] = {
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},               // Position (Offset 0)
            {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)}    // Color (Offset 12)
    };
    vk_rs.linePipeline = createPipelineHelper(am, "single_color.vert.spv", "single_color.frag.spv",
                                              lineBinding, lineAttribs, 2,
                                              VK_PRIMITIVE_TOPOLOGY_LINE_LIST, true, true, renderPass);

    vk_rs.blitPipeline = createPipelineHelper(am, "blit.vert.spv", "blit.frag.spv",
                                              worldBinding, worldAttribs, 2,
                                              VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true, true, renderPass);
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

        VkImageView attachments[] = { vk_rs.swapchainImageViews[i], vk_rs.depthImageView };
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

        VkDescriptorBufferInfo bufferInfo = { vk_rs.uniformBuffer, 0, sizeof(UboViewData) };

        VkDescriptorImageInfo imageInfos[3] = {
                { .sampler = vk_rs.atlas.sampler, .imageView = vk_rs.atlas.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
                { .sampler = vk_rs.light.sampler, .imageView = vk_rs.light.view, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
                { .sampler = vk_rs.surface.sampler, .imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
        };

        VkWriteDescriptorSet writes[3];
        writes[0] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set0, .dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .pBufferInfo = &bufferInfo };
        writes[1] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set1, .dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .pImageInfo = &imageInfos[0] };
        writes[2] = (VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set1, .dstBinding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .pImageInfo = &imageInfos[1] };

        vkUpdateDescriptorSets(vkinfo.device, 3, writes, 0, NULL);
    }
}

PFN_vkGetAndroidHardwareBufferPropertiesANDROID pfnGetAndroidHardwareBufferPropertiesANDROID;

static void createSurface() {
    if (!pfnGetAndroidHardwareBufferPropertiesANDROID) {
        pfnGetAndroidHardwareBufferPropertiesANDROID = (PFN_vkGetAndroidHardwareBufferPropertiesANDROID)
                vkGetDeviceProcAddr(vkinfo.device, "vkGetAndroidHardwareBufferPropertiesANDROID");
    }

    vk_rs.surface.buffers = malloc(xrinfo.renderTarget.swapchainImageCount * sizeof(surface_buffer_t));
    media_status_t status = AImageReader_newWithUsage(
            2560, 1440,
            AIMAGE_FORMAT_RGBA_8888,
            AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE,
            (int)xrinfo.renderTarget.swapchainImageCount,
            &vk_rs.surface.reader
    );

    AImageReader_getWindow(vk_rs.surface.reader, &vk_rs.surface.window);

    JNIEnv* env = getJniEnv();
    jobject jSurface = ANativeWindow_toSurface(env, vk_rs.surface.window);
    setVulkanSurface(env, jSurface);

    PFN_vkCreateSamplerYcbcrConversion pfnCreateSamplerYcbcrConversion =
            (PFN_vkCreateSamplerYcbcrConversion)vkGetDeviceProcAddr(vkinfo.device, "vkCreateSamplerYcbcrConversion");

    VkSamplerYcbcrConversionCreateInfo convInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .ycbcrModel = VK_SAMPLER_YCBCR_MODEL_CONVERSION_RGB_IDENTITY,
            .ycbcrRange = VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
            .components = {VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY},
            .chromaFilter = VK_FILTER_LINEAR,
            .forceExplicitReconstruction = VK_FALSE
    };
    pfnCreateSamplerYcbcrConversion(vkinfo.device, &convInfo, NULL, &vk_rs.surface.conversion);

    VkSamplerYcbcrConversionInfo conversionInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
            .conversion = vk_rs.surface.conversion
    };

    VkSamplerCreateInfo samplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = &conversionInfo,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .anisotropyEnable = VK_FALSE,
            .unnormalizedCoordinates = VK_FALSE
    };
    vkCreateSampler(vkinfo.device, &samplerInfo, NULL, &vk_rs.surface.sampler);
}

static void importSurfaceData(frame_begin_end_state_t* state) {
    AImage* image = NULL;
    if (AImageReader_acquireLatestImage(vk_rs.surface.reader, &image) != AMEDIA_OK) return;

    AHardwareBuffer* buffer = NULL;
    AImage_getHardwareBuffer(image, &buffer);

    uint32_t slot = state->frame.imageIndex;
    if (vk_rs.surface.buffers[slot].hb != NULL) {
        vkDestroyImageView(vkinfo.device, vk_rs.surface.buffers[slot].view, NULL);
        vk_rs.surface.buffers[slot].view = NULL;
        vkDestroyImage(vkinfo.device, vk_rs.surface.buffers[slot].image, NULL);
        vk_rs.surface.buffers[slot].image = NULL;
        vkFreeMemory(vkinfo.device, vk_rs.surface.buffers[slot].memory, NULL);
        vk_rs.surface.buffers[slot].memory = NULL;
        AHardwareBuffer_release(vk_rs.surface.buffers[slot].hb);
        vk_rs.surface.buffers[slot].hb = NULL;
    }

    if (vk_rs.surface.buffers[slot].hb != buffer) {
        vk_rs.surface.buffers[slot].hb = buffer;
        AHardwareBuffer_acquire(buffer);

        AHardwareBuffer_Desc desc;
        AHardwareBuffer_describe(buffer, &desc);

        VkAndroidHardwareBufferFormatPropertiesANDROID formatProps = {
                .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_FORMAT_PROPERTIES_ANDROID
        };
        VkAndroidHardwareBufferPropertiesANDROID props = {
                .sType = VK_STRUCTURE_TYPE_ANDROID_HARDWARE_BUFFER_PROPERTIES_ANDROID,
                .pNext = &formatProps
        };
        pfnGetAndroidHardwareBufferPropertiesANDROID(vkinfo.device, buffer, &props);

        VkExternalMemoryImageCreateInfo externalInfo = {
                .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
                .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID
        };

        VkImageCreateInfo icharInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = &externalInfo,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = formatProps.format,
                .extent = {2560, 1440, 1},
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        vkCreateImage(vkinfo.device, &icharInfo, NULL, &vk_rs.surface.buffers[slot].image);

        VkImportAndroidHardwareBufferInfoANDROID importInfo = {
                .sType = VK_STRUCTURE_TYPE_IMPORT_ANDROID_HARDWARE_BUFFER_INFO_ANDROID,
                .buffer = buffer
        };

        VkMemoryDedicatedAllocateInfo dedicatedInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
                .pNext = &importInfo,
                .image = vk_rs.surface.buffers[slot].image,
                .buffer = VK_NULL_HANDLE
        };

        VkMemoryAllocateInfo allocInfo = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .pNext = &dedicatedInfo,
                .allocationSize = props.allocationSize,
                .memoryTypeIndex = findMemoryType(props.memoryTypeBits, 0)
        };

        vkAllocateMemory(vkinfo.device, &allocInfo, NULL, &vk_rs.surface.buffers[slot].memory);
        vkBindImageMemory(vkinfo.device, vk_rs.surface.buffers[slot].image,
                          vk_rs.surface.buffers[slot].memory, 0);

        VkSamplerYcbcrConversionInfo conversionInfo = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO,
                .conversion = vk_rs.surface.conversion
        };

        VkImageViewCreateInfo viewInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = &conversionInfo,
                .image = vk_rs.surface.buffers[slot].image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = formatProps.format,
                .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };

        vkCreateImageView(vkinfo.device, &viewInfo, NULL, &vk_rs.surface.buffers[slot].view);

        VkDescriptorImageInfo webViewInfo = {
                .sampler = VK_NULL_HANDLE,
                .imageView = vk_rs.surface.buffers[slot].view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vk_rs.descriptorSets[state->frame.imageIndex * 2 + 1],
                .dstBinding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &webViewInfo
        };
        vkUpdateDescriptorSets(vkinfo.device, 1, &write, 0, NULL);
    }

    vk_rs.surface.buffers[slot].last_frame_used = state->frame.imageIndex;
    AImage_delete(image);
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "ConstantParameter"
static bool loadTexture(vk_texture_t* texture, asset_info_t* uploadInfo) {
    return loadKtx(uploadInfo, texture);
}
#pragma clang diagnostic pop

static bool loadTextures(AAssetManager* assetManager) {
    asset_info_t atexUploadInfo = {assetManager, "atlas_texture.ktx"};
    asset_info_t ltexUploadInfo = {assetManager, "light_texture.ktx"};
    return loadTexture(&vk_rs.atlas, &atexUploadInfo) &&
           loadTexture(&vk_rs.light, &ltexUploadInfo);
}

bool initRenderer(AAssetManager *assetManager) {
    if (!vkinfo.initialized) {
        LOGE("Vulkan must be initialized before Renderer");
        return false;
    }

    createSurface();
    loadTextures(assetManager);

    createBuffer(sizeof(UboViewData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, &vk_rs.uniformBuffer, &vk_rs.uniformAlloc);
    vmaMapMemory(vkinfo.allocator, vk_rs.uniformAlloc, &vk_rs.uniformMappedData);

    loadAssetModel(&vk_rs.worldModel, "simplemodel.x", assetManager);
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
    XrMatrix4x4f_CreateRotation(&model_rotate, 0, 180, 0);
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
        updateLines(line, (XrVector3f){1,0,0}, start, end);
    }

    XrExtent2Di rect = state->frame.outputRect.extent;
    ensureRenderResources((VkExtent2D){(uint32_t)rect.width, (uint32_t)rect.height});

    uint32_t imgIndex = state->frame.imageIndex;
    VkFramebuffer currentFb = vk_rs.framebuffers[imgIndex];

    vkResetCommandBuffer(vk_rs.cmdBuffers[imgIndex], 0);
    VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(vk_rs.cmdBuffers[imgIndex], &beginInfo);

    VkClearValue clearValues[2] = {
            {.color = {{0.1f, 0.1f, 0.1f, 1.0f}}},
            {.depthStencil = {1.0f, 0}}
    };

    VkRenderPassBeginInfo rpInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = vk_rs.renderPass,
            .framebuffer = currentFb,
            .renderArea.extent = {rect.width, rect.height},
            .clearValueCount = 2,
            .pClearValues = clearValues
    };

    vkCmdBeginRenderPass(vk_rs.cmdBuffers[imgIndex], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {0, 0, (float)rect.width, (float)rect.height, 0.0f, 1.0f};
    vkCmdSetViewport(vk_rs.cmdBuffers[imgIndex], 0, 1, &viewport);
    VkRect2D scissor = {{0,0}, {rect.width, rect.height}};
    vkCmdSetScissor(vk_rs.cmdBuffers[imgIndex], 0, 1, &scissor);

    VkDeviceSize offsets[] = {0};

    // World
    vkCmdBindPipeline(vk_rs.cmdBuffers[imgIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.worldPipeline);
    VkDescriptorSet sets[] = {
            vk_rs.descriptorSets[state->frame.imageIndex * 2 + 0],
            vk_rs.descriptorSets[state->frame.imageIndex * 2 + 1],
    };
    vkCmdBindDescriptorSets(vk_rs.cmdBuffers[imgIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.pipelineLayout, 0, 2, sets, 0, NULL);
    vkCmdBindVertexBuffers(vk_rs.cmdBuffers[imgIndex], 0, 1, &vk_rs.worldModel.buffer, offsets);
    vkCmdDraw(vk_rs.cmdBuffers[imgIndex], vk_rs.worldModel.vertexCount, 1, 0, 0);

    vkCmdBindPipeline(vk_rs.cmdBuffers[imgIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.blitPipeline);
    vkCmdBindVertexBuffers(vk_rs.cmdBuffers[imgIndex], 0, 1, &vk_rs.targetRectModel.buffer, offsets);
    vkCmdDraw(vk_rs.cmdBuffers[imgIndex], vk_rs.targetRectModel.vertexCount, 1, 0, 0);

    // Rays
    vkCmdBindPipeline(vk_rs.cmdBuffers[imgIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, vk_rs.linePipeline);
    vkCmdBindVertexBuffers(vk_rs.cmdBuffers[imgIndex], 0, 1, &vk_rs.leftRay.buffer, offsets);
    vkCmdDraw(vk_rs.cmdBuffers[imgIndex], vk_rs.leftRay.vertexCount, 1, 0, 0);
    vkCmdBindVertexBuffers(vk_rs.cmdBuffers[imgIndex], 0, 1, &vk_rs.rightRay.buffer, offsets);
    vkCmdDraw(vk_rs.cmdBuffers[imgIndex], vk_rs.rightRay.vertexCount, 1, 0, 0);

    vkCmdEndRenderPass(vk_rs.cmdBuffers[imgIndex]);
    vkEndCommandBuffer(vk_rs.cmdBuffers[imgIndex]);

    VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &vk_rs.cmdBuffers[imgIndex]
    };
    vkQueueSubmit(vkinfo.graphicsQueue, 1, &submitInfo, vk_rs.renderFences[imgIndex]);
}

static void destroyVkModel(vk_model_t* model) {
    if (model->buffer) {
        vmaDestroyBuffer(vkinfo.allocator, model->buffer, model->allocation);
        model->buffer = VK_NULL_HANDLE;
    }
}

static void destroyVkTexture(vk_texture_t* tex) {
    if (tex->view) vkDestroyImageView(vkinfo.device, tex->view, NULL);
    if (tex->image) vmaDestroyImage(vkinfo.allocator, tex->image, tex->allocation);
}

void cleanupRenderer() {
    if (vkinfo.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(vkinfo.device);
    }

    if (vk_rs.surface.buffers) {
        for (int i = 0; i < xrinfo.renderTarget.swapchainImageCount; i++) {
            if (vk_rs.surface.buffers[i].hb != NULL) {
                vkDestroyImageView(vkinfo.device, vk_rs.surface.buffers[i].view, NULL);
                vkDestroyImage(vkinfo.device, vk_rs.surface.buffers[i].image, NULL);
                vkFreeMemory(vkinfo.device, vk_rs.surface.buffers[i].memory, NULL);
                AHardwareBuffer_release(vk_rs.surface.buffers[i].hb);
            }
        }
        free(vk_rs.surface.buffers);
        vk_rs.surface.buffers = NULL;
    }

    if (vk_rs.surface.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(vkinfo.device, vk_rs.surface.sampler, NULL);
    }
    if (vk_rs.surface.conversion != VK_NULL_HANDLE) {
        PFN_vkDestroySamplerYcbcrConversion pfnDestroySamplerYcbcrConversion =
                (PFN_vkDestroySamplerYcbcrConversion)vkGetDeviceProcAddr(vkinfo.device, "vkDestroySamplerYcbcrConversion");

        pfnDestroySamplerYcbcrConversion(vkinfo.device, vk_rs.surface.conversion, NULL);
    }
    if (vk_rs.surface.reader != NULL) {
        AImageReader_delete(vk_rs.surface.reader);
        vk_rs.surface.window = NULL;
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

    vkDestroyPipeline(vkinfo.device, vk_rs.worldPipeline, NULL);
    vkDestroyPipeline(vkinfo.device, vk_rs.linePipeline, NULL);
    vkDestroyPipeline(vkinfo.device, vk_rs.blitPipeline, NULL);
    vkDestroyPipelineLayout(vkinfo.device, vk_rs.pipelineLayout, NULL);

    if (vk_rs.descriptorPool) {
        vkDestroyDescriptorPool(vkinfo.device, vk_rs.descriptorPool, NULL);
        free(vk_rs.descriptorSets);
    }
    vkDestroyDescriptorSetLayout(vkinfo.device, vk_rs.set0Layout, NULL);
    vkDestroyDescriptorSetLayout(vkinfo.device, vk_rs.set1Layout, NULL);
    vkDestroyDescriptorSetLayout(vkinfo.device, vk_rs.descriptorSetLayout, NULL);

    destroyVkModel(&vk_rs.worldModel);
    destroyVkModel(&vk_rs.targetRectModel);
    destroyVkModel(&vk_rs.leftRay);
    destroyVkModel(&vk_rs.rightRay);

    destroyVkTexture(&vk_rs.atlas);
    destroyVkTexture(&vk_rs.light);

    if (vk_rs.uniformMappedData) {
        vmaUnmapMemory(vkinfo.allocator, vk_rs.uniformAlloc);
    }
    vmaDestroyBuffer(vkinfo.allocator, vk_rs.uniformBuffer, vk_rs.uniformAlloc);

    destroyFramebuffers();
    vkDestroyRenderPass(vkinfo.device, vk_rs.renderPass, NULL);

    LOGI("Renderer cleanup complete.");
}