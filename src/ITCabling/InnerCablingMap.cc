#include "ITCabling/InnerCablingMap.hh"
#include <Tracker.hh>


InnerCablingMap::InnerCablingMap(Tracker* tracker) {
  try {
    // CONNECT MODULES TO SERIAL POWER CHAINS
    connectModulesToPowerChains(tracker);

    // CONNECT MODULES TO E-LINKS
    connectModulesToELinks(tracker); // TO DO: remove class ELINK !! Just put numElinksPerModule in Module class thanks to visitor which is already there.

    // CONNECT ELINKS TO LPGBTS
    connectELinksToGBTs(powerChains_, GBTs_);
    
    // CONNECT LPGBTS TO BUNDLES
    connectGBTsToBundles(GBTs_, bundles_);

    // CONNECT BUNDLES TO DTCs
    connectBundlesToDTCs(bundles_, DTCs_);

    // COMPUTE SERVICES CHANNELS ASSIGNMENTS OF POWER CABLES
    //computePowerServicesChannels();
  }

  catch (PathfulException& pe) { pe.pushPath(fullid(*this)); throw; }
}


/* MODULES TO SERIAL POWER CHAINS CONNECTIONS.
 */
void InnerCablingMap::connectModulesToPowerChains(Tracker* tracker) {
  ModulesToPowerChainsConnector powerChainsBuilder;
  tracker->accept(powerChainsBuilder);
  powerChainsBuilder.postVisit();
  powerChains_ = powerChainsBuilder.getPowerChains();
}


/* MODULES TO ELINKS CONNECTIONS.
 */
void InnerCablingMap::connectModulesToELinks(Tracker* tracker) {
  ModulesToELinksConnector eLinksBuilder;
  tracker->accept(eLinksBuilder);
  //eLinksBuilder.postVisit();
  eLinks_ = eLinksBuilder.getELinks();
}


/* ELINKS TO GBTS CONNECTIONS.
 */
void InnerCablingMap::connectELinksToGBTs(std::map<int, PowerChain*>& powerChains, std::map<std::string, GBT*>& GBTs) {

  for (auto& it : powerChains) {
    // COLLECT ALL INFORMATION NEEDED TO BUILD GBTS
    
    PowerChain* myPowerChain = it.second;

    const bool isBarrel = myPowerChain->isBarrel();
    const std::string subDetectorName = myPowerChain->subDetectorName();
    const int layerNumber = myPowerChain->layerDiskNumber();
    const int ringNumber = myPowerChain->ringNumber();
    const int numModulesInPowerChain = myPowerChain->numModules();

    const int layerOrRingNumber = (isBarrel ? layerNumber : ringNumber);
    const int numELinksPerModule = inner_cabling_functions::computeNumELinksPerModule(subDetectorName, layerOrRingNumber);

    const std::pair<int, int> gbtsInPowerChain = computeMaxNumModulesPerGBTInPowerChain(numELinksPerModule, numModulesInPowerChain, isBarrel);
    const int maxNumModulesPerGBTInPowerChain = gbtsInPowerChain.first;
    const int numGBTsInPowerChain = gbtsInPowerChain.second;
    

    for (auto& m : myPowerChain->modules()) {
      const int powerChainId = myPowerChain->myid();
      const bool isBarrelLong = myPowerChain->isBarrelLong();
      const int ringRef = (isBarrelLong ? m.uniRef().ring - 1 : m.uniRef().ring - 2);
      const int phiRefInPowerChain = m.getPhiRefInPowerChain();
      //std::cout << "phiRefInPowerChain = " << phiRefInPowerChain << std::endl;
      
      const int myGBTPhiIndex = computeGBTPhiIndex(isBarrel, ringRef, phiRefInPowerChain, maxNumModulesPerGBTInPowerChain, numGBTsInPowerChain);
      const std::string myGBTId = computeGBTId(powerChainId, myGBTPhiIndex);

      // BUILD GBTS AND STORE THEM
      createAndStoreGBTs(myPowerChain, m, myGBTId, myGBTPhiIndex, numELinksPerModule, GBTs);
    }
  }

  // CHECK GBTS
  checkModulesToGBTsCabling(GBTs);
}



// MODULES TO GBTS !!!!!

