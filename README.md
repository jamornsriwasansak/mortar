# vulkan-mortar

A physically-based renderer utilizing vulkan raytracing api.

![The Toilet Scene](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/toilet.jpg)
![Fireplace Room](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/fireplace.jpg)

### Input
At this moment, the application accepts OBJ format for loading models and HDR format for environment map.
It guesses the ggx parameters from MTL file provided along with OBJ.
Roughness values are mapped from specular exponents (Ns values) using Blinn-Phong to microfacet mapping mentioned in [Brian Karis' blog](http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html).

As I do not have much time at this moment, changing the scene requires you to modify the code by yourself.
An example is in "src/main.cpp"

There are two pathtracers in the system.
PathTracer class is quite slow but I am certain that it definitely produces correct result.
FastPathTracer class is much faster but does not take care of BSDF (only BRDF) and handled edge cases through several assumptions.

### Features
In its current early stage, it supports:
* Environment map importance sampling
* Alpha-testing
* Ambient occlusion Integrator
* Path tracer with next event estimation utilizing MIS [Veach's thesis](https://graphics.stanford.edu/papers/veach_thesis) \[1997]
* Low discreprancy sampler with blue-noise property from [Heitz et al.](https://eheitzresearch.wordpress.com/762-2/) \[2019]
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

### Plan
* Implement one indirect ray, two shadow rays + path mollification which seems to be a standard in denoising papers. For instance [Chaitanya et al.](http://www.achaitanya.com/files/projects/dnn/dnn.html ) \[2017] and [Scheid et al.](https://research.nvidia.com/publication/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A) \[2017]
* Implement ASVGF

### Issues
1. Rough dielectric (frosty glass), especially PDF evaluation, was not tested properly. It might not produce correct result if such material is used.
2. The application will crash while exitting due to an unhandled GLFW surface destructor. I believe there is a way to handle this properly with newer vulkan versions. I will thus revisit this issue in the future.

<sup>I started this project around early of March since I was curious of the vulkan raytracing performance. On the first day of this project, I spent an entire day building a new PC to house RTX2060. This is probably the most costly project I ever invested with my own money. :(
