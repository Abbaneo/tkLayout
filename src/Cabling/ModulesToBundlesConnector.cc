#include "Cabling/ModulesToBundlesConnector.hh"
#include <Tracker.hh>


void ModulesToBundlesConnector::visit(Barrel& b) {
  isBarrel_ = true;
  barrelName_ = b.myid();	
}


void ModulesToBundlesConnector::visit(Layer& l) {
  layerNumber_ = l.layerNumber();
  numRods_ = l.numRods();
  totalNumFlatRings_ = l.buildNumModulesFlat() * 2 - 1; // Total number of flat rings on both (+Z) side and (-Z) side
}


void ModulesToBundlesConnector::visit(RodPair& r) {
  bundleType_ = computeBundleType(isBarrel_, barrelName_, layerNumber_);

  rodPhi_ = r.Phi();
}
      
     
void ModulesToBundlesConnector::visit(BarrelModule& m) {
  side_ = (m.uniRef().side > 0.);

  bool isPositiveCablingSide = side_;
  bool isTilted = false;
  bool isExtraFlatPart = false;

  // 2S BUNDLES
  if (barrelName_ == "TB2S") {

    isPositiveCablingSide = side_;
    isTilted = false;
    isExtraFlatPart = false;
  }

  // PS BUNDLES
  else if (barrelName_ == "TBPS") {

    // FLAT PART
    if (!m.isTilted()) {
      double phiSegmentWidth = (2.*M_PI) / numRods_;
      isPositiveCablingSide = computeBarrelFlatPartRodCablingSide(rodPhi_, phiSegmentWidth);
      isTilted = false;
      isExtraFlatPart = false;

      // For layer 3, need to add a second bundle for flat part
      if (isPositiveCablingSide && (totalNumFlatRings_ > maxNumModulesPerBundle_) && !side_) isExtraFlatPart = true;
      if (!isPositiveCablingSide && (totalNumFlatRings_ > maxNumModulesPerBundle_) && side_) isExtraFlatPart = true;
    }

    // TILTED PART
    else if (m.isTilted()) {

      isPositiveCablingSide = side_;
      isTilted = true;
      isExtraFlatPart = false;
    }       
  }

  const PhiPosition& modulePhiPosition = PhiPosition(rodPhi_, numRods_, isPositiveCablingSide, isBarrel_, layerNumber_);

  bundleTypeIndex_ = computeBundleTypeIndex(isBarrel_, bundleType_, totalNumFlatRings_, maxNumModulesPerBundle_, isTilted, isExtraFlatPart);
  int bundleId = computeBundleId(isBarrel_, isPositiveCablingSide, layerNumber_, modulePhiPosition.phiSegmentRef(), bundleTypeIndex_);

  // BUILD BUNDLE IF NECESSARY, AND CONNECT MODULE TO BUNDLE
  buildBundle(m, bundles_, negBundles_, bundleId, bundleType_, barrelName_, layerNumber_, modulePhiPosition, isPositiveCablingSide, isTilted);
}


void ModulesToBundlesConnector::visit(Endcap& e) {
  isBarrel_ = false;
  endcapName_ = e.myid();
}


void ModulesToBundlesConnector::visit(Disk& d) {
  diskNumber_ = d.diskNumber();
  side_ = d.side();
}


void ModulesToBundlesConnector::visit(Ring& r)   { 
  ringNumber_ = r.myid();
  numModulesInRing_ = r.numModules();

  bundleType_ = computeBundleType(isBarrel_, endcapName_, diskNumber_, ringNumber_);
  bundleTypeIndex_ = computeBundleTypeIndex(isBarrel_, bundleType_);
}


void ModulesToBundlesConnector::visit(EndcapModule& m) {
  double modPhi = m.center().Phi();

  bool isPositiveCablingSide = side_;

  const PhiPosition& modulePhiPosition = PhiPosition(modPhi, numModulesInRing_, isPositiveCablingSide, isBarrel_, diskNumber_, endcapName_, bundleType_);

  int bundleId = computeBundleId(isBarrel_, isPositiveCablingSide, diskNumber_, modulePhiPosition.phiRegionRef(), bundleTypeIndex_);

  // BUILD BUNDLE IF NECESSARY, AND CONNECT MODULE TO BUNDLE
  buildBundle(m, bundles_, negBundles_, bundleId, bundleType_, endcapName_, diskNumber_, modulePhiPosition, modulePhiPosition.phiSectorRef(), isPositiveCablingSide);
}



