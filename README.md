#### Note
Recently, I deprecated the old Vulkan backend and replaced with a new backend that supports both Vulkan and DX12.
The shader language is also changed from GLSL to HLSL.
Many features are therefore also deprecated.
For now, please do not clone the code.
I will slowly port the features from the old backend to the new one.

---------------------------

# Mortar - A fast (enough) physically based rendering engine

Mortar is a small physically based renderer with agnostic api.
It can use either Vulkan or DX12 as a backend.
For now, it was intended for offline usage but seems fast enough for real-time as well.

![The Toilet Scene](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/toilet.jpg)
![Fireplace Room](https://raw.githubusercontent.com/jamornsriwasansak/vulkan-mortar/master/readme/fireplace.jpg)
> Note: These images are generated by vulkan path tracing code that is now [deprecated](https://github.com/jamornsriwasansak/mortar/tree/master/deprecated-vk-pt-src).

### Input
At this moment, the application accepts OBJ format for loading models and HDR format for environment map.
It guesses the ggx parameters from MTL file provided along with OBJ.
Roughness values are mapped from specular exponents (Ns values) using Blinn-Phong to microfacet mapping mentioned in [Brian Karis' blog](http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html).

As I do not have much time at this moment, changing the scene requires you to modify the code by yourself.
An example is in "src/mainloop.cpp"

### Features
In its current early stage, it supports:
* DX12 and Vulkan backend with same interface.
* Alpha-testing
* GPU-based PCG random number generator ported from [pcg-random.org](https://www.pcg-random.org/)
* Lambert diffuse BRDF
* FPS-style camera for traversing the scene
* Spatiotemporal reservoir resampling (Biased) from [Bitterli et al. 2020](https://research.nvidia.com/publication/2020-07_Spatiotemporal-reservoir-resampling)
#### Feature (Deprecated and will be added back)
* GPU-based Low discreprancy sampler with blue-noise property ported from [Heitz et al.](https://eheitzresearch.wordpress.com/762-2/) \[2019]  ([code](https://github.com/jamornsriwasansak/mortar/blob/master/deprecated-vk-pt-src/shaders/rng/bluesobol.glsl))
* GGX Microfacet BSDF (rough conductor and rough dielectric) ([code](https://github.com/jamornsriwasansak/mortar/blob/master/deprecated-vk-pt-src/shaders/common/bsdf.glsl))
* Environment map importance sampling  ([code](https://github.com/jamornsriwasansak/mortar/blob/master/deprecated-vk-pt-src/common/envmap.h))
* Ambient occlusion Integrator ([code](https://github.com/jamornsriwasansak/mortar/blob/master/deprecated-vk-pt-src/shaders/renderer/rtao/rtao.rgen))
* Path tracer with next event estimation utilizing MIS [Veach's thesis](https://graphics.stanford.edu/papers/veach_thesis) \[1997] ([code](https://github.com/jamornsriwasansak/mortar/blob/master/deprecated-vk-pt-src/shaders/renderer/pathtracer/pathtracer.rgen))
* GPU-based voxelizer ([code](https://github.com/jamornsriwasansak/mortar/blob/master/deprecated-vk-pt-src/shaders/compute/voxelizer/voxelizer.comp))
* Voxel-based probe placement from [Silvenoinen et al.](https://arisilvennoinen.github.io/Projects/RTGI/index.html) ([code](https://github.com/jamornsriwasansak/mortar/blob/master/deprecated-vk-pt-src/misc_app/probeplacer_silvennoinen17.h))

I plan to add Spatiotemporal reservoir resampling for direct light soon.

### Dependencies
The following dependencies aside from vulkan sdk are already included as submodules
* vulkan sdk >= 1.2
* D3D12MemoryAllocator
* DirectX-Headers
* DirectXShaderCompiler
* SPIRV-Reflect
* VulkanMemoryAllocator
* assimp
* glfw
* glm
* spdlog
* stb
