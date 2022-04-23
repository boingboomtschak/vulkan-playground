all: triangle pixelsort

triangle:
	glslc triangle.vert -o spv/triangle-vert.spv
	glslc triangle.frag -o spv/triangle-frag.spv

pixelsort:
	glslc pixelsort.vert -o spv/pixelsort-vert.spv
	glslc pixelsort.frag -o spv/pixelsort-frag.spv
	glslc pixelsort.comp -o spv/pixelsort-comp.spv
