// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <render.hpp>
#include <engine.hpp>

#include <nytl/mat.hpp>
#include <vpp/vk.hpp>
#include <vpp/util/file.hpp>
#include <vpp/renderPass.hpp>
#include <vpp/swapchain.hpp>
#include <vpp/bufferOps.hpp>

#include <dlg/dlg.hpp> // dlg
#include <random>

// shader data
#include <shaders/particles.frag.h>
#include <shaders/particles.vert.h>
#include <shaders/particles.comp.h>

vpp::Pipeline createGraphicsPipeline(const vpp::Device&, vk::RenderPass, 
	vk::PipelineLayout, vk::SampleCountBits);
vpp::Pipeline createComputePipeline(const vpp::Device& device,
	vk::PipelineLayout layout);
vpp::RenderPass createRenderPass(const vpp::Device&, vk::Format, 
	vk::SampleCountBits);

struct Particle {
	nytl::Vec2f pos;
	nytl::Vec2f vel;
};

constexpr auto neededUniformSize = 4 * sizeof(float);
constexpr auto memoryType = 1; // -1 to just choose a suited one

Renderer::Renderer(const vpp::Device& dev, vk::SurfaceKHR surface, 
	vk::SampleCountBits samples, const vpp::Queue& present)
{
	// FIXME: size
	sampleCount_ = samples;
	scInfo_ = vpp::swapchainCreateInfo(dev, surface, {800u, 500u});

	renderPass_ = createRenderPass(dev, scInfo_.imageFormat, samples);

	// descriptor
	vk::DescriptorPoolSize typeCounts[2] {};
	typeCounts[0].type = vk::DescriptorType::storageBuffer;
	typeCounts[0].descriptorCount = 1;

	typeCounts[1].type = vk::DescriptorType::uniformBuffer;
	typeCounts[1].descriptorCount = 1;

	vk::DescriptorPoolCreateInfo descriptorPoolInfo;
	descriptorPoolInfo.poolSizeCount = (pushConstants_) ? 1 : 2;
	descriptorPoolInfo.pPoolSizes = typeCounts;
	descriptorPoolInfo.maxSets = 1;

	descriptorPool_ = {dev, descriptorPoolInfo};

	uboData_.resize(neededUniformSize / sizeof(float));

	gfxPipelineLayout_ = {dev, {}, {}};
	gfxPipeline_ = createGraphicsPipeline(dev, renderPass_, 
		gfxPipelineLayout_, sampleCount_);

	auto compBindings = {
		vpp::descriptorBinding(
			vk::DescriptorType::storageBuffer, 
			vk::ShaderStageBits::compute, 0),
		vpp::descriptorBinding(
			vk::DescriptorType::uniformBuffer, 
			vk::ShaderStageBits::compute, 1)
	};

	std::size_t count = (pushConstants_) ? 1 : 2;
	compDescriptorLayout_ = {dev, {compBindings.begin(), count}};
	compDescriptor_ = {compDescriptorLayout_, descriptorPool_};

	vk::PipelineLayoutCreateInfo computeLayoutInfo;
	computeLayoutInfo.setLayoutCount = 1;
	computeLayoutInfo.pSetLayouts = &compDescriptorLayout_.vkHandle();

	vk::PushConstantRange localRange;
	if(pushConstants_) {
		localRange.stageFlags = vk::ShaderStageBits::compute;
		localRange.size = neededUniformSize;
		localRange.offset = 0;

		computeLayoutInfo.pushConstantRangeCount = 1;
		computeLayoutInfo.pPushConstantRanges = &localRange;
	}

	compPipelineLayout_ = {dev, computeLayoutInfo};
	compPipeline_ = createComputePipeline(dev, compPipelineLayout_);

	// buffer
	vk::BufferCreateInfo bufInfo;
	bufInfo.usage = vk::BufferUsageBits::vertexBuffer
		| vk::BufferUsageBits::storageBuffer
		| vk::BufferUsageBits::transferDst;
	bufInfo.size = sizeof(Particle) * particleCount_;
	auto mem = memoryType;
	auto bits = dev.memoryTypeBits(vk::MemoryPropertyBits::deviceLocal);
	if(memoryType == -1 || !(bits & (1 << memoryType))) {
		mem = bits;
	}

	particleBuffer_ = {dev, bufInfo, static_cast<unsigned int>(mem)};
	particleBuffer_.ensureMemory();

	if(!pushConstants_) {
		bufInfo.usage = vk::BufferUsageBits::uniformBuffer;
		bufInfo.size = neededUniformSize;
		auto mem = dev.memoryTypeBits(vk::MemoryPropertyBits::hostVisible);

		compUbo_ = {dev, bufInfo, mem};
		compUbo_.ensureMemory();
	}

	// create & upload particles
	{
		constexpr auto distrFrom = -0.85f;
		constexpr auto distrTo = 0.85f;

		std::mt19937 rgen;
		rgen.seed(std::time(nullptr));
		std::uniform_real_distribution<float> distr(distrFrom, distrTo);

		std::vector<Particle> particles;
		particles.resize(particleCount_);
		for(auto i = 0u; i < particleCount_; ++i) {
			particles[i].pos[0] = distr(rgen);
			particles[i].pos[1] = distr(rgen);
			particles[i].vel = {0.f, 0.f};
		}

		vpp::writeStaging430(particleBuffer_, vpp::raw(particles));
	}

	// write descriptor
	{
		vpp::DescriptorSetUpdate update(compDescriptor_);
		update.storage({{particleBuffer_, 0, vk::wholeSize}});
		if(!pushConstants_) {
			update.uniform({{compUbo_, 0, vk::wholeSize}});
		}
	}

	// init renderer
	auto mode = pushConstants_ ? RecordMode::always : RecordMode::all;
	vpp::DefaultRenderer::init(renderPass_, scInfo_, present, {}, mode);
}