const std::pair<int, int> InnerCablingMap::computeMaxNumModulesPerGBTInPowerChain(const int numELinksPerModule, const int numModulesInPowerChain, const bool isBarrel) {
  //std::cout << "STARTTTTTTTTTTTTTTT InnerCablingMap::computeMaxNumModulesPerGBTInPowerChain  " << " numELinksPerModule = " <<  numELinksPerModule << ", numModulesInPowerChain = " << numModulesInPowerChain << ", isBarrel = " << isBarrel << std::endl;

  int numModules = numModulesInPowerChain;
  if (isBarrel) {
    if (numModules % 2 == 1) logERROR(any2str("Found an odd number of modules in BPIX power chain, which is not supported."));
    else numModules /= 2;  // Divide by 2 because in BPIX, the GBTs assignment works by rod.
                           // This is becasue it makes the powering of the GBTs much easier.
  }
  //std::cout << "numModules = " << numModules << std::endl;

  const int numELinks = numELinksPerModule * numModules;
  //std::cout << "numELinks = " << numELinks << std::endl;

  const double numGBTsExact = static_cast<double>(numELinks) / inner_cabling_maxNumELinksPerGBT;
  //std::cout << "numGBTsExact = " << numGBTsExact << std::endl;
  const int numGBTs = (fabs(numGBTsExact - round(numGBTsExact)) < inner_cabling_roundingTolerance ? 
		       round(numGBTsExact) 
		       : std::ceil(numGBTsExact)
		       );

  if (numGBTs == 0) logERROR(any2str("Power chain has ") + any2str(numModules) 
			     + any2str(" modules, but found numGBTs == ") +  any2str(numGBTs) + any2str(", that's not enough!!")
			     );

  const double maxNumModulesPerGBTExact = static_cast<double>(numModules) / numGBTs;
  const int maxNumModulesPerGBTInPowerChain = (fabs(maxNumModulesPerGBTExact - round(maxNumModulesPerGBTExact)) < inner_cabling_roundingTolerance ? 
					       round(maxNumModulesPerGBTExact) 
					       : std::ceil(maxNumModulesPerGBTExact)
					       );
  // TO DO: MINOR ISSUE OF TAKING THE ceil IS THAT it doesnt handle optimally the (RARE) case where we have 3 + 2 + 2
  // Since it will lead to following repartition: 3 + 3 + 1
  // One could take the floor, and then look at the number of remaining modules in power chain.
  // This case does not appear in practice, but can be worked on if needed.

  //std::cout << "numGBTs = " << numGBTs << std::endl;
  //std::cout << "maxNumModulesPerGBTInPowerChain = " << maxNumModulesPerGBTInPowerChain << std::endl;

  return std::make_pair(maxNumModulesPerGBTInPowerChain, numGBTs);
}


/* Compute the phi index associated to each GBT.
 */
const int InnerCablingMap::computeGBTPhiIndex(const bool isBarrel, const int ringRef, const int phiRefInPowerChain, const int maxNumModulesPerGBTInPowerChain, const int numGBTsInPowerChain) const {
  //std::cout << "InnerCablingMap::computeGBTPhiIndex " << std::endl;

  const int moduleRef = (isBarrel ? ringRef : phiRefInPowerChain);
  //std::cout << "moduleRef = " << moduleRef << std::endl;

  if (maxNumModulesPerGBTInPowerChain == 0) logERROR(any2str("Found maxNumModulesPerGBTInPowerChain == 0."));

  const double myGBTIndexExact = static_cast<double>(moduleRef) / maxNumModulesPerGBTInPowerChain;
  //std::cout << "myGBTIndexExact = " << myGBTIndexExact << std::endl;
  int myGBTIndex = (fabs(myGBTIndexExact - round(myGBTIndexExact)) < inner_cabling_roundingTolerance ? 
		    round(myGBTIndexExact) 
		    : std::floor(myGBTIndexExact)
		    );
  if (isBarrel && phiRefInPowerChain == 1) myGBTIndex += numGBTsInPowerChain;

  //std::cout << "myGBTIndex = " << myGBTIndex << std::endl;

  return myGBTIndex;
}


/* Compute the Id associated to each GBT.
 */
const std::string InnerCablingMap::computeGBTId(const int powerChainId, const int myGBTIndex) const {
  std::ostringstream GBTIdStream;
  GBTIdStream << powerChainId << "_" << myGBTIndex;
  const std::string GBTId = GBTIdStream.str();
  return GBTId;
}


/* Create a GBT, if does not exist yet.
 * Store it in the GBTs container.
 */
void InnerCablingMap::createAndStoreGBTs(PowerChain* myPowerChain, Module& m, const std::string myGBTId, const int myGBTPhiIndex, const int numELinksPerModule, std::map<std::string, GBT*>& GBTs) {

  auto found = GBTs.find(myGBTId);
  if (found == GBTs.end()) {
    GBT* myGBT = GeometryFactory::make<GBT>(myPowerChain, myGBTId, myGBTPhiIndex, numELinksPerModule);
    connectOneModuleToOneGBT(m, myGBT);
    GBTs.insert(std::make_pair(myGBTId, myGBT));  
  }
  else {
    connectOneModuleToOneGBT(m, found->second);
  }
}


/* Connect module to GBT and vice-versa.
 */
void InnerCablingMap::connectOneModuleToOneGBT(Module& m, GBT* myGBT) const {
  myGBT->addModule(&m);
  m.setGBT(myGBT);
}


/* Check bundles-cables connections.
 */

void InnerCablingMap::checkModulesToGBTsCabling(const std::map<std::string, GBT*>& GBTs) const {
  for (const auto& it : GBTs) {
    const std::string myGBTId = it.first;
    const GBT* myGBT = it.second;

    // CHECK THE NUMBER OF ELINKS PER GBT.
    const int numELinks = myGBT->numELinks();

    if (numELinks > inner_cabling_maxNumELinksPerGBT) {
      logERROR(any2str("GBT ")  + any2str(myGBTId) + any2str(" is connected to ") + any2str(numELinks) + any2str(" ELinks. ")
	       + "Maximum number of ELinks per GBT allowed is " + any2str(inner_cabling_maxNumELinksPerGBT)
	       );
    }
  }
}






// GBTs to BUNDLES !!!!


/* GBTS TO BUNDLES CONNECTIONS.
 */
