Note: I stopped maintaining this repository due to recent Vulkan updates. It broke the pipeline. I might come back to this repository again.
Note2: While I am quite certain that the result is accurate to some degree, I did not verify this renderer against another reference renderers (e.g. mitsuba, pbrt). Please use at your own risk.

# vulkan-path-tracing

A small physically-based renderer utilizing vulkan raytracing api. It focuses on flexibility rather than performance. For instance, hit shader and any-hit shader are simplistic. Therefore, you might lose performance due to cache coherency. This however allow me to share most of the code between ambient occlusion integrator and pt-nee integrator.

![The Toilet Scene](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/toilet.jpg)
![Fireplace Room](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/fireplace.jpg)

### Input
At this moment, the application accepts OBJ format for loading models and HDR format for environment map.
It guesses the ggx parameters from MTL file provided along with OBJ.
Roughness values are mapped from specular exponents (Ns values) using Blinn-Phong to microfacet mapping mentioned in [Brian Karis' blog](http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html).

As I do not have much time at this moment, changing the scene requires you to modify the code by yourself.
An example is in "src/main.cpp"

### Features
In its current early stage, it supports:
* Ambient occlusion Integrator
* Path tracer with next event estimation utilizing MIS [Veach's thesis](https://graphics.stanford.edu/papers/veach_thesis) \[1997]
* Environment map importance sampling
* Alpha-testing
* GPU-based Low discreprancy sampler with blue-noise property ported from [Heitz et al.](https://eheitzresearch.wordpress.com/762-2/) \[2019]
* GPU-based PCG random number generator ported from [pcg-random.org](https://www.pcg-random.org/)
* Lambert diffuse BRDF
* GGX Microfacet BSDF (rough conductor and rough dielectric)
* FPS-style camera for traversing the scene


### Dependencies
* vulkan sdk >= 1.2 (before October 2020)
* glslang (included as a submodule)
* tinyobjloader (included as a submodule)
* glfw (included as a submodule)
* stb (included as a submodule)
* glm (included as a submodule)

### Known issues
1. If you want to use vulkan sdk 1.1, please look for commits before May 29th 2020. Current implementation is using KHR extensions but all commits before May 29th use NV extension.
2. The application will crash while exitting due to an unhandled GLFW surface destructor.
