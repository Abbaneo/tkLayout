#include "Disk.h"
#include "messageLogger.h"
#include "ConversionStation.h"

const std::vector<double> Disk::getSmallDeltasFromTree() const {
  std::vector<double> smallDeltas;

  for (int i = 1; i <= numRings(); i++) {
    Ring* ringTemplate = GeometryFactory::make<Ring>();
    ringTemplate->store(propertyTree());
    if (ringNode.count(i) > 0) ringTemplate->store(ringNode.at(i));
    smallDeltas.push_back(ringTemplate->smallDelta());
  }
  return smallDeltas;
}

const std::vector<double> Disk::getDsDistancesFromTree() const {
  std::vector<double> dsDistances;

  for (int i = 1; i <= numRings(); i++) {
    RectangularModule* modTemplate = GeometryFactory::make<RectangularModule>();
    modTemplate->store(propertyTree());
    if (ringNode.count(i) > 0) modTemplate->store(ringNode.at(i));
    dsDistances.push_back(modTemplate->dsDistance());
  }
  return dsDistances;
}

void Disk::check() {
  PropertyObject::check();
  
  if (numRings.state() && innerRadius.state()) throw PathfulException("Only one between numRings and innerRadius can be specified");
  else if (!numRings.state() && !innerRadius.state()) throw PathfulException("At least one between numRings and innerRadius must be specified"); 
}

double Disk::getSmallDelta(const vector<double>& diskSmallDeltas, int ringNumber) const {
  if (ringNumber > diskSmallDeltas.size()) throw PathfulException("Tries to access information from ring which is not in disk.");
  return diskSmallDeltas.at(ringNumber - 1);
}

double Disk::getDsDistance(const vector<double>& diskDsDistances, int ringNumber) const {
  if (ringNumber > diskDsDistances.size()) throw PathfulException("Tries to access information from ring which is not in disk.");
  return diskDsDistances.at(ringNumber - 1);
}

void Disk::cutAtEta(double eta) { 
  for (auto& r : rings_) r.cutAtEta(eta); 
  //  rings_.erase_if([](const Ring& r) { return r.numModules() == 0; }); // get rid of rods which have been completely pruned
  rings_.erase_if([](const Ring& r) { return r.modules().size() == 0; }); // get rid of rods which have been completely pruned
  numRings(rings_.size());
  minZ.clear();
  minR.clear();
  maxZ.clear();
  maxR.clear();
}

/*void Disk::buildBottomUp(const vector<double>& buildDsDistances) {
//  auto childmap = propertyTree().getChildMap<int>("Ring");
  double lastRho = 0;
  double lastSmallDelta;

  for (int i = 1, parity = -bigParity(); lastRho < outerRadius(); i++, parity *= -1) {
    Ring* ring = GeometryFactory::make<Ring>();
    ring->buildDirection(Ring::BOTTOMUP);
    ring->buildCropRadius(outerRadius());
    ring->store(propertyTree());
    if (ringNode.count(i) > 0) ring->store(ringNode.at(i)); //childmap.count(i) > 0 ? childmap[i] : propertyTree()
    if (i == 1) {
      ring->buildStartRadius(innerRadius());
    } else {

      // Calcaluate new and last position in Z
      double newZ  = buildZ() + (parity > 0 ? + bigDelta() : - bigDelta()) - ring->smallDelta() - getDsDistance(buildDsDistances, i)/2;  
      double lastZ = buildZ() + (parity > 0 ? - bigDelta() : + bigDelta()) + lastSmallDelta     + getDsDistance(buildDsDistances, i)/2;
      //double originZ = parity > 0 ? -zError() : +zError();



      // Calculate shift in Z position of extreme cases (either innemost or outermost disc)
      // Remember that disc put always in to the centre of endcap
      double shiftZ        = parity > 0 ? -zHalfLength() : +zHalfLength();

      // Calculate shift in Z due to beam spot
      double errorShiftZ   = parity > 0 ? -zError() : +zError();

      // Calculate next rho taking into account overlap in extreme cases of innermost or outermost disc
      double nextRhoWOverlap  = (lastRho - rOverlap())/(lastZ - shiftZ)*(newZ - shiftZ);
      // Calculate next rho taking into account overlap zError in extreme cases of innermost or outermost disc
      double nextRhoWZError   = (lastRho)/(lastZ - shiftZ - errorShiftZ)*(newZ - shiftZ - errorShiftZ);

      // New next rho as a minimum
      double nextRho = MIN(nextRhoWOverlap, nextRhoWZError);
      ring->buildStartRadius(nextRho);

       // For debug test only
       //std::cout << ">>> noOverlap:        " << (lastRho)/lastZ * newZ              << " New [ ; ]: " << (lastRho)/(lastZ - zHalfLength()) * (newZ - zHalfLength()) << " " << (lastRho)/(lastZ + zHalfLength()) * (newZ + zHalfLength()) << std::endl;
       //std::cout << ">>> yesOverlap:       " << (lastRho - rOverlap())/lastZ * newZ << " New [ ; ]: " << (lastRho - rOverlap())/(lastZ - zHalfLength()) * (newZ - zHalfLength()) << " " << (lastRho - rOverlap())/(lastZ + zHalfLength()) * (newZ + zHalfLength()) << std::endl;
       //std::cout << ">>> yesZErr:          " << (lastRho)/lastZ * newZ              << " New [ ; ]: " << (lastRho)/(lastZ - zError() - zHalfLength()) * (newZ - zError() - zHalfLength()) << " " << (lastRho)/(lastZ + zError() + zHalfLength()) * (newZ + zError() + zHalfLength()) << std::endl;
       //
       //std::cout << ">>> ShiftZ: " << shiftZ << " ErrorZ: " << errorShiftZ << " nR: "<< nextRhoWOverlap << " nRShifted: " << nextRhoWZError << std::endl;
    }
    ring->myid(i);
    ring->build();
    ring->translateZ(parity > 0 ? bigDelta() : -bigDelta());
    rings_.push_back(ring);
    //ringIndexMap_.insert(i, ring);
    ringIndexMap_[i] = ring;

    // Keep for next calculation
    lastRho        = ring->maxR();
    lastSmallDelta = ring->smallDelta();
  }
  numRings(rings_.size());
}*/