void InnerCablingMap::connectGBTsToBundles(std::map<std::string, GBT*>& GBTs, std::map<int, InnerBundle*>& bundles) {

  for (auto& it : GBTs) {
    // COLLECT ALL INFORMATION NEEDED TO BUILD BUNDLES   
    GBT* myGBT = it.second;

    const std::string subDetectorName = myGBT->subDetectorName();
    
    const int layerDiskNumber = myGBT->layerDiskNumber();
    const int powerChainPhiRef = myGBT->powerChainPhiRef();
    const bool isRingInnerEnd = myGBT->isRingInnerEnd();

    std::cout << "subDetectorName = " << subDetectorName << std::endl;
    std::cout << "layerDiskNumber = " << layerDiskNumber << std::endl;
    std::cout << "powerChainPhiRef = " << powerChainPhiRef << std::endl;
        
    const int myBundleIndex = computeBundleIndex(subDetectorName, layerDiskNumber, powerChainPhiRef, isRingInnerEnd);

    const bool isPositiveZEnd = myGBT->isPositiveZEnd();
    const bool isPositiveXSide = myGBT->isPositiveXSide();
    const int myBundleId = computeBundleId(isPositiveZEnd, isPositiveXSide, subDetectorName, layerDiskNumber, myBundleIndex);

    // BUILD BUNDLES AND STORE THEM
    createAndStoreBundles(myGBT, bundles, myBundleId, isPositiveZEnd, isPositiveXSide, subDetectorName, layerDiskNumber, myBundleIndex);    
  }

  // CHECK BUNDLES
  checkGBTsToBundlesCabling(bundles);
}


const int InnerCablingMap::computeBundleIndex(const std::string subDetectorName, const int layerNumber, const int powerChainPhiRef, const bool isRingInnerEnd) const {
  int myBundleIndex = 0;

  if (subDetectorName == inner_cabling_tbpx) {

    // TO DO: THIS SHOULD BE COMPUTED AS A FUNCTION OF LAYER NUMBER, NOT HARCODED!!!
    const int maxNumPowerChainsPerBundleBarrelLayer1 = 1;
    const int maxNumPowerChainsPerBundleBarrelLayer2 = 3;
    const int maxNumPowerChainsPerBundleBarrelLayer3 = 3;
    const int maxNumPowerChainsPerBundleBarrelLayer4 = 4;

    int maxNumPowerChainsPerBundleBarrelLayer = 0;
    if (layerNumber == 1) maxNumPowerChainsPerBundleBarrelLayer = maxNumPowerChainsPerBundleBarrelLayer1;
    else if (layerNumber == 2) maxNumPowerChainsPerBundleBarrelLayer = maxNumPowerChainsPerBundleBarrelLayer2;
    else if (layerNumber == 3) maxNumPowerChainsPerBundleBarrelLayer = maxNumPowerChainsPerBundleBarrelLayer3;
    else if (layerNumber == 4) maxNumPowerChainsPerBundleBarrelLayer = maxNumPowerChainsPerBundleBarrelLayer4;
    else logERROR("Did not find supported layer number.");

    if (maxNumPowerChainsPerBundleBarrelLayer == 0) logERROR(any2str("Found maxNumPowerChainsPerBundleBarrelLayer == 0."));

    std::cout << "maxNumPowerChainsPerBundleBarrelLayer = " << maxNumPowerChainsPerBundleBarrelLayer << std::endl;

    const double myBundleIndexExact = static_cast<double>(powerChainPhiRef) / maxNumPowerChainsPerBundleBarrelLayer;
    int myBundleIndex = (fabs(myBundleIndexExact - round(myBundleIndexExact)) < inner_cabling_roundingTolerance ? 
			 round(myBundleIndexExact) 
			 : std::floor(myBundleIndexExact)
			 );
    std::cout << "myBundleIndexExact = " << myBundleIndexExact << std::endl;
    std::cout << "myBundleIndex = " << myBundleIndex << std::endl;
  }
  else if (subDetectorName == inner_cabling_tfpx || subDetectorName == inner_cabling_tepx) {
    myBundleIndex = (isRingInnerEnd ? 0 : 1);
  }
  else logERROR("Unsupported detector name.");

  return myBundleIndex;
}


/* Compute the Id associated to each bundle.
 */
const int InnerCablingMap::computeBundleId(const bool isPositiveZEnd, const bool isPositiveXSide, const std::string subDetectorName, const int layerDiskNumber, const int myBundleIndex) const {

  const int innerTrackerQuarterIndex = inner_cabling_functions::computeInnerTrackerQuarterIndex(isPositiveZEnd, isPositiveXSide);
  const int subdetectorIndex = inner_cabling_functions::computeSubDetectorIndex(subDetectorName);

  const int bundleId = innerTrackerQuarterIndex * 1000 + subdetectorIndex * 100 + layerDiskNumber * 10 + myBundleIndex;
  return bundleId;
}


void InnerCablingMap::createAndStoreBundles(GBT* myGBT, std::map<int, InnerBundle*>& bundles, const int bundleId, const bool isPositiveZEnd, const bool isPositiveXSide, const std::string subDetectorName, const int layerDiskNumber, const int myBundleIndex) {

  auto found = bundles.find(bundleId);
  if (found == bundles.end()) {
    InnerBundle* myBundle = GeometryFactory::make<InnerBundle>(bundleId, isPositiveZEnd, isPositiveXSide, subDetectorName, layerDiskNumber, myBundleIndex);
    connectOneGBTToOneBundle(myGBT, myBundle);
    bundles.insert(std::make_pair(bundleId, myBundle));  
  }
  else {
    connectOneGBTToOneBundle(myGBT, found->second);
  }
}


