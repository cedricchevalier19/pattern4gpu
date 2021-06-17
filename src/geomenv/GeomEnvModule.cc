#include "geomenv/GeomEnvModule.h"

#include <arcane/geometry/IGeometry.h>
#include <arcane/materials/MeshBlockBuildInfo.h>
#include <arcane/materials/MeshEnvironmentBuildInfo.h>
#include <arcane/materials/IMeshBlock.h>
#include <arcane/materials/MeshMaterialModifier.h>
#include <arcane/materials/MatItemEnumerator.h>
#include <arcane/materials/ComponentPartItemVectorView.h>
#include <arcane/materials/MaterialVariableBuildInfo.h>
#include <arcane/materials/CellToAllEnvCellConverter.h>
#include <arcane/IMesh.h>
#include <arcane/IParallelMng.h>
#include <arcane/IItemFamily.h>
#include <arcane/utils/ArcaneGlobal.h>
#include <arcane/utils/StringBuilder.h>

using namespace Arcane;
using namespace Arcane::Materials;

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
GeomEnvModule::
GeomEnvModule(const ModuleBuildInfo& mbi)
  : ArcaneGeomEnvObject(mbi), 
  m_mesh_material_mng(IMeshMaterialMng::getReference(defaultMesh()))
{
}

GeomEnvModule::
~GeomEnvModule() {
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

class IShape {
 public:
  IShape(String name, IMesh* mesh) :
  m_name (name), 
  m_node_coord(mesh->nodesCoordinates()) {
  }
  virtual ~IShape() {}

  const String &name() const { return m_name; }
  virtual void isInside(VariableNodeBool &node_inside, NodeGroup node_group) = 0;

 protected:
  String m_name;
  const VariableNodeReal3& m_node_coord;
};

class ShapeLayer3D : public IShape {
 protected:
  inline bool _isInsidePt(Real x, Real y, Real z) {
    const Real z_min = m_cmin.x * x + m_cmin.y * y + m_cmin.z;
    const Real z_max = m_cmax.x * x + m_cmax.y * y + m_cmax.z;
    return (z_min<=z && z<z_max);
  }
 public:
  ShapeLayer3D(String name, IMesh* mesh, Real3 pmin, Real3 pmax) :
  IShape (name, mesh) {
    m_cmin.z = pmin.z;
    ARCANE_ASSERT(pmin.x != 0., ("pmin.x ne peut pas être nul !"));
    ARCANE_ASSERT(pmin.y != 0., ("pmin.y ne peut pas être nul !"));
    m_cmin.x = -pmin.z/pmin.x;
    m_cmin.y = -pmin.z/pmin.y;

    m_cmax.z = pmax.z;
    ARCANE_ASSERT(pmax.x != 0., ("pmax.x ne peut pas être nul !"));
    ARCANE_ASSERT(pmax.y != 0., ("pmax.y ne peut pas être nul !"));
    m_cmax.x = -pmax.z/pmax.x;
    m_cmax.y = -pmax.z/pmax.y;
  }
  virtual ~ShapeLayer3D() {}

  void isInside(VariableNodeBool &node_inside, NodeGroup node_group) override {
    ENUMERATE_NODE(inode, node_group) {
      const Real3& pt = m_node_coord[inode];
      node_inside[inode] = _isInsidePt(pt.x, pt.y, pt.z);
    }
  }
 protected:
  Real3 m_cmin;
  Real3 m_cmax;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

template<typename T>
class MinMaxSumRed {
 public:
  MinMaxSumRed(Integer nvals, IParallelMng* parallel_mng) :
  m_parallel_mng (parallel_mng),
  m_nvals (nvals),
  values (nvals),
  min_values (nvals),
  max_values (nvals),
  sum_values (nvals),
  min_ranks (nvals),
  max_ranks (nvals) {
    m_comm_size=m_parallel_mng->commSize();
  }

  void allreduce() {
    m_parallel_mng->computeMinMaxSum(values, min_values, max_values, sum_values, min_ranks, max_ranks);
  }

  String strMinMaxAvg(Integer idx) {
    StringBuilder strb("[min=");
    strb+=min_values[idx];
    strb+=", max=";
    strb+=max_values[idx];
    strb+=", avg=";
    strb+=T(sum_values[idx]/Real(m_comm_size));
    strb+="]";
    return strb.toString();
  }

  String strSumMinMaxAvg(Integer idx) {
    StringBuilder strb;
    strb+=sum_values[idx];
    strb+=" ";
    strb+=strMinMaxAvg(idx);
    return strb.toString();
  }

 public:
  UniqueArray<T> values;
  UniqueArray<T> min_values;
  UniqueArray<T> max_values;
  UniqueArray<T> sum_values;
  UniqueArray<T> min_ranks;
  UniqueArray<T> max_ranks;
 protected:
  IParallelMng* m_parallel_mng;
  Integer m_comm_size; //! Nb de processus dans le communicateur
  Integer m_nvals;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

void GeomEnvModule::
initGeomEnv()
{
  debug() << "Dans InitGeomEnv";
  // On va d'abord créer les environnements en les lisant dans le JDD

  MeshBlockBuildInfo mbbi("BLOCK1",allCells());

  // Définition des différents objets qui vont composer la scene géométrique
  UniqueArray<Real3> l_pts; // points qui délimitent les différentes couches
  l_pts.add(Real3(-0.5,-0.5,-0.5));
  l_pts.add(Real3(0.5,0.5,0.5));
  l_pts.add(Real3(0.7,0.7,0.7));
  l_pts.add(Real3(0.9,0.9,0.9));
  l_pts.add(Real3(1.9,1.9,1.9));
//  l_pts.add(Real3(4,4,4));
  Integer nb_sh=l_pts.size()-1;
  UniqueArray<IShape*> l_shape(nb_sh);
  for(Integer ish(0) ; ish<nb_sh ; ++ish) {
    StringBuilder str_build("MIL");
    str_build+=ish;
    l_shape[ish] = new ShapeLayer3D(str_build.toString(), mesh(), l_pts[ish], l_pts[ish+1]);
  }
  // Fin défintion scene géométrique

  // On a un seul matériau par environnement
  for( Integer i=0,n=l_shape.size(); i<n; ++i ){
    String mat_name = l_shape[i]->name()+String("_mat");
    debug() << "Add material name=" << mat_name;
    m_mesh_material_mng->registerMaterialInfo(mat_name);
    const String &env_name = l_shape[i]->name();
    MeshEnvironmentBuildInfo env_build(env_name);
    debug() << "Add material=" << mat_name << " in environment=" << env_name;
    env_build.addMaterial(mat_name);
    debug() << "Materiau cree";
    IMeshEnvironment* env = m_mesh_material_mng->createEnvironment(env_build);
    debug() << "Environment cree";
    debug() << "Add environment " << env_name << " to BLOCK1";
    mbbi.addEnvironment(env);
  }

  IMeshBlock* block1 = m_mesh_material_mng->createBlock(mbbi);

  m_mesh_material_mng->endCreate(subDomain()->isContinue());

  // On précalcule les volumes globaux sur les mailles
  Numerics::IGeometryMng* geom_service=options()->geometry();
  geom_service->init();
  Numerics::IGeometry* geom=geom_service->geometry();
  VariableCellReal cell_volume(VariableBuildInfo(mesh(),"TemporaryCellVolume"));

  ENUMERATE_CELL(icell, allCells()) {
    cell_volume[icell]=geom->computeMeasure(*icell);
  }

  MeshMaterialModifier modifier(m_mesh_material_mng);
  VariableNodeBool node_inside(VariableBuildInfo(mesh(),"TemporaryNodeInside"));

  Integer max_nb_env = block1->nbEnvironment();
  // tableau de travail, liste des mailles qui appartiendront aux environnements
  UniqueArray<Int32UniqueArray> mat_indexes(max_nb_env); 

  // Liste des volumes partiels en cohérence avec mat_indexes
  UniqueArray<RealUniqueArray> partial_volume(max_nb_env);

  ENUMERATE_ENV(ienv, m_mesh_material_mng) {
    IMeshEnvironment* env = *ienv;
    Integer env_id = env->id();

    // On demande à la forme géométrique si tels ou tels noeuds lui appartient
    l_shape[env_id]->isInside(node_inside, allNodes());

    auto& mat_indexes_env=mat_indexes[env_id];
    auto& partial_volume_env=partial_volume[env_id];

    ENUMERATE_CELL(icell, allCells()) {
      // Pour chaque maille, si au moins un noeud est dans la forme
      // géométrique, alors la maille appartiendra à l'environnement
      Cell cell = *icell;
      Integer nb_node_inside=0;
      ENUMERATE_NODE(inode, cell.nodes()) {
        if (node_inside[inode]) {
          nb_node_inside++;
        }
      }
      if (nb_node_inside>0) {
        mat_indexes_env.add(icell.localId());

        // On calcule le volume partiel au prorata du nb de noeuds présents
        // dans la forme géométrique
        Real part_vol_cell = nb_node_inside*cell_volume[icell]/Real(cell.nbNode());
        partial_volume_env.add(part_vol_cell);
      }
    }

    if (!mat_indexes_env.empty()) {
      // Hypothèse : un SEUL matériau par environnement
      ARCANE_ASSERT(env->nbMaterial() == 1, ("Un environnement ne doit contenir qu'un seul matériau"));
      IMeshMaterial* mat = env->materials()[0];
      modifier.addCells(mat, mat_indexes_env);
    }
  }
  modifier.endUpdate(); // Pour etre sur que c'est pris en compte pour les statistiques
  // On peut créer maintenant l'objet car la composition des environnements
  // est connue

  for(Integer ish(0) ; ish<l_shape.size() ; ish++) {
    delete l_shape[ish];
  }

  CellToAllEnvCellConverter allenvcell_converter(m_mesh_material_mng);
  // On remplit le tableau des volumes partiels m_volume
  auto internals=defaultMesh()->itemsInternal(IK_Cell).data();
  ENUMERATE_ENV(ienv, m_mesh_material_mng) {
    IMeshEnvironment* env = *ienv;
    Integer env_id = env->id();

    auto& mat_indexes_env=mat_indexes[env_id];
    auto& partial_volume_env=partial_volume[env_id];

    // Un peu compliqué car mat_indexes[env_id] n'est pas forcément trié de la
    // même façon que la liste des EnvCell pour env_id
    for(Integer cptr=0 ; cptr<mat_indexes_env.size() ; ++cptr) {
      Integer lid=mat_indexes_env[cptr];
      Cell cell(internals, lid);
      AllEnvCell all_env_cell = allenvcell_converter[cell];
      EnvCell envcell = env->findEnvCell(all_env_cell);
      ARCANE_ASSERT(mat_indexes_env[cptr]==envcell.globalCell().localId(), ("Incohérence entre les lids des mailles"));
      m_volume[envcell] = partial_volume_env[cptr];
    }
  }
  // On calcule le volume global (et on vérifie qu'il est cohérent avec celui
  // calculé par Arcane)
  ENUMERATE_CELL(icell, allCells()) {
    Cell cell=(*icell);
    AllEnvCell all_env_cell = allenvcell_converter[cell];
    Real vol_sum = 0.;
    ENUMERATE_CELL_ENVCELL (envcell_i, all_env_cell) {
      vol_sum += m_volume[envcell_i];
    }
//    Real vol_ref=cell_volume[icell];
//    Real ecart=math::abs(vol_sum-vol_ref)/vol_ref;
//    ARCANE_ASSERT(ecart<1.e-10, ("Ecart trop important sur volume calculé"));
    m_volume[icell]=vol_sum;
  }

  // Sortie du volume pour la visu
  if (options()->visuVolume()) {
    m_volume_visu.resize(max_nb_env);
    m_volume_visu.fill(0.);
    ENUMERATE_ENV(ienv, m_mesh_material_mng) {
      IMeshEnvironment* env = *ienv;
      Integer env_id = env->id();

      ENUMERATE_ENVCELL (envcell_i, env) {
        Cell cell = (*envcell_i).globalCell();
        m_volume_visu[cell][env_id] = m_volume[envcell_i];
      }
    }
  }

  // Statistiques
  auto str_ratio = [](Integer part, Integer tot) {
    Integer pourmille=(1000*part)/tot;
    StringBuilder strb("ratio=");
    strb+=Real(pourmille)/1000.;
    return strb.toString();
  };
  IParallelMng* parallel_mng = defaultMesh()->parallelMng();
  MinMaxSumRed<Integer> ncell(2, parallel_mng);
  ncell.values[0]=allCells().size();
  ncell.values[1]=allCells().own().size();
  ARCANE_ASSERT(ncell.values[1]==ownCells().size(), ("allCells().own().size() != ownCells().size()"));
  ncell.allreduce();
  Integer nb_tot_cells=ncell.sum_values[0];
  Integer nb_tot_cells_inner=ncell.sum_values[1];
  info() << "Nb total de mailles intérieures     : " << ncell.strSumMinMaxAvg(1);
  info() << "Nb total de mailles intérieures+ftm : " << ncell.strSumMinMaxAvg(0);

  MinMaxSumRed<Integer> npurmix(2*max_nb_env, parallel_mng);
  ENUMERATE_ENV(ienv, m_mesh_material_mng) {
    IMeshEnvironment* env = *ienv;
    Integer env_id=env->id();
    npurmix.values[2*env_id+0]=env->pureEnvItems().nbItem();
    npurmix.values[2*env_id+1]=env->impureEnvItems().nbItem();
  }
  npurmix.allreduce();
  ENUMERATE_ENV(ienv, m_mesh_material_mng) {
    IMeshEnvironment* env = *ienv;
    Integer env_id=env->id();
    info() << "Dans environnement " << env->name() << ", nb de mailles pures et mixtes : " 
      << npurmix.strSumMinMaxAvg(2*env_id+0) << ", " << npurmix.strSumMinMaxAvg(2*env_id+1);
  }

  // nb_cell_env[0] = nb de mailles dont le nb d'env == 0
  // nb_cell_env[1] = nb de mailles dont le nb d'env == 1
  // nb_cell_env[2] = nb de mailles dont le nb d'env >= 2
  MinMaxSumRed<Integer> ncell_env(6, parallel_mng);
  ncell_env.values.fill(0);
  
  // On en profite pour créer la liste des mailles actives
  Int32UniqueArray lids;
  ENUMERATE_CELL(icell, allCells()) {
    Cell cell(*icell);
    AllEnvCell all_env_cell = allenvcell_converter[cell];
    Integer nb_env=all_env_cell.nbEnvironment();
    Integer nb_env_bounded=std::min(nb_env,2);
    ncell_env.values[0+nb_env_bounded]++;
    if (cell.isOwn()) {
      ncell_env.values[3+nb_env_bounded]++;
    }
    m_nbenv[icell]=Real(nb_env);

    if (nb_env>0) {
      lids.add(icell.localId());
    }
  }
  ncell_env.allreduce();
  ArrayView<Integer> nb_cell_env(ncell_env.sum_values.subView(0,3));
  ArrayView<Integer> nb_cell_env_inner(ncell_env.sum_values.subView(3,3));
  info() << "Nb de mailles vides  intérieures : " << ncell_env.strSumMinMaxAvg(3+0) << ", " << str_ratio(nb_cell_env_inner[0], nb_tot_cells_inner);
  info() << "Nb de mailles pures  intérieures : " << ncell_env.strSumMinMaxAvg(3+1) << ", " << str_ratio(nb_cell_env_inner[1], nb_tot_cells_inner);
  info() << "Nb de mailles mixtes intérieures : " << ncell_env.strSumMinMaxAvg(3+2) << ", " << str_ratio(nb_cell_env_inner[2], nb_tot_cells_inner);
  info() << "Nb de mailles vides  intérieures+ftm : " << ncell_env.strSumMinMaxAvg(0+0) << ", " << str_ratio(nb_cell_env[0], nb_tot_cells);
  info() << "Nb de mailles pures  intérieures+ftm : " << ncell_env.strSumMinMaxAvg(0+1) << ", " << str_ratio(nb_cell_env[1], nb_tot_cells);
  info() << "Nb de mailles mixtes intérieures+ftm : " << ncell_env.strSumMinMaxAvg(0+2) << ", " << str_ratio(nb_cell_env[2], nb_tot_cells);

  // On crée le groupe des mailles actives "active_cells"
  IItemFamily* family = allCells().itemFamily();
  m_active_cells = family->createGroup("active_cells",lids,true);
  MinMaxSumRed<Integer> nactiv(2, parallel_mng); // [0] = ftm comprise   ,  [1] = inner
  nactiv.values[0]=m_active_cells.size();
  nactiv.values[1]=m_active_cells.own().size();
  nactiv.allreduce();
  ARCANE_ASSERT((nactiv.sum_values[0]+nb_cell_env[0])==nb_tot_cells, ("Nbs de mailles actives + vides != nb total de mailles int+ftm"));
  ARCANE_ASSERT((nactiv.sum_values[1]+nb_cell_env_inner[0])==nb_tot_cells_inner, ("Nbs de mailles actives + vides != nb total de mailles only int"));
  info() << "Nb mailles actives intérieures     : " << nactiv.strSumMinMaxAvg(1)
    << ", " << str_ratio(nactiv.sum_values[1], nb_tot_cells_inner);
  info() << "Nb mailles actives intérieures+ftm : " << nactiv.strSumMinMaxAvg(0)
    << ", " << str_ratio(nactiv.sum_values[0], nb_tot_cells);
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/

ARCANE_REGISTER_MODULE_GEOMENV(GeomEnvModule);