void Renderer::update(double delta) 
{
	auto width = scInfo_.imageExtent.width;
	auto height = scInfo_.imageExtent.height;

	uboData_[0] = delta;
	uboData_[1] = attractFac_;
	uboData_[2] = 2 * (attractPos_[0] / width) - 1;
	uboData_[3] = 2 * (attractPos_[1] / height) - 1;

	if(!pushConstants_) {
		auto view = compUbo_.memoryMap();
		std::memcpy(view.ptr(), uboData_.data(), neededUniformSize);
	}
}

void Renderer::createMultisampleTarget(const vk::Extent2D& size)
{
	auto width = size.width;
	auto height = size.height;

	// img
	vk::ImageCreateInfo img;
	img.imageType = vk::ImageType::e2d;
	img.format = scInfo_.imageFormat;
	img.extent.width = width;
	img.extent.height = height;
	img.extent.depth = 1;
	img.mipLevels = 1;
	img.arrayLayers = 1;
	img.sharingMode = vk::SharingMode::exclusive;
	img.tiling = vk::ImageTiling::optimal;
	img.samples = sampleCount_;
	img.usage = vk::ImageUsageBits::transientAttachment | vk::ImageUsageBits::colorAttachment;
	img.initialLayout = vk::ImageLayout::undefined;

	// view
	vk::ImageViewCreateInfo view;
	view.viewType = vk::ImageViewType::e2d;
	view.format = img.format;
	view.components.r = vk::ComponentSwizzle::r;
	view.components.g = vk::ComponentSwizzle::g;
	view.components.b = vk::ComponentSwizzle::b;
	view.components.a = vk::ComponentSwizzle::a;
	view.subresourceRange.aspectMask = vk::ImageAspectBits::color;
	view.subresourceRange.levelCount = 1;
	view.subresourceRange.layerCount = 1;

	// create the viewable image
	// will set the created image in the view info for us
	multisampleTarget_ = {device(), img, view};
}

