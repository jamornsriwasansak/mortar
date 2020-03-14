# vulkan-mortar

A physically-based renderer utilizing vulkan raytracing api.

![The Toilet Scene](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/toilet.png)

### Features
In its current early stage, it supports:
* Diffuse material
* IBL environment map
* Ambient occlusion Integrator
* Primitive path tracer
* Low discreprancy sampler with blue-noise property [Heitz et al. 2019](https://eheitzresearch.wordpress.com/762-2/)

I plan to add microfacet and MIS probably within a few days.

### Dependencies
* vulkan
* glslang (included as a submodule)
* tinyobjloader (included as a submodule)
* glfw (included as a submodule)
* stb (included as a submodule)
* glm (included as a submodule)

### Note
I started this project around March, 1st since I was curious of the vulkan raytracing performance. On the first day of this project, I spent the entire day building a new PC to house RTX2060. This is probably the most costly project I ever invested. :(
