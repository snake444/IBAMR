// ---------------------------------------------------------------------
//
// Copyright (c) 2014 - 2019 by the IBAMR developers
// All rights reserved.
//
// This file is part of IBAMR.
//
// IBAMR is free software and is distributed under the 3-clause BSD
// license. The full text of the license can be found in the file
// COPYRIGHT at the top level directory of IBAMR.
//
// ---------------------------------------------------------------------

#ifndef included_IBAMR_IBFEMethod
#define included_IBAMR_IBFEMethod

/////////////////////////////// INCLUDES /////////////////////////////////////

#include "ibamr/IBFEDirectForcingKinematics.h"
#include "ibamr/IBStrategy.h"
#include "ibamr/ibamr_enums.h"

#include "ibtk/FEDataManager.h"
#include "ibtk/LibMeshSystemVectors.h"
#include "ibtk/SAMRAIDataCache.h"
#include "ibtk/SAMRAIGhostDataAccumulator.h"
#include "ibtk/ibtk_utilities.h"
#include "ibtk/libmesh_utilities.h"

#include "BoxGeneratorStrategy.h"
#include "GriddingAlgorithm.h"
#include "IntVector.h"
#include "LoadBalancer.h"
#include "PatchHierarchy.h"
#include "SideVariable.h"
#include "TagAndInitializeStrategy.h"
#include "Variable.h"
#include "tbox/Database.h"
#include "tbox/Pointer.h"

#include "libmesh/coupling_matrix.h"
#include "libmesh/enum_fe_family.h"
#include "libmesh/enum_order.h"
#include "libmesh/enum_quadrature_type.h"
#include "libmesh/explicit_system.h"

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace IBTK
{
class RobinPhysBdryPatchStrategy;
class SAMRAIDataCache;
} // namespace IBTK
namespace SAMRAI
{
namespace hier
{
template <int DIM>
class BasePatchHierarchy;
template <int DIM>
class BasePatchLevel;
} // namespace hier
namespace tbox
{
class Database;
template <class TYPE>
class Array;
} // namespace tbox
namespace xfer
{
template <int DIM>
class CoarsenSchedule;
template <int DIM>
class RefineSchedule;
template <int DIM>
class RefinePatchStrategy;
} // namespace xfer
} // namespace SAMRAI
namespace libMesh
{
class EquationSystems;
class Mesh;
class Point;
class System;
template <typename T>
class NumericVector;
template <typename T>
class PetscVector;
class ExplicitSystem;
class MeshBase;
template <typename T>
class VectorValue;
} // namespace libMesh

/////////////////////////////// CLASS DEFINITION /////////////////////////////

