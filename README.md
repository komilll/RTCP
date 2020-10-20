<h1>RTCP - RayTracing Project</h1>

Project is being created as part of B.Sc. thesis. Original is not created in English, therefore below README file serves as summary for broader audience.

<h2>1. RTAO</h2>
RTAO is superior to SSAO because it doesn't rely only on screenspace information. It works with actual geometry which allows to correctly aproximate darkenings in creases and holes in objects. RTAO is finding occlusion by casting rays to actual geometry and it works fine even for small objects. However for SSAO, some radius has to be chosen. Too small radius might cause large objects to fail to render correctly. Too big will ignore small ones.

Implemention non-interactive RTAO can be done in few steps:
<ol>
  <li>Calculate world-space coordinates from NDC
  <li>Find random direction in the hemipshere around normal
  <li>Cast ray
  <li>Depending on distance from origin, AO value is calculated. The closer hit is, the darker surface will be. It works that way because if point is surrounded by geometry it means that it'll get less light from environment
</ol>

Full implementation can be find in RT_AO.hlsl file.

<h2>2. Path Tracing - GGX Visible Normals</h2>
Joe Schutte's blog - https://schuttejoe.github.io/post/ggximportancesamplingpart1/ - based on Heitz work, is providing discussion how to create PDF according to GGX NDF. NDF is most important when determining BRDF lobe shape. Therefore, we want to sample according to NDF. However there are some problems with this technique which causes fireflies. [Heitz2017] dealt with a problem by improvement of choosing direction of microfacet normal.

Detailed explanation is provided here - https://schuttejoe.github.io/post/ggximportancesamplingpart2/ - provided technique is very simple and fast, but results are noticeably better.

<h2>3. Postprocesses</h2>
It is important to use exposure settings. Generated image's brightness might differ based on number of samples and path length.

Apart from exposure, RTCP is using ACES Filmic Tone Mapper and at the end, result is converted from linear to sRGB colorspace. Full code is provided in PS_Postprocess.hlsl file.

<h2>4. Energy compensation</h2>
Due to multiple bounces of light, energy might be added or removed and it'll be not preserved to be correct at the end of path. [Turquin2019] solves problem in simple fashion. However, explanation is beyond scope of this README. You can find more informations about it here - https://blog.selfshadow.com/publications/turquin/ms_comp_final.pdf

<h2>5. Gallery<h2>
<center><h4>Diffuse + specular pathtracing - 5000 frames, path length - 8</h4></center>
<center><img src="Images/5000spp_8bounce_specular.jpg"></center>
  
<center><h4>RTAO comparison</h4></center>
<center><img src="Images/RTAO.png"></center>

<center><h4>Exposure settings: -12.0, -10.0 (standard), -8.0</h4></center>
<center><img src="Images/Exposure.png"></center>

<center><h4>Energy compensation + energy compensation zoom (left: no energy compensation)</h4></center>
<center><img src="Images/energy_compensation.png"></center>
<center><img src="Images/energy_compensation_zoom.png"></center>

<h4>Important links:</h4>
<ul>
  <li>Correlated Multi-Jittered Sampling - https://graphics.pixar.com/library/MultiJitteredSampling/paper.pdf
  <li>Importance sampling around visible normals - https://schuttejoe.github.io/post/ggximportancesamplingpart2/
  <li>ACES filmic tone mapping curve - https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
  <li>Ray Tracing Gems - http://www.realtimerendering.com/raytracinggems/
  <li>Energy convervation - https://blog.selfshadow.com/publications/turquin/ms_comp_final.pdf 
  <li>Eric Heitz - Understanding the Masking-Shadowing Functionin Microfacet-Based BRDFs http://jcgt.org/published/0003/02/03/paper.pdf 
  <li>Eric Heitz, Eugene d’Eon - Importance Sampling Microfacet-Based BSDFs using the Distribution of Visible Normals https://hal.inria.fr/hal-00996995v1/document 
  <li>Eric Heitz - A Simpler and Exact Sampling Routine for the GGXDistribution of Visible Normals https://hal.archives-ouvertes.fr/hal-01509746/document 
</ul>

