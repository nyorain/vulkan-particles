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
source_root = meson.source_root().split('\\')
add_project_arguments('-DDLG_BASE_PATH="' + '/'.join(source_root) + '/"', language: 'cpp')
add_project_arguments('-DVKPP_DYNAMIC_DISPATCH=1', language: ['c', 'cpp'])

dep_vpp = dependency('vpp', fallback: ['vpp', 'vpp_dep'])
dep_ny = dependency('ny', fallback: ['ny', 'ny_dep'])
dep_vulkan = dependency('vulkan')

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