namespace IBAMR
{
class IBFEDirectForcingKinematics;

/*!
 * \brief Class IBFEMethod is an implementation of the abstract base class
 * IBStrategy that provides functionality required by the IB method with finite
 * element elasticity.
 *
 * By default, the libMesh data is partitioned once at the beginning of the
 * computation by libMesh's default partitioner.
 *
 * <h2>Options Controlling Finite Element Vector Data Layout</h2>
 * IBFEMethod performs an L2 projection to transfer the velocity of the fluid
 * from the Eulerian grid to the finite element representation. The parallel
 * performance of this operation can be substantially improved by doing
 * assembly into the ghost region of each vector (instead of accumulating into
 * an internal PETSc object). By default this class will use the 'accumulate
 * into the ghost region' assembly strategy. The assembly strategy can be
 * selected by changing the database variable vector_assembly_accumulation
 * from <code>GHOSTED</code>, the default, to <code>CACHE</code>, which will
 * use PETSc's VecCache object to distribute data.
 *
 * <h2>Options Controlling Interpolation and Spreading</h2>
 * Like other classes inheriting from IBStrategy, most options regarding the
 * actual IB method implementation can be specified with the provided input
 * database. Parameters starting with <code>IB_</code> set and override those
 * with the same name starting with <code>interp_</code> or
 * <code>spread_</code>: e.g., <code>IB_delta_fcn</code> overrides both
 * <code>interp_delta_fcn</code> and <code>spread_delta_fcn</code>.
 * <ul>
 *   <li><code>interp_quad_type</code>: Quadrature type for interpolation,
 *   provided as a string. Can be any quadrature type known to libMesh.
 *   Defaults to <code>"QGAUSS"</code>.</li>
 *   <li><code>spread_quad_type</code>: Quadrature type for spreading,
 *   provided as a string. Parsed in the same was as <code>interp_quad_type</code>.</li>
 *   <li><code>IB_quad_type</code>: overriding alias for the two previous
 *   entries - has the same default.</li>
 *   <li><code>interp_use_adaptive_quadrature</code>: Whether or not the current
 *   deformation of each element should be considered when determining which
 *   quadrature rule to use. Defaults to <code>TRUE</code>.</li>
 *   <li><code>spread_point_density</code>: Same as above, but for spreading.
 *   <li><code>IB_point_density</code>: overriding alias for the two previous
 *   entries - has the same default.</li>
 *   <li><code>interp_point_density</code>: Parameter for adaptively computing the
 *   number of quadrature points in a quadrature rule. Defaults to
 *   <code>2.0</code>. See IBTK::getQuadratureKey() for a detailed
 *   description.</li>
 *   <li><code>spread_point_density</code>: Same as above, but for spreading.
 *   <li><code>IB_point_density</code>: overriding alias for the two previous
 *   entries - has the same default.</li>
 *   <li><code>interp_use_consistent_mass_matrix</code>: Whether or not mass
 *   lumping should be applied when solving the L2 projection for computing
 *   the velocity of the structure. Defaults to FALSE. Note that no linear
 *   system is solved when computing forces so this parameter does not have a
 *   spreading equivalent.</li>
 *   <li><code>use_consistent_mass_matrix</code>: Overriding alias of the
 *   previous entry.
 *   <li><code>IB_use_consistent_mass_matrix</code>: Overriding alias of
 *   the previous entry.</li>
 *   <li><code>interp_use_nodal_quadrature</code>: Whether or not nodal
 *   quadrature should be used, which is essentially interpolation instead of
 *   projection. This is an experimental feature. Defaults to
 *   <code>FALSE</code>.</li>
 *   <li><code>spread_use_nodal_quadrature</code>: Same as above, but for spreading.
 *   <li><code>IB_use_nodal_quadrature</code>: overriding alias for the two previous
 *   entries - has the same default.</li>
 * </ul>
 *
 * <h2>Options Controlling libMesh Partitioning</h2>
 * <em>This feature is experimental: at the present time the default settings
 * have the best performance and are the correct choice.</em>
 *
 * This class can repartition libMesh data in a way that matches SAMRAI's
 * distribution of patches; put another way, if a certain region of space on
 * the finest level is assigned to processor N, then all libMesh nodes and
 * elements within that region will also be assigned to processor N. The
 * actual partitioning here is done by the IBTK::BoxPartitioner class. See the
 * discussion in IBTK::HierarchyIntegrator and IBTK::FEDataManager for
 * descriptions on how this partitioning is performed.
 *
 * The choice of libMesh partitioner depends on the libmesh_partitioner_type
 * parameter in the input database:
 * <ul>
 *  <li>If <code>libmesh_partitioner_type</code> is
 *      <code>LIBMESH_DEFAULT</code> then this class will never repartition
 *      libMesh data, since the default libMesh partitioner is already used at
 *      the beginning of the computation and, since no degrees of freedom are
 *      added or removed, any subsequent partitioning would have no
 *      effect.</li>
 *
 *  <li>If <code>libmesh_partitioner_type</code> is <code>SAMRAI_BOX</code>
 *      then this class will always repartition the libMesh data with
 *      IBTK::BoxPartitioner every time the Eulerian data is regridded.</li>
 * </ul>
 * The default value for <code>libmesh_partitioner_type</code> is
 * <code>LIBMESH_DEFAULT</code>. The intent of these choices is to
 * automatically use the fairest (that is, partitioning based on equal work
 * when computing force densities and L2 projections) partitioner.
 *
 * <h2>Options Controlling IB Data Partitioning</h2>
 *
 * The main computational expenses of this class are
 * IBFEMethod::interpolateVelocity() and IBFEMethod::spreadForce(). These two
 * methods compute at IB points placed inside the patches owned on the current
 * processor: i.e., they use the Eulerian partitioning of the domain. This
 * partitioning scales very poorly at higher processor counts with some
 * Lagrangian geometries since the Eulerian partitioning places equal number
 * of cells, which do not necessarily coincide with IB points, on different
 * processors: i.e., some processors will have a large number of IB points and
 * some may have zero.
 *
 * To get around this, this class can optionally work with a different
 * partitioning of the Eulerian data that is partitioned so that each
 * processor has roughly the same number of IB points, or some more elaborate
 * partitioning scheme that takes into account the number of mesh nodes as
 * well. This class will set up this scratch hierarchy and manage its state
 * (see IBFEMethod::d_scratch_hierarchy). The scratch hierarchy can be set up
 * by adding the following parameters to the input database:
 *
 * @code
 * use_scratch_hierarchy = TRUE
 * workload_quad_point_weight = 1.0
 *
 * // The values supplied here should usually be the same as those provided to
 * // the top-level GriddingAlgorithm.
 * GriddingAlgorithm
 * {
 *     max_levels = MAX_LEVELS
 *     ratio_to_coarser
 *     {
 *         level_1 = REF_RATIO,REF_RATIO
 *         level_2 = REF_RATIO,REF_RATIO
 *     }
 *
 *     largest_patch_size
 *     {
 *         // We recommend using very large values here: large patches
 *         // are more efficient, especially with the merging load balancer.
 *         level_0 = 512,512
 *     }
 *
 *     smallest_patch_size
 *     {
 *         // on the other hand, smaller patch sizes here typically enable
 *         // better load balancing at the cost of creating more total work
 *         // due to an increased number of ghost cells (and, therefore,
 *         // an increased number of elements in more than one patch).
 *         level_0 = 16,16
 *     }
 *
 *     efficiency_tolerance = 0.80e0
 *     combine_efficiency   = 0.80e0
 *     coalesce_boxes = TRUE
 *     allow_patches_smaller_than_minimum_size_to_prevent_overlaps = TRUE
 * }
 *
 * // Smaller workload factors improve load balancing but increase the total
 * // amount of work since more elements will end up on multiple patches.
 * // This value is a good compromise.
 * LoadBalancer
 * {
 *    type                = "MERGING"
 *    bin_pack_method     = "SPATIAL"
 *    max_workload_factor = 0.5
 * }
 * @endcode
 *
 * i.e., providing <code>use_scratch_hierarchy = TRUE</code> (the default is
 * <code>FALSE</code>) turns on the scratch hierarchy and the remaining
 * parameters determine how patches are generated and load balanced. The extra
 * argument <code>type</code> to <code>LoadBalancer</code> specifies whether
 * an IBTK::MergingLoadBalancer (chosen by <code>"MERGING"</code>) or the
 * default SAMRAI LoadBalancer (chosen by <code>"DEFAULT"</code>) is
 * used. Since IBTK::MergingLoadBalancer is usually what one wants
 * <code>"MERGING"</code> is the default. The merging option is better since
 * it reduces the total number of elements which end up in patch ghost
 * regions since some patches will be merged together.
 *
 * The parameter <code>workload_quad_point_weight</code> is the multiplier
 * assigned to an IB point when calculating the work per processor: in the
 * future additional weights, such as <code>workload_node_point_weight</code>
 * will also be added.
 *
 * <h2>Options Controlling Logging</h2>
 * The logging options set by this class are propagated to the owned
 * IBTK::FEDataManager objects.
 * <ol>
 *   <li><code>enable_logging</code>: set to <code>TRUE</code> to enable logging.</code>.
 *   Defaults to <code>false</code>.</li>
 *   <li><code>skip_initial_workload_log</code>: For testing purposes (see
 *   d_skip_initial_workload_log) it is necessary to disable some output: this
 *   option disables logging of workload data (quadrature point counts, etc.)
 *   before the first time step if set to <code>TRUE</code>. Defaults to
 *   <code>false</code>.</li>
 * </ol>
 *
 * <h2>Handling Restart Data</h2>
 * The caching of the IBFE restart data is not managed by SAMRAI's
 * SAMRAI::tbox::RestartManager. It is instead handled by
 * IBFEMethod::writeFEDataToRestartFile() given a restart_dump_dirname and
 * time_step_number. Each instance of IBFEMethod is registered for restart by
 * default, but the this option can be turned off. During a restart, the data
 * is handled by the SAMRAI::tbox::RestartManager automatically to reinitiate
 * the IBFEMethod.
 */
class IBFEMethod : public IBStrategy
{
public:
    static const std::string COORDS_SYSTEM_NAME;
    static const std::string COORD_MAPPING_SYSTEM_NAME;
    static const std::string FORCE_SYSTEM_NAME;
    static const std::string PHI_SYSTEM_NAME;
    static const std::string SOURCE_SYSTEM_NAME;
    static const std::string VELOCITY_SYSTEM_NAME;

