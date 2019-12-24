// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <engine.hpp>
#include <window.hpp>
#include <render.hpp>

#include <ny/backend.hpp> // ny::Backend
#include <ny/appContext.hpp> // ny::AppContext
#include <ny/windowContext.hpp> // ny::WindowContext
#include <ny/windowListener.hpp> // ny::WindowListener
#include <ny/windowSettings.hpp> // ny::WindowSettings

#include <vpp/instance.hpp> // vpp::Instance
#include <vpp/physicalDevice.hpp> // vpp::Instance
#include <vpp/device.hpp> // vpp::Device
#include <vpp/queue.hpp> // vpp::Queue
#include <vpp/swapchain.hpp> // vpp::Swapchain
#include <vpp/renderer.hpp> // vpp::SwapchainRenderer
// #include <vpp/debug.hpp> // vpp::DebugCallback

#include <dlg/dlg.hpp> // dlg

namespace vk {
vk::DynamicDispatch dispatch;
}

#include <chrono>
using Clock = std::chrono::high_resolution_clock;

struct Engine::Impl {
	std::unique_ptr<ny::AppContext> appContext;
	vpp::Instance instance;
	// std::unique_ptr<vpp::DebugCallback> debugCallback;
	std::unique_ptr<ny::WindowContext> windowContext;
	std::unique_ptr<vpp::Device> device;

	MainWindowListener windowListener;
	std::unique_ptr<Renderer> renderer {};
};

Engine::Engine()
{
	// for now hardcoded stuff
	constexpr auto startSize = nytl::Vec2ui{1100, 800};
	constexpr auto useValidation = false; // TODO
	constexpr auto startMsaa = vk::SampleCountBits::e1;
	constexpr auto layerName = "VK_LAYER_LUNARG_standard_validation";

	impl_ = std::make_unique<Impl>();

	// ny backend and appContext
	auto& backend = ny::Backend::choose();
	if(!backend.vulkan()) {
		throw std::runtime_error("Engine: ny backend has no vulkan support!");
	}

	impl_->appContext = backend.createAppContext();

	// vulkan init
	// instance
	auto iniExtensions = impl_->appContext->vulkanExtensions();
	iniExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

	vk::ApplicationInfo appInfo ("msaa-triangle", 1, "msaa-triangle", 1, VK_API_VERSION_1_0);
	vk::InstanceCreateInfo instanceInfo;
	instanceInfo.pApplicationInfo = &appInfo;

	instanceInfo.enabledExtensionCount = iniExtensions.size();
	instanceInfo.ppEnabledExtensionNames = iniExtensions.data();

	if(useValidation) {
		instanceInfo.enabledLayerCount = 1;
		instanceInfo.ppEnabledLayerNames = &layerName;
	}

	try {
		impl_->instance = {instanceInfo};
		if(!impl_->instance.vkInstance())
			throw std::runtime_error("vkCreateInstance returned a nullptr");
	} catch(const std::exception& error) {
		dlg_error("Vulkan instance creation failed: {}", error.what());
		dlg_error("\tThis may indicate that your system that does support vulkan");
		dlg_error("\tThis application requires vulkan to work; rethrowing");
		throw;
	}

	vk::dispatch.init(impl_->instance);

	// debug callback
	if(useValidation) {
		// impl_->debugCallback = std::make_unique<vpp::DebugCallback>(impl_->instance);
	}

	// init ny window
	auto vkSurface = vk::SurfaceKHR {};
	auto ws = ny::WindowSettings {};

	ws.surface = ny::SurfaceType::vulkan;
	ws.listener = &impl_->windowListener;
	ws.size = startSize;
	ws.vulkan.instance = (VkInstance) impl_->instance.vkHandle();
	ws.vulkan.storeSurface = &(std::uintptr_t&) (vkSurface);

	impl_->windowContext = impl_->appContext->createWindowContext(ws);

	uint32_t count = 0;
	auto phdevs = vk::dispatch.vkEnumeratePhysicalDevices((VkInstance) impl_->instance.vkHandle(), &count, NULL);
	dlg_warn("Found {} physical devices", count);
	// for(auto phdev : phdevs) {
	// 	dlg_debug("Found device: {}", vpp::description(phdev, "\n\t"));
	// }

	const vpp::Queue* presentQueue {};
	impl_->device = std::make_unique<vpp::Device>(impl_->instance,
		vkSurface, presentQueue);
	impl_->renderer = std::make_unique<Renderer>(*impl_->device,
		vkSurface, startMsaa, *presentQueue);

	impl_->windowListener.windowContext = impl_->windowContext.get();
	impl_->windowListener.appContext = impl_->appContext.get();
	impl_->windowListener.renderer = impl_->renderer.get();
	impl_->windowListener.presentQueue = presentQueue;
	impl_->windowListener.run = &run_;
	impl_->windowListener.wait = &wait_;
}

Engine::~Engine()
{
}

void Engine::mainLoop()
{
	constexpr auto printFrames = true;

	using secf = std::chrono::duration<float, std::ratio<1, 1>>;

	auto lastFrame = Clock::now();
	auto secCounter = 0.f;
	auto fpsCounter = 0u;

	run_ = true;

	// TODO: to make this work on android an additional idle-check
	// loop is needed. See other android-working implementations using ny and
	// vpp for examples.
	while(run_) {
		if(!impl_->appContext->pollEvents()) {
			dlg_info("pollEvents returned false");
			return;
		}

		// waiting on surface on android
		// will be set/unset by a callback from ny
		while(wait_) {
			if(!impl_->appContext->waitEvents()) {
				dlg_info("waitEvents return false");
				return;
			}
		}

		auto now = Clock::now();
		auto deltaCount = std::chrono::duration_cast<secf>(now - lastFrame).count();
		lastFrame = now;

		// update attraction positions
		renderer().points_.clear();
		auto& win = impl_->windowListener;
		if(win.mousePressed) {
			renderer().points_.push_back(win.mousePos);
		}

		for(auto& p : win.points) {
			renderer().points_.push_back(p.pos);
		}

		renderer().update(deltaCount);
		renderer().renderBlock();

		if(printFrames) {
			++fpsCounter;
			secCounter += deltaCount;
			if(secCounter >= 1.f) {
				dlg_info("{} fps", fpsCounter);
				secCounter = 0.f;
				fpsCounter = 0;
			}
		}
	}

	dlg_info("Exiting main loop with grace");
}

// get functions
ny::AppContext& Engine::appContext() const { return *impl_->appContext; }
ny::WindowContext& Engine::windowContext() const { return *impl_->windowContext; }

vpp::Instance& Engine::vulkanInstance() const { return impl_->instance; }
vpp::Device& Engine::vulkanDevice() const { return *impl_->device; }
Renderer& Engine::renderer() const { return *impl_->renderer; }

int main(int, char**)
{
	Engine engine;
	engine.mainLoop();
}
