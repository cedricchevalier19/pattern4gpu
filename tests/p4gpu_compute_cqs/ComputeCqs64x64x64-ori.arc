<?xml version='1.0'?>
<case codeversion="1.0" codename="Pattern4GPU" xml:lang="en">
  <arcane>
    <title>Benchmark pour évaluer le calcul des Cqs sur allCells() et la maj du vecteur sur active_cells</title>
    <timeloop>ComputeCqsAndVectorLoop</timeloop>
  </arcane>

<!--   <arcane-post-processing> -->
<!--     <output-period>1</output-period> -->
<!--     <output> -->
<!--       <variable>Nbenv</variable> -->
<!--       <variable>VolumeVisu</variable> -->
<!--       <variable>Volume</variable> -->
<!--     </output> -->
<!--     <format> -->
<!--       <binary-file>false</binary-file> -->
<!--     </format> -->
<!--   </arcane-post-processing> -->

  <!-- ***************************************************************** -->
  <!--Definition du maillage cartesien -->
  <mesh nb-ghostlayer="3" ghostlayer-builder-version="3">
    <meshgenerator>
      <cartesian>
        <nsd>2 2 1</nsd>
        <origine>0. 0. 0.</origine>
        <lx nx="64" prx="1.0">1.</lx>
        <ly ny="64" pry="1.0">1.</ly>
        <lz nz="64" pry="1.0">1.</lz>
      </cartesian>
    </meshgenerator>
  </mesh>

  <!-- Configuration du module GeomEnv -->
  <geom-env>
    <visu-volume>false</visu-volume>
    <geom-scene>env5m3</geom-scene>
  </geom-env>

  <!-- Configuration du service AccEnvDefault -->
  <acc-env-default>
    <acc-mem-advise>true</acc-mem-advise>
    <device-affinity>node_rank</device-affinity>
    <!-- <heterog-partition>none</heterog-partition> -->
  </acc-env-default>

  <!-- Configuration du module Pattern4GPU -->
  <pattern4-g-p-u>

    <init-cqs-version>ori</init-cqs-version>
    <init-node-vector-version>ori</init-node-vector-version>
    <init-node-coord-bis-version>ori</init-node-coord-bis-version>
    <init-cell-arr12-version>ori</init-cell-arr12-version>
    <compute-cqs-vector-version>ori</compute-cqs-vector-version>
  </pattern4-g-p-u>
</case>