    SAMRAI::tbox::Pointer<SAMRAI::pdat::SideVariable<NDIM, double> > mask_var;
    int mask_current_idx, mask_new_idx, mask_scratch_idx;

    /*!
     * \brief Constructor.
     */
    IBFEMethod(const std::string& object_name,
               SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> input_db,
               libMesh::MeshBase* mesh,
               int max_level_number,
               bool register_for_restart = true,
               const std::string& restart_read_dirname = "",
               unsigned int restart_restore_number = 0);

    /*!
     * \brief Constructor.
     */
    IBFEMethod(const std::string& object_name,
               SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> input_db,
               const std::vector<libMesh::MeshBase*>& meshes,
               int max_level_number,
               bool register_for_restart = true,
               const std::string& restart_read_dirname = "",
               unsigned int restart_restore_number = 0);

    /*!
     * \brief Destructor.
     */
    ~IBFEMethod();

    /*!
     * Return a pointer to the finite element data manager object for the
     * specified part.
     */
    IBTK::FEDataManager* getFEDataManager(unsigned int part = 0) const;

    /*!
     * Indicate that a part should use stress normalization.
     */
    void registerStressNormalizationPart(unsigned int part = 0);

    /*!
     * Typedef specifying interface for coordinate mapping function.
     */
    using CoordinateMappingFcnPtr = void (*)(libMesh::Point& x, const libMesh::Point& X, void* ctx);

    /*!
     * Struct encapsulating coordinate mapping function data.
     */
    struct CoordinateMappingFcnData
    {
        CoordinateMappingFcnData(CoordinateMappingFcnPtr fcn = nullptr, void* ctx = nullptr) : fcn(fcn), ctx(ctx)
        {
        }

        CoordinateMappingFcnPtr fcn;
        void* ctx;
    };

    /*!
     * Register the (optional) function used to initialize the physical
     * coordinates from the Lagrangian coordinates.
     *
     * \note If no function is provided, the initial physical coordinates are
     * taken to be the same as the Lagrangian coordinate system, i.e., the
     * initial coordinate mapping is assumed to be the identity mapping.
     */
    void registerInitialCoordinateMappingFunction(const CoordinateMappingFcnData& data, unsigned int part = 0);

    /*!
     * Get the initial coordinate mapping function data.
     */
    CoordinateMappingFcnData getInitialCoordinateMappingFunction(unsigned int part = 0) const;

    /*!
     * Typedef specifying interface for initial velocity specification function.
     */
    using InitialVelocityFcnPtr = void (*)(libMesh::VectorValue<double>& U0, const libMesh::Point& X0, void* ctx);

    /*!
     * Struct encapsulating initial velocity specification function data.
     */
    struct InitialVelocityFcnData
    {
        InitialVelocityFcnData(InitialVelocityFcnPtr fcn = nullptr, void* ctx = nullptr) : fcn(fcn), ctx(ctx)
        {
        }

        InitialVelocityFcnPtr fcn;
        void* ctx;
    };

    /*!
     * Register the (optional) function used to initialize the velocity of the
     * solid mesh.
     *
     * \note If no function is provided, the initial velocity is taken to be
     * zero.
     */
    void registerInitialVelocityFunction(const InitialVelocityFcnData& data, unsigned int part = 0);

    /*!
     * Get the initial velocity function data.
     */
    InitialVelocityFcnData getInitialVelocityFunction(unsigned int part = 0) const;

    /*!
     * Typedef specifying interface for PK1 stress tensor function.
     */
    using PK1StressFcnPtr = IBTK::TensorMeshFcnPtr;

    /*!
     * Struct encapsulating PK1 stress tensor function data.
     */
    struct PK1StressFcnData
    {
        PK1StressFcnData(PK1StressFcnPtr fcn = nullptr,
                         const std::vector<IBTK::SystemData>& system_data = std::vector<IBTK::SystemData>(),
                         void* const ctx = nullptr,
                         const libMesh::QuadratureType& quad_type = libMesh::INVALID_Q_RULE,
                         const libMesh::Order& quad_order = libMesh::INVALID_ORDER)
            : fcn(fcn), system_data(system_data), ctx(ctx), quad_type(quad_type), quad_order(quad_order)
        {
        }

        PK1StressFcnPtr fcn;
        std::vector<IBTK::SystemData> system_data;
        void* ctx;
        libMesh::QuadratureType quad_type;
        libMesh::Order quad_order;
    };

    /*!
     * Register the (optional) function to compute the first Piola-Kirchhoff
     * stress tensor, used to compute the forces on the Lagrangian finite
     * element mesh.
     *
     * \note It is possible to register multiple PK1 stress functions with this
     * class.  This is intended to be used to implement selective reduced
     * integration.
     */
    void registerPK1StressFunction(const PK1StressFcnData& data, unsigned int part = 0);

    /*!
     * Get the PK1 stress function data.
     */
    std::vector<PK1StressFcnData> getPK1StressFunction(unsigned int part = 0) const;

    /*!
     * Typedef specifying interface for Lagrangian body force distribution
     * function.
     */
    using LagBodyForceFcnPtr = IBTK::VectorMeshFcnPtr;

    /*!
     * Struct encapsulating Lagrangian body force distribution data.
     */
    struct LagBodyForceFcnData
    {
        LagBodyForceFcnData(LagBodyForceFcnPtr fcn = nullptr,
                            const std::vector<IBTK::SystemData>& system_data = std::vector<IBTK::SystemData>(),
                            void* const ctx = nullptr)
            : fcn(fcn), system_data(system_data), ctx(ctx)
        {
        }

        LagBodyForceFcnPtr fcn;
        std::vector<IBTK::SystemData> system_data;
        void* ctx;
    };

    /*!
     * Register the (optional) function to compute body force distributions on
     * the Lagrangian finite element mesh.
     *
     * \note It is \em NOT possible to register multiple body force functions
     * with this class.
     */
    void registerLagBodyForceFunction(const LagBodyForceFcnData& data, unsigned int part = 0);

    /*!
     * Get the Lagrangian body force function data.
     */
    LagBodyForceFcnData getLagBodyForceFunction(unsigned int part = 0) const;

    /*!
     * Typedef specifying interface for Lagrangian pressure force distribution
     * function.
     */
    using LagSurfacePressureFcnPtr = IBTK::ScalarSurfaceFcnPtr;

    /*!
     * Struct encapsulating Lagrangian surface pressure distribution data.
     */
    struct LagSurfacePressureFcnData
    {
        LagSurfacePressureFcnData(LagSurfacePressureFcnPtr fcn = nullptr,
                                  const std::vector<IBTK::SystemData>& system_data = std::vector<IBTK::SystemData>(),
                                  void* const ctx = nullptr)
            : fcn(fcn), system_data(system_data), ctx(ctx)
        {
        }