void ModulesToBundlesConnector::postVisit() {
  // STAGGER MODULES
  staggerModules(bundles_);
  staggerModules(negBundles_);

  // CHECK
  checkModulesToBundlesCabling(bundles_);
  checkModulesToBundlesCabling(negBundles_);
}


bool ModulesToBundlesConnector::computeBarrelFlatPartRodCablingSide(const double rodPhi, const double phiSegmentWidth) {
  double phiSegmentStartOneCablingSide = computePhiSegmentStart(rodPhi, phiSegmentWidth, true);
  int phiSegmentRefOneCablingSide = computePhiSegmentRef(rodPhi, phiSegmentStartOneCablingSide, phiSegmentWidth, true);  
  bool isPositiveCablingSide = ((phiSegmentRefOneCablingSide % 2) == 1);
  return isPositiveCablingSide;
}


std::string ModulesToBundlesConnector::computeBundleType(const bool isBarrel, const std::string subDetectorName, const int layerDiskNumber, const int ringNumber) const {
  std::string bundleType;

  if (isBarrel) {
    if (subDetectorName == "TB2S") {
      bundleType = "2S";
    }
    else if (subDetectorName == "TBPS") {
      bundleType = (layerDiskNumber == 1 ? "PS10G" : "PS5G");
    }
  }

  else {
    if (subDetectorName == "TEDD_1") {
      if (ringNumber <= 4) bundleType = "PS10G";
      else if (ringNumber >= 5 && ringNumber <= 7) bundleType = "PS5GA";
      else if (ringNumber >= 8 && ringNumber <= 10) bundleType = "PS5GB";
      else if (ringNumber >= 11) bundleType = "2S";
    }

    else if (subDetectorName == "TEDD_2") {
      if (ringNumber <= 3) bundleType = "null";
      else if (ringNumber >= 4 && ringNumber <= 6) bundleType = "PS5GA";
      else if (ringNumber >= 7 && ringNumber <= 10) bundleType = "PS5GB";
      else if (ringNumber >= 11) bundleType = "2S";
    }
  }

  return bundleType;
}


int ModulesToBundlesConnector::computeBundleTypeIndex(const bool isBarrel, const std::string bundleType, const int totalNumFlatRings, const int maxNumModulesPerBundle, const bool isTilted, const bool isExtraFlatPart) const {
  int bundleTypeIndex;
  if (isBarrel) {
    if (bundleType == "2S") bundleTypeIndex = 0;
    else {
      if (!isTilted) {
	if (totalNumFlatRings <= maxNumModulesPerBundle) bundleTypeIndex = 1;
	else {
	  if (!isExtraFlatPart) bundleTypeIndex = 1;
	  else bundleTypeIndex = 2;
	}
      } 
      else bundleTypeIndex = 0;
    }
  }
  else {
    if (bundleType == "PS10G") bundleTypeIndex = 0;
    else if (bundleType == "PS5GA") bundleTypeIndex = 1;
    else if (bundleType == "PS5GB") bundleTypeIndex = 2;
    else if (bundleType == "2S") bundleTypeIndex = 3;
  }
  return bundleTypeIndex;
}


int ModulesToBundlesConnector::computeBundleId(const bool isBarrel, const bool isPositiveCablingSide, const int layerDiskNumber, const int phiRef, const int bundleTypeIndex) const {
  int cablingSideIndex = 0;
  if (isBarrel) {
    cablingSideIndex = (isPositiveCablingSide ? 1 : 3);
  }
  else {
    cablingSideIndex = (isPositiveCablingSide ? 2 : 4);
  }

  int bundleId = cablingSideIndex * 10000 + layerDiskNumber * 1000 + phiRef * 10 + bundleTypeIndex;
  return bundleId;
}