/* Connect GBT to Bundle and vice-versa.
 */
void InnerCablingMap::connectOneGBTToOneBundle(GBT* myGBT, InnerBundle* myBundle) const {
  myBundle->addGBT(myGBT);
  myGBT->setBundle(myBundle);
}


/* Check GBTs-Bundle connections.
 */

void InnerCablingMap::checkGBTsToBundlesCabling(const std::map<int, InnerBundle*>& bundles) const {
  for (const auto& it : bundles) {
    const int myBundleId = it.first;
    const InnerBundle* myBundle = it.second;

    // CHECK THE NUMBER OF GBTs per Bundle
    const int numGBTs = myBundle->numGBTs();

    if (numGBTs > inner_cabling_maxNumGBTsPerBundle) {
      logERROR(any2str("InnerBundle ")  + any2str(myBundleId) + any2str(" is connected to ") + any2str(numGBTs) + any2str(" GBTs. ")
	       + "Maximum number of GBTs per Bundle allowed is " + any2str(inner_cabling_maxNumGBTsPerBundle)
	       );
    }
  }
}








// BUNDLES TO DTCS !!!!!!

void InnerCablingMap::connectBundlesToDTCs(std::map<int, InnerBundle*>& bundles, std::map<int, InnerDTC*>& DTCs) {

 for (auto& it : bundles) {
    // COLLECT ALL INFORMATION NEEDED TO BUILD DTCS   
    InnerBundle* myBundle = it.second;

    const std::string subDetectorName = myBundle->subDetectorName();   
    const int layerDiskNumber = myBundle->layerDiskNumber();
        
    const int myDTCId = computeDTCId(subDetectorName, layerDiskNumber);

    // BUILD DTCS AND STORE THEM
    createAndStoreDTCs(myBundle, DTCs, myDTCId);    
  }

  // CHECK DTCS
  checkBundlesToDTCsCabling(DTCs);
}


/* Compute the Id associated to each DTC.
 */
const int InnerCablingMap::computeDTCId(const std::string subDetectorName, const int layerDiskNumber) const {
  int myDTCId = 0;

  if (subDetectorName == inner_cabling_tbpx) myDTCId = layerDiskNumber;

  else if (subDetectorName == inner_cabling_tfpx) {
    if (layerDiskNumber == 1) myDTCId = 5;
    else if (layerDiskNumber == 2) myDTCId = 4;
    else if (layerDiskNumber == 3) myDTCId = 5;
    else if (layerDiskNumber == 4) myDTCId = 3;
    else if (layerDiskNumber == 5) myDTCId = 5;
    else if (layerDiskNumber == 6) myDTCId = 4;
    else if (layerDiskNumber == 7) myDTCId = 2;
    else if (layerDiskNumber == 8) myDTCId = 1;
    else logERROR(any2str("Unexpected diskNumber in FPX : ") + any2str(layerDiskNumber));
  }

  else if (subDetectorName == inner_cabling_tepx) {
    if (layerDiskNumber == 1) myDTCId = 6;
    else if (layerDiskNumber == 2) myDTCId = 7;
    else if (layerDiskNumber == 3) myDTCId = 6;
    else if (layerDiskNumber == 4) myDTCId = 7;
    else logERROR(any2str("Unexpected diskNumber in EPX : ") + any2str(layerDiskNumber));
  }

  return myDTCId;
}


void InnerCablingMap::createAndStoreDTCs(InnerBundle* myBundle, std::map<int, InnerDTC*>& DTCs, const int DTCId) {

  auto found = DTCs.find(DTCId);
  if (found == DTCs.end()) {
    InnerDTC* myDTC = GeometryFactory::make<InnerDTC>(DTCId);
    connectOneBundleToOneDTC(myBundle, myDTC);
    DTCs.insert(std::make_pair(DTCId, myDTC));  
  }
  else {
    connectOneBundleToOneDTC(myBundle, found->second);
  }
}


/* Connect Bundle to DTC and vice-versa.
 */
void InnerCablingMap::connectOneBundleToOneDTC(InnerBundle* myBundle, InnerDTC* myDTC) const {
  myDTC->addBundle(myBundle);
  myBundle->setDTC(myDTC);
}


/* Check Bundles-DTC connections.
 */

void InnerCablingMap::checkBundlesToDTCsCabling(const std::map<int, InnerDTC*>& DTCs) const {
  for (const auto& it : DTCs) {
    const int myDTCId = it.first;
    const InnerDTC* myDTC = it.second;

    // CHECK THE NUMBER OF Bundles per DTC
    const int numBundles = myDTC->numBundles();

    if (numBundles > inner_cabling_maxNumBundlesPerCable) {
      logERROR(any2str("InnerDTC ")  + any2str(myDTCId) + any2str(" is connected to ") + any2str(numBundles) + any2str(" Bundles. ")
	       + "Maximum number of Bundles per DTC allowed is " + any2str(inner_cabling_maxNumBundlesPerCable)
	       );
    }
  }
}








/* BUNDLES TO CABLES CONNECTIONS.
 */
