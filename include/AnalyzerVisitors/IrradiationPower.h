#ifndef IRRADIATIONPOWER_H
#define IRRADIATIONPOWER_H

#include <string>
#include <map>
#include <vector>
#include <utility>

#include "global_constants.h"
#include "Units.h"
#include "Tracker.h"
#include "SimParms.h"
#include "Visitor.h"
#include "SummaryTable.h"

class IrradiationPowerVisitor : public GeometryVisitor {
  double timeIntegratedLumi_;
  double referenceTemp_;
  double operatingTemp_;
  double alphaParam_;
  double biasVoltage_;
  const IrradiationMapsManager* irradiationMap_;

public:
  MultiSummaryTable sensorsIrradiationPowerSummary;
  void preVisit();
  void visit(SimParms& sp);
  void visit(Barrel& b);
  void visit(Endcap& e);
  void visit(DetectorModule& m);
  const double computeSensorsIrradiationPower(const double& irradiation, const double& timeIntegratedLumi,
					const double& alphaParam, const double& volume, const double& referenceTemp,
					const double& operatingTemp, const double& biasVoltage) const;
};


#endif
