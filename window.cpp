// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#include <window.hpp>
#include <render.hpp>

#include <dlg/dlg.hpp> // dlg
#include <ny/key.hpp> // ny::Keycode
#include <ny/mouseButton.hpp> // ny::MouseButton
#include <ny/image.hpp> // ny::Image
#include <ny/event.hpp> // ny::*Event
#include <ny/keyboardContext.hpp> // ny::KeyboardContext
#include <ny/appContext.hpp> // ny::AppContext
#include <ny/windowContext.hpp> // ny::WindowContext
#include <ny/windowSettings.hpp> // ny::WindowEdge
#include <nytl/vecOps.hpp> // operator<<

void MainWindowListener::mouseButton(const ny::MouseButtonEvent& ev)
{
	mousePos = static_cast<nytl::Vec2f>(ev.position);
	if(ev.button == ny::MouseButton::left) {
		mousePressed = ev.pressed;
		if(ev.pressed) {
			auto mods = ac().keyboardContext()->modifiers();
			bool alt = mods & ny::KeyboardModifier::alt;

			if(wc().customDecorated() || alt) {
				if(ev.position[0] < 0 || ev.position[1] < 0 ||
					static_cast<unsigned int>(ev.position[0]) > size_[0] ||
					static_cast<unsigned int>(ev.position[1]) > size_[1])
						return;

				ny::WindowEdges resizeEdges = ny::WindowEdge::none;
				if(ev.position[0] < 100)
					resizeEdges |= ny::WindowEdge::left;
				else if(static_cast<unsigned int>(ev.position[0]) > size_[0] - 100)
					resizeEdges |= ny::WindowEdge::right;

				if(ev.position[1] < 100)
					resizeEdges |= ny::WindowEdge::top;
				else if(static_cast<unsigned int>(ev.position[1]) > size_[1] - 100)
					resizeEdges |= ny::WindowEdge::bottom;

				if(resizeEdges != ny::WindowEdge::none) {
					dlg_info("Starting to resize window");
					wc().beginResize(ev.eventData, resizeEdges);
				} else {
					dlg_info("Starting to move window");
					wc().beginMove(ev.eventData);
				}

				return;
			}
		}
	}
}
void MainWindowListener::key(const ny::KeyEvent& keyEvent)
{
	auto keycode = keyEvent.keycode;
	auto mods = ac().keyboardContext()->modifiers();
	if(keyEvent.pressed && (mods & ny::KeyboardModifier::shift)) {
		if(keycode == ny::Keycode::f) {
			dlg_info("f pressed. Toggling fullscreen");
			if(toplevelState_ != ny::ToplevelState::fullscreen) {
				wc().fullscreen();
				toplevelState_ = ny::ToplevelState::fullscreen;
			} else {
				wc().normalState();
				toplevelState_ = ny::ToplevelState::normal;
			}
		} else if(keycode == ny::Keycode::n) {
			dlg_info("n pressed. Resetting window to normal state");
			wc().normalState();
		} else if(keycode == ny::Keycode::escape) {
			dlg_info("escape pressed. Closing window and exiting");
			*run = false;
		} else if(keycode == ny::Keycode::m) {
			dlg_info("m pressed. Toggle window maximize");
			if(toplevelState_ != ny::ToplevelState::maximized) {
				wc().maximize();
				toplevelState_ = ny::ToplevelState::maximized;
			} else {
				wc().normalState();
				toplevelState_ = ny::ToplevelState::normal;
			}
		} else if(keycode == ny::Keycode::i) {
			dlg_info("i pressed, Minimizing window");
			toplevelState_ = ny::ToplevelState::minimized;
			wc().minimize();
		} else if(keycode == ny::Keycode::d) {
			dlg_info("d pressed. Trying to toggle decorations");
			wc().customDecorated(!wc().customDecorated());
		}
	} else if(keyEvent.pressed) {
		if(keycode == ny::Keycode::k1) {
			dlg_info("Using no multisampling");
			renderer->samples(vk::SampleCountBits::e1);
		} else if(keycode == ny::Keycode::k2) {
			dlg_info("Using 2 multisamples");
			renderer->samples(vk::SampleCountBits::e2);
		} else if(keycode == ny::Keycode::k4) {
			dlg_info("Using 4 multisamples");
			renderer->samples(vk::SampleCountBits::e4);
		} else if(keycode == ny::Keycode::k8) {
			dlg_info("Using 8 multisamples");
			renderer->samples(vk::SampleCountBits::e8);
		}
	}
}
void MainWindowListener::mouseMove(const ny::MouseMoveEvent& ev)
{
	mousePos = static_cast<nytl::Vec2f>(ev.position);
}
void MainWindowListener::mouseWheel(const ny::MouseWheelEvent& ev)
{
	// auto fac = std::pow(1.2, ev.value);
	// attractFactor *= fac;
	// renderer->mouseAttract *= fac;
}
void MainWindowListener::mouseCross(const ny::MouseCrossEvent& ev)
{
	// causes attraction to end when the mouse leaves the window
	// it will resume with the same attraction when in enters the window again
	// releasing the button (even while outside the window) will reset mouseAttractLeft
	// usually
	/*
	if(ev.entered) {
		renderer->mouseAttract = mouseAttractLeft;
		mouseAttractLeft = 0.f;
	} else {
		mouseAttractLeft = renderer->mouseAttract;
		renderer->mouseAttract = 0.f;
	}
	*/
}
void MainWindowListener::state(const ny::StateEvent& stateEvent)
{
	if(stateEvent.state != toplevelState_) {
		toplevelState_ = stateEvent.state;
	}
}
void MainWindowListener::close(const ny::CloseEvent&)
{
	*run = false;
}
void MainWindowListener::resize(const ny::SizeEvent& ev)
{
	dlg_info("resize: {}", ev.size);
	size_ = ev.size;
	renderer->resize(ev.size);
}