/*
void InnerCablingMap::connectBundlesToCables(std::map<int, Bundle*>& bundles, std::map<int, Cable*>& cables, std::map<const std::string, const DTC*>& DTCs) {

  for (auto& b : bundles) {
    // COLLECT ALL INFORMATION NEEDED TO BUILD CABLES
    const double phiSectorWidth = b.second->phiPosition().phiSectorWidth();

    const Category& bundleType = b.second->type();
    const Category& cableType = computeCableType(bundleType);

    const int bundleId = b.first;
    std::map<int, std::pair<int, int> > cablesPhiSectorRefAndSlot = computeCablesPhiSectorRefAndSlot(bundles);
    const int cablePhiSectorRef = cablesPhiSectorRefAndSlot.at(bundleId).first;
    const int slot = cablesPhiSectorRefAndSlot.at(bundleId).second;

    const int cableTypeIndex = computeCableTypeIndex(cableType);
    bool isPositiveInnerCablingSide = b.second->isPositiveCablingSide();
    const int cableId = computeCableId(cablePhiSectorRef, cableTypeIndex, slot, isPositiveCablingSide);

    // BUILD CABLES and DTCS AND STORE THEM
    createAndStoreCablesAndDTCs(b.second, cables, DTCs, cableId, phiSectorWidth, cablePhiSectorRef, cableType, slot, isPositiveCablingSide);
  }

  // CHECK CABLES
  checkBundlesToCablesCabling(cables);
}
*/

/* Compute cabling type associated to cable.
 */
/*
const Category InnerCablingMap::computeCableType(const Category& bundleType) const {
 Category cableType = bundleType;
 if (bundleType == Category::PS10GA || bundleType == Category::PS10GB) cableType = Category::PS10G;
 return cableType;
}
*/

/* Compute phiSectorRef and slot.
 * Per phiSector, there are several DTCS.
 * 1 slot = 1 DTC.
 * A staggering of bundles is done on the fly, when the number of bundles per cable is too high.
 */