void Disk::buildTopDown(const vector<double>& firstDiskSmallDeltas, const vector<double>& lastDiskSmallDeltas, const vector<double>& firstDiskDsDistances, const vector<double>& lastDiskDsDistances) {
  double lastRho;
  
  for (int i = numRings(), parity = -bigParity(); i > 0; i--, parity *= -1) {

    // CREATES RING
    Ring* ring = GeometryFactory::make<Ring>();
    ring->buildDirection(Ring::TOPDOWN);
    ring->store(propertyTree());
    if (ringNode.count(i) > 0) ring->store(ringNode.at(i));

    // INITIALIZATION
    if (i == numRings()) {
      ring->buildStartRadius(outerRadius());
    }
    
    // ALL THIS IS TO AUTOMATICALLY CALCULATE RING RADIUSHIGH FOR RING (i) FROM RIMG (i+1)
    // In what follows, 'innermost' corresponds to 'lower Z', and 'outermost' corresponds to 'bigger Z'.
    else {

      // 1) FIND THE Z OF THE MOST STRINGENT POINTS IN RING (i+1) AND RING (i)

      double lastZ;          // Z of the most stringent point in Ring i+1
      double newZ;           // Z of the most stringent point in Ring i
      double lastSmallDelta; // smallDelta (Ring i+1)
      double newSmalDelta;   // smallDelta (Ring i)
      double lastDsDistance; // dsDistance (Ring i+1)
      double newDsDistance;  // dsDistance (Ring i)
     
      // Case where Ring (i+1) is the innermost ring, and Ring (i) is the outermost ring.
      if (parity > 0) {
	// In this case, the innermost disk of the Endcaps block is the most stringent.
	// As a result, geometry information is taken from that disk.
	lastSmallDelta = getSmallDelta(firstDiskSmallDeltas, i+1);
	newSmallDelta = getSmallDelta(firstDiskSmallDeltas, i);
	lastDsDistance = getDsDistance(firstDiskDsDistances, i+1);
	newDsDistance = getDsDistance(firstDiskDsDistances, i);

	// Calculates Z of the most stringent points
	lastZ = buildZ() - zHalfLength() - bigDelta() - lastSmallDelta - lastDsDistance / 2.;
	newZ  = buildZ() - zHalfLength() + bigDelta() + newSmallDelta + newDsDistance / 2.;
      }

      // Case where Ring (i+1) is the outermost ring, and Ring (i) is the innermost ring.
      else {
	// In this case, the outermost disk of the Endcaps block is the most stringent.
	// As a result, geometry information is taken from that disk.
	lastSmallDelta = getSmallDelta(lastDiskSmallDeltas, i+1);
	newSmallDelta = getSmallDelta(lastDiskSmallDeltas, i);
	lastDsDistance = getDsDistance(lastDiskDsDistances, i+1);
	newDsDistance = getDsDistance(lastDiskDsDistances, i);

	// Calculates Z of the most stringent points
	lastZ = buildZ() + zHalfLength() + bigDelta() - lastSmallDelta - lastDsDistance / 2.;
        newZ  = buildZ() + zHalfLength() - bigDelta() + newSmallDelta + newDsDistance / 2.;
      }


      // 2) CALCULATES RING RADIUSHIGH FOR RING (i) FROM RING (i+1)
      
      // Case A : Consider zError
      double zError   = parity > 0 ? +zError() : -zError();
      double nextRhoWithZError   = lastRho / (lastZ - zError) * (newZ - zError);

      // Case B : Consider rOverlap
      double nextRhoWithROverlap  = (lastRho + rOverlap()) / lastZ * newZ;
      
      // Takes the most stringent of cases A and B
      double nextRho = MAX(nextRhoWithZError, nextRhoWithROverlap);

      ring->buildStartRadius(nextRho);
    }

    // NOW TAHT RADIUS HAS BEEN CALCULATED, FINISHES BUILDING THE RING AND STORE IT
    ring->myid(i);
    ring->build();
    ring->translateZ(parity > 0 ? bigDelta() : -bigDelta());
    rings_.insert(rings_.begin(), ring);
    ringIndexMap_[i] = ring;

    // Keep for next calculation
    lastRho        = ring->minR();
  }
}

