shaders_src = [
	'particles.frag',
	'particles.vert',
	'particles.comp']

shaders = []
glslang = find_program('glslangValidator')

foreach shader : shaders_src
	name = shader.underscorify() + '_data'
	header = custom_target(
		shader + '_spv',
		output: shader + '.h',
		input: shader,
		command: [glslang, '-V', '@INPUT@', '-o', '@OUTPUT@', '--vn', name])

	shaders += [header]
endforeach