/*
const std::map<int, std::pair<int, int> > CablingMap::computeCablesPhiSectorRefAndSlot(const std::map<int, Bundle*>& bundles) const {
  std::map<int, std::pair<int, int> > cablesPhiSectorRefAndSlot;

  // Used to stagger several bundles
  std::map<int, int> Layer3FlatPhiSectorsCounter;
  std::map<int, int> Layer3TiltedPhiSectorsCounter;
  std::map<int, int> Layer4PhiSectorsCounter;
  std::map<int, int> Layer5PhiSectorsCounter;
  std::map<int, int> Layer6PhiSectorsCounter;

  for (const auto& b : bundles) {
    const Bundle* myBundle = b.second;

    // COLLECT RELEVANT INFO
    const PhiPosition& bundlePhiPosition = myBundle->phiPosition();
    const int phiSegmentRef = bundlePhiPosition.phiSegmentRef(); 
    const double phiSectorWidth = bundlePhiPosition.phiSectorWidth();
    const int phiSectorRef = bundlePhiPosition.phiSectorRef();  
   
    const int numPhiSectors = round(2 * M_PI / phiSectorWidth);
    const int nextPhiSectorRef = computeNextPhiSliceRef(phiSectorRef, numPhiSectors);
    const int previousPhiSectorRef = computePreviousPhiSliceRef(phiSectorRef, numPhiSectors);

    const Category& bundleType = myBundle->type();
    const Category& cableType = computeCableType(bundleType);

    const std::string subDetectorName = myBundle->subDetectorName();
    const int layerDiskNumber = myBundle->layerDiskNumber();

    const bool isPositiveCablingSide = myBundle->isPositiveCablingSide();

    // by default
    int cablePhiSectorRef = phiSectorRef;
    int slot = 0;

    // PS10G
    if (cableType == Category::PS10G) {
      // BARREL FLAT PART + ENDCAPS DISKS 1, 3, 5
      if ((subDetectorName == cabling_tbps && !myBundle->isTiltedPart()) || (subDetectorName == cabling_tedd1 && layerDiskNumber == 1) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 3) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 5)) {
	slot = 1;
      }
      // BARREL TILTED PART + ENDCAPS DISKS 2, 4
      if ((subDetectorName == cabling_tbps && myBundle->isTiltedPart()) || (subDetectorName == cabling_tedd1 && layerDiskNumber == 2) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 4)) {
	slot = 2;
      }
    }

    // PS5G
    else if (cableType == Category::PS5G) {
      if (subDetectorName == cabling_tbps && layerDiskNumber == 2) {
	slot = 3;
      }

      else if ((subDetectorName == cabling_tedd1 && layerDiskNumber == 1) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 3) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 5)) {
	slot = 4;
      }

      // STAGGERING
      else if ( (subDetectorName == cabling_tbps && layerDiskNumber == 3) || (subDetectorName == cabling_tedd1 && layerDiskNumber == 2) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 4) ) {
	// TBPS
	if (subDetectorName == cabling_tbps) {
	  // TILTED PART
	  if (myBundle->isTiltedPart()) {
	    int& myPhiSectorCounter = Layer3TiltedPhiSectorsCounter[phiSectorRef];
	    myPhiSectorCounter += 1;
	    // In case already 4 bundles from tilted part, assign to next phi Sector
	    if (myPhiSectorCounter > 4) {
	      myPhiSectorCounter -= 1;
	      Layer3TiltedPhiSectorsCounter[nextPhiSectorRef] += 1;
	      cablePhiSectorRef = nextPhiSectorRef;
	    }
	    slot = 6;
	  }
	  // FLAT PART : assign TBPS bundles with TEDD bundles
	  else {
	    int& myPhiSectorCounter = Layer3FlatPhiSectorsCounter[phiSectorRef];
	    myPhiSectorCounter += 1;
	    // In case already 4 bundles from flat part, assign to next phi Sector
	    if (myPhiSectorCounter > 4) {
	      myPhiSectorCounter -= 1;
	      Layer3FlatPhiSectorsCounter[nextPhiSectorRef] += 1;
	      cablePhiSectorRef = nextPhiSectorRef;
	    }
	    slot = 5;
	  }
	}
	// TEDD_2
	else {
	  if (subDetectorName == cabling_tedd1 && layerDiskNumber == 2) slot = 5;
	  else if (subDetectorName == cabling_tedd2 && layerDiskNumber == 4) slot = 6;
	}
      }
    }

    // 2S
    else if (cableType == Category::SS) {
      int phiSectorRefThird = femod(phiSectorRef % 3, 3);

      // STAGGERING TB2S LAYER 4
      if (subDetectorName == cabling_tb2s && layerDiskNumber == 4) {
	int& myPhiSectorCounter = Layer4PhiSectorsCounter[phiSectorRef];
	myPhiSectorCounter += 1;
	// In a few cases, need to reduce to 5 bundles (additional bundle from Layer 5 will be added).
	// As a result, the first bundle in the Phi Sector is assigned to the previous phiSector.
	if (phiSectorRefThird == 0 && myPhiSectorCounter == 1) {
	  cablePhiSectorRef = previousPhiSectorRef;
	}
	slot = 1;
      }

      // STAGGERING TB2S LAYER 5
      else if (subDetectorName == cabling_tb2s && layerDiskNumber == 5) {
	int& myPhiSectorCounter = Layer5PhiSectorsCounter[phiSectorRef];
	myPhiSectorCounter += 1;
	// STAGGER BUNDLES : ASSIGN BUNDLES FROM LAYER 5 TO LAYER 4
	// In 2 cases out of 3, also one bundle from Layer 5 added.
	// if (phiSectorRefThird == 0) : should have 5 bundles in Layer 4 (+ 1 added from Layer 5)
	// if (phiSectorRefThird == 1) : should have 5 bundles in Layer 4 (+ 1 added from Layer 5)
	// if (phiSectorRefThird == 2) : should have 6 bundles in Layer 4 (no bundle added from Layer 5)
	if (phiSectorRefThird != 2 && myPhiSectorCounter == 4) {
	  slot = 1;
	}
	else {
	  slot = 2;
	}
      }

      else if ( (subDetectorName == cabling_tb2s && layerDiskNumber == 6) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 3) ) {
	// STAGGER BUNDLES : ASSIGN BUNDLES FROM LAYER 6 TO DISK 3
	if (subDetectorName == cabling_tb2s) {
	  int& myPhiSectorCounter = Layer6PhiSectorsCounter[phiSectorRef];
	  myPhiSectorCounter += 1;
	  if (myPhiSectorCounter == 1 || myPhiSectorCounter == 5 || myPhiSectorCounter == 8) slot = 4;
	  else slot = 3;
	}
	else slot = 4;
      }

      else if ( (subDetectorName == cabling_tedd1 && layerDiskNumber == 1) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 4) ) {
	slot = 5;
      }

      else if ( (subDetectorName == cabling_tedd1 && layerDiskNumber == 2) || (subDetectorName == cabling_tedd2 && layerDiskNumber == 5) ) {
	slot = 6;
      }
    }
  
    if (slot == 0) {
      std::cout << "bundleType = "  << bundleType << " cableType = " << cableType <<  " subDetectorName  =" << subDetectorName << " layerDiskNumber = " << layerDiskNumber << " isPositiveCablingSide = " << isPositiveCablingSide << std::endl;
      logERROR("Connection from ribbon to cable : ribbon category is unknown. Slot was not defined properly.");
    }


    // COLLECT phiSectorRef and slots which have been computed.
    const std::pair<int, int> phiSectorRefAndSlot = std::make_pair(cablePhiSectorRef, slot);
    const int bundleId = b.first;
    cablesPhiSectorRefAndSlot.insert(std::make_pair(bundleId, phiSectorRefAndSlot));
  }

  return cablesPhiSectorRefAndSlot;
}
*/


/* Compute cabling type associated to a cable.
 */
/*
const int InnerCablingMap::computeCableTypeIndex(const Category& cableType) const {
  int cableTypeIndex;
  if (cableType == Category::PS10G) cableTypeIndex = 0;
  else if (cableType == Category::PS5G) cableTypeIndex = 1;
  else if (cableType == Category::SS) cableTypeIndex = 2;
  return cableTypeIndex;
}

*/

/* Compute Id associated to a cable.
 */
/*
const int CablingMap::computeCableId(const int cablePhiSectorRef, const int cableTypeIndex, const int slot, const bool isPositiveCablingSide) const {
  int cablingSideIndex = (isPositiveCablingSide ? 0 : 1);

  const int cableId = cablingSideIndex * 1000 + cablePhiSectorRef * 100 + cableTypeIndex * 10 + slot;
  return cableId;
}
*/


/* Connect bundle to cable and vice-versa.
 */
/*
void InnerCablingMap::connectOneBundleToOneCable(Bundle* bundle, Cable* cable) const {
  cable->addBundle(bundle);
  bundle->setCable(cable);
}
*/


