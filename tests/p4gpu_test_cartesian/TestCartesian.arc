<?xml version='1.0'?>
<case codeversion="1.0" codename="Pattern4GPU" xml:lang="en">
  <arcane>
    <title>Test des acces élémentaires directionnels</title>
    <timeloop>TestCartesianLoop</timeloop>
  </arcane>

  <!-- ***************************************************************** -->
  <!--Definition du maillage cartesien -->
  <mesh nb-ghostlayer="3" ghostlayer-builder-version="3">
    <meshgenerator>
      <cartesian>
        <nsd>2 2 1</nsd>
        <origine>0. 0. 0.</origine>
        <lx nx="15" prx="1.0">1.</lx>
        <ly ny="14" pry="1.0">1.</ly>
        <lz nz="13" pry="1.0">1.</lz>
      </cartesian>
    </meshgenerator>
  </mesh>

</case>