        LagSurfacePressureFcnPtr fcn;
        std::vector<IBTK::SystemData> system_data;
        void* ctx;
    };

    /*!
     * Register the (optional) function to compute surface pressure
     * distributions on the Lagrangian finite element mesh.
     *
     * \note It is \em NOT possible to register multiple pressure functions with
     * this class.
     */
    void registerLagSurfacePressureFunction(const LagSurfacePressureFcnData& data, unsigned int part = 0);

    /*!
     * Get the Lagrangian surface pressure function data.
     */
    LagSurfacePressureFcnData getLagSurfacePressureFunction(unsigned int part = 0) const;

    /*!
     * Typedef specifying interface for Lagrangian surface force distribution
     * function.
     */
    using LagSurfaceForceFcnPtr = IBTK::VectorSurfaceFcnPtr;

    /*!
     * Struct encapsulating Lagrangian surface force distribution data.
     */
    struct LagSurfaceForceFcnData
    {
        LagSurfaceForceFcnData(LagSurfaceForceFcnPtr fcn = nullptr,
                               const std::vector<IBTK::SystemData>& system_data = std::vector<IBTK::SystemData>(),
                               void* const ctx = nullptr)
            : fcn(fcn), system_data(system_data), ctx(ctx)
        {
        }

        LagSurfaceForceFcnPtr fcn;
        std::vector<IBTK::SystemData> system_data;
        void* ctx;
    };

    /*!
     * Register the (optional) function to compute surface force distributions
     * on the Lagrangian finite element mesh.
     *
     * \note It is \em NOT possible to register multiple surface force functions
     * with this class.
     */
    void registerLagSurfaceForceFunction(const LagSurfaceForceFcnData& data, unsigned int part = 0);

    /*!
     * Get the Lagrangian surface force function data.
     */
    LagSurfaceForceFcnData getLagSurfaceForceFunction(unsigned int part = 0) const;

    /*!
     * Typedef specifying interface for Lagrangian mass source/sink distribution
     * function.
     */
    using LagBodySourceFcnPtr = IBTK::ScalarMeshFcnPtr;

    /*!
     * Struct encapsulating Lagrangian mass source/sink distribution data.
     */
    struct LagBodySourceFcnData
    {
        LagBodySourceFcnData(LagBodySourceFcnPtr fcn = nullptr,
                             const std::vector<IBTK::SystemData>& system_data = std::vector<IBTK::SystemData>(),
                             void* const ctx = nullptr)
            : fcn(fcn), system_data(system_data), ctx(ctx)
        {
        }

        LagBodySourceFcnPtr fcn;
        std::vector<IBTK::SystemData> system_data;
        void* ctx;
    };

    /*!
     * Register the (optional) function to compute a mass source/sink
     * distribution on the Lagrangian finite element mesh.
     */
    void registerLagBodySourceFunction(const LagBodySourceFcnData& data, unsigned int part = 0);

    /*!
     * Get the Lagrangian body source function data.
     */
    LagBodySourceFcnData getLagBodySourceFunction(unsigned int part = 0) const;

    /*!
     * Register the (optional) direct forcing kinematics object with the finite
     * element mesh.
     */
    void registerDirectForcingKinematics(SAMRAI::tbox::Pointer<IBAMR::IBFEDirectForcingKinematics> data,
                                         unsigned int part = 0);

    /*!
     * Return the number of ghost cells required by the Lagrangian-Eulerian
     * interaction routines.
     */
    const SAMRAI::hier::IntVector<NDIM>& getMinimumGhostCellWidth() const override;

    /*!
     * Setup the tag buffer.
     */
    void setupTagBuffer(SAMRAI::tbox::Array<int>& tag_buffer,
                        SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > gridding_alg) const override;

    /*!
     * Inactivate a structure/part. See IBAMR::IBStrategy::inactivateLagrangianStructure().
     *
     * @note Since this class assumes that structures live on the finest grid
     * level the second argument is ignored.
     */
    virtual void inactivateLagrangianStructure(int structure_number = 0,
                                               int level_number = std::numeric_limits<int>::max()) override;

    /*!
     * Activate a previously inactivated structure/part to be used again in
     * FSI calculations. See IBAMR::IBStrategy::activateLagrangianStructure().
     *
     * @note Since this class assumes that structures live on the finest grid
     * level the second argument is ignored.
     */
    virtual void activateLagrangianStructure(int structure_number = 0,
                                             int level_number = std::numeric_limits<int>::max()) override;

    /*!
     * Determine whether or not the given structure or part is currently
     * activated. See IBAMR::IBStrategy::getLagrangianStructureIsActivated().
     *
     * @note Since this class assumes that structures live on the finest grid
     * level the second argument is ignored.
     */
    virtual bool getLagrangianStructureIsActivated(int structure_number = 0,
                                                   int level_number = std::numeric_limits<int>::max()) const override;

    /*!
     * Method to prepare to advance data from current_time to new_time.
     */
    void preprocessIntegrateData(double current_time, double new_time, int num_cycles) override;

    /*!
     * Method to clean up data following call(s) to integrateHierarchy().
     */
    void postprocessIntegrateData(double current_time, double new_time, int num_cycles) override;

    /*!
     * Interpolate the Eulerian velocity to the curvilinear mesh at the
     * specified time within the current time interval.
     */
    void interpolateVelocity(
        int u_data_idx,
        const std::vector<SAMRAI::tbox::Pointer<SAMRAI::xfer::CoarsenSchedule<NDIM> > >& u_synch_scheds,
        const std::vector<SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineSchedule<NDIM> > >& u_ghost_fill_scheds,
        double data_time) override;

    /*!
     * Advance the positions of the Lagrangian structure using the forward Euler
     * method.
     */
    void forwardEulerStep(double current_time, double new_time) override;

    /*!
     * Advance the positions of the Lagrangian structure using the (explicit)
     * midpoint rule.
     */
    void midpointStep(double current_time, double new_time) override;

    /*!
     * Advance the positions of the Lagrangian structure using the (explicit)
     * trapezoidal rule.
     */
    void trapezoidalStep(double current_time, double new_time) override;

    /*!
     * Compute the Lagrangian force at the specified time within the current
     * time interval.
     */
    void computeLagrangianForce(double data_time) override;

    /*!
     * Spread the Lagrangian force to the Cartesian grid at the specified time
     * within the current time interval.
     */
    void
    spreadForce(int f_data_idx,
                IBTK::RobinPhysBdryPatchStrategy* f_phys_bdry_op,
                const std::vector<SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineSchedule<NDIM> > >& f_prolongation_scheds,
                double data_time) override;