/* Check bundles-cables connections.
 */
/*
void InnerCablingMap::checkBundlesToCablesCabling(std::map<int, Cable*>& cables) {
  for (auto& c : cables) {

    // CHECK WHETHER THE PHI SLICES REF MAKE SENSE.
    const int phiSectorRef = c.second->phiSectorRef();
    if (phiSectorRef <= -1) {
      logERROR(any2str("Building cabling map : a cable was not correctly created. ")
	       + "Cable " + any2str(c.first) + ", with cableType = " + any2str(c.second->type())
	       + ", has phiSectorRef = " + any2str(phiSectorRef)
	       + ", slot = " + any2str(c.second->slot())
	       );
    }

    // CHECK THE NUMBER OF BUNDLES PER CABLE.
    const int numBundles = c.second->numBundles();
    if (numBundles > cabling_maxNumBundlesPerCable) {
      logERROR(any2str("Building cabling map : Staggering bundles. ")
	       + "Cable "  + any2str(c.first) + " is connected to " + any2str(numBundles) + " bundles."
	       + "Maximum number of bundles per cable allowed is " + any2str(cabling_maxNumBundlesPerCable)
	       );
    }

  }
}
*/


/* BUNDLES TO POWER SERVICE CHANNELS CONNECTIONS.
 * VERY IMPORTANT: connection scheme from modules to optical bundles = connection scheme from modules to power cables.
 * As a result, 1 single Bundle object is used for both schemes.
 * Regarding the connections to services channels, each Bundle is then assigned:
 * - 1 Optical Services Channel Section (considering the Bundle as an optical Bundle);
 * - 1 Power Services Channels section (making as if the Bundle is a power cable);
 * 
 * The optical channel mapping is done so that all bundles connected to the same DTC,
 * are routed through the same channel.
 *
 * The assignments of power cables to services channels must be done after the bundles to DTCs connections are established.
 * Indeed, the power channel mapping is done so that all modules connected to the same DTC, 
 * have their power cables routed through 2 consecutive channels sections at most.
 */
/*
void InnerCablingMap::computePowerServicesChannels() {
  for (bool isPositiveCablingSide : { true, false }) {

    // BARREL ONLY: IN VIEW OF THE POWER CHANNEL ASSIGNMENT, SPLIT EACH NONANT INTO 2 SEMI-NONANTS
    routeBarrelBundlesPoweringToSemiNonants(isPositiveCablingSide);
 
    // ASSIGN POWER SERVICES CHANNELS
    std::map<int, Cable*>& cables = (isPositiveCablingSide ? cables_ : negCables_);
    for (auto& c : cables) {
      c.second->assignPowerChannelSections();
    }

    // CHECK POWER SERVICES CHANNELS
    const std::map<int, Bundle*>& bundles = (isPositiveCablingSide ? bundles_ : negBundles_);
    checkBundlesToPowerServicesChannels(bundles);
  }
}
*/


/* Barrel only: in view of the power channel assignment, split each nonant into 2 semi-nonants.
   The cooling pipes design should be invariant by rotation of 180° around CMS_Y, to avoid different cooling designs on both (Z) side.
   The cooling pipes and power cables are assigned to similar channels slots (A or C).
   As a result, the channel assignmennt of power cables need to follow the same symmetry as the cooling pipes.
   Hence, the CHANNEL ASSIGNMENNT OF POWER CABLES NEED TO BE INVARIANT BY ROTATION OF 180DEG AROUND CMS_Y.
   This is a priori not trivial, since the power cables scheme follow the bundles scheme, hence the optical map mirror symmetry.
   This is only possible if, for each phi nonant, one define a semi-nonant Phi boundary in a certain way.
   This is what is done here: the semi-nonant Phi boundaries are defined
   so that ALL NONANTS AND SEMI-NONANTS PHI BOUNDARIES ARE INVARIANT BY ROTATION OF 180° AROUND CMS_Y.
 */