void ModulesToBundlesConnector::buildBundle(DetectorModule& m, std::map<int, Bundle*>& bundles, std::map<int, Bundle*>& negBundles, const int bundleId, const std::string bundleType, const std::string subDetectorName, const int layerDiskNumber, const PhiPosition& modulePhiPosition, const bool isPositiveCablingSide, const bool isTiltedPart) {
  
  std::map<int, Bundle*>& stereoBundles = (isPositiveCablingSide ? bundles : negBundles);

  Bundle* bundle = nullptr;
  auto found = stereoBundles.find(bundleId);
  if (found == stereoBundles.end()) {
    bundle = createAndStoreBundle(bundles, negBundles, bundleId, bundleType, subDetectorName, layerDiskNumber, modulePhiPosition, isPositiveCablingSide, isTiltedPart);
  }
  else {
    bundle = found->second;
  }

  connectModuleToBundle(m, bundle);
}


Bundle* ModulesToBundlesConnector::createAndStoreBundle(std::map<int, Bundle*>& bundles, std::map<int, Bundle*>& negBundles, const int bundleId, const std::string bundleType, const std::string subDetectorName, const int layerDiskNumber, const PhiPosition& modulePhiPosition, const bool isPositiveCablingSide, const bool isTiltedPart) {

  Bundle* bundle = GeometryFactory::make<Bundle>(bundleId, bundleType, subDetectorName, layerDiskNumber, modulePhiPosition, isPositiveCablingSide, isTiltedPart);

  if (isPositiveCablingSide) {
    bundles.insert(std::make_pair(bundleId, bundle));
  }
  else {
    negBundles.insert(std::make_pair(bundleId, bundle));
  }
  return bundle;
}


void ModulesToBundlesConnector::connectModuleToBundle(DetectorModule& m, Bundle* bundle) const {
  bundle->addModule(&m);
  m.setBundle(bundle);
}
 