void Renderer::record(const RenderBuffer& buf)
{
	static const auto clearValue = vk::ClearValue {{0.f, 0.f, 0.f, 1.f}};
	const auto width = scInfo_.imageExtent.width;
	const auto height = scInfo_.imageExtent.height;

	auto cmdBuf = buf.commandBuffer;
	vk::beginCommandBuffer(cmdBuf, {});

	// compute
	if(pushConstants_) {
		auto data = uboData_.data();
		vk::cmdPushConstants(cmdBuf, compPipelineLayout_, 
			vk::ShaderStageBits::compute, 0,
			neededUniformSize, data);
	}

	// compute
	vk::cmdBindPipeline(cmdBuf, vk::PipelineBindPoint::compute, compPipeline_);
	vk::cmdBindDescriptorSets(cmdBuf, vk::PipelineBindPoint::compute,
		compPipelineLayout_, 0, {compDescriptor_}, {});
	vk::cmdDispatch(cmdBuf, particleCount_ / 16, 1, 1);

	// TODO: barrier or stuff

	// render pass
	vk::cmdBeginRenderPass(cmdBuf, {
		renderPass(),
		buf.framebuffer,
		{0u, 0u, width, height},
		1,
		&clearValue
	}, {});

	vk::Viewport vp {0.f, 0.f, (float) width, (float) height, 0.f, 1.f};
	vk::cmdSetViewport(cmdBuf, 0, 1, vp);
	vk::cmdSetScissor(cmdBuf, 0, 1, {0, 0, width, height});

	vk::cmdBindPipeline(cmdBuf, vk::PipelineBindPoint::graphics, gfxPipeline_);
	vk::cmdBindVertexBuffers(cmdBuf, 0, {particleBuffer_}, {0});
	vk::cmdDraw(cmdBuf, particleCount_, 1, 0, 0);

	vk::cmdEndRenderPass(cmdBuf);
	vk::endCommandBuffer(cmdBuf);
}

void Renderer::resize(nytl::Vec2ui size)
{
	vpp::DefaultRenderer::resize({size[0], size[1]}, scInfo_);
}

void Renderer::samples(vk::SampleCountBits samples)
{
	sampleCount_ = samples;
	if(sampleCount_ != vk::SampleCountBits::e1) {
		createMultisampleTarget(scInfo_.imageExtent);
	}

	renderPass_ = createRenderPass(device(), scInfo_.imageFormat, samples);
	vpp::DefaultRenderer::renderPass_ = renderPass_;
	gfxPipeline_ = createGraphicsPipeline(device(), renderPass_, 
		gfxPipelineLayout_, sampleCount_);

	initBuffers(scInfo_.imageExtent, renderBuffers_);
	invalidate();
}

void Renderer::initBuffers(const vk::Extent2D& size, 
	nytl::Span<RenderBuffer> bufs)
{
	if(sampleCount_ != vk::SampleCountBits::e1) {
		createMultisampleTarget(scInfo_.imageExtent);
		vpp::DefaultRenderer::initBuffers(size, bufs, 
			{multisampleTarget_.vkImageView()});
	} else {
		vpp::DefaultRenderer::initBuffers(size, bufs, {});
	}
}

void Renderer::surfaceDestroyed()
{
	wait();
	swapchain_ = {};
}

void Renderer::surfaceCreated(vk::SurfaceKHR surface)
{
	// TODO: size
	// TODO: we can not really assume that format and everything stays
	// the same and should probably recreate pipeline and renderPass
	scInfo_ = vpp::swapchainCreateInfo(present_->device(), 
		surface, scInfo_.imageExtent);
	swapchain_ = {present_->device(), scInfo_};
	createBuffers(scInfo_.imageExtent, scInfo_.imageFormat);
	invalidate();
}