/*
void InnerCablingMap::routeBarrelBundlesPoweringToSemiNonants(const bool isPositiveInnerCablingSide) {
  std::map<int, Bundle*>& bundles = (isPositiveCablingSide ? bundles_ : negBundles_);
  const std::map<int, Bundle*>& stereoBundles = (isPositiveCablingSide ? negBundles_ : bundles_);

  // phiSectorRefMarker keeps track of the Phi nonant we are in.
  int phiSectorRefMarker = -1;
  // phiSectorRefMarker keeps track of the stereo Phi nonant we are in.
  // 'stereo' means on the other cabling side, by a rotation of 180° around CMS_Y.
  int stereoPhiSectorRefMarker = -1;

  // TILTED PART OF TBPS + TB2S
  // Loop on all bundles of a given cabling side (sorted by their BundleIds).
  for (auto& b : bundles) {
    Bundle* myBundle = b.second;
    const bool isBarrel = myBundle->isBarrel();
    const bool isBarrelPSFlatPart = myBundle->isBarrelPSFlatPart();

    // Only for Barrel: tilted TBPS, or TB2S.
    if (isBarrel && !isBarrelPSFlatPart) {
      // Should the bundle be assigned to the lower or upper semi-nonant ?
      // 'lower' and 'upper' are defined by 'smaller' or 'bigger' Phi, 
      // in the trigonometric sense in the (XY) plane in CMS global frame of reference.
      bool isLower; // what we want to compute!

      // Identifier of the Phi nonant we are in.
      const int phiSectorRef = myBundle->getCable()->phiSectorRef();
      // In case of a switch to a different Phi nonant, initialize variables.
      if (phiSectorRef != phiSectorRefMarker) {	
	phiSectorRefMarker = phiSectorRef;
	// Starts by assigning to bundle to the lower semi-nonant.
	isLower = true;
	stereoPhiSectorRefMarker = -1;	
      }

      // Get the bundle located on the other cabling side, by a rotation of 180° around CMS_Y.
      const int stereoBundleId = myBundle->stereoBundleId();
      auto found = stereoBundles.find(stereoBundleId);
      if (found != stereoBundles.end()) {
	const Bundle* myStereoBundle = found->second;
	// Get the Phi nonant in which the stereoBundle is located.
	const int stereoPhiSectorRef = myStereoBundle->getCable()->phiSectorRef();
	
	// Decisive point!! 
	// As soon as a change in the identifier of the stereoBundle Phi nonant is detected,
	// one assigns the bundle to the upper semi-nonant.
	if (stereoPhiSectorRefMarker != -1 && stereoPhiSectorRefMarker != stereoPhiSectorRef) isLower = false;

	// Lastly, assign the semi-nonant attribution decision to the bundle.
	myBundle->setIsPowerRoutedToBarrelLowerSemiNonant(isLower);

	// Keeps track of the Phi nonant in which the stereoBundle is located.
	stereoPhiSectorRefMarker = stereoPhiSectorRef;
      }
      else {
	logERROR(any2str("Could not find stereo bundle, id ") 
		    + any2str(stereoBundleId)
		    );
      }
    }

    // NB: All this is possible because the bundles and stereoBundles are sorted by their Ids.
    // One relies on 2 characteristics of the BundleId scheme:
    // - all Bundles connected to the same DTC will have consecutive Ids.
    // - the Id increment is in Phi, in the (XY) plane in CMS global frame of reference.
  }


  // FLAT PART OF TBPS
  // For a given bundle, connected to untilted modules:
  // Take the same semi-nonant assignment as the bundle located at the same Phi and connected to the tilted modules.

  // Loop on all bundles of a given cabling side (sorted by their BundleIds).
  for (auto& b : bundles) {
    Bundle* myBundle = b.second;
    const bool isBarrelPSFlatPart = myBundle->isBarrelPSFlatPart();

    // Only for Barrel: flat part of TBPS
    if (isBarrelPSFlatPart) {
      const int tiltedBundleId = myBundle->tiltedBundleId();
 
      // Get the bundle located at the same Phi, but connected to the tilted modules.
      auto found = bundles.find(tiltedBundleId);
      if (found != bundles.end()) {
	const Bundle* myTiltedBundle = found->second;

	// Decisive point!!
	// Get the semi-nonant attribution of the tiltedBundle.
	const bool isLower = myTiltedBundle->isPowerRoutedToBarrelLowerSemiNonant();

	// Lastly, assign the semi-nonant attribution decision to the bundle.
	myBundle->setIsPowerRoutedToBarrelLowerSemiNonant(isLower);
      }
      else {
	logERROR(any2str("Could not find bundle connected to tilted modules, id ") 
		 + any2str(tiltedBundleId)
		 );
      }
    }
  }

}
*/


/* Check services channels sections containing power cables.
 */
/*
void InnerCablingMap::checkBundlesToPowerServicesChannels(const std::map<int, Bundle*>& bundles) {
  std::map<std::pair<const int, const ChannelSlot>, int > channels;

  for (auto& b : bundles) {
    const ChannelSection* mySection = b.second->powerChannelSection();
    const int myChannelNumber = mySection->channelNumber();
    const ChannelSlot& myChannelSlot = mySection->channelSlot();

    // Check channel number.
    if (fabs(myChannelNumber) == 0 || fabs(myChannelNumber) > cabling_numServicesChannels) {
      logERROR(any2str("Invalid channel number = ")
	       + any2str(myChannelNumber)
	       + any2str(". Should not be 0 or have abs value > ")
	       + any2str(cabling_numServicesChannels) + any2str(".")
	       );
    }
    // Check channel slot.
    if (myChannelSlot != ChannelSlot::A && myChannelSlot != ChannelSlot::C) {
      logERROR(any2str("Power cable: Invalid channel slot = ") 
	       + any2str(myChannelSlot)
	       + any2str(". Should be ") + any2str(ChannelSlot::A)
	       + any2str(" or ") + any2str(ChannelSlot::C) + any2str(".")
	       );
    }

    // Compute number of power cables per channel section.
    std::pair<const int, const ChannelSlot> myChannel = std::make_pair(myChannelNumber, myChannelSlot);
    channels[myChannel] += 1;
  }

  // Check number of power cables per channel section.
  for (const auto& c : channels) { 
    if (c.second > cabling_maxNumPowerCablesPerChannel) {
      logERROR(any2str("Services channel ") + any2str(c.first.first) 
	       + any2str(" section ") + any2str(c.first.second) 
	       + any2str(" has " ) + any2str(c.second) + any2str(" power cables.")
	       + any2str(" Max number of power cables per channel section is ") 
	       + any2str(cabling_maxNumPowerCablesPerChannel)
	       );
    }
  }
}
*/