    /*!
     * Indicate whether there are any internal fluid sources/sinks.
     */
    bool hasFluidSources() const override;

    /*!
     * Compute the Lagrangian source/sink density at the specified time within
     * the current time interval.
     */
    void computeLagrangianFluidSource(double data_time) override;

    /*!
     * Spread the Lagrangian source/sink density to the Cartesian grid at the
     * specified time within the current time interval.
     */
    void spreadFluidSource(
        int q_data_idx,
        IBTK::RobinPhysBdryPatchStrategy* q_phys_bdry_op,
        const std::vector<SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineSchedule<NDIM> > >& q_prolongation_scheds,
        double data_time) override;

    /*!
     * Get the default interpolation spec object used by the class.
     */
    IBTK::FEDataManager::InterpSpec getDefaultInterpSpec() const;

    /*!
     * Get the default spread spec object used by the class.
     */
    IBTK::FEDataManager::SpreadSpec getDefaultSpreadSpec() const;

    /*!
     * Set the workload spec object used with a particular mesh part.
     */
    void setWorkloadSpec(const IBTK::FEDataManager::WorkloadSpec& workload_spec, unsigned int part = 0);

    /*!
     * Set the interpolation spec object used with a particular mesh part.
     */
    void setInterpSpec(const IBTK::FEDataManager::InterpSpec& interp_spec, unsigned int part = 0);

    /*!
     * Set the spread spec object used with a particular mesh part.
     */
    void setSpreadSpec(const IBTK::FEDataManager::SpreadSpec& spread_spec, unsigned int part = 0);

    /*!
     * Initialize the FE equation systems objects.  This method must be called
     * prior to calling initializeFEData().
     */
    void initializeFEEquationSystems();

    /*!
     * Initialize FE data.  This method must be called prior to calling
     * IBHierarchyIntegrator::initializePatchHierarchy().
     */
    void initializeFEData();

    /*!
     * Reinitialize FE data by calling `reinit` on each part's EquationSystem,
     * reassembling the system matrices, and setting boundary conditions.
     */
    void reinitializeFEData();

    /*!
     * \brief Register Eulerian variables with the parent IBHierarchyIntegrator.
     */
    void registerEulerianVariables() override;

    /*!
     * Initialize Lagrangian data corresponding to the given AMR patch hierarchy
     * at the start of a computation.  If the computation is begun from a
     * restart file, data may be read from the restart databases.
     *
     * A patch data descriptor is provided for the Eulerian velocity in case
     * initialization requires interpolating Eulerian data.  Ghost cells for
     * Eulerian data will be filled upon entry to this function.
     */
    void initializePatchHierarchy(
        SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
        SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > gridding_alg,
        int u_data_idx,
        const std::vector<SAMRAI::tbox::Pointer<SAMRAI::xfer::CoarsenSchedule<NDIM> > >& u_synch_scheds,
        const std::vector<SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineSchedule<NDIM> > >& u_ghost_fill_scheds,
        int integrator_step,
        double init_data_time,
        bool initial_time) override;

    /*!
     * Register a load balancer and work load patch data index with the IB
     * strategy object.
     *
     * @deprecated This method is no longer necessary with the current
     * workload estimation scheme.
     */
    void registerLoadBalancer(SAMRAI::tbox::Pointer<SAMRAI::mesh::LoadBalancer<NDIM> > load_balancer,
                              int workload_data_idx) override;

    /*!
     * Add the estimated computational work from the current object (i.e., the
     * work required by the owned Lagrangian objects) per cell into the
     * specified <code>workload_data_idx</code>.
     */
    void addWorkloadEstimate(SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
                             const int workload_data_idx) override;

    /*!
     * Begin redistributing Lagrangian data prior to regridding the patch
     * hierarchy.
     */
    void beginDataRedistribution(SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
                                 SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > gridding_alg) override;

    /*!
     * Complete redistributing Lagrangian data following regridding the patch
     * hierarchy.
     */
    void endDataRedistribution(SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
                               SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > gridding_alg) override;

    /*!
     * this function only exists for compatibility with the base class and
     * does nothing: data reinitialization is handled by
     * endDataRedistribution() instead.
     *
     * The reasoning is this: since this class stores data only on particular
     * levels (at the present time, the structure is always on the finest
     * level) setting up level data is nontrivial when generating the initial
     * grid (i.e., when tagging cells that contain interaction points for
     * refinement). In a sense there is no level data to compute until we are
     * done regridding.
     *
     * \see SAMRAI::mesh::StandardTagAndInitStrategy::initializeLevelData
     */
    void initializeLevelData(SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchHierarchy<NDIM> > hierarchy,
                             int level_number,
                             double init_data_time,
                             bool can_be_refined,
                             bool initial_time,
                             SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchLevel<NDIM> > old_level,
                             bool allocate_data) override;

    /*!
     * Reset cached hierarchy dependent data.
     *
     * \see SAMRAI::mesh::StandardTagAndInitStrategy::resetHierarchyConfiguration
     */
    void resetHierarchyConfiguration(SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchHierarchy<NDIM> > hierarchy,
                                     int coarsest_level,
                                     int finest_level) override;

    /*!
     * Set integer tags to "one" in cells where refinement of the given level
     * should occur according to user-supplied feature detection criteria.
     *
     * The name here is misleading, but SAMRAI expects us to use one of two
     * tagging methods to refine the grid, and IBAMR consistently uses
     * gradient detection: hence this function has the same name but tags
     * cells in a different way.
     *
     * \see SAMRAI::mesh::StandardTagAndInitStrategy::applyGradientDetector
     */
    void applyGradientDetector(SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchHierarchy<NDIM> > hierarchy,
                               int level_number,
                               double error_data_time,
                               int tag_index,
                               bool initial_time,
                               bool uses_richardson_extrapolation_too) override;

    /*!
     * Write out object state to the given database.
     */
    void putToDatabase(SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> db) override;

    /*!
     * For technical reasons this class does not use SAMRAI's RestartManager, so
     * restart files must be separately written for the IBFE objects. This function
     * saves the solutions to the defined EquationSystems in an xdr file in
     * restart_dump_dirname for each FE part. An example snippet is included below to show
     * the distinct IBFE restart data saving step. The data will then be automatically
     * read back into the system along with the RestartManager data during restart.
     *
     * @code
     * if (dump_restart_data && (iteration_num % restart_dump_interval == 0 || last_step))
     * {
     *     RestartManager::getManager()->writeRestartFile(restart_dump_dirname, iteration_num);
     *     ib_method_ops->writeFEDataToRestartFile(restart_dump_dirname, iteration_num);
     * }
     * @endcode
     */
    void writeFEDataToRestartFile(const std::string& restart_dump_dirname, unsigned int time_step_number);

