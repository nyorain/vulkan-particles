project('vulkan-particles', 'cpp',
  license: 'BSL',
	version: '0.1.0',
	meson_version: '>=0.42',
  default_options: ['cpp_std=c++1z'])

android = get_option('android')

# default arrguments
# warnings and stuff
add_project_arguments(
	'-Wall',
	'-Wextra',
	'-Wno-unused-parameter',
	'-Wpedantic',
	'-Wno-missing-braces',
	language: 'cpp')

# project-specific stuff
add_project_arguments(
	'-DDLG_BASE_PATH="@0@"'.format(meson.source_root()),
	language: 'cpp')

dep_vpp = dependency('vpp', fallback: ['vpp', 'vpp_dep'])
dep_ny = dependency('ny', fallback: ['ny', 'ny_dep'])
dep_vulkan = meson.get_compiler('cpp').find_library('vulkan')

subdir('assets/shaders')
shader_inc = include_directories('assets') # for headers in build folder

src = [
	shaders,
	'engine.cpp',
	'render.cpp',
	'window.cpp']

if android
	shared_module('particles', src,
		dependencies: [dep_vpp, dep_vulkan, dep_ny],
		include_directories: shader_inc)
else
	executable('particles', src,
		dependencies: [dep_vpp, dep_vulkan, dep_ny],
		include_directories: shader_inc)
endif