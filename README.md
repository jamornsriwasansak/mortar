# vulkan-mortar

A physically-based renderer utilizing vulkan raytracing api.

![The Toilet Scene](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/toilet.jpg)
![Fireplace Room](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/fireplace.jpg)

### Features
In its current early stage, it supports:
* IBL environment map
* Alpha-testing
* Ambient occlusion Integrator
* Path tracer with next event estimation
* Low discreprancy sampler with blue-noise property from [Heitz et al. 2019](https://eheitzresearch.wordpress.com/762-2/)
* PCG random number generator ported from [pcg-random.org](https://www.pcg-random.org/)
* Lambert diffuse BRDF
* GGX Microfacet BSDF (rough conductor and rough dielectric)

### Dependencies
* vulkan sdk
* glslang (included as a submodule)
* tinyobjloader (included as a submodule)
* glfw (included as a submodule)
* stb (included as a submodule)
* glm (included as a submodule)

### Issue
1. Rough dielectric was not tested properly. It might not produce correct result if such material is used.
2. There will be crash when exit due to unhandled GLFW surface deleter.
3. 

<sup>I started this project around March, 1st since I was curious of the vulkan raytracing performance. On the first day of this project, I spent an entire day building a new PC to house RTX2060. This is probably the most costly project I ever invested with my own money. :(
