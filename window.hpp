// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include <vpp/fwd.hpp>
#include <ny/windowListener.hpp>
#include <nytl/vec.hpp>

class Renderer;

// ny::WindowListener implementation
class MainWindowListener : public ny::WindowListener {
public:
	// yeah, TODO
	// none of them should be public, some of them should not be here
	ny::AppContext* appContext;
	ny::WindowContext* windowContext;
	const vpp::Queue* presentQueue;
	Renderer* renderer;
	bool* run;
	bool* wait;

public:
	MainWindowListener() = default;
	~MainWindowListener() = default;

	void mouseButton(const ny::MouseButtonEvent&) override;
	void mouseWheel(const ny::MouseWheelEvent&) override;
	void mouseMove(const ny::MouseMoveEvent&) override;
	void mouseCross(const ny::MouseCrossEvent&) override;
	void key(const ny::KeyEvent&) override;
	void state(const ny::StateEvent&) override;
	void close(const ny::CloseEvent&) override;
	void resize(const ny::SizeEvent&) override;
	void surfaceCreated(const ny::SurfaceCreatedEvent&) override;
	void surfaceDestroyed(const ny::SurfaceDestroyedEvent&) override;

protected:
	ny::AppContext& ac() const;
	ny::WindowContext& wc() const;

protected:
	nytl::Vec2ui size_;
	ny::ToplevelState toplevelState_;
};
