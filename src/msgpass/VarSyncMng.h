#ifndef VAR_SYNC_MNG_H
#define VAR_SYNC_MNG_H

#include <arcane/IMesh.h>
#include <arcane/utils/MultiArray2.h>

using namespace Arcane;

/*---------------------------------------------------------------------------*/
/* Encapsule la liste des items à envoyer/recevoir */
/*---------------------------------------------------------------------------*/
template<typename ItemType>
class SyncItems {
 public:
  SyncItems(IMesh* mesh, Int32ConstArrayView neigh_ranks);
  virtual ~SyncItems() {}

  auto nbOwnedItemIdxPn() const {
    return m_nb_owned_item_pn.constView();
  }

  auto nbGhostItemIdxPn() const {
    return m_nb_ghost_item_pn.constView();
  }

  auto ownedItemIdxPn() const {
    return m_owned_item_idx_pn.constView();
  }

  auto ghostItemIdxPn() const {
    return m_ghost_item_idx_pn.constView();
  }

 protected:
  // "shared" ou "owned" : les items intérieurs au sous-domaine et qui doivent être envoyés
  // "ghost" : les items fantômes pour lesquels on va recevoir des informations
  // _pn : _per_neigh, info par voisin
  
  // Nb d'items par voisin et listes des indexes de ces items
  IntegerUniqueArray m_nb_owned_item_pn;
  IntegerUniqueArray m_nb_ghost_item_pn;
  MultiArray2<Integer> m_owned_item_idx_pn;
  MultiArray2<Integer> m_ghost_item_idx_pn;
};

/*---------------------------------------------------------------------------*/
/* Encapsule les buffers des valeurs à envoyer/recevoir                      */
/*---------------------------------------------------------------------------*/
template<typename DataType>
class MultiBufView {
 public:
  MultiBufView();
  MultiBufView(ArrayView<Byte*> ptrs, Int64ConstArrayView sizes);
  MultiBufView(const MultiBufView<DataType>& rhs);

  MultiBufView<DataType>& operator=(const MultiBufView<DataType>& rhs);

  //! Accès en lecture/écriture au i-ème buffer
  ArrayView<DataType> operator[](Integer i);

  //! Retourne [beg_ptr, end_ptr[ qui contient tous les buffers (peut-être espacés de trous)
  Span<Byte> rangeSpan();

 protected:
  SharedArray< ArrayView<DataType> > m_multi_buf;
};

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
class SyncBuffers {
 public:
  SyncBuffers() {}
  virtual ~SyncBuffers() {}

  //
  void resetBuf();

  // 
  template<typename DataType>
  void addEstimatedMaxSz(ConstMultiArray2View<Integer> item_idx_pn);

  // 
  void allocIfNeeded();

  /*!
   * \brief A partir des nb d'items à communiquer, estime une borne sup de la taille du buffer en octets
   */
  template<typename DataType>
  Int64 estimatedMaxBufSz(IntegerConstArrayView item_sizes);

  /*!
   * \brief A partir de la vue sur m_buf_bytes, construit une vue par voisin des buffers
   */
  template<typename DataType>
  MultiBufView<DataType> multiBufView(
    ConstMultiArray2View<Integer> item_idx_pn);

 protected:
  /*!
   * \brief A partir de la vue sur un buffer déjà alloué, construit une vue par voisin des buffers
   */
  template<typename DataType>
  MultiBufView<DataType> _multiBufView(
      IntegerConstArrayView item_sizes,
      Span<Byte> buf_bytes);

 protected:
  Int64 m_buf_estim_sz=0;  //! Taille qui va servir à allouer
  Int64 m_first_av_pos=0;  //! Première position disponible
  UniqueArray<Byte> m_buf_bytes;  //! Buffer des données à échanger
};

/*---------------------------------------------------------------------------*/
/* Gère les synchronisations des mailles fantômes par Message Passing        */
/*---------------------------------------------------------------------------*/
class VarSyncMng {
 public:
  VarSyncMng(IMesh* mesh);
  virtual ~VarSyncMng();

  // Equivalent à un var.synchronize() où var est une variable globale (i.e. non multi-mat)
  template<typename ItemType, typename DataType>
  void globalSynchronize(MeshVariableScalarRefT<ItemType, DataType> var);

 protected:

  // Retourne l'instance de SyncItems<T> en fonction de T
  template<typename ItemType>
  SyncItems<ItemType>* _getSyncItems();

 protected:

  IParallelMng* m_pm;  //! pour effectuer les send/receive proprement dit

  Integer m_nb_nei;  //! Nb de voisins (m_neigh_ranks.size())
  Int32ConstArrayView m_neigh_ranks;  //! Liste des rangs des voisins

  SyncItems<Cell>* m_sync_cells=nullptr;
  SyncItems<Node>* m_sync_nodes=nullptr;

  SyncBuffers* m_sync_buffers=nullptr;
};

#endif