    /*!
     * Return a pointer to the scratch hierarchy used by this object. See the
     * main documentation of this class for more information.
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchHierarchy<NDIM> > getScratchHierarchy();

protected:
    /*!
     * \brief Compute the stress normalization field Phi.
     */
    void computeStressNormalization(libMesh::PetscVector<double>& Phi_vec,
                                    libMesh::PetscVector<double>& X_vec,
                                    double data_time,
                                    unsigned int part);

    /*!
     * \brief Assemble the RHS for the interior elastic density, possibly
     * splitting off the normal component of the transmission force along the
     * physical boundary of the Lagrangian structure.
     */
    void assembleInteriorForceDensityRHS(libMesh::PetscVector<double>& G_rhs_vec,
                                         libMesh::PetscVector<double>& X_vec,
                                         libMesh::PetscVector<double>* Phi_vec,
                                         double data_time,
                                         unsigned int part);

    /*!
     * \brief Spread the transmission force density along the physical boundary
     * of the Lagrangian structure.
     */
    void spreadTransmissionForceDensity(int f_data_idx,
                                        libMesh::PetscVector<double>& X_ghost_vec,
                                        double data_time,
                                        unsigned int part);

    /*!
     * \brief Impose jump conditions determined from the interior and
     * transmission force densities along the physical boundary of the
     * Lagrangian structure.
     */
    void imposeJumpConditions(int f_data_idx,
                              libMesh::PetscVector<double>& F_ghost_vec,
                              libMesh::PetscVector<double>& X_ghost_vec,
                              double data_time,
                              unsigned int part);

    /*!
     * \brief Initialize the physical coordinates using the supplied coordinate
     * mapping function.  If no function is provided, the initial coordinates
     * are taken to be the Lagrangian coordinates.
     */
    void initializeCoordinates(unsigned int part);

    /*!
     * \brief Compute dX = x - X, useful mainly for visualization purposes.
     */
    void updateCoordinateMapping(unsigned int part);

    /*!
     * \brief Initialize the velocity field using the supplied initial velocity
     * specification function.  If no function is provided, the initial
     * velocity is taken to be zero.
     */
    void initializeVelocity(unsigned int part);

    /*!
     * Get the transfer schedule from the primary hierarchy to the scratch
     * hierarchy associated with the given level and index. If necessary the
     * schedule is created and stored in a map.
     *
     * If needed, a SAMRAI::xfer::RefinePatchStrategy object can be provided
     * for filling ghost data at physical boundaries.
     */
    SAMRAI::xfer::RefineSchedule<NDIM>&
    getPrimaryToScratchSchedule(const int level_number,
                                const int primary_data_idx,
                                const int scratch_data_idx,
                                SAMRAI::xfer::RefinePatchStrategy<NDIM>* patch_strategy = nullptr);

    /*!
     * Get the transfer schedule from the scratch hierarchy to the primary
     * hierarchy associated with the given level and index. If necessary the
     * schedule is created and stored in a map.
     *
     * If needed, a SAMRAI::xfer::RefinePatchStrategy object can be provided
     * for filling ghost data at physical boundaries.
     */
    SAMRAI::xfer::RefineSchedule<NDIM>&
    getScratchToPrimarySchedule(const int level_number,
                                const int primary_data_idx,
                                const int scratch_data_idx,
                                SAMRAI::xfer::RefinePatchStrategy<NDIM>* patch_strategy = nullptr);

    /*!
     * Indicates whether the integrator should output logging messages.
     */
    bool d_do_log = false;

    /*!
     * Whether or not the initial (i.e., before the regrid prior to
     * timestepping) workload calculations should be logged. This output is
     * generally not stable between machines and so this is usually disabled
     * in tests.
     */
    bool d_skip_initial_workload_log = false;

    /*!
     * Whether or not we have started time integration. This is only used to
     * determine whether or not we print some initial logging output: see
     * d_skip_initial_workload_log for more information.
     */
    bool d_started_time_integration = false;

    /*!
     * Boolean controlling whether or not the scratch hierarchy should be
     * used.
     */
    bool d_use_scratch_hierarchy = false;

    /*!
     * Pointers to the patch hierarchy and gridding algorithm objects associated
     * with this object.
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > d_hierarchy;
    SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > d_gridding_alg;
    bool d_is_initialized = false;

    /*!
     * Scratch data caching objects.
     *
     * These are shared by all of the FEDataManagers associated with this class.
     *
     * Note that SAMRAIDataCache objects are associated with only a single
     * PatchHierarchy object, and so different scratch data caching objects are
     * needed for the regular and scratch patch hierarchies.
     */
    std::shared_ptr<IBTK::SAMRAIDataCache> d_primary_eulerian_data_cache, d_scratch_eulerian_data_cache;

    /*!
     * Pointer to one of the above data caches, named in the same way as
     * d_active_fe_data_managers - i.e., this object points to
     * d_scratch_eulerian_data_cache if we are using the scratch hierarchy and
     * otherwise points to d_primary_eulerian_data_cache.
     */
    std::shared_ptr<IBTK::SAMRAIDataCache> d_active_eulerian_data_cache;

    /*!
     * Pointer to the scratch patch hierarchy (which is only used for the
     * evaluation of IB terms, i.e., in IBFEMethod::interpolateVelocity(),
     * IBFEMethod::spreadForce(), and IBFEMethod::spreadFluidSource()).
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > d_scratch_hierarchy;

    int d_lagrangian_workload_current_idx = IBTK::invalid_index;
    int d_lagrangian_workload_new_idx = IBTK::invalid_index;
    int d_lagrangian_workload_scratch_idx = IBTK::invalid_index;

    SAMRAI::tbox::Pointer<SAMRAI::hier::Variable<NDIM> > d_lagrangian_workload_var;
    const std::string d_lagrangian_workload_coarsen_type = "CONSERVATIVE_COARSEN";
    const std::string d_lagrangian_workload_refine_type = "CONSERVATIVE_LINEAR_REFINE";

    /*!
     * Refinement schedules for transferring data from d_hierarchy to
     * d_scratch_hierarchy. The key type is the level number and a pair of
     * indices (the primary and scratch, in that order).
     *
     * @note this function assumes that only data on the finest level needs to
     * be transferred.
     */
    std::map<std::pair<int, std::pair<int, int> >, SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineSchedule<NDIM> > >
        d_scratch_transfer_forward_schedules;

