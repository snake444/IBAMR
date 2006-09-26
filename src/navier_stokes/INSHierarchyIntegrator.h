#ifndef included_INSHierarchyIntegrator
#define included_INSHierarchyIntegrator

// Filename: INSHierarchyIntegrator.h
// Last modified: <25.Sep.2006 23:23:09 boyce@boyce-griffiths-powerbook-g4-15.local>
// Created on 02 Apr 2004 by Boyce Griffith (boyce@bigboy.speakeasy.net)

/////////////////////////////// INCLUDES /////////////////////////////////////

// IBAMR INCLUDES
#include <ibamr/AdvDiffHierarchyIntegrator.h>
#include <ibamr/ConvergenceMonitor.h>
#include <ibamr/GodunovAdvector.h>
#include <ibamr/HierarchyProjector.h>
#include <ibamr/SetDataStrategy.h>

// SAMRAI-tools INCLUDES
#include <stools/LinearSolver.h>
#include <stools/HierarchyMathOps.h>

// SAMRAI INCLUDES
#include <CellVariable.h>
#include <CoarsenAlgorithm.h>
#include <CoarsenSchedule.h>
#include <ComponentSelector.h>
#include <FaceVariable.h>
#include <Geometry.h>
#include <GriddingAlgorithm.h>
#include <HierarchyCellDataOpsReal.h>
#include <HierarchyFaceDataOpsReal.h>
#include <NodeVariable.h>
#include <PatchHierarchy.h>
#include <PatchLevel.h>
#include <PoissonSpecifications.h>
#include <RefineAlgorithm.h>
#include <RefineSchedule.h>
#include <RobinBcCoefStrategy.h>
#include <SAMRAIVectorReal.h>
#include <StandardTagAndInitStrategy.h>
#include <VariableContext.h>
#include <VisItDataWriter.h>
#include <tbox/Array.h>
#include <tbox/Database.h>
#include <tbox/Pointer.h>
#include <tbox/Serializable.h>

// C++ STDLIB INCLUDES
#include <list>
#include <map>
#include <ostream>
#include <string>
#include <vector>

/////////////////////////////// CLASS DEFINITION /////////////////////////////

