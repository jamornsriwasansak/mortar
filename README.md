# vulkan-mortar

A physically-based renderer utilizing vulkan raytracing api.

![The Toilet Scene](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/toilet.jpg)
![Fireplace Room](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/fireplace.jpg)

### Input
The application guesses the ggx parameters from MTL file provided along with OBJ.
Roughness values are mapped from specular exponents (Ns values) using Blinn-Phong to microfacet mapping mentioned in [Brian Karis' blog](http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html).

### Features
In its current early stage, it supports:
* IBL environment map
* Alpha-testing
* Ambient occlusion Integrator
* Path tracer with next event estimation utilizing MIS [Veach's thesis 1997](https://graphics.stanford.edu/papers/veach_thesis)
* Low discreprancy sampler with blue-noise property from [Heitz et al. 2019](https://eheitzresearch.wordpress.com/762-2/)
* PCG random number generator ported from [pcg-random.org](https://www.pcg-random.org/)
* Lambert diffuse BRDF
* GGX Microfacet BSDF (rough conductor and rough dielectric)
* FPS-style camera for traversing the scene

### Dependencies
* vulkan sdk
* glslang (included as a submodule)
* tinyobjloader (included as a submodule)
* glfw (included as a submodule)
* stb (included as a submodule)
* glm (included as a submodule)

### Issues
1. Rough dielectric (frosty glass), especially PDF evaluation, was not tested properly. It might not produce correct result if such material is used.
2. The application will crash while exitting due to an unhandled GLFW surface destructor.

<sup>I started this project around early of March since I was curious of the vulkan raytracing performance. On the first day of this project, I spent an entire day building a new PC to house RTX2060. This is probably the most costly project I ever invested with my own money. :(