    /*!
     * Refinement schedules for transferring data from d_scratch_hierarchy to
     * d_hierarchy. The key type is the level number and a pair of indices
     * (the primary and scratch, in that order).
     *
     * @note this function assumes that only data on the finest level needs to
     * be transferred.
     */
    std::map<std::pair<int, std::pair<int, int> >, SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineSchedule<NDIM> > >
        d_scratch_transfer_backward_schedules;

    /*
     * The current time step interval.
     */
    double d_current_time = std::numeric_limits<double>::quiet_NaN(),
           d_new_time = std::numeric_limits<double>::quiet_NaN(),
           d_half_time = std::numeric_limits<double>::quiet_NaN();

    /*!
     * Meshes provided to this object. These are set up and managed outside
     * this class. These meshes are modified by IBFEMethod since this class
     * creates several libMesh Systems (and hence stores DoF information in
     * these meshes).
     */
    std::vector<libMesh::MeshBase*> d_meshes;

    /*!
     * Maximum level number in the patch hierarchy.
     */
    int d_max_level_number = -1;

    /*!
     * The libMesh Systems set up by this system (for example, for velocity
     * projection) consist of one variable per spatial component. By default,
     * libMesh assumes that all variables in a given System couple to
     * eachother which, since we only ever solve projection problems in this
     * class, is not the case. Hence we can save some memory by explicitly
     * informing libMesh that the variables in a system only couple to
     * themselves by providing a diagonal coupling matrix to each System.
     */
    libMesh::CouplingMatrix d_diagonal_system_coupling;

    /*!
     * EquationSystems objects, one per part. These contain the actual
     * matrices and solution vectors for each relevant libMesh system.
     */
    std::vector<std::unique_ptr<libMesh::EquationSystems> > d_equation_systems;

    /// Number of parts owned by the present object.
    const unsigned int d_num_parts = 1;

    /// Indexing information determining whether a given part is active or not.
    /// The default state for each part is to be active. Parts are active
    /// unless inactivated via inactivateLagrangianStructure().
    std::vector<bool> d_part_is_active{ true };

    /// FEDataManager objects associated with the primary hierarchy (i.e.,
    /// d_hierarchy). These are used by some other objects (such as
    /// IBFEPostProcessor); IBFEMethod keeps them up to date (i.e.,
    /// reinitializing data after regrids).
    std::vector<IBTK::FEDataManager*> d_primary_fe_data_managers;

    /// FEDataManager objects that use the scratch hierarchy instead of
    /// d_hierarchy. These are only used internally by IBFEMethod and are not
    /// intended to be accessed by any other object.
    std::vector<IBTK::FEDataManager*> d_scratch_fe_data_managers;

    /// The FEDataManager objects that are actually used in computations. This
    /// vector will be equal to either d_primary_fe_data_managers or
    /// d_scratch_fe_data_managers, dependent on which is actually used in IB
    /// calculations.
    std::vector<IBTK::FEDataManager*> d_active_fe_data_managers;

    /// Pointer to object used to accumulate forces during spreading.
    std::unique_ptr<IBTK::SAMRAIGhostDataAccumulator> d_ghost_data_accumulator;

    /// Minimum ghost cell width.
    SAMRAI::hier::IntVector<NDIM> d_ghosts = 0;

    /// Vectors of pointers to the systems for each part (for position, velocity, force
    /// density, sources, and body stress normalization).
    std::vector<libMesh::ExplicitSystem*> d_X_systems, d_U_systems, d_F_systems, d_Q_systems, d_Phi_systems;

    /*!
     * Object managing access to libMesh system vectors for the structure position.
     */
    std::unique_ptr<IBTK::LibMeshSystemVectors> d_X_vecs;

    /*!
     * Object managing access to libMesh system vectors for the structure velocity.
     */
    std::unique_ptr<IBTK::LibMeshSystemVectors> d_U_vecs;

    /*!
     * Object managing access to libMesh system vectors for the structure force.
     */
    std::unique_ptr<IBTK::LibMeshSystemVectors> d_F_vecs;

    /*!
     * Vectors of pointers to the fluid source or sink density vectors. All of
     * these vectors are owned by the libMesh::System objects except for the
     * ones in d_Q_IB_ghost_vecs, which are owned by the FEDataManager
     * objects.
     */
    std::vector<libMesh::PetscVector<double>*> d_Q_half_vecs, d_Q_rhs_vecs, d_Q_IB_ghost_vecs;

    /// Vector of pointers to body stress normalization vectors (both solutions and RHS).
    std::vector<libMesh::PetscVector<double>*> d_Phi_half_vecs, d_Phi_rhs_vecs;

    /*!
     * Vectors containing entries for relevant IB ghost data: see
     * FEDataManager::buildIBGhostedVector.
     *
     * Unlike the other vectors, d_U_IB_rhs_vecs is for assembly and may not
     * be used: see the main documentation of this class for more information.
     */
    std::vector<std::unique_ptr<libMesh::PetscVector<double> > > d_F_IB_solution_vecs;
    std::vector<std::unique_ptr<libMesh::PetscVector<double> > > d_Q_IB_solution_vecs;
    std::vector<std::unique_ptr<libMesh::PetscVector<double> > > d_U_IB_rhs_vecs;
    std::vector<std::unique_ptr<libMesh::PetscVector<double> > > d_X_IB_solution_vecs;

    /*!
     * Whether or not to use the ghost region for velocity assembly. See the
     * main documentation of this class for more information.
     */
    bool d_use_ghosted_velocity_rhs = true;

    /*!
     * Whether or not the libMesh equation systems objects have been
     * initialized (i.e., whether or not initializeFEEquationSystems has been
     * called).
     */
    bool d_fe_equation_systems_initialized = false;

    /*!
     * Whether or not all finite element data (including that initialized by
     * initializeFEEquationSystems), such system matrices, is available.
     */
    bool d_fe_data_initialized = false;

    /*!
     * Type of partitioner to use. See the main documentation of this class
     * for more information.
     */
    LibmeshPartitionerType d_libmesh_partitioner_type = LIBMESH_DEFAULT;

    /*!
     * Whether or not to use AMR in the finite element discretization. This
     * feature is not yet implemented and currently defaults to false.
     */
    bool d_libmesh_use_amr = false;

