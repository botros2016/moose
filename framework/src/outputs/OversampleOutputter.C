/****************************************************************/
/*               DO NOT MODIFY THIS HEADER                      */
/* MOOSE - Multiphysics Object Oriented Simulation Environment  */
/*                                                              */
/*           (c) 2010 Battelle Energy Alliance, LLC             */
/*                   ALL RIGHTS RESERVED                        */
/*                                                              */
/*          Prepared by Battelle Energy Alliance, LLC           */
/*            Under Contract No. DE-AC07-05ID14517              */
/*            With the U. S. Department of Energy               */
/*                                                              */
/*            See COPYRIGHT for full restrictions               */
/****************************************************************/

// MOOSE includes
#include "OversampleOutputter.h"
#include "FEProblem.h"
#include "DisplacedProblem.h"
#include "FileMesh.h"
#include "MooseApp.h"

template<>
InputParameters validParams<OversampleOutputter>()
{

  // Get the parameters from the parent object
  InputParameters params = validParams<PetscOutputter>();

  params.addParam<bool>("oversample", false, "Set to true to enable oversampling");
  params.addParam<unsigned int>("refinements", 0, "Number of uniform refinements for oversampling");
  params.addParam<Point>("position", "Set a positional offset, this vector will get added to the nodal cooardinates to move the domain.");
  params.addParam<bool>("append_oversample", false, "Append '_oversample' to the output file base");
  params.addParam<MeshFileName>("file", "The name of the mesh file to read, for oversampling");

  // 'Oversampling' Group
  params.addParamNamesToGroup("refinements oversample position append_oversample file", "Oversampling");

  return params;
}

OversampleOutputter::OversampleOutputter(const std::string & name, InputParameters & parameters) :
    PetscOutputter(name, parameters),
    _mesh_ptr(getParam<bool>("use_displaced") ?
              &_problem_ptr->getDisplacedProblem()->mesh() : &_problem_ptr->mesh()),
    _oversample(getParam<bool>("oversample")),
    _refinements(getParam<unsigned int>("refinements")),
    _change_position(isParamValid("position")),
    _position(_change_position ? getParam<Point>("position") : Point())
{
  // Call the initialization method
  init();

  // Append the '_oversample' to the file base, if desired and oversampling is being used
  if (_oversample && isParamValid("append_oversample") && getParam<bool>("append_oversample"))
    _file_base = _file_base + "_oversample";
}

OversampleOutputter::~OversampleOutputter()
{
  // When the Oversamle::initOversample() is called it creates new objects for the _mesh_ptr and _es_ptr
  // that contain the refined mesh and variables. Also, the _mesh_functions vector and _serialized_solution
  // pointer are populated. In this case, it is the responsibility of the output object to clean these things
  // up. If oversampling is not being used then you must not delete the _mesh_ptr and _es_ptr because
  // they are owned by other objects.
  if (_oversample || _change_position)
  {
    // Delete the mesh and equation system pointers
    delete _mesh_ptr;
    delete _es_ptr;

    // Delete the mesh functions
    for (unsigned int sys_num=0; sys_num < _mesh_functions.size(); ++sys_num)
      for (unsigned int var_num=0; var_num < _mesh_functions[sys_num].size(); ++var_num)
        delete _mesh_functions[sys_num][var_num];

    // Delete the oversampled solution vector
    delete _serialized_solution;
  }
}

void
OversampleOutputter::outputInitial()
{
  // Perform filename check
  if (!_output_file)
    return;

  // Perform oversample solution projection
  if (_oversample || _change_position)
    update();

  // Call the initial output method
  OutputBase::outputInitial();
}

void
OversampleOutputter::outputStep()
{
  // Perform filename check
  if (!_output_file)
    return;

  // Perform oversample solution projection
  if (_oversample || _change_position)
    update();

  // Call the step output method
  OutputBase::outputStep();
}

void
OversampleOutputter::outputFinal()
{
  // Perform filename check
  if (!_output_file)
    return;

  // Perform oversample solution projection
  if (_oversample || _change_position)
    update();

  // Call the final output methods
  OutputBase::outputFinal();
}

