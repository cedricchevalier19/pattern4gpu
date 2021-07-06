<?xml version='1.0'?>
<case codeversion="1.0" codename="Pattern4GPU" xml:lang="en">
  <arcane>
    <title>Benchmark pour comparer différentes boucles pour accéder aux items sur maillages cartésiens</title>
    <timeloop>CartesianLoop</timeloop>
  </arcane>

  <!-- ***************************************************************** -->
  <!--Definition du maillage cartesien -->
  <mesh nb-ghostlayer="3" ghostlayer-builder-version="3">
    <meshgenerator>
      <cartesian>
        <nsd>1 1 1</nsd>
        <origine>0. 0. 0.</origine>
        <lx nx="64" prx="1.0">1.</lx>
        <ly ny="64" pry="1.0">1.</ly>
        <lz nz="64" pry="1.0">1.</lz>
      </cartesian>
    </meshgenerator>
  </mesh>

</case>
