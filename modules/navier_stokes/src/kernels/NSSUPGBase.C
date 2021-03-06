#include "NSSUPGBase.h"


template<>
InputParameters validParams<NSSUPGBase>()
{
  // Initialize the params object from the base class
  InputParameters params = validParams<NSKernel>();

  params.addRequiredCoupledVar("temperature", "");
  params.addRequiredCoupledVar("enthalpy", "");

  return params;
}




NSSUPGBase::NSSUPGBase(const std::string & name, InputParameters parameters)
    : NSKernel(name, parameters),

      // Material properties
      _viscous_stress_tensor(getMaterialProperty<RealTensorValue>("viscous_stress_tensor")),
      _dynamic_viscosity(getMaterialProperty<Real>("dynamic_viscosity")),
      _thermal_conductivity(getMaterialProperty<Real>("thermal_conductivity")),

      // SUPG-related material properties
      _hsupg(getMaterialProperty<Real>("hsupg")),
      _tauc(getMaterialProperty<Real>("tauc")),
      _taum(getMaterialProperty<Real>("taum")),
      _taue(getMaterialProperty<Real>("taue")),
      _strong_residuals(getMaterialProperty<std::vector<Real> >("strong_residuals")),

      // Momentum equation inviscid flux matrices
      _calA(getMaterialProperty<std::vector<RealTensorValue> >("calA")),

      // "velocity column" matrices
      _calC(getMaterialProperty<std::vector<RealTensorValue> >("calC")),

      // energy inviscid flux matrices
      _calE(getMaterialProperty<std::vector<std::vector<RealTensorValue> > >("calE")),

      // Old coupled variable values
//      _rho_old(coupledValueOld("rho")),
//      _rho_u_old(coupledValueOld("rhou")),
//      _rho_v_old(coupledValueOld("rhov")),
//      _rho_w_old( _dim == 3 ? coupledValueOld("rhow") : _zero),
//      _rho_e_old(coupledValueOld("rhoe")),

      // Time derivative derivatives (no, that's not a typo).  You can
      // just think of these as 1/dt for simplicity, they usually are...
      _d_rhodot_du(coupledDotDu("rho")),
      _d_rhoudot_du(coupledDotDu("rhou")),
      _d_rhovdot_du(coupledDotDu("rhov")),
      _d_rhowdot_du(_mesh.dimension() == 3 ? coupledDotDu("rhow") : _zero),
      _d_rhoedot_du(coupledDotDu("rhoe")),

      // Coupled aux variables
      _temperature(coupledValue("temperature")),
      _enthalpy(coupledValue("enthalpy"))
{
}