void Disk::build(const vector<double>& firstDiskSmallDeltas, const vector<double>& lastDiskSmallDeltas, const vector<double>& firstDiskDsDistances, const vector<double>& lastDiskDsDistances) {
  ConversionStation* conversionStation;
  materialObject_.store(propertyTree());
  materialObject_.build();

  try {
    logINFO(Form("Building %s", fullid(*this).c_str()));
    if (numRings.state()) buildTopDown(firstDiskSmallDeltas, lastDiskSmallDeltas, firstDiskDsDistances, lastDiskDsDistances);
    //else buildBottomUp(buildDsDistances);
    translateZ(placeZ());
  } catch (PathfulException& pe) { pe.pushPath(fullid(*this)); throw; }

  for (auto& currentStationNode : stationsNode) {
    conversionStation = new ConversionStation();
    conversionStation->store(currentStationNode.second);
    conversionStation->check();
    conversionStation->build();
      
    if(conversionStation->stationType() == ConversionStation::Type::FLANGE) {
      if(flangeConversionStation_ == nullptr) { //take only first defined flange station
        flangeConversionStation_ = conversionStation;
      }
    } else if(conversionStation->stationType() == ConversionStation::Type::SECOND) {
      secondConversionStations_.push_back(conversionStation);
    }
  }

  cleanup();
  builtok(true);
}


void Disk::translateZ(double z) { averageZ_ += z; for (auto& r : rings_) r.translateZ(z); }

void Disk::mirrorZ() {
  averageZ_ = -averageZ_;
  for (auto& r : rings_) r.mirrorZ();
}

const MaterialObject& Disk::materialObject() const {
  return materialObject_;
}

ConversionStation* Disk::flangeConversionStation() const {
  return flangeConversionStation_;
}

const std::vector<ConversionStation*>& Disk::secondConversionStations() const {
  return secondConversionStations_;
}

std::vector<std::set<const Module*>> Disk::getModuleSurfaces() const {
  class SurfaceSplitterVisitor : public ConstGeometryVisitor {
  private:
    int sideIndex;
    double ringAvgZ;
    int iRing=0;
  public:
    double diskAverageZ;
    std::set<const Module*> mod[4];
    void visit(const Ring& r) {
      sideIndex = 0;
      ringAvgZ = r.averageZ();
      if (ringAvgZ>diskAverageZ) sideIndex+=2;
    }
    void visit(const Module& m) {
      int deltaSide=0;
      if (m.center().Z()>ringAvgZ) deltaSide+=1;
      mod[sideIndex+deltaSide].insert(&m);
    }
  };
  SurfaceSplitterVisitor v;
  v.diskAverageZ = averageZ_;
  this->accept(v);
  std::vector<std::set<const Module*>> result;
  for (int i=0; i<4; ++i) result.push_back(v.mod[i]);
  return result;
}
