#pragma once

#include "EncodePipeline.h"

namespace alvr {

class EncodePipelineRaw : public EncodePipeline {
public:
    EncodePipelineRaw(Renderer* render);

    void PushFrame(uint64_t targetTimestampNs, bool idr) override;
    bool GetEncoded(FramePacket& packet) override;
    void SetParams(FfiDynamicEncoderParams params) override;
    int GetCodec() override;

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;

	uint64_t m_pts = 0;

	Renderer* m_renderer = nullptr;

	VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descriptorLayout = VK_NULL_HANDLE;
    VkShaderModule m_shader = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    VkSemaphore m_inputSemaphore = VK_NULL_HANDLE;
    VkImageView m_inputView = VK_NULL_HANDLE;

    VkBuffer m_outputBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_outputMemory = VK_NULL_HANDLE;
    VkFence m_outputFence = VK_NULL_HANDLE;

    uint64_t m_outputSize = 0;
    void* m_outputData = nullptr;
};

}
