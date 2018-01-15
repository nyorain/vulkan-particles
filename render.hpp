// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <vpp/device.hpp> // vpp::Device
#include <vpp/queue.hpp> // vpp::Queue
#include <vpp/buffer.hpp> // vpp::Buffer
#include <vpp/pipeline.hpp> // vpp::Pipeline
#include <vpp/renderer.hpp> // vpp::DefaultRenderer
#include <vpp/descriptor.hpp> // vpp::DescriptorSet
#include <vpp/commandBuffer.hpp>
#include <vpp/renderPass.hpp>
#include <vpp/framebuffer.hpp>
#include <vpp/sync.hpp>
#include <vpp/queue.hpp>
#include <vpp/vk.hpp> // FIXME
#include <nytl/vec.hpp>

class Engine;

class Renderer : public vpp::DefaultRenderer {
public:
	nytl::Vec2f attractPos_ {};
	float attractFac_ {0.f};

public:
	Renderer() = default;
	Renderer(const vpp::Device&, vk::SurfaceKHR, vk::SampleCountBits samples,
		const vpp::Queue& present);
	~Renderer() = default;

	Renderer(Renderer&&) noexcept = default;
	Renderer& operator=(Renderer&&) noexcept = default;

	void update(double delta);
	void resize(nytl::Vec2ui size);
	void samples(vk::SampleCountBits);

	void surfaceDestroyed();
	void surfaceCreated(vk::SurfaceKHR surface);

protected:
	void createMultisampleTarget(const vk::Extent2D& size);
	void record(const RenderBuffer&) override;
	void initBuffers(const vk::Extent2D&, nytl::Span<RenderBuffer>) override;

protected:
	vpp::Pipeline gfxPipeline_;
	vpp::PipelineLayout gfxPipelineLayout_;

	vpp::Pipeline compPipeline_;
	vpp::PipelineLayout compPipelineLayout_;

	vpp::ViewableImage multisampleTarget_;
	vpp::RenderPass renderPass_;
	vk::SampleCountBits sampleCount_;
	vk::SwapchainCreateInfoKHR scInfo_;

	bool pushConstants_ {false};
	// unsigned int particleCount_ {350000}; // android
	// unsigned int particleCount_ {3000000}; // main pc
	unsigned int particleCount_ {750000}; 
	vpp::Buffer particleBuffer_;
	vpp::Buffer compUbo_;
	vpp::DescriptorPool descriptorPool_;
	vpp::DescriptorSetLayout gfxDescriptorLayout_;
	vpp::DescriptorSetLayout compDescriptorLayout_;
	vpp::DescriptorSet gfxDescriptor_;
	vpp::DescriptorSet compDescriptor_;

	std::vector<float> uboData_;
};
