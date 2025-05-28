#include "EncodePipelineRaw.h"

#include "ALVR-common/packet_types.h"
#include "Renderer.h"


alvr::EncodePipelineRaw::EncodePipelineRaw(Renderer* render) {
    m_renderer = render;
	m_inputSemaphore = m_renderer->GetOutput().semaphore;
	m_width = m_renderer->GetOutput().imageInfo.extent.width;
	m_height = m_renderer->GetOutput().imageInfo.extent.height;
    m_outputSize = m_width * m_height * sizeof(uint8_t) * 3;

    // Command buffer
	VkCommandBufferAllocateInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.commandPool = m_renderer->m_commandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = 1;
	VK_CHECK(vkAllocateCommandBuffers(m_renderer->m_dev, &commandBufferInfo, &m_commandBuffer));

    // Descriptors
    VkDescriptorSetLayoutBinding descriptorBindings[2];
    descriptorBindings[0] = {};
    descriptorBindings[0].binding = 0;
    descriptorBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    descriptorBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorBindings[0].descriptorCount = 1;
    descriptorBindings[1] = {};
    descriptorBindings[1].binding = 1;
    descriptorBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    descriptorBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorBindings[1].descriptorCount = 1;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
	descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
	descriptorSetLayoutInfo.bindingCount = 2;
	descriptorSetLayoutInfo.pBindings = descriptorBindings;
	VK_CHECK(vkCreateDescriptorSetLayout(m_renderer->m_dev, &descriptorSetLayoutInfo, nullptr, &m_descriptorLayout));

	// Shader
	VkShaderModuleCreateInfo moduleInfo = {};
	moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleInfo.codeSize = IMAGE_TO_LINEAR_SHADER_COMP_SPV_LEN;
	moduleInfo.pCode = (uint32_t*) IMAGE_TO_LINEAR_SHADER_COMP_SPV_PTR;
	VK_CHECK(vkCreateShaderModule(m_renderer->m_dev, &moduleInfo, nullptr, &m_shader));

	// Pipeline
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &m_descriptorLayout;
	VK_CHECK(vkCreatePipelineLayout(m_renderer->m_dev, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

	VkPipelineShaderStageCreateInfo stageInfo = {};
	stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.pName = "main";
	stageInfo.module = m_shader;

	VkComputePipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.stage = stageInfo;
	VK_CHECK(vkCreateComputePipelines(m_renderer->m_dev, nullptr, 1, &pipelineInfo, nullptr, &m_pipeline));

    // Image view
	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8_UNORM;
	viewInfo.image = m_renderer->GetOutput().image;
	viewInfo.subresourceRange = {};
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	VK_CHECK(vkCreateImageView(m_renderer->m_dev, &viewInfo, nullptr, &m_inputView));

    // Buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = m_outputSize;
	bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK(vkCreateBuffer(m_renderer->m_dev, &bufferInfo, nullptr, &m_outputBuffer));

	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAllocInfo {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vkGetBufferMemoryRequirements(m_renderer->m_dev, m_outputBuffer, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;

	VkMemoryPropertyFlags memType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
									| VK_MEMORY_PROPERTY_HOST_CACHED_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	memAllocInfo.memoryTypeIndex = m_renderer->memoryTypeIndex(memType, memReqs.memoryTypeBits);
	VK_CHECK(vkAllocateMemory(m_renderer->m_dev, &memAllocInfo, nullptr, &m_outputMemory));
	VK_CHECK(vkBindBufferMemory(m_renderer->m_dev, m_outputBuffer, m_outputMemory, 0));

	VK_CHECK(vkMapMemory(m_renderer->m_dev, m_outputMemory, 0, VK_WHOLE_SIZE, 0, &m_outputData));

    // Output semaphore
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VK_CHECK(vkCreateFence(m_renderer->m_dev, &fenceInfo, nullptr, &m_outputFence));
}

void alvr::EncodePipelineRaw::PushFrame(uint64_t targetTimestampNs, bool idr) {
	m_pts = targetTimestampNs;

	VkCommandBufferBeginInfo commandBufferBegin = {};
	commandBufferBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &commandBufferBegin));

	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);

    VkDescriptorImageInfo descriptorImageInfoIn = {};
    descriptorImageInfoIn.imageView = m_inputView;
    descriptorImageInfoIn.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	VkWriteDescriptorSet descriptorWriteSets[2];
	descriptorWriteSets[0] = {};
	descriptorWriteSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWriteSets[0].dstBinding = 0;
	descriptorWriteSets[0].descriptorCount = 1;
	descriptorWriteSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorWriteSets[0].pImageInfo = &descriptorImageInfoIn;

    VkDescriptorBufferInfo descriptorBufferInfoOut = {};
	descriptorBufferInfoOut.buffer = m_outputBuffer;
	descriptorBufferInfoOut.offset = 0;
	descriptorBufferInfoOut.range = VK_WHOLE_SIZE;

	descriptorWriteSets[1] = {};
	descriptorWriteSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWriteSets[1].dstBinding = 1;
	descriptorWriteSets[1].descriptorCount = 1;
	descriptorWriteSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWriteSets[1].pBufferInfo = &descriptorBufferInfoOut;

	m_renderer->d.vkCmdPushDescriptorSetKHR(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 2, descriptorWriteSets);

    auto ceil_divide = [](uint32_t a, uint32_t b) -> uint32_t {
        return (a + (b - 1)) / b;
    };

	vkCmdDispatch(m_commandBuffer, ceil_divide(m_width, 8), ceil_divide(m_height, 8), 1);

	vkEndCommandBuffer(m_commandBuffer);

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_inputSemaphore;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_commandBuffer;
	VK_CHECK(vkQueueSubmit(m_renderer->m_queue, 1, &submitInfo, m_outputFence));

	// Wait for fence to be signaled
	VK_CHECK(vkWaitForFences(m_renderer->m_dev, 1, &m_outputFence, VK_TRUE, UINT64_MAX));
	VK_CHECK(vkResetFences(m_renderer->m_dev, 1, &m_outputFence));
}

bool alvr::EncodePipelineRaw::GetEncoded(FramePacket& packet) {
	packet.pts = m_pts;
    packet.isIDR = false;
    packet.size = m_outputSize;
    packet.data = (uint8_t*) m_outputData;

    return true;
}

void alvr::EncodePipelineRaw::SetParams(FfiDynamicEncoderParams params) {

}

int alvr::EncodePipelineRaw::GetCodec() {
    return ALVR_CODEC_RAW;
}