namespace IBAMR
{
/*!
 * Class INSHierarchyIntegrator manages the data required to
 * approximately solve the incompressible Navier-Stokes equations via
 * an approximate cell centered projection method.  Optional time
 * dependent forcing terms and divergence specifications may be
 * registered with the integrator.
 *
 * This integrator employs adaptive local spatial refinement.  All
 * levels of the patch hierarchy are synchronously integrated in time.
 * In particular, subcycling in time is not employed.
 *
 * The viscous terms are treated by the
 * AdvDiffHierarchyIntegrator object supplied to the
 * constructor.  The advective terms are discretized by the supplied
 * GodunovAdvector object.
 *
 * @see AdvDiffHierarchyIntegrator
 * @see GodunovAdvector
 * @see algs::HyperbolicLevelIntegrator<NDIM>
 * @see SAMRAI::mesh::StandardTagAndInitStrategy<NDIM>
 * @see algs::TimeRefinementIntegrator<NDIM>
 * @see algs::TimeRefinementLevelStrategy<NDIM>
 */
class INSHierarchyIntegrator
    : public SAMRAI::mesh::StandardTagAndInitStrategy<NDIM>,
      public SAMRAI::tbox::Serializable
{
public:
    typedef std::map<std::string,SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineAlgorithm<NDIM> > >           RefineAlgMap;
    typedef std::map<std::string,std::vector<SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineSchedule<NDIM> > > >  RefineSchedMap;

    typedef std::map<std::string,SAMRAI::tbox::Pointer<SAMRAI::xfer::CoarsenAlgorithm<NDIM> > >          CoarsenAlgMap;
    typedef std::map<std::string,std::vector<SAMRAI::tbox::Pointer<SAMRAI::xfer::CoarsenSchedule<NDIM> > > > CoarsenSchedMap;

    /*!
     * The constructor for INSHierarchyIntegrator sets some default
     * values, reads in configuration information from input and
     * restart databases, and registers the integrator object with the
     * restart manager when requested.
     *
     * When assertion checking is active, passing in any null pointer
     * or an empty string will result in an unrecoverable exception.
     */
    INSHierarchyIntegrator(
        const std::string& object_name,
        SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> input_db,
        SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
        SAMRAI::tbox::Pointer<GodunovAdvector> explicit_predictor,
        SAMRAI::tbox::Pointer<AdvDiffHierarchyIntegrator> adv_diff_hier_integrator,
        SAMRAI::tbox::Pointer<HierarchyProjector> hier_projector,
        bool register_for_restart=true);

    /*!
     * The destructor for INSHierarchyIntegrator unregisters the
     * integrator object with the restart manager when so registered.
     */
    virtual ~INSHierarchyIntegrator();

    /*!
     * Supply initial conditions for the (cell centered) velocity.
     */
    void registerVelocityInitialConditions(
        SAMRAI::tbox::Pointer<SetDataStrategy> U_init);

    /*!
     * Supply initial conditions for the (cell centered) pressure.
     *
     * NOTE: These initial conditions are used for output purposes
     * only.  They are not actually used in the computation.
     */
    void registerPressureInitialConditions(
        SAMRAI::tbox::Pointer<SetDataStrategy> P_init);

    /*!
     * Supply a (possibly time dependent) cell centered forcing term.
     */
    void registerForceSpecification(
        SAMRAI::tbox::Pointer<SetDataStrategy> F_set);

    /*!
     * Supply a (possibly time dependent) cell centered divergence
     * specification.
     */
    void registerDivergenceSpecification(
        SAMRAI::tbox::Pointer<SetDataStrategy> Q_set);

    /*!
     * Register a cell centered quantity to be advected and diffused
     * according to the computed advection velocity and specified
     * diffusion coefficient.
     *
     * Conservative differencing is employed in evaluating the
     * advective term when conservation_form is true.  Otherwise,
     * non-conservative differencing is used to update the quantity.
     *
     * Optional concrete SetDataStrategy and PhysicalBCDataStrategy
     * objects allow for the specification of initial and boundary
     * data for the advected and diffused quantity Q.  If an
     * initialization object is not specified, Q is initialized to
     * zero.  If a boundary condition object is not specified for Q,
     * it is necessary that the computational domain have only
     * periodic boundaries.  (I.e. the domain can have no "physical"
     * boundaries.)
     */
    void registerAdvectedAndDiffusedQuantity(
        SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > Q_var,
        const double Q_mu=0.0,
        const bool conservation_form=true,
        SAMRAI::tbox::Pointer<SetDataStrategy> Q_init=NULL,
        SAMRAI::tbox::Pointer<PhysicalBCDataStrategy> Q_bc=NULL);

    /*!
     * Register a VisIt data writer so this object will write plot
     * files that may be postprocessed with the VisIt visualization
     * tool.
     */
    void registerVisItDataWriter(
        SAMRAI::tbox::Pointer<SAMRAI::appu::VisItDataWriter<NDIM> > visit_writer);

    /*!
     * Register a convergence monitor, used to determine convergence
     * for problems with known analytic solutions.
     */
    void registerConvergenceMonitor(
        SAMRAI::tbox::Pointer<ConvergenceMonitor> monitor);

    ///
    ///  The following routines:
    ///
    ///      getHierarchyMathOps(),
    ///      setHierarchyMathOps(),
    ///      isManagingHierarchyMathOps()
    ///
    ///  allow for the sharing of a single HierarchyMathOps object
    ///  between mutiple HierarchyIntegrator objects.
    ///

    /*!
     * Return a pointer to the HierarchyMathOps object being used by
     * this integrator.
     */
    SAMRAI::tbox::Pointer<STOOLS::HierarchyMathOps> getHierarchyMathOps() const;

    /*!
     * Set the HierarchyMathOps object being used by this integrator.
     *
     * When manage_ops is true, the HierarchyMathOps object is managed
     * by the integrator.  In particular, the integrator is
     * responsible for invoking HierarchyMathOps::setPatchHierarchy()
     * and HierarchyMathOps::resetLevels() following any changes to
     * the configuration of the patch hierarchy.
     */
    void setHierarchyMathOps(
        SAMRAI::tbox::Pointer<STOOLS::HierarchyMathOps> hier_math_ops,
        const bool manage_ops=false);

    /*!
     * Returns whether this integrator is managing the state of its
     * HierarchyMathOps object.
     *
     * When the integrator is managing the state of its
     * HierarchyMathOps object, the integrator is responsible for
     * invoking HierarchyMathOps::setPatchHierarchy() and
     * HierarchyMathOps::resetLevels() following any changes to the
     * configuration of the patch hierarchy.
     */
    bool isManagingHierarchyMathOps() const;

    ///
    ///  The following routines:
    ///
    ///      initializeHierarchyIntegrator(),
    ///      initializeHierarchy(),
    ///      advanceHierarchy(),
    ///      getStableTimestep(),
    ///      atRegridPoint(),
    ///      getIntegratorTime(),
    ///      getStartTime(),
    ///      getEndTime(),
    ///      getIntegratorStep(),
    ///      getMaxIntegratorSteps(),
    ///      stepsRemaining(),
    ///      getPatchHierarchy(),
    ///      getGriddingAlgorithm(),
    ///      getGodunovAdvector(),
    ///      getAdvDiffHierarchyIntegrator(),
    ///      getHierarchyProjector()
    ///
    ///  allow the INSHierarchyIntegrator to be used as a hierarchy
    ///  integrator.
    ///

    /*!
     * Initialize the variables and communications algorithms managed
     * and used by the integrator.
     *
     * This method must be called prior to any calls to
     * initializeHierarchy() or advanceHierarchy().  Otherwise, when
     * assertion checking is active an unrecoverable exception will
     * occur.
     */
    virtual void initializeHierarchyIntegrator(
        SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > gridding_alg);

    /*!
     * Set AMR patch hierarchy configuration and data at start of
     * simulation.  If the computation is begun from a restart file,
     * the hierarchy and data are read from the hierarchy database.
     * Otherwise, the hierarchy and data are initialized by the
     * gridding algorithm data member.  In this case, the coarsest
     * level is constructed and initialized.  Then, error estimation
     * is performed to determine if and where it should be refined.
     * Successively finer levels are created and initialized until the
     * maximum allowable number of levels is achieved or no further
     * refinement is needed.  The double return value is the time
     * increment for the first data advance step.
     *
     * This function assumes that the hierarchy exists, but that it
     * contains no patch levels, when it is called.  On return from
     * this function, the initial hierarchy configuration and
     * simulation data is set properly for the advanceHierarchy()
     * function to be called.  In particular, on each level
     * constructed only the data needed for initialization exists.
     *
     * When assertion checking is active, the hierachy database
     * pointer must be non-null.
     */
    virtual double initializeHierarchy();

    /*!
     * Integrate data on all patches on all levels of the patch
     * hierarchy from current time (current_time) to new time
     * (new_time).
     *
     * When assertion checking is active, an unrecoverable exception
     * will result if the new time is not greater than the given time.
     */
    virtual double advanceHierarchy(
        const double dt,
        const bool rebalance_coarsest=false);

    /*!
     * Returns the maximum stable timestep according to the hyperbolic
     * CFL condition and a growth condition.  The growth condition is
     * imposed to prevent excessive changes in the maximum stable
     * timestep as the computation progresses.
     *
     * Note that additional "acceleration" timestep restrictions are
     * imposed by the integrator when body forcing terms are supplied.
     */
    virtual double getStableTimestep();

    /*!
     * Return true if the current step count indicates that regridding
     * should occur.
     */
    bool atRegridPoint() const;

    /*!
     * Return the current integration time for coarsest hierarchy
     * level.
     */
    double getIntegratorTime() const;

    /*!
     * Return the initial integration time.
     */
    double getStartTime() const;

    /*!
     * Return the final integration time.
     */
    double getEndTime() const;

    /*!
     * Return the integration step count for entire hierarchy (i.e.,
     * number of steps taken on the coarsest level).
     */
    int getIntegratorStep() const;

    /*!
     * Return the maximum number of integration steps allowed for
     * entire hierarchy (i.e., steps allowed on coarsest level).
     */
    int getMaxIntegratorSteps() const;

    /*!
     * Return true if any integration steps remain, false otherwise.
     */
    bool stepsRemaining() const;

    /*!
     * Return a const pointer to the patch hierarchy managed by integrator.
     */
    const SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > getPatchHierarchy() const;

    /*!
     * Return a pointer to the gridding algorithm object.
     */
    SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > getGriddingAlgorithm() const;

    /*!
     * Return a pointer to the Godunov advector being used to predict
     * the advection velocities.
     */
    SAMRAI::tbox::Pointer<GodunovAdvector> getGodunovAdvector() const;

    /*!
     * Return a pointer to the AdvDiffHierarchyIntegrator
     * being used to integrate the advection-diffusion equation.
     */
    SAMRAI::tbox::Pointer<AdvDiffHierarchyIntegrator> getAdvDiffHierarchyIntegrator() const;

    /*!
     * Return a pointer to the HierarchyProjector being used to
     * inforce incompressibility.
     */
    SAMRAI::tbox::Pointer<HierarchyProjector> getHierarchyProjector() const;

    ///
    ///  The following routines:
    ///
    ///      rebalanceCoarsestLevel(),
    ///      regridHierarchy(),
    ///      predictAdvectionVelocity(),
    ///      integrateAdvDiff(),
    ///      projectHierarchy(),
    ///      updatePressure(),
    ///      synchronizeHierarchy(),
    ///      synchronizeNewLevels(),
    ///      resetTimeDependentHierData(),
    ///      resetHierDataToPreadvanceState()
    ///
    ///  allow the INSHierarchyIntegrator to provide data management
    ///  for a time integrator which making use of this class.
    ///

    /*!
     * Rebalance the hierarchy.
     */
    virtual void rebalanceCoarsestLevel();

    /*!
     * Regrid the hierarchy.
     */
    virtual void regridHierarchy();

    /*!
     * This routine predicts a time-centered advection velocity using
     * an explicit Godunov-like extrapolation.  This MAC advection
     * velocity is exactly projected on the composite grid to ensure
     * that it satisfies the specified divergence condition.
     *
     * This method is additionally responsible for performing a
     * "synchronization projection" following any regridding
     * operation.
     */
    virtual void predictAdvectionVelocity(
        const double current_time,
        const double new_time);

    /*!
     * This routine integrates the advection-diffusion equation for
     * the cell centered intermediate, unprojected velocity field.
     */
    virtual void integrateAdvDiff(
        const double current_time,
        const double new_time);

    /*!
     * This routine approximately projects the cell centered
     * intermediate velocity field, approximately enforcing the
     * specified divergence condition.
     */
    virtual void projectVelocity(
        const double current_time,
        const double new_time);

    /*!
     * This routine updates the value of the pressure.  The exact form
     * of this update may require the solution to additional systems
     * of linear equations.
     */
    virtual void updatePressure(
        const double current_time,
        const double new_time,
        const bool override_current_pressure=false);

    /*!
     * Synchronize the hierarchy.
     */
    virtual void synchronizeHierarchy();

    /*!
     * Coarsen current solution data from finest hierarchy level specified
     * down through the coarsest hierarchy level specified, if initial_time
     * is true.  In this case, the hierarchy is being constructed at the
     * initial simulation time,  After data is coarsened, the application-
     * specific initialization routine is called to set data before that
     * solution is further coarsened to the next coarser level in the
     * hierarchy.  This operation makes the solution consistent between
     * coarser levels and finer levels that did not exist when the coarse
     * levels where created and initialized originally.
     *
     * When initial_time is false, this routine does nothing since the
     * standard hyperbolic AMR algorithm for conservation laws requires
     * no data synchronization after regridding beyond interpolation of
     * data from coarser levels in the hierarchy in some conservative fashion.
     *
     * When assertion checking is active, an unrecoverable exception will
     * result if the hierarchy pointer is null, the level numbers do
     * not properly match existing levels in the hierarchy (either
     * coarsest_level > finest_level or some level is null).
     */
    virtual void synchronizeNewLevels(
        const SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > hierarchy,
        const int coarsest_level,
        const int finest_level,
        const double sync_time,
        const bool initial_time);

    /*!
     * Reset time dependent data.
     */
    virtual void resetTimeDependentHierData(
        const double new_time);

    /*!
     * Deallocate all new simulation data.
     */
    virtual void resetHierDataToPreadvanceState();

    ///
    ///  The following routines:
    ///
    ///      initializeLevelData(),
    ///      resetHierarchyConfiguration(),
    ///      applyGradientDetector()
    ///
    ///  are concrete implementations of functions declared in the
    ///  SAMRAI::mesh::StandardTagAndInitStrategy<NDIM> abstract base class.
    ///

    /*!
     * Initialize data on a new level after it is inserted into an AMR
     * patch hierarchy by the gridding algorithm.  The level number
     * indicates that of the new level.  The old_level pointer
     * corresponds to the level that resided in the hierarchy before
     * the level with the specified number was introduced.  If the
     * pointer is null, there was no level in the hierarchy prior to
     * the call and the level data is set based on the user routines
     * and the simulation time.  Otherwise, the specified level
     * replaces the old level and the new level receives data from the
     * old level appropriately before it is destroyed.
     *
     * Typically, when data is set, it is interpolated from coarser
     * levels in the hierarchy.  If the data is to be set, the level
     * number must match that of the old level, if non-null.  If the
     * old level is non-null, then data is copied from the old level
     * to the new level on regions of intersection between those
     * levels before interpolation occurs.  Then, user-supplied patch
     * routines are called to further initialize the data if needed.
     * The boolean argument initial_time is passed into the user's
     * routines.
     *
     * The boolean argument initial_time indicates whether the level
     * is being introduced for the first time (i.e., at initialization
     * time), or after some regrid process during the calculation
     * beyond the initial hierarchy construction.  This information is
     * provided since the initialization of the data on a patch may be
     * different in each of those circumstances.  The can_be_refined
     * boolean argument indicates whether the level is the finest
     * level allowed in the hierarchy.  This may or may not affect the
     * data initialization process depending on the problem.
     *
     * When assertion checking is active, an unrecoverable exception
     * will result if the hierarchy pointer is null, the level number
     * does not match any level in the hierarchy, or the old level
     * number does not match the level number (if the old level
     * pointer is non-null).
     */
    virtual void initializeLevelData(
        const SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchHierarchy<NDIM> > hierarchy,
        const int level_number,
        const double init_data_time,
        const bool can_be_refined,
        const bool initial_time,
        const SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchLevel<NDIM> > old_level=SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchLevel<NDIM> >(NULL),
        const bool allocate_data=true);

    /*!
     * Reset cached communication schedules after the hierarchy has
     * changed (for example, due to regidding) and the data has been
     * initialized on the new levels.  The intent is that the cost of
     * data movement on the hierarchy will be amortized across
     * multiple communication cycles, if possible.  The level numbers
     * indicate the range of levels in the hierarchy that have
     * changed.  However, this routine updates communication schedules
     * every level finer than and including that indexed by the
     * coarsest level number given.
     *
     * When assertion checking is active, an unrecoverable exception
     * will result if the hierarchy pointer is null, any pointer to a
     * level in the hierarchy that is coarser than the finest level is
     * null, or the given level numbers not specified properly; e.g.,
     * coarsest_level > finest_level.
     */
    virtual void resetHierarchyConfiguration(
        const SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchHierarchy<NDIM> > hierarchy,
        const int coarsest_level,
        const int finest_level);

    /*!
     * Set integer tags to "one" in cells where refinement of the
     * given level should occur according to some user-supplied
     * gradient criteria.  The double time argument is the regrid
     * time.  The integer "tag_index" argument is the patch descriptor
     * index of the cell centered integer tag array on each patch in
     * the hierarchy.  The boolean argument initial_time indicates
     * whether the level is being subject to refinement at the initial
     * simulation time.  If it is false, then the error estimation
     * process is being invoked at some later time after the AMR
     * hierarchy was initially constructed.  The boolean argument
     * uses_richardson_extrapolation_too is true when Richardson
     * extrapolation error estimation is used in addition to the
     * gradient detector, and false otherwise.  This argument helps
     * the user to manage multiple regridding criteria.  This
     * information is passed along to the user's patch tagging
     * routines since the application of the gradient detector may be
     * different in each case.
     *
     * When assertion checking is active, an unrecoverable exception
     * will result if the hierarchy pointer is null or the level
     * number does not match any existing level in the hierarchy.
     */
    virtual void applyGradientDetector(
        const SAMRAI::tbox::Pointer<SAMRAI::hier::BasePatchHierarchy<NDIM> > hierarchy,
        const int level_number,
        const double error_data_time,
        const int tag_index,
        const bool initial_time,
        const bool uses_richardson_extrapolation_too);

    ///
    ///  The following routines:
    ///
    ///      getVelocityVar(),
    ///      getPressureVar(),
    ///      getAdvectionVelocityVar(),
    ///      getForceVar(),
    ///      getDivergenceVar()
    ///
    ///  allows access to the various state variables maintained by
    ///  the integrator.
    ///

    /*!
     * Return a pointer to the fluid velocity state variable.
     */
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > getVelocityVar();

    /*!
     * Return a pointer to the fluid pressure state variable.
     */
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > getPressureVar();

    /*!
     * Return a pointer to the advection velocity variable.
     */
    SAMRAI::tbox::Pointer<SAMRAI::pdat::FaceVariable<NDIM,double> > getAdvectionVelocityVar();

    /*!
     * Return a pointer to the body force variable.
     */
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > getForceVar();

    /*!
     * Return a pointer to the specified divergence variable.
     */
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > getDivergenceVar();

    ///
    ///  The following routines:
    ///
    ///      getCurrentContext(),
    ///      getNewContext(),
    ///      getOldContext(),
    ///      getScratchContext(),
    ///      getPlotContext()
    ///
    ///  allow access to the various variable contexts maintained by
    ///  the integrator.
    ///

    /*!
     * Return pointer to "current" variable context used by
     * integrator.  Current data corresponds to state data at the
     * beginning of a timestep, or when a new level is initialized.
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::VariableContext> getCurrentContext() const;

    /*!
     * Return pointer to "new" variable context used by integrator.
     * New data corresponds to advanced state data at the end of a
     * timestep.  The data is one timestep later than the "current"
     * data.
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::VariableContext> getNewContext() const;

    /*!
     * Return pointer to "old" variable context used by integrator.
     * Old data corresponds to an extra time level of state data used
     * for Richardson extrapolation error estimation.  The data is one
     * timestep earlier than the "current" data.
     *
     * Note that only in certain cases when using time-dependent error
     * estimation, such as Richardson extrapolation, is the returned
     * pointer will non-null.  See contructor for more information.
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::VariableContext> getOldContext() const;

    /*!
     * Return pointer to "scratch" variable context used by
     * integrator.  Scratch data typically corresponds to storage that
     * user-routines in the concrete GodunovAdvector object
     * manipulate; in particular, scratch data contains ghost cells.
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::VariableContext> getScratchContext() const;

    /*!
     * Return pointer to variable context used for plotting.  This
     * context corresponds to the data storage that should be written
     * to plot files.  Typically, this is the same as the "current"
     * context.
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::VariableContext> getPlotContext() const;

    ///
    ///  The following routines:
    ///
    ///      putToDatabase()
    ///
    ///  are concrete implementations of functions declared in the
    ///  SAMRAI::tbox::Serializable abstract base class.
    ///

    /*!
     * Write out object state to the given database.
     *
     * When assertion checking is active, database pointer must be
     * non-null.
     */
    virtual void putToDatabase(
        SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> db);

    ///
    ///  The following routines:
    ///
    ///      printClassData()
    ///
    ///  are provided for your viewing pleasure.
    ///

    /*!
     * Print all data members for INSHierarchyIntegrator
     * class.
     */
    virtual void printClassData(
        std::ostream& os) const;

protected:
    /*!
     * Register a "state" variable with the integrator.  When a refine
     * operator is specified, the data for the variable is
     * automatically maintained as the patch hierarchy evolves.
     *
     * All state variables are registered with three contexts:
     * current, new, and scratch.  The current context of a state
     * variable is maintained from timestep to timestep and,
     * optionally, as the patch hierarchy evolves.
     *
     * When a coarsen operator is specified, at the end of each
     * timestep refined regions of the new context are re-filled with
     * the underlying fine data.  Whether or not a coarsen operation
     * occurs, data in the current context is then overwritten by data
     * in the new context.
     *
     * NOTE: If a refine operator is not specified, the data for the
     * variable is UNDEFINED following any changes to the hierarchy
     * configuration.
     *
     * TODO: Add variable registration to a HierarchyIntegrator base
     * class.
     */
    void registerVariable(
        int& current_idx,
        int& new_idx,
        int& scratch_idx,
        const SAMRAI::tbox::Pointer<SAMRAI::hier::Variable<NDIM> > variable,
        const SAMRAI::hier::IntVector<NDIM>& scratch_ghosts=SAMRAI::hier::IntVector<NDIM>(0),
        const std::string& coarsen_name="NO_COARSEN",
        const std::string& refine_name="NO_REFINE");

    /*!
     * Register a "scratch" variable with the integrator.  This data
     * IS NOT maintained as the patch hierarchy evolves.
     *
     * All scratch variables are registered with the scratch
     * context.
     *
     * TODO: Add variable registration to a HierarchyIntegrator base
     * class.
     */
    void registerVariable(
        int& scratch_idx,
        const SAMRAI::tbox::Pointer<SAMRAI::hier::Variable<NDIM> > variable,
        const SAMRAI::hier::IntVector<NDIM>& ghosts=SAMRAI::hier::IntVector<NDIM>(0));

    /*!
     * Advected and diffused quantities Q.
     */
    std::vector<SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > > d_Q_vars;

    /*!
     * Objects to set initial and boundary conditions as well as
     * forcing terms for each advected and diffused quantity.
     */
    std::vector<SAMRAI::tbox::Pointer<SetDataStrategy> >        d_Q_inits;
    std::vector<SAMRAI::tbox::Pointer<PhysicalBCDataStrategy> > d_Q_bcs;

    /*!
     * The diffusivity coefficients associated with each advected and
     * diffused quantity.
     */
    std::vector<double> d_Q_mus;

    /*!
     * Whether or not to use conservative differencing for each
     * advected and diffused quantity.
     */
    std::vector<bool> d_Q_cons_form;

private:
    /*!
     * @brief Default constructor.
     *
     * NOTE: This constructor is not implemented and should not be
     * used.
     */
    INSHierarchyIntegrator();

    /*!
     * @brief Copy constructor.
     *
     * NOTE: This constructor is not implemented and should not be
     * used.
     *
     * @param from The value to copy to this object.
     */
    INSHierarchyIntegrator(
        const INSHierarchyIntegrator& from);

    /*!
     * @brief Assignment operator.
     *
     * NOTE: This operator is not implemented and should not be used.
     *
     * @param that The value to assign to this object.
     *
     * @return A reference to this object.
     */
    INSHierarchyIntegrator& operator=(
        const INSHierarchyIntegrator& that);

    /*!
     * Compute the appropriate source term which must be added to the
     * momentum equation when the fluid contains sources and sinks.
     */
    void computeDivSourceTerm(
        const int F_idx,
        const int Q_idx,
        const int u_idx,
        const int coarsest_ln,
        const int finest_ln);

    /*!
     * Compute the "acceleration" timestep restriction
     *
     *      dt <= min sqrt(2 dx / |F - grad P|/rho)
     */
    double computeStableDt(
        const int F_idx,
        const int coarsest_ln,
        const int finest_ln);

    /*!
     * Read input values, indicated above, from given database.  The
     * boolean argument is_from_restart should be set to true if the
     * simulation is beginning from restart.  Otherwise it should be
     * set to false.
     *
     * When assertion checking is active, the database pointer must be
     * non-null.  Otherwise, all your base are belong to us.
     */
    void getFromInput(
        SAMRAI::tbox::Pointer<SAMRAI::tbox::Database> db,
        bool is_from_restart);

    /*!
     * Read object state from the restart file and initialize class
     * data members.  The database from which the restart data is read
     * is determined by the object_name specified in the constructor.
     *
     * Unrecoverable Errors:
     *
     *    -   The database corresponding to object_name is not found
     *        in the restart file.
     *
     *    -   The class version number and restart version number do not
     *        match.
     *
     */
    void getFromRestart();

    /*
     * The object name is used as a handle to databases stored in
     * restart files and for error reporting purposes.  The boolean is
     * used to control restart file writing operations.
     */
    std::string d_object_name;
    bool d_registered_for_restart;

    /*
     * Pointers to the patch hierarchy and gridding algorithm objects
     * associated with this time integration object.
     *
     * The gridding algorithm provides grid generation and regridding
     * routines for the AMR hierarchy.
     */
    SAMRAI::tbox::Pointer<SAMRAI::hier::PatchHierarchy<NDIM> > d_hierarchy;
    SAMRAI::tbox::Pointer<SAMRAI::mesh::GriddingAlgorithm<NDIM> > d_gridding_alg;

    /*
     * The ConvergenceMonitor object is used to monitor the
     * convergence of the computed solution to an exact solution.
     */
    SAMRAI::tbox::Pointer<ConvergenceMonitor> d_convergence_monitor;

    /*
     * We cache a pointer to the VisIt data writer to register plot
     * variables.
     */
    SAMRAI::tbox::Pointer<SAMRAI::appu::VisItDataWriter<NDIM> > d_visit_writer;
    double d_P_scale, d_F_scale, d_Q_scale;

    /*
     * The GodunovAdvector provides the numerical routines necessary
     * to explicitly predict a time and face centered advection
     * velocity.
     */
    SAMRAI::tbox::Pointer<GodunovAdvector> d_explicit_predictor;

    /*
     * The AdvDiffHierarchyIntegrator maintains the linear
     * solvers and related data needed to handle the implicit
     * integration of the diffusive terms and the explicit integration
     * of the advective terms.
     */
    SAMRAI::tbox::Pointer<AdvDiffHierarchyIntegrator> d_adv_diff_hier_integrator;

    /*
     * The algs::HyperbolicLevelIntegrator<NDIM> supplies generic operations
     * needed to handle the explicit integration of advection terms.
     * It is maintained by the AdvDiffHierarchyIntegrator.
     */
    SAMRAI::tbox::Pointer<SAMRAI::algs::HyperbolicLevelIntegrator<NDIM> > d_hyp_level_integrator;

    /*
     * The HierarchyProjector maintains the linear solvers and related
     * data needed to enforce the incompressibility constraint.
     */
    SAMRAI::tbox::Pointer<HierarchyProjector> d_hier_projector;

    /*
     * Integrator data read from input or set at initialization.
     */
    double d_start_time;
    double d_end_time;
    double d_grow_dt;
    int d_max_integrator_steps;
    double d_cfl;

    /*
     * The number of initial cycles to perform each timestep.
     */
    int d_num_cycles;

    /*
     * The number of initial cycles to perform in order to obtain a
     * sufficiently accurate guess for P(n=1/2).
     */
    int d_num_init_cycles;

    /*
     * The regrid interval indicates the number of integration steps
     * taken between invocations of the regridding process.
     */
    int d_regrid_interval;

    /*
     * The tag buffer indicates the number of cells on each level by
     * which tagged cells will be buffered after they have selected
     * for refinement.  These values are passed into the gridding
     * algorithm routines during hierarchy construction and
     * regridding.  The tag buffer helps to guarantee that refined
     * cells near important features in the solution will remain
     * refined until the level is regridded next.
     */
    bool d_using_default_tag_buffer;
    SAMRAI::tbox::Array<int> d_tag_buffer;

    /*
     * This boolean value determines whether a re-projection
     * ("synchronization projection") of the velocity field occurs
     * following a regridding of the patch hierarchy.
     */
    bool d_using_synch_projection;

    /*
     * This boolean value determines whether the advection term is
     * computed using conservative or non-conservative differencing.
     */
    bool d_conservation_form;

    /*
     * Tag cells based on the magnitude of vorticity.
     */
    bool d_using_vorticity_tagging;
    SAMRAI::tbox::Array<double> d_Omega_eps;
    double d_Omega_max;

    /*
     * Tag cells based on the presence of sources/sinks.
     */
    bool d_using_div_tagging;

    /*
     * These boolean values determine the form of the pressure update.
     */
    bool d_project_predicted_flux;
    bool d_reproject_pressure;
    bool d_second_order_pressure_update;

    /*
     * These boolean values indicate whether to output the pressure
     * and applied force for visualization.
     */
    bool d_output_P;
    bool d_output_F;
    bool d_output_Q;

    /*
     * This boolean value indicates whether to store the node centered
     * stramfunction for visualization.  Note that this quantity is
     * only correctly computed for single-patch domains.
     */
#if (NDIM == 2)
    bool d_output_psi;
#endif

    /*
     * This boolean value indicates whether to store the cell centered
     * vorticity (curl U) for visualization.
     */
    bool d_output_Omega;

    /*
     * These boolean values indicate whether to store the cell
     * centered divergences of U, u, and u_adv for visualization.
     */
    bool d_output_Div_U;
    bool d_output_Div_u;
    bool d_output_Div_u_adv;

    /*
     * Integrator data that evolves during time integration and
     * maintains the state of the timestep sequence over the levels in
     * the AMR hierarchy.
     */
    double d_old_dt;
    double d_stable_dt;
    double d_integrator_time;
    int    d_integrator_step;

    /*
     * A maximum timestep constraint over the specified time interval.
     */
    double d_dt_max;
    double d_dt_max_time_max;
    double d_dt_max_time_min;

    /*
     * Indicates whether the integrator has been initialized.
     */
    bool d_is_initialized;

    /*
     * Indicates whether the integrator should output logging
     * messages.
     */
    bool d_do_log;

    /*
     * Indicates whether the velocity field needs to be re-projected.
     */
    bool d_reproject_after_regrid;

    /*
     * Indicates whether the integrator is attempting to initialize
     * the pressue.
     */
    int d_cycle;
    bool d_performing_init_cycles;

    /*
     * The fluid density (rho), dynamic viscosity (mu), and kinematic
     * viscosity (nu).
     *
     * NOTE: rho_water = 1.00 g cm^-3
     *       mu_water  = 0.01 g cm^-1 s^-1
     *       nu_water  = 0.01 cm^2 s^-1
     */
    double d_rho, d_mu, d_nu;

    /*
     * Hierarchy operations objects.
     */
    SAMRAI::tbox::Pointer<SAMRAI::math::HierarchyCellDataOpsReal<NDIM,double> > d_hier_cc_data_ops;
    SAMRAI::tbox::Pointer<SAMRAI::math::HierarchyFaceDataOpsReal<NDIM,double> > d_hier_fc_data_ops;
    SAMRAI::tbox::Pointer<STOOLS::HierarchyMathOps> d_hier_math_ops;
    bool d_is_managing_hier_math_ops;

    /*
     * Communications algorithms and schedules.
     */
    RefineAlgMap    d_ralgs;
    RefineSchedMap  d_rscheds;

    CoarsenAlgMap   d_calgs;
    CoarsenSchedMap d_cscheds;

    SAMRAI::tbox::Pointer<SAMRAI::xfer::RefineAlgorithm<NDIM> > d_fill_after_regrid;

    /*
     * Objects to set initial conditions (note that the initial value
     * of the pressure is for visualization purposes only) as well as
     * constant or time-dependent body forcing.
     */
    SAMRAI::tbox::Pointer<SetDataStrategy> d_U_init, d_P_init;
    SAMRAI::tbox::Pointer<SetDataStrategy> d_F_set;
    SAMRAI::tbox::Pointer<SetDataStrategy> d_Q_set;

    /*
     * Linear solvers and associated data.
     *
     * The cell weights are used to compute norms of data defined on
     * the patch hierarchy.
     */
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > d_sol_var, d_rhs_var, d_tmp_var, d_wgt_var;
    double d_volume;

    SAMRAI::tbox::Pointer<SAMRAI::solv::SAMRAIVectorReal<NDIM,double> > d_sol_vec, d_rhs_vec;

    double d_poisson_abs_residual_tol, d_poisson_rel_residual_tol;
    SAMRAI::tbox::Pointer<STOOLS::LinearSolver>    d_poisson_solver;

    const SAMRAI::solv::PoissonSpecifications*     d_helmholtz1_spec;
    const SAMRAI::solv::RobinBcCoefStrategy<NDIM>* d_helmholtz1_bc_coef;
    SAMRAI::tbox::Pointer<STOOLS::LinearSolver>    d_helmholtz1_solver;

    const SAMRAI::solv::PoissonSpecifications*     d_helmholtz2_spec;
    const SAMRAI::solv::RobinBcCoefStrategy<NDIM>* d_helmholtz2_bc_coef;
    SAMRAI::tbox::Pointer<STOOLS::LinearSolver>    d_helmholtz2_solver;

    const SAMRAI::solv::PoissonSpecifications*     d_helmholtz4_spec;
    const SAMRAI::solv::RobinBcCoefStrategy<NDIM>* d_helmholtz4_bc_coef;
    SAMRAI::tbox::Pointer<STOOLS::LinearSolver>    d_helmholtz4_solver;

    /*
     * SAMRAI::hier::Variable<NDIM> lists and SAMRAI::hier::ComponentSelector objects
     * are used for data management.
     */
    std::list<SAMRAI::tbox::Pointer<SAMRAI::hier::Variable<NDIM> > > d_state_variables;
    std::list<SAMRAI::tbox::Pointer<SAMRAI::hier::Variable<NDIM> > > d_scratch_variables;

    std::list<SAMRAI::tbox::Pointer<SAMRAI::hier::Variable<NDIM> > > d_copy_scratch_to_current_fast;
    std::list<SAMRAI::tbox::Pointer<SAMRAI::hier::Variable<NDIM> > > d_copy_scratch_to_current_slow;

    SAMRAI::hier::ComponentSelector d_current_data;
    SAMRAI::hier::ComponentSelector d_new_data;
    SAMRAI::hier::ComponentSelector d_scratch_data;

    /*!
     * State and temporary variables.
     */
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > d_U_var, d_F_var, d_F_div_var, d_Q_var;
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > d_Grad_P_var, d_G_var, d_H_var, d_V_var;
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > d_P_var, d_Phi_var, d_Grad_Phi_var;

    SAMRAI::tbox::Pointer<SAMRAI::pdat::FaceVariable<NDIM,double> > d_u_var, d_u_adv_var;
    SAMRAI::tbox::Pointer<SAMRAI::pdat::FaceVariable<NDIM,double> > d_grad_Phi_var;

    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > d_Omega_var;

#if (NDIM == 3)
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > d_Omega_Norm_var;
#endif

    /*
     * Debugging (plotting) variables.
     */
    SAMRAI::tbox::Pointer<SAMRAI::pdat::CellVariable<NDIM,double> > d_Div_U_var, d_Div_u_var, d_Div_u_adv_var;

    /*
     * SAMRAI::hier::Patch<NDIM> data descriptor indices for all variables
     * managed by the integrator.
     *
     * State variables have three contexts: current, scratch, and new.
     */
    int d_u_current_idx, d_u_new_idx, d_u_scratch_idx;

    int d_P_current_idx, d_P_new_idx, d_P_scratch_idx;
    int d_F_current_idx, d_F_new_idx, d_F_scratch_idx;
    int d_Q_current_idx, d_Q_new_idx, d_Q_scratch_idx;

    int d_F_div_current_idx, d_F_div_new_idx, d_F_div_scratch_idx;
    int d_Omega_current_idx, d_Omega_new_idx, d_Omega_scratch_idx;

#if (NDIM == 3)
    int d_Omega_Norm_idx;
#endif

    int d_Div_U_current_idx,     d_Div_U_new_idx,     d_Div_U_scratch_idx;
    int d_Div_u_current_idx,     d_Div_u_new_idx,     d_Div_u_scratch_idx;
    int d_Div_u_adv_current_idx, d_Div_u_adv_new_idx, d_Div_u_adv_scratch_idx;

    /*
     * SAMRAI::hier::Patch<NDIM> data descriptor indices for all variables
     * managed by the integrator.
     *
     * Scratch variables have only one context.
     */
    int d_Phi_idx, d_Grad_Phi_idx, d_grad_Phi_idx, d_G_idx, d_H_idx, d_V_idx;

    /*
     * SAMRAI::hier::Patch<NDIM> data descriptors for all variables managed by
     * the AdvDiffHierarchyIntegrator class.
     *
     * TIME_DEP variables have three contexts: current, scratch, and
     * new.  Note that the new context is only available for use after
     * the advection-diffusion equation has been solved.
     *
     * NO_FILL variables have only one context.
     */
    int d_U_current_idx,     d_U_new_idx,     d_U_scratch_idx;
    int d_u_adv_current_idx, d_u_adv_new_idx, d_u_adv_scratch_idx;

    int d_Grad_P_current_idx, d_Grad_P_new_idx, d_Grad_P_scratch_idx;

    int d_sol_idx, d_rhs_idx, d_tmp_idx;

    /*
     * SAMRAI::hier::Patch<NDIM> data descriptors for all variables managed by
     * the HierarchyMathOps class.
     *
     * Such variables have only one context.
     */
    int d_wgt_idx;
};
}// namespace IBAMR

/////////////////////////////// INLINE ///////////////////////////////////////

//#include "INSHierarchyIntegrator.I"

//////////////////////////////////////////////////////////////////////////////

#endif //#ifndef included_HierarchyProjector
