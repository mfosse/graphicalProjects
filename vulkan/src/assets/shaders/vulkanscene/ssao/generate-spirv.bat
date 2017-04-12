glslangvalidator -V debug.vert -o debug.vert.spv
glslangvalidator -V debug.frag -o debug.frag.spv

glslangvalidator -V deferred.vert -o deferred.vert.spv
glslangvalidator -V deferred.frag -o deferred.frag.spv
glslangvalidator -V mrtMesh.vert -o mrtMesh.vert.spv
glslangvalidator -V mrtMesh.frag -o mrtMesh.frag.spv
glslangvalidator -V mrtSkinnedMesh.vert -o mrtSkinnedMesh.vert.spv
glslangvalidator -V mrtSkinnedMesh.frag -o mrtSkinnedMesh.frag.spv

glslangvalidator -V fullscreen.vert -o fullscreen.vert.spv
glslangvalidator -V ssao.frag -o ssao.frag.spv

glslangvalidator -V composition.vert -o composition.vert.spv
glslangvalidator -V composition.frag -o composition.frag.spv

glslangvalidator -V blur.frag -o blur.frag.spv

rem glslangvalidator -V shadow.vert -o shadow.vert.spv
rem glslangvalidator -V shadow.frag -o shadow.frag.spv
rem glslangvalidator -V shadow.geom -o shadow.geom.spv

pause