// utility
vpp::Pipeline createGraphicsPipeline(const vpp::Device& device,
	vk::RenderPass renderPass, vk::PipelineLayout layout, vk::SampleCountBits sampleCount)
{
	// auto msaa = sampleCount != vk::SampleCountBits::e1;
	auto vertex = vpp::ShaderModule(device, particles_vert_data);
	auto fragment = vpp::ShaderModule(device, particles_frag_data);

	vpp::ShaderProgram stages({
		{vertex, vk::ShaderStageBits::vertex},
		{fragment, vk::ShaderStageBits::fragment}
	});

	vk::GraphicsPipelineCreateInfo pipeInfo;

	pipeInfo.renderPass = renderPass;
	pipeInfo.layout = layout;

	pipeInfo.stageCount = stages.vkStageInfos().size();
	pipeInfo.pStages = stages.vkStageInfos().data();

	constexpr auto stride = sizeof(float) * 4; // vec2 pos, velocity
	vk::VertexInputBindingDescription bufferBinding {0, stride, vk::VertexInputRate::vertex};

	// vertex position attribute
	vk::VertexInputAttributeDescription attributes[2];
	attributes[0].format = vk::Format::r32g32Sfloat;

	attributes[1].format = vk::Format::r32g32Sfloat;
	attributes[1].location = 1;
	attributes[1].offset = sizeof(float) * 2;

	vk::PipelineVertexInputStateCreateInfo vertexInfo;
	vertexInfo.vertexBindingDescriptionCount = 1;
	vertexInfo.pVertexBindingDescriptions = &bufferBinding;
	vertexInfo.vertexAttributeDescriptionCount = 2;
	vertexInfo.pVertexAttributeDescriptions = attributes;
	pipeInfo.pVertexInputState = &vertexInfo;

	vk::PipelineInputAssemblyStateCreateInfo assemblyInfo;
	assemblyInfo.topology = vk::PrimitiveTopology::pointList;
	pipeInfo.pInputAssemblyState = &assemblyInfo;

	vk::PipelineRasterizationStateCreateInfo rasterizationInfo;
	rasterizationInfo.polygonMode = vk::PolygonMode::fill;
	rasterizationInfo.cullMode = vk::CullModeBits::none;
	rasterizationInfo.frontFace = vk::FrontFace::counterClockwise;
	rasterizationInfo.depthClampEnable = false;
	rasterizationInfo.rasterizerDiscardEnable = false;
	rasterizationInfo.depthBiasEnable = false;
	rasterizationInfo.lineWidth = 1.f;
	pipeInfo.pRasterizationState = &rasterizationInfo;

	vk::PipelineMultisampleStateCreateInfo multisampleInfo;
	multisampleInfo.rasterizationSamples = sampleCount;
	multisampleInfo.sampleShadingEnable = false;
	multisampleInfo.alphaToCoverageEnable = false;
	pipeInfo.pMultisampleState = &multisampleInfo;

	vk::PipelineColorBlendAttachmentState blendAttachment;
	blendAttachment.blendEnable = true;
	blendAttachment.alphaBlendOp = vk::BlendOp::add;
	blendAttachment.srcColorBlendFactor = vk::BlendFactor::srcAlpha;
	blendAttachment.dstColorBlendFactor = vk::BlendFactor::oneMinusSrcAlpha;
	blendAttachment.srcAlphaBlendFactor = vk::BlendFactor::zero;
	blendAttachment.dstAlphaBlendFactor = vk::BlendFactor::one;
	blendAttachment.colorWriteMask =
		vk::ColorComponentBits::r |
		vk::ColorComponentBits::g |
		vk::ColorComponentBits::b |
		vk::ColorComponentBits::a;

	vk::PipelineColorBlendStateCreateInfo blendInfo;
	blendInfo.attachmentCount = 1;
	blendInfo.pAttachments = &blendAttachment;
	pipeInfo.pColorBlendState = &blendInfo;

	vk::PipelineViewportStateCreateInfo viewportInfo;
	viewportInfo.scissorCount = 1;
	viewportInfo.viewportCount = 1;
	pipeInfo.pViewportState = &viewportInfo;

	const auto dynStates = {vk::DynamicState::viewport, vk::DynamicState::scissor};

	vk::PipelineDynamicStateCreateInfo dynamicInfo;
	dynamicInfo.dynamicStateCount = dynStates.size();
	dynamicInfo.pDynamicStates = dynStates.begin();
	pipeInfo.pDynamicState = &dynamicInfo;

	// setup cache
	constexpr auto cacheName = "graphicsCache.bin";
	vpp::PipelineCache cache {device, cacheName};

	vk::Pipeline ret;
	vk::createGraphicsPipelines(device, cache, 1, pipeInfo, nullptr, ret);

	try {
		vpp::save(cache, cacheName);
	} catch(const std::exception& err) {
		dlg_warn("vpp::save(PipelineCache): {}", err.what());
	}

	return {device, ret};
}

