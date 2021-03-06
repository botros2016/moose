/*****************************************/
/* Written by andrew.wilkins@csiro.au    */
/* Please contact me if you make changes */
/*****************************************/

//  Base class for fluid density as a function of pressure
//
#ifndef RICHARDSDENSITY_H
#define RICHARDSDENSITY_H

#include "GeneralUserObject.h"

class RichardsDensity;


template<>
InputParameters validParams<RichardsDensity>();

class RichardsDensity : public GeneralUserObject
{
 public:
  RichardsDensity(const std::string & name, InputParameters parameters);

  void initialize();
  void execute();
  void finalize();

  // These functions must be over-ridden in the derived class
  // to provide the actual values of density and its derivatives
  virtual Real density(Real p) const = 0;
  virtual Real ddensity(Real p) const = 0;
  virtual Real d2density(Real p) const = 0;

};

#endif // RICHARDSDENSITY_H