// TODO: completetly recreating renderer is an overkill...
void MainWindowListener::surfaceCreated(const ny::SurfaceCreatedEvent& ev)
{
	dlg_info("Surface created!");
	auto vkSurface = (vk::SurfaceKHR) ev.surface.vulkan;
	renderer->surfaceCreated(vkSurface);
	*wait = false;
}
void MainWindowListener::surfaceDestroyed(const ny::SurfaceDestroyedEvent&)
{
	dlg_info("Surface destroyed!");
	renderer->wait();
	renderer->surfaceDestroyed();
	*wait = true;
}

void MainWindowListener::touchBegin(const ny::TouchBeginEvent& ev) {
	auto it = std::find_if(points.begin(), points.end(),
		[&](auto& point) { return point.id == ev.id; });
	if(it != points.end()) {
		dlg_warn("Reused touch id (update) {}", ev.id);
		it->pos = ev.pos;
		return;
	}

	points.push_back({ev.id, ev.pos});
}

void MainWindowListener::touchUpdate(const ny::TouchUpdateEvent& ev) {
	auto it = std::find_if(points.begin(), points.end(),
		[&](auto& point) { return point.id == ev.id; });
	if(it == points.end()) {
		dlg_warn("Invalid touch id (update) {}", ev.id);
		return;
	}

	it->pos = ev.pos;
}

void MainWindowListener::touchEnd(const ny::TouchEndEvent& ev) {
	auto it = std::find_if(points.begin(), points.end(),
		[&](auto& point) { return point.id == ev.id; });
	if(it == points.end()) {
		dlg_warn("Invalid touch id (end) {}", ev.id);
		return;
	}

	points.erase(it);
}

void MainWindowListener::touchCancel(const ny::TouchCancelEvent&) {
	points.clear();
}

ny::AppContext& MainWindowListener::ac() const { return *appContext; }
ny::WindowContext& MainWindowListener::wc() const { return *windowContext; }
