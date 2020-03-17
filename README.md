# vulkan-mortar

A physically-based renderer utilizing vulkan raytracing api.

![The Toilet Scene](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/toilet2.png)

### Features
In its current early stage, it supports:
* Diffuse material
* IBL environment map
* Alpha-testing
* Ambient occlusion Integrator
* Primitive path tracer
* Low discreprancy sampler with blue-noise property from [Heitz et al. 2019](https://eheitzresearch.wordpress.com/762-2/)
* GGX Microfacet BSDF

I plan to add microfacet and MIS probably within a few days.

### Dependencies
* vulkan
* glslang (included as a submodule)
* tinyobjloader (included as a submodule)
* glfw (included as a submodule)
* stb (included as a submodule)
* glm (included as a submodule)

### Note
I started this project around March, 1st since I was curious of the vulkan raytracing performance. On the first day of this project, I spent an entire day building a new PC to house RTX2060. This is probably the most costly project I ever invested with my own money. :(
