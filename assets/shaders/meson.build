shaders_src = [
	'particles.frag',
	'particles.vert',
	'particles.comp']

shaders = []
glslang = find_program('glslangValidator')

bth_exe = find_program('bintoheader', required: false)
if not bth_exe.found()
	bth_sub = subproject('bintoheader')
	bth_exe = bth_sub.get_variable('bintoheader_executable')
endif

foreach shader : shaders_src
	spv = custom_target(
		shader + '_spv',
		output: shader + '.spv',
		input: shader,
		command: [glslang, '-V', '@INPUT@', '-o', '@OUTPUT@'])

	name = shader.underscorify() + '_data'
	header = custom_target(
		shader + '_header',
		output: shader + '.h',
		depends: spv,
		command: [bth_exe, spv.full_path(), '@OUTPUT@', '-size', '32', '-name', name])

	shaders += [header]
endforeach
