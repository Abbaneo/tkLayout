/*
 * BeamPipe.h
 *
 *  Created on: 11. 5. 2016
 *      Author: Z.Drasal (CERN)
 */

#ifndef BEAMPIPE_H_
#define BEAMPIPE_H_

#include "Property.h"
#include "Visitable.h"

// Forward declaration
class GeometryVisitor;
class ConstGeometryVisitor;
namespace insur {
  class InactiveTube;
}

class BeamPipe : public PropertyObject, public Identifiable<string>, public Buildable, Visitable {

 public:

  //! Constructor -> object set through build method
  BeamPipe(const PropertyTree& treeProperty);

  //! Destructor
  virtual ~BeamPipe();

  //! Buidl method - setting all parameters
  void build();

  //! GeometryVisitor pattern -> beam pipe visitable
  void accept(GeometryVisitor& v);

  //! GeometryVisitor pattern -> beam pipe visitable (const. option)
  void accept(ConstGeometryVisitor& v) const;

  //! Get beam pipe as inactive tube
  const insur::InactiveTube* getMaterial() const;

  // Beam pipe radius, thickness, thickness in rad. length, in int. length
  ReadonlyProperty<double, Default>   radius;           //!< Beam pipe radius
  ReadonlyProperty<double, Default>   thickness;        //!< Beam pipe thickness
  ReadonlyProperty<double, Default>   maxZ;             //!< Beam pipe extends from minimum Z to maximum Z
  ReadonlyProperty<double, Default>   radLength;        //!< Beam pipe rad. length in [%] @ 90 deg
  ReadonlyProperty<double, Default>   intLength;        //!< Beam pipe int. lenght in [%] @ 90 deg

 private:

  //! Copy constructor
  BeamPipe(const BeamPipe&) = default;

  insur::InactiveTube* m_tube; //!< Beam pipe as inactive tube;

}; // Class

#endif /* BEAMPIPE_H_ */