    /*!
     * Method parameters.
     */
    IBTK::FEDataManager::InterpSpec d_default_interp_spec;
    IBTK::FEDataManager::SpreadSpec d_default_spread_spec;
    IBTK::FEDataManager::WorkloadSpec d_default_workload_spec;
    std::vector<IBTK::FEDataManager::WorkloadSpec> d_workload_spec;
    std::vector<IBTK::FEDataManager::InterpSpec> d_interp_spec;
    std::vector<IBTK::FEDataManager::SpreadSpec> d_spread_spec;
    bool d_split_normal_force = false, d_split_tangential_force = false;
    bool d_use_jump_conditions = false;
    std::vector<libMesh::FEFamily> d_fe_family;
    std::vector<libMesh::Order> d_fe_order;
    std::vector<libMesh::QuadratureType> d_default_quad_type;
    std::vector<libMesh::Order> d_default_quad_order;
    bool d_use_consistent_mass_matrix = true;

    /*!
     * Data related to handling stress normalization.
     */
    double d_epsilon = 0.0;
    bool d_has_stress_normalization_parts = false;
    std::vector<bool> d_is_stress_normalization_part;

    /*!
     * Functions used to compute the initial coordinates of the Lagrangian mesh.
     */
    std::vector<CoordinateMappingFcnData> d_coordinate_mapping_fcn_data;

    /*!
     * Functions used to compute the initial coordinates of the Lagrangian mesh.
     */
    std::vector<InitialVelocityFcnData> d_initial_velocity_fcn_data;

    /*!
     * Functions used to compute the first Piola-Kirchhoff stress tensor.
     */
    std::vector<std::vector<PK1StressFcnData> > d_PK1_stress_fcn_data;

    /*!
     * Objects used to impose direct forcing kinematics.
     */
    std::vector<SAMRAI::tbox::Pointer<IBAMR::IBFEDirectForcingKinematics> > d_direct_forcing_kinematics_data;

    /*!
     * Functions used to compute additional body and surface forces on the
     * Lagrangian mesh.
     */
    std::vector<LagBodyForceFcnData> d_lag_body_force_fcn_data;
    std::vector<LagSurfacePressureFcnData> d_lag_surface_pressure_fcn_data;
    std::vector<LagSurfaceForceFcnData> d_lag_surface_force_fcn_data;

    /*!
     * Functions used to compute source/sink strength on the Lagrangian mesh.
     */
    bool d_has_lag_body_source_parts = false;
    std::vector<bool> d_lag_body_source_part;
    std::vector<LagBodySourceFcnData> d_lag_body_source_fcn_data;

    /*!
     * Nonuniform load balancing data structures.
     */
    SAMRAI::tbox::Pointer<SAMRAI::mesh::LoadBalancer<NDIM> > d_load_balancer;
    int d_workload_idx = IBTK::invalid_index;

    /*!
     * The object name is used as a handle to databases stored in restart files
     * and for error reporting purposes.
     */
    std::string d_object_name;

    /*!
     * A boolean value indicating whether the class is registered with the
     * restart database.
     */
    bool d_registered_for_restart;

    /*!
     * Directory and time step number to use when restarting.
     */
    std::string d_libmesh_restart_read_dir;
    int d_libmesh_restart_restore_number;

    /*!
     * Restart file type for libMesh equation systems (e.g. xda or xdr).
     */
    std::string d_libmesh_restart_file_extension;

    /*!
     * database for the GriddingAlgorithm used with the scratch hierarchy.
     */
    SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> d_scratch_gridding_algorithm_db;

    /*!
     * database for the LoadBalancer used with the scratch hierarchy.
     */
    SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> d_scratch_load_balancer_db;

    /**
     * Error detector used with the scratch hierarchy.
     *
     * @note this object has to be persistent since d_scratch_gridding_alg
     * requires it: see the note for that member object.
     */
    SAMRAI::tbox::Pointer<SAMRAI::mesh::TagAndInitializeStrategy<NDIM> > d_scratch_error_detector;

    /**
     * Box generator used with the scratch hierarchy.
     *
     * @note this object has to be persistent since d_scratch_gridding_alg
     * requires it: see the note for that member object.
     */
    SAMRAI::tbox::Pointer<SAMRAI::mesh::BoxGeneratorStrategy<NDIM> > d_scratch_box_generator;

    /**
     * Load balancer used with the scratch hierarchy.
     *
     * @note this object has to be persistent since d_scratch_gridding_alg
     * requires it: see the note for that member object.
     */
    SAMRAI::tbox::Pointer<SAMRAI::mesh::LoadBalancer<NDIM> > d_scratch_load_balancer;

    /**
     * Gridding algorithm used with the scratch hierarchy.
     *
     * @note this object has to be persistent because, due to a bug in SAMRAI,
     * it is impossible to create a SAMRAI::mesh::GriddingAlgorithm object in
     * a restarted simulation without a corresponding entry in the restart
     * database.
     */
    SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > d_scratch_gridding_algorithm;

private:
    /*!
     * \brief Default constructor.
     *
     * \note This constructor is not implemented and should not be used.
     */
    IBFEMethod();

    /*!
     * \brief Copy constructor.
     *
     * \note This constructor is not implemented and should not be used.
     *
     * \param from The value to copy to this object.
     */
    IBFEMethod(const IBFEMethod& from) = delete;

    /*!
     * \brief Assignment operator.
     *
     * \note This operator is not implemented and should not be used.
     *
     * \param that The value to assign to this object.
     *
     * \return A reference to this object.
     */
    IBFEMethod& operator=(const IBFEMethod& that) = delete;

    /*!
     * Implementation of class constructor.
     */
    void commonConstructor(const std::string& object_name,
                           SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> input_db,
                           const std::vector<libMesh::MeshBase*>& meshes,
                           int max_level_number,
                           bool register_for_restart,
                           const std::string& restart_read_dirname,
                           unsigned int restart_restore_number);

    /*!
     * Read input values from a given database.
     */
    void getFromInput(SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> db, bool is_from_restart);

    /*!
     * Read object state from the restart file and initialize class data
     * members.
     */
    void getFromRestart();

    /*!
     * Do the actual work in reinitializeFEData and initializeFEData. if @p
     * use_present_data is `true` then the current content of the solution
     * vectors is used: more exactly, the coordinates and velocities (computed
     * by initializeCoordinates and initializeVelocity) are considered as
     * being up to date, as is the direct forcing kinematic data.
     */
    void doInitializeFEData(const bool use_present_data);

    /*!
     * Update the caches of IB-ghosted vectors.
     */
    void updateCachedIBGhostedVectors();

    /*!
     * At the present time this class and FEDataManager assume that the finite
     * element mesh is always on the finest grid level. This function
     * explicitly asserts that this condition is met.
     */
    void assertStructureOnFinestLevel() const;
};
} // namespace IBAMR

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_IBAMR_IBFEMethod