void ModulesToBundlesConnector::staggerModules(std::map<int, Bundle*>& bundles) {

  for (auto& b : bundles) {
    if (b.second->subDetectorName() == "TEDD_1" || b.second->subDetectorName() == "TEDD_2") {
      const bool isBarrel = false;

      while (b.second->numModules() > maxNumModulesPerBundle_) {
	const bool isPositiveCablingSide = b.second->isPositiveCablingSide();
	const int diskNumber = b.second->layerDiskNumber();

	const std::string bundleType = b.second->type();
	const int bundleTypeIndex = computeBundleTypeIndex(isBarrel, bundleType);

	const PhiPosition& bundlePhiPosition = b.second->phiPosition();

	const double phiRegionStart = bundlePhiPosition.phiRegionStart();

	const double phiRegionWidth = bundlePhiPosition.phiRegionWidth();
	const int numPhiRegions = round(2 * M_PI / phiRegionWidth);

	const int phiRegionRef = bundlePhiPosition.phiRegionRef();
	const int nextPhiRegionRef = computeNextPhiSliceRef(phiRegionRef, numPhiRegions);
	const int previousPhiRegionRef = computePreviousPhiSliceRef(phiRegionRef, numPhiRegions);

	const int bundleId = b.first;
	const int nextBundleId = computeBundleId(isBarrel, isPositiveCablingSide, diskNumber, nextPhiRegionRef, bundleTypeIndex);
	const int previousBundleId = computeBundleId(isBarrel, isPositiveCablingSide, diskNumber, previousPhiRegionRef, bundleTypeIndex);

	const double minPhiBorder = fabs( femod((b.second->minPhi() - phiRegionStart), phiRegionWidth) );
	const double maxPhiBorder = fabs( femod((b.second->maxPhi() - phiRegionStart), phiRegionWidth) - phiRegionWidth);
	      

	auto previousBundleSearch = bundles.find(previousBundleId);
	auto nextBundleSearch = bundles.find(nextBundleId);
	if (previousBundleSearch != bundles.end() && nextBundleSearch != bundles.end()) {
	  Bundle* previousBundle = previousBundleSearch->second;
	  Bundle* nextBundle = nextBundleSearch->second;

	  const int previousBundleNumModules = previousBundle->numModules();
	  const int nextBundleNumModules = nextBundle->numModules();
	  // Cannot assign the extra module : both neighbouring phi regions are full !
	  if (previousBundleNumModules >= maxNumModulesPerBundle_ && nextBundleNumModules >= maxNumModulesPerBundle_) {
	    logERROR(any2str("Building cabling map : Staggering modules.")
		     + "I am a module in side " + any2str(isPositiveCablingSide)
		     + ", disk " + any2str(diskNumber)
		     + ", bundleType " + any2str(bundleType) 
		     + ", phiRegionRef " + any2str(phiRegionRef)
		     + ", phiRegionWidth " + any2str(phiRegionWidth)
		     + ". My phiRegion has more than " + any2str(maxNumModulesPerBundle_) + " modules."
		     + " My 2 neighbouring phiRegions have also more than " + any2str(maxNumModulesPerBundle_) + " modules."
		     + " Hence, I cannot be staggered to any other phiRegion :/ "
		     );
	    break;
	  }

	  // Assign a module to the next phi region
	  else if (previousBundleNumModules >= maxNumModulesPerBundle_ || maxPhiBorder <= minPhiBorder) {
	    logINFO(any2str("Building cabling map : Staggering modules.")
		    + " I am a module in side " + any2str(isPositiveCablingSide)
		    + ", disk " + any2str(diskNumber)
		    + ", bundleType " + any2str(bundleType)
		    + ", phiRegionRef " + any2str(phiRegionRef)
		    + ". maxPhiBorder " + any2str((maxPhiBorder * 180. / M_PI))
		    + ". My region has " + any2str(b.second->numModules())
		    + " > maxNumModulesPerBundle = " + any2str(maxNumModulesPerBundle_)
		    + ". I am moved to the next phiRegion, which presently has " + any2str(nextBundleNumModules) + " modules."
		    );
		    
	    Module* maxPhiMod = b.second->maxPhiModule();
	    maxPhiMod->setBundle(nextBundle);  
	    nextBundle->moveMaxPhiModuleFromOtherBundle(b.second);
	  }

	  // Assign a module to the previous phi region
	  else if (nextBundleNumModules >= maxNumModulesPerBundle_ || minPhiBorder < maxPhiBorder) {
	    logINFO(any2str("Building cabling map : Staggering modules.")
		    + " I am a module in side " + any2str(isPositiveCablingSide)
		    + ", disk " + any2str(diskNumber)
		    + ", bundleType " + any2str(bundleType)
		    + ", phiRegionRef " + any2str(phiRegionRef)
		    + ". minPhiBorder " + any2str((minPhiBorder * 180. / M_PI))
		    + ". My region has " + any2str(b.second->numModules())
		    + " > maxNumModulesPerBundle = " + any2str(maxNumModulesPerBundle_)
		    + ". I am moved to the previous phiRegion, which presently has " + any2str(previousBundleNumModules) + " modules."
		    );

	    Module* minPhiMod = b.second->minPhiModule();
	    minPhiMod->setBundle(previousBundle);	  
	    previousBundle->moveMinPhiModuleFromOtherBundle(b.second);
	  }
	}
	else { 
	  logERROR(any2str("Building cabling map : Staggering modules.")
		   + "Error building previousBundleId or nextBundleId"); 
	  break; 
	}

      }
    }
  }

}


void ModulesToBundlesConnector::checkModulesToBundlesCabling(const std::map<int, Bundle*>& bundles) const {
  for (auto& b : bundles) {

    const PhiPosition& bundlePhiPosition = b.second->phiPosition();
    const int phiSegmentRef = bundlePhiPosition.phiSegmentRef();
    const int phiRegionRef = bundlePhiPosition.phiRegionRef();
    const int phiSectorRef = bundlePhiPosition.phiSectorRef();
    if (phiSegmentRef <= -1 || phiRegionRef <= -1 || phiSectorRef <= -1) {
      logERROR(any2str("Building cabling map : a bundle was not correctly created. ")
	       + "Bundle " + any2str(b.first) + ", with bundleType = " + any2str(b.second->type()) 
	       + ", has phiSegmentRef = " + any2str(phiSegmentRef)
	       + ", phiRegionRef = " + any2str(phiRegionRef)
	       + ", phiSectorRef = " + any2str(phiSectorRef)
	       );
    }

    const int bundleNumModules = b.second->numModules();
    if (bundleNumModules > maxNumModulesPerBundle_) {
      logERROR(any2str("Building cabling map : Staggering modules. ")
	       + "Bundle "  + any2str(b.first) + " is connected to " + any2str(bundleNumModules) + " modules."
	       + "Maximum number of modules per bundle allowed is " + any2str(maxNumModulesPerBundle_)
	       );
    }

  }
}