void
OversampleOutputter::init()
{
  // Perform the mesh cloning, if needed
  if (_change_position || _oversample)
    cloneMesh();
  else
    return;

  // Re-position the oversampled mesh
  if (_change_position)
    for (MeshBase::node_iterator nd = _mesh_ptr->getMesh().nodes_begin(); nd != _mesh_ptr->getMesh().nodes_end(); ++nd)
      *(*nd) += _position;

  // Perform the mesh refinement
  if (_oversample && _refinements > 0)
  {
    MeshRefinement mesh_refinement(_mesh_ptr->getMesh());
    mesh_refinement.uniformly_refine(_refinements);
  }

  // Create the new EquationSystems
  _es_ptr = new EquationSystems(_mesh_ptr->getMesh());

  // Reference the system from which we are copying
  EquationSystems & source_es = _problem_ptr->es();

  // Initialize the _mesh_functions vector
  unsigned int num_systems = source_es.n_systems();
  _mesh_functions.resize(num_systems);

  // Loop over the number of systems
  for(unsigned int sys_num = 0; sys_num < num_systems; sys_num++)
  {
    // Reference to the current system
    System & source_sys = source_es.get_system(sys_num);

    // Add the system to the new EquationsSystems
    ExplicitSystem & dest_sys = _es_ptr->add_system<ExplicitSystem>(source_sys.name());

    // Loop through the variables in the System
    unsigned int num_vars = source_sys.n_vars();
    if (num_vars > 0)
    {
      _mesh_functions[sys_num].resize(num_vars);
      _serialized_solution = NumericVector<Number>::build().release();
      _serialized_solution->init(source_sys.n_dofs(), false, SERIAL);

      // Need to pull down a full copy of this vector on every processor so we can get values in parallel
      source_sys.solution->localize(*_serialized_solution);

      // Add the variables to the system... simultaneously creating MeshFunctions for them.
      for (unsigned int var_num = 0; var_num < num_vars; var_num++)
      {
        // Create a variable in the dest_sys to match... but of LINEAR LAGRANGE type
        dest_sys.add_variable(source_sys.variable_name(var_num), FEType());
        _mesh_functions[sys_num][var_num] = new MeshFunction(source_es, *_serialized_solution, source_sys.get_dof_map(), var_num);
        _mesh_functions[sys_num][var_num]->init();
      }
    }
  }

  // Initialize the newly created EquationSystem
  _es_ptr->init();
}

void
OversampleOutputter::update()
{
  // Get a reference to actual equation system
  EquationSystems & source_es = _problem_ptr->es();

  // Loop throuch each system
  for (unsigned int sys_num = 0; sys_num < source_es.n_systems(); ++sys_num)
  {
    if (_mesh_functions[sys_num].size())
    {
      // Get references to the source and destination systems
      System & source_sys = source_es.get_system(sys_num);
      System & dest_sys = _es_ptr->get_system(sys_num);

      // Update the solution for the oversampled mesh
      _serialized_solution->clear();
      _serialized_solution->init(source_sys.n_dofs(), false, SERIAL);
      source_sys.solution->localize(*_serialized_solution);

      // Update the mesh functions
      // TODO: Why do we need to recreate these MeshFunctions each time?
      for (unsigned int var_num = 0; var_num < _mesh_functions[sys_num].size(); ++var_num)
      {
        delete _mesh_functions[sys_num][var_num];
        _mesh_functions[sys_num][var_num] = new MeshFunction(source_es, *_serialized_solution, source_sys.get_dof_map(), var_num);
        _mesh_functions[sys_num][var_num]->init();
      }

      // Now loop over the nodes of the oversampled mesh setting values for each variable.
      for (MeshBase::const_node_iterator nd = _mesh_ptr->localNodesBegin(); nd != _mesh_ptr->localNodesEnd(); ++nd)
        for (unsigned int var_num = 0; var_num < _mesh_functions[sys_num].size(); ++var_num)
          dest_sys.solution->set((*nd)->dof_number(sys_num, var_num, 0), (*_mesh_functions[sys_num][var_num])(**nd - _position)); // 0 value is for component
    }
  }
}

void
OversampleOutputter::cloneMesh()
{
  // Create the new mesh from a file
  if (isParamValid("file"))
  {
    InputParameters mesh_params = _problem_ptr->mesh().parameters();
    mesh_params.set<MeshFileName>("file") = getParam<MeshFileName>("file");
    mesh_params.set<bool>("nemesis") = false;
    mesh_params.set<bool>("skip_partitioning") = false;
    _mesh_ptr = new FileMesh("output_problem_mesh", mesh_params);
    _mesh_ptr->allowRecovery(false); // We actually want to reread the initial mesh
    _mesh_ptr->init();
    _mesh_ptr->prepare();
    _mesh_ptr->meshChanged();
  }

  // Clone the existing mesh
  else
  {
    if (_app.isRecovering())
      mooseWarning("Recovering or Restarting with Oversampling may not work (especially with adapted meshes)!!  Refs #2295");

    _mesh_ptr= &(_problem_ptr->mesh().clone());
  }
}
