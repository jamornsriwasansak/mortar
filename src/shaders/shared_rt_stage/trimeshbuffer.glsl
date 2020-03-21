#define DECLARE_TRIMESH_LAYOUT(SET) \
	layout(set = SET, binding = 0) buffer Face\
	{\
		uint faces[];\
	} faces_arrays[];\
	layout(set = SET, binding = 1) buffer PuArray\
	{\
		vec4 position_and_us[];\
	} pus_arrays[];\
	layout(set = SET, binding = 2) buffer NvArray\
	{\
		vec4 normal_and_vs[];\
	} nvs_arrays[];\
	layout(set = SET, binding = 3) buffer MaterialId\
	{\
		uint material_ids[];\
	} material_ids_arrays[];\
	layout(set = SET, binding = 4) buffer MaterialBuffer\
	{\
		Material materials[];\
	} mat;\
	layout(set = SET, binding = 5) uniform sampler2D textures[];