<h4>All links:</h4>
<h5>AO</h5>
<ul>
   <li>Martin Mittring 2007 - Finding next gen: CryEngine 2 - https://www.semanticscholar.org/paper/Finding-next-gen%3A-CryEngine-2-Mittring/fe2cf22b09709ef2a27768dc2b7693c07c02c69d 
   <li>Louis Bavoil et al 2008 - Image-space horizon-based ambient occlusion - https://www.researchgate.net/publication/215506032_Image-space_horizon-based_ambient_occlusion 
   <li>Pooria Ghavamian 2019 - Real-time Raytracing and Screen-space Ambient Occlusion http://www.diva-portal.org/smash/record.jsf?pid=diva2%3A1337203&dswid=3596 
   <li>https://developer.nvidia.com/vxao-voxel-ambient-occlusion 
</ul>

<h5>Importance sampling:</h5>
<ul>
   <li>Chris Wyman - http://cwyman.org/code/dxrTutors/tutors/Tutor14/tutorial14.md.html 
   <li>Joe Schutte - Importance Sampling techniques for GGX with Smith Masking-Shadowing: Part 1 https://schuttejoe.github.io/post/ggximportancesamplingpart1/ 
   <li><Cao Jaiyin - Sampling Microfacet BRDF https://agraphicsguy.wordpress.com/2015/11/01/sampling-microfacet-brdf/
   <li>Bruce Walter - Notes on the Ward BRDF  https://www.graphics.cornell.edu/~bjw/wardnotes.pdf 
   <li>Eric Heitz - Understanding the Masking-Shadowing Functionin Microfacet-Based BRDFs http://jcgt.org/published/0003/02/03/paper.pdf 
   <li>Eric Heitz, Eugene d’Eon - Importance Sampling Microfacet-Based BSDFs using the Distribution of Visible Normals https://hal.inria.fr/hal-00996995v1/document 
   <li>Eric Heitz - A Simpler and Exact Sampling Routine for the GGXDistribution of Visible Normals https://hal.archives-ouvertes.fr/hal-01509746/document 
</ul>

<h5>Postprocesses:</h5>
<ul>
   <li>Krzysztof Narkowicz - ACES Filmic Tone Mapping Curve https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/ 
   <li>Academy Color Encoding System Developer Resources - aces-dev https://github.com/ampas/aces-dev 
   <li>Michael Stokes et al. - A Standard Default Color Space for the Internet - sRGB https://www.w3.org/Graphics/Color/sRGB 
   <li>[Turquin2019] Emmanuel Turquin - Practical multiple scattering compensation for microfacet models - https://blog.selfshadow.com/publications/turquin/ms_comp_final.pdf 
   <li>Krzysztof Narkowicz - Comparing Microfacet Multiple Scattering with Real-Life Measurements https://knarkowicz.wordpress.com/2019/08/11/comparing-microfacet-multiple-scattering-with-real-life-measurements/ 
   <li>[Li2018] - Al6061 surface roughness and optical reflectance when machined by single point diamond turning at a low feed rate - https://journals.plos.org/plosone/article?id=10.1371/journal.pone.0195083 
</ul>

<h5>PBR and shading models:</h5>
<ul>
  <li>Brian Karis [Karis2013] - Real Shading in Unreal Engine 4 https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf 
  <li>Sebastien Lagarde et al - Moving Frostbite to Physically Based Rendering 3.0 https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf 
  <li>[CT1982] Robert L. Cook, Kenneth E. Torrance -  A Reflectance Model for Computer Graphics https://dl.acm.org/doi/10.1145/357290.357293 
  <li>[Walter2007] Bruce Walter et al - Microfacet Models for Refraction through Rough Surfaces https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.pdf 
  <li>[Schlick94] Christophe Schlick - An Inexpensive BRDF Model for Physically-Based Rendering 
  <li>[Kajiya1986] James T. Kajira - The Rendering Equation
</ul>

<h5>Other:</h5>
<ul>
   <li>Schied et al. 2017 - Spatiotemporal Variance-Guided Filtering: Real-Time Reconstruction for Path-Traced Global Illumination
https://research.nvidia.com/publication/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A 
   <li>Zwicker et. al 2015 - Recent Advances in Adaptive Sampling and Reconstructionfor Monte Carlo Rendering
   <li>Diede Apers, Petter Edblom, Charles de Rousiers, and Sébastien Hillaire (Electronic Arts) - Interactive Light Map and Irradiance Volume Preview in Frostbite
   <li>Johan Köhler (Treyarch) - Practical Order Independent Transparency
   <li>[Kajiya1986] James T. Kajiya - The Rendering Equation
   <li>[Green2003] Robin Green - Spherical Harmonic Lighting: The Gritty Details
   <li>[Whitted1980], Whitted et al - An Improved Illumination Model for Shaded Display
</ul>