vpp::Pipeline createComputePipeline(const vpp::Device& device,
	vk::PipelineLayout layout)
{
	auto computeShader = vpp::ShaderModule(device, particles_comp_data);

	vk::ComputePipelineCreateInfo info;
	info.layout = layout;
	info.stage.module = computeShader;
	info.stage.pName = "main";
	info.stage.stage = vk::ShaderStageBits::compute;

	constexpr auto cacheName = "computeCache.bin";

	vpp::PipelineCache cache {device, cacheName};

	vk::Pipeline vkPipeline;
	vk::createComputePipelines(device, cache, 1, info, nullptr, vkPipeline);

	try {
		vpp::save(cache, cacheName);
	} catch(const std::exception& err) {
		dlg_warn("vpp::save(PipelineCache): {}", err.what());
	}

	return {device, vkPipeline};
}

vpp::RenderPass createRenderPass(const vpp::Device& dev,
	vk::Format format, vk::SampleCountBits sampleCount)
{
	vk::AttachmentDescription attachments[2] {};
	auto msaa = sampleCount != vk::SampleCountBits::e1;

	auto swapchainID = 0u;
	if(msaa) {
		// multisample color attachment
		attachments[0].format = format;
		attachments[0].samples = sampleCount;
		attachments[0].loadOp = vk::AttachmentLoadOp::clear;
		attachments[0].storeOp = vk::AttachmentStoreOp::dontCare;
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::dontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::dontCare;
		attachments[0].initialLayout = vk::ImageLayout::undefined;
		attachments[0].finalLayout = vk::ImageLayout::presentSrcKHR;

		swapchainID = 1u;
	}

	// swapchain color attachments we want to resolve to
	attachments[swapchainID].format = format;
	attachments[swapchainID].samples = vk::SampleCountBits::e1;
	if(msaa) attachments[swapchainID].loadOp = vk::AttachmentLoadOp::dontCare;
	else attachments[swapchainID].loadOp = vk::AttachmentLoadOp::clear;
	attachments[swapchainID].storeOp = vk::AttachmentStoreOp::store;
	attachments[swapchainID].stencilLoadOp = vk::AttachmentLoadOp::dontCare;
	attachments[swapchainID].stencilStoreOp = vk::AttachmentStoreOp::dontCare;
	attachments[swapchainID].initialLayout = vk::ImageLayout::undefined;
	attachments[swapchainID].finalLayout = vk::ImageLayout::presentSrcKHR;

	// refs
	vk::AttachmentReference colorReference;
	colorReference.attachment = 0;
	colorReference.layout = vk::ImageLayout::colorAttachmentOptimal;

	vk::AttachmentReference resolveReference;
	resolveReference.attachment = 1;
	resolveReference.layout = vk::ImageLayout::colorAttachmentOptimal;

	// deps
	std::array<vk::SubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = vk::subpassExternal;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = vk::PipelineStageBits::bottomOfPipe;
	dependencies[0].dstStageMask = vk::PipelineStageBits::colorAttachmentOutput;
	dependencies[0].srcAccessMask = vk::AccessBits::memoryRead;
	dependencies[0].dstAccessMask = vk::AccessBits::colorAttachmentRead |
		vk::AccessBits::colorAttachmentWrite;
	dependencies[0].dependencyFlags = vk::DependencyBits::byRegion;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = vk::subpassExternal;
	dependencies[1].srcStageMask = vk::PipelineStageBits::colorAttachmentOutput;;
	dependencies[1].dstStageMask = vk::PipelineStageBits::bottomOfPipe;
	dependencies[1].srcAccessMask = vk::AccessBits::colorAttachmentRead |
		vk::AccessBits::colorAttachmentWrite;
	dependencies[1].dstAccessMask = vk::AccessBits::memoryRead;
	dependencies[1].dependencyFlags = vk::DependencyBits::byRegion;

	// only subpass
	vk::SubpassDescription subpass;
	subpass.pipelineBindPoint = vk::PipelineBindPoint::graphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	if(sampleCount != vk::SampleCountBits::e1)
		subpass.pResolveAttachments = &resolveReference;

	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = 1 + msaa;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	if(msaa) {
		renderPassInfo.dependencyCount = dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();
	}

	return {dev, renderPassInfo};
}
