/**
 * @file Analyzer.cc
 * @brief This is the implementation of the class that analyses a material budget
 */
#include <TH1D.h>
#include <TH2D.h>
#include <Analyzer.h>
#include <TProfile.h>
#include <TLegend.h>
#include <Palette.h>
#include <SimParms.h>
#include "AnalyzerVisitors/MaterialBillAnalyzer.h"

#undef MATERIAL_SHADOW

#include <TError.h>
Int_t gErrorIgnoreLevel = kError;

namespace insur {

  int Analyzer::bsCounter =0;

  const double Analyzer::ZeroHitsRequired = 0;
  const double Analyzer::OneHitRequired = 0.0001;







  // public
  /**
   * A comparison function for the first elements in two pairs of integers.
   * @param p The first pair
   * @param q The second pair
   * @return True if <i>p.first</i> is smaller than <i>q.first</i>, false otherwise
   */
  bool compareIntPairFirst(std::pair<int, int> p, std::pair<int, int> q) {
    return (p.first < q.first);
  }

  /**
   * A comparison function for the second elements in two pairs of integers.
   * @param p The first pair
   * @param q The second pair
   * @return True if <i>p.second</i> is smaller than <i>q.second</i>, false otherwise
   */
  bool compareIntPairSecond(std::pair<int, int> p, std::pair<int, int> q) {
    return (p.second < q.second);
  }

  /**
   * The constructor sets a number of internal constants.
   */
  Analyzer::Analyzer() {

    // Not strictly necessary, but it's useful to keep
    // the color the same for the most used module types
    lastPickedColor = 1;
    //colorPicker("pt2S");
    //colorPicker("rphi");
    //colorPicker("stereo");
    //colorPicker("ptIn");
    geomLite           = nullptr; geomLiteCreated=false;
    geomLiteXY         = nullptr; geomLiteXYCreated=false;
    geomLiteYZ         = nullptr; geomLiteYZCreated=false;
    geomLiteEC         = nullptr; geomLiteECCreated=false;
    geometryTracksUsed = 0;
    materialTracksUsed = 0;
  }


  // private
  /* High-level function finding all hits for a given tracker (and pixel)
   * and adding them to the track. The total crossed material is returned.
   * @param mb A reference to the instance of <i>MaterialBudget</i> that is to be analysed
   * @param momenta A list of momentum values for which to perform the efficiency measurements
   * @param etaSteps The number of wedges in the fan of tracks covered by the eta scan
   * @param pm A pointer to a second material budget associated to a pixel detector; may be <i>NULL</i>
   * @return the total crossed material amount
   */
  Material Analyzer::findAllHits(MaterialBudget* mb, const double& eta, const double& theta, const double& phi, Track& track) {

    // Get tracker
    const Tracker & tracker = mb->getTracker();
    bool isPixelType  = tracker.isPixelType();

    Material totalMaterial;
    //      active volumes, barrel
    totalMaterial  = findHitsModules(mb->getBarrelModuleCaps(), eta, theta, phi, track, isPixelType);
    //std::cout << ">MB>> " << totalMaterial.radiation << std::endl;
    //      active volumes, endcap
    totalMaterial += findHitsModules(mb->getEndcapModuleCaps(), eta, theta, phi, track, isPixelType);
    //std::cout << ">ME>> " << totalMaterial.radiation << std::endl;
    //      services, barrel
    totalMaterial += findHitsInactiveSurfaces(mb->getInactiveSurfaces().getBarrelServices(), eta, theta, track, isPixelType);
    //std::cout << ">MIB>> " << totalMaterial.radiation << std::endl;
    //      services, endcap
    totalMaterial += findHitsInactiveSurfaces(mb->getInactiveSurfaces().getEndcapServices(), eta, theta, track, isPixelType);
    //std::cout << ">MEB>> " << totalMaterial.radiation << std::endl;
    //      supports
    totalMaterial += findHitsInactiveSurfaces(mb->getInactiveSurfaces().getSupports(), eta, theta, track, isPixelType);
    //std::cout << ">MS>> " << totalMaterial.radiation << std::endl;
    //std::cout << std::endl;

    return totalMaterial;
  }


  /* TODO: finish this :-)
void Analyzer::createTaggedTrackCollection(std::vector<MaterialBudget*> materialBudgets,
                                           int etaSteps,
                                           double maxEta,
                                           bool forceClean = false) {
  
  if (forceClean) taggedTrackCollectionMap_.clear();
  if (taggedTrackCollectionMap_.size()!=0) return;
    
  double etaStep, eta, theta, phi;
  
  // prepare etaStep, phiStep, nTracks, nScans
  if (etaSteps > 1) etaStep = maxEta / (double)(etaSteps - 1);
  else etaStep = maxEta;
  const int nTracks& = etaSteps;

  
        
}
    
  */                                

void Analyzer::analyzeTaggedTracking(std::vector<MaterialBudget*> mbVector, const std::vector<double>& momenta, int nTracks) {

  // Local variables
  double etaStep, eta, theta, phi;
  
  // Initialize
  materialTracksUsed = nTracks;
  double efficiency  = SimParms::getInstance()->efficiency();

  // Prepare etaStep
  if (nTracks > 1) etaStep = getEtaMaxTracker() / (double)(nTracks); // / (double)(nTracks - 1);
  else             etaStep = getEtaMaxTracker();
  
  // Tracks with pt const at given eta or p const at given eta
  std::map<std::string, TrackCollectionMap> taggedTrackPtCollectionMap;
  std::map<std::string, TrackCollectionMap> taggedTrackPtCollectionMapIdeal;
  std::map<std::string, TrackCollectionMap> taggedTrackPCollectionMap;
  std::map<std::string, TrackCollectionMap> taggedTrackPCollectionMapIdeal;
  
  // Tracks pruned
  bool isPruned = false;

  for (int i_eta = 0; i_eta < nTracks; i_eta++) {
    
    // Define track
    Track track;

    eta   = i_eta * etaStep;
    theta = 2 * atan(exp(-eta));
    phi   = myDice.Rndm() * M_PI * 2.0;

    track.setTheta(theta);      
    track.setPhi(phi);
    track.setMagField(SimParms::getInstance()->magneticField());

    // Go through all material budget contributors (pixel tracker, strip tracker, ...)
    Material tmpMat;
    for (auto & mb : mbVector) {

      // Assign hits to track
      tmpMat += findAllHits(mb, eta, theta, phi, track);

      // TODO: BeamPipe --> needs to be finished,  Add the hit on the beam pipe - CURRENT WORKAROUND
      if (mb==*(mbVector.begin())) {

        double outerRadius = SimParms::getInstance()->bpRadius()+SimParms::getInstance()->bpThickness();
        double position    = SimParms::getInstance()->bpRadius()+SimParms::getInstance()->bpThickness()/2.;
        double radLength   = SimParms::getInstance()->bpRadLength()/100.;
        double intLength   = SimParms::getInstance()->bpIntLength()/100.;
        //double minRTracker = mb->getTracker().minR();

        //if (outerRadius>=minRTracker) {

        //  std::stringstream message("");
        //  message << "Beam pipe not built -> outer radius: " << outerRadius << " bigger than tracker inner radius: " << minRTracker << "!!!";
        //  logERROR(message.str());
        //}
        //else
        if (radLength<=0 && intLength<=0) {

          std::stringstream message("");
          message << "Beam pipe not built, radiation length: " << radLength << " or interaction length: " << intLength << " zero or negative!!!";
          logERROR(message.str());
        }
        else {

          Hit* hit = new Hit(position/sin(theta));
          hit->setOrientation(Hit::Horizontal);
          hit->setObjectKind(Hit::Inactive);
          Material tmp;
          tmp.radiation   = radLength/sin(theta);
          tmp.interaction = intLength/sin(theta);
          hit->setCorrectedMaterial(tmp);
          track.addHit(hit);
        }
      }
    }
    // Debug: material amount
    // std::cerr << "eta = " << eta
    //           << ", material.radiation = " << tmp.radiation
    //           << ", material.interaction = " << tmp.interaction
    //           << std::endl;

    // Go through tracks with non-zero number of hits
    if (!track.noHits()) {
      for (string tag : track.tags()) {
        
        // Analyze only tracks of the same tag, i.e. TRK, ...
        track.keepTaggedOnly(tag);

        // Add IP constraint
        if (SimParms::getInstance()->useIPConstraint()) track.addIPConstraint(SimParms::getInstance()->rError(), SimParms::getInstance()->zErrorCollider());
        track.sort();
        //track.print();


        // Remove some hits randomly based on inefficiency parameter
        if (efficiency!=1) track.addEfficiency(efficiency, false);

        // For each momentum/transverse momentum compute the tracks error
        for (const auto& pIter : momenta ) {
          int    parameter = pIter * 1000; // Store p or pT in MeV as int (key to the map)
          double momentum  = pIter;        
            
          // Case I) Initial momentum is equal to pT
          double pT = momentum;
              
          // Active+passive material
          Track trackPt(track);
          trackPt.setTransverseMomentum(pT);

          // Remove tracks with less than 3 hits
          bool pruned = trackPt.pruneHits();
          if (pruned) isPruned = true;

          if (trackPt.nActiveHits(true)>2) {
            trackPt.computeErrors();
            TrackCollectionMap &myMap     = taggedTrackPtCollectionMap[tag];
            TrackCollection &myCollection = myMap[parameter];
            myCollection.push_back(trackPt);
          }

          // Ideal (no material)
          Track idealTrackPt(trackPt);
          idealTrackPt.removeMaterial();
          if (idealTrackPt.nActiveHits(true)>2) {
            idealTrackPt.computeErrors();
            TrackCollectionMap &myMapIdeal     = taggedTrackPtCollectionMapIdeal[tag];
            TrackCollection &myCollectionIdeal = myMapIdeal[parameter];
            myCollectionIdeal.push_back(idealTrackPt);
          }

          // Case II) Initial momentum is equal to p
          pT = momentum*sin(theta);

          // Active+passive material
          Track trackP(track);
          trackP.setTransverseMomentum(pT);

          // Remove tracks with less than 3 hits
          pruned = trackP.pruneHits();
          if (pruned) isPruned = true;

          if (trackP.nActiveHits(true)>2) {
            trackP.computeErrors();
            TrackCollectionMap &myMapII     = taggedTrackPCollectionMap[tag];
            TrackCollection &myCollectionII = myMapII[parameter];
            myCollectionII.push_back(trackP);
          }

          // Ideal (no material)
          Track idealTrackP(trackP);
          idealTrackP.removeMaterial();
          if (idealTrackP.nActiveHits(true)>2) {
            idealTrackP.computeErrors();
            TrackCollectionMap &myMapIdealII     = taggedTrackPCollectionMapIdeal[tag];
            TrackCollection &myCollectionIdealII = myMapIdealII[parameter];
            myCollectionIdealII.push_back(idealTrackP);
          }
        }    
      }
    }
  }
  
  // Log pruning procedure -> no result mode might occur if starting momenta wrongly set
  if (isPruned) {

    std::string message = std::string("Resolution - some tracks pruned! Hits that don't follow the parabolic approximation removed. Check momenta if no result appears!");
    logWARNING(message);
  }

  // Momentum = Pt
  //
  // For each tracking system compute the resolution graphs // TODO: consts here
  for (/*const*/ auto& ttcmIt : taggedTrackPtCollectionMap) {
    const string& myTag = ttcmIt.first;
    clearGraphs_Pt(GraphBag::RealGraph, myTag);
    /*const*/ TrackCollectionMap& myTrackCollection = ttcmIt.second;
    for (const auto& tcmIt : myTrackCollection) {
      const int &parameter = tcmIt.first;
      const TrackCollection& myCollection = tcmIt.second;
      //std::cout << myCollection.size() << std::endl; 
      calculateGraphsConstPt(parameter, myCollection, GraphBag::RealGraph, myTag);
    }
  }

  for (/*const*/ auto& ttcmIt : taggedTrackPtCollectionMapIdeal) {
    const string& myTag = ttcmIt.first;
    clearGraphs_Pt(GraphBag::IdealGraph, myTag);
    /*const*/ TrackCollectionMap& myTrackCollection = ttcmIt.second;
    for (const auto& tcmIt : myTrackCollection) {
      const int &parameter = tcmIt.first;
      const TrackCollection& myCollection = tcmIt.second;
      calculateGraphsConstPt(parameter, myCollection, GraphBag::IdealGraph, myTag);
    }
  }

  // Momentum = P
  //
  // For each tracking system compute the resolution graphs // TODO: consts here
  for (/*const*/ auto& ttcmIt : taggedTrackPCollectionMap) {
    const string& myTag = ttcmIt.first;
    clearGraphs_P(GraphBag::RealGraph, myTag);
    /*const*/ TrackCollectionMap& myTrackCollection = ttcmIt.second;
    for (const auto& tcmIt : myTrackCollection) {
      const int &parameter = tcmIt.first;
      const TrackCollection& myCollection = tcmIt.second;
      //std::cout << myCollection.size() << std::endl;
      calculateGraphsConstP(parameter, myCollection, GraphBag::RealGraph, myTag);
    }
  }

  for (/*const*/ auto& ttcmIt : taggedTrackPCollectionMapIdeal) {
    const string& myTag = ttcmIt.first;
    clearGraphs_P(GraphBag::IdealGraph, myTag);
    /*const*/ TrackCollectionMap& myTrackCollection = ttcmIt.second;
    for (const auto& tcmIt : myTrackCollection) {
      const int &parameter = tcmIt.first;
      const TrackCollection& myCollection = tcmIt.second;
      calculateGraphsConstP(parameter, myCollection, GraphBag::IdealGraph, myTag);
    }
  }

}

  /**
   * The analysis function performing all necessary the trigger efficiencies
   * @param tracker 
   * @param thresholdProbabilities
   * @param etaSteps The number of wedges in the fan of tracks covered by the eta scan
   */
  void Analyzer::analyzeTriggerEfficiency(Tracker& tracker,
                                          const std::vector<double>& triggerMomenta,
                                          const std::vector<double>& thresholdProbabilities,
                                          int nTracks) {

    double efficiency = SimParms::getInstance()->efficiency();

    materialTracksUsed = nTracks;

    double etaStep, z0, eta, theta, phi;
    double zError = SimParms::getInstance()->zErrorCollider();

    // prepare etaStep, phiStep, nTracks, nScans
    if (nTracks > 1) etaStep = getEtaMaxTracker() / (double)(nTracks);
    else             etaStep = getEtaMaxTracker();

    prepareTriggerPerformanceHistograms(nTracks, getEtaMaxTracker(), triggerMomenta, thresholdProbabilities);

    // reset the list of tracks
    std::vector<Track> tv;

    // Loop over nTracks (eta range [0, getEtaMaxTrigger()])
    for (int i_eta = 0; i_eta < nTracks; i_eta++) {
      phi = myDice.Rndm() * M_PI * 2.0;
      z0 = myDice.Gaus(0, zError);
      int nHits;
      Track track;
      eta = i_eta * etaStep;
      theta = 2 * atan(exp(-eta));
      track.setTheta(theta);      
      track.setPhi(phi);
      track.setMagField(SimParms::getInstance()->magneticField());

      nHits = findHitsModules(tracker, z0, eta, theta, phi, track);

      if (nHits) {
        // Keep only triggering hits
        // std::cerr << "Material before = " << track.getCorrectedMaterial().radiation;
        track.keepTriggerOnly();
        track.sort();
        track.setTriggerResolution(true);

        // std::cerr << " material after = " << track.getCorrectedMaterial().radiation << std::endl;

        if (efficiency!=1) track.addEfficiency(efficiency, false);
        if (track.nActiveHits(true)>0) { // At least 3 points are needed to measure the arrow
          tv.push_back(track);
        }    
      }
    }

    // Compute the number of triggering points along the selected tracks
    fillTriggerEfficiencyGraphs(tracker, triggerMomenta, tv);

    // Fill the trigger performance maps
    fillTriggerPerformanceMaps(tracker);

  }



void Analyzer::fillAvailableSpacing(Tracker& tracker, std::vector<double>& spacingOptions) {
  double aSpacing;
  std::set<double> foundSpacing;

  // Loop over all the layers
  for (auto& aModule : tracker.modules()) {
    if (aModule->sensorLayout() == PT) {
      aSpacing = aModule->dsDistance();
      if (aSpacing>0) {
        foundSpacing.insert(aSpacing);
      }
    }
  }

  spacingOptions.clear();
  for(std::set<double>::iterator it=foundSpacing.begin(); it!=foundSpacing.end(); ++it) {
    spacingOptions.push_back(*it);
  }

}

void Analyzer::createTriggerDistanceTuningPlots(Tracker& tracker, const std::vector<double>& triggerMomenta) {
  TriggerDistanceTuningPlotsVisitor v(myProfileBag, 
                                      triggerMomenta);
  SimParms::getInstance()->accept(v);
  tracker.accept(v);
  v.postVisit();
  optimalSpacingDistribution = v.optimalSpacingDistribution;
  optimalSpacingDistributionAW = v.optimalSpacingDistributionAW; 
  triggerRangeLowLimit = v.triggerRangeLowLimit; 
  triggerRangeHighLimit = v.triggerRangeHighLimit; 
  spacingTuningFrame = v.spacingTuningFrame;
  spacingTuningGraphs = v.spacingTuningGraphs;
  spacingTuningGraphsBad = v.spacingTuningGraphsBad;
  moduleOptimalSpacings = v.moduleOptimalSpacings;
}






void Analyzer::fillTriggerEfficiencyGraphs(const Tracker& tracker,
                                           const std::vector<double>& triggerMomenta,
                                           const std::vector<Track>& trackVector) {

  // Prepare the graphs to record the number of triggered points
  //std::map<double, TGraph>& trigGraphs = myGraphBag.getGraphs(GraphBag::TriggerGraph|GraphBag::TriggeredGraph);
  std::map<double, TProfile>& trigProfiles = myProfileBag.getProfiles(profileBag::TriggerProfile|profileBag::TriggeredProfile);
  std::map<double, TProfile>& trigFractionProfiles = myProfileBag.getProfiles(profileBag::TriggerProfile|profileBag::TriggeredFractionProfile);
  std::map<double, TProfile>& trigPurityProfiles = myProfileBag.getProfiles(profileBag::TriggerProfile|profileBag::TriggerPurityProfile);

  TProfile& totalProfile = trigProfiles[profileBag::Triggerable];

  std::map<std::string, std::map<std::string, TH1I*>>& stubEfficiencyCoverageProfiles = getStubEfficiencyCoverageProfiles();

  double maxEta = getEtaMaxTracker(); //getEtaMaxTrigger();

  for (std::vector<Track>::const_iterator itTrack = trackVector.begin();
       itTrack != trackVector.end(); ++itTrack) {
    const Track& myTrack=(*itTrack);

    double eta = myTrack.getEta();
    int nHits = myTrack.nActiveHits(false, false);
    totalProfile.Fill(eta, nHits);
    std::vector<std::pair<Module*,HitType>> hitModules = myTrack.getHitModules();

    for(std::vector<double>::const_iterator itMomentum = triggerMomenta.begin();
        itMomentum!=triggerMomenta.end(); ++itMomentum) {
      TProfile& myProfile = trigProfiles[(*itMomentum)];
      TProfile& myFractionProfile = trigFractionProfiles[(*itMomentum)];
      TProfile& myPurityProfile = trigPurityProfiles[(*itMomentum)];
      double nExpectedTriggerPoints = myTrack.expectedTriggerPoints(*itMomentum);
      if (nExpectedTriggerPoints>=0) { // sanity check (! nan)
        myProfile.Fill(eta, nExpectedTriggerPoints);
        if (nHits>0) {
          myFractionProfile.Fill(eta, nExpectedTriggerPoints*100/double(nHits));
           double curAvgTrue=0;
           double curAvgInteresting=0;
           double curAvgFake=0;
           double bgReductionFactor; // Reduction of the combinatorial background for ptPS modules by turning off the appropriate pixels
           for (const auto& modAndType : hitModules) {
             Module* hitModule = modAndType.first;
             PtErrorAdapter pterr(*hitModule);
             // Hits that we would like to have from tracks above this threshold
             curAvgInteresting += pterr.getParticleFrequencyPerEventAbove(*itMomentum);
             // ... out of which we only see these
             curAvgTrue += pterr.getTriggerFrequencyTruePerEventAbove(*itMomentum);
               
             // The background is given by the contamination from low pT tracks...
             curAvgFake += pterr.getTriggerFrequencyTruePerEventBelow(*itMomentum);
             // ... plus the combinatorial background from occupancy (can be reduced using ptPS modules)
             if (hitModule->reduceCombinatorialBackground()) bgReductionFactor = hitModule->geometricEfficiency(); else bgReductionFactor=1;
             curAvgFake += pterr.getTriggerFrequencyFakePerEvent()*SimParms::getInstance()->numMinBiasEvents() * bgReductionFactor;

             std::string layerName = hitModule->uniRef().cnt + "_" + any2str(hitModule->uniRef().layer);
             if (modAndType.second == HitType::STUB) {
               std::string momentumString = any2str(*itMomentum, 2);
               if (stubEfficiencyCoverageProfiles[layerName].count(momentumString) == 0) {
                 stubEfficiencyCoverageProfiles[layerName][momentumString] = new TH1I(Form("stubEfficiencyCoverageProfile%s%s", layerName.c_str(), momentumString.c_str()), (layerName + ";#eta;Stubs").c_str(), trackVector.size(), 0.0, maxEta); 
               }
               stubEfficiencyCoverageProfiles[layerName][momentumString]->Fill(myTrack.getEta(), 1);
             } 
           }
           myPurityProfile.Fill(eta, 100*curAvgTrue/(curAvgTrue+curAvgFake));
        }
      }
    }
  }
//  for (auto i : stubEfficiencyCoverageProfiles) {
//    std::cout << "--------------------- " << i.first << " ------------------ " << std::endl;
//    for (auto j : i.second) {
//      std::cout << j.first << std::endl;
//      j.second->Print("all");
//    }
//  }
  if (totalProfile.GetMaximum() < maximum_n_planes) totalProfile.SetMaximum(maximum_n_planes);

}

/**
 * The main analysis function provides a frame for the scan in eta, defers summing up the radiation
 * and interaction lengths for each volume category to subfunctions, and sorts those results into the
 * correct histograms.
 * @param mbVector A vector to the pointer to a <i>MaterialBudget</i> that is to be analysed
 * @param nTracks  A number of tracks to shoot for each momentum
 */
void Analyzer::analyzeMaterialBudget(std::vector<MaterialBudget*> mbVector, int nTracks) {

  // Define track parameters
  double etaStep, eta, theta, phi;

  // Prepare etaStep, phiStep, nTracks, nScans
  if (nTracks > 1) etaStep = getEtaMaxMaterial() / (double)(nTracks); //(double)(nTracks - 1);
  else             etaStep = getEtaMaxMaterial();

  // Reset histograms ...
  clearMaterialBudgetHistograms();
  clearCells();

  // Reset the number of bins and the histogram boundaries (0.0 to getEtaMaxMaterial()) for all histograms, recalculate the cell boundaries
  setHistogramBinsBoundaries(nTracks, 0.0, getEtaMaxMaterial());
  setCellBoundaries(nTracks, 0.0, geom_max_radius + geom_inactive_volume_width, 0.0, getEtaMaxMaterial());

  for (int i_eta = 0; i_eta < nTracks; i_eta++) {

    phi = myDice.Rndm() * M_PI * 2.0;
    Material tmp;

    // Track parameters
    Track track;
    eta   = i_eta * etaStep;
    theta = 2 * atan(exp(-eta));
    track.setTheta(theta);
    track.setPhi(phi);
    track.setMagField(SimParms::getInstance()->magneticField());

    // Go through all material budget contributors (pixel tracker, strip tracker, ...)
    for (auto & mb : mbVector) {

      Tracker& tracker       = mb->getTracker();
      double stripEfficiency = SimParms::getInstance()->efficiency();
      double pixelEfficiency = SimParms::getInstance()->pixelEfficiency();
      bool   isPixelType     = tracker.isPixelType();

      //
      // Calculate materials & add hits contributing to the track
      //
      // Active volumes, barrel
      std::map<std::string, Material> sumComponentsRI;
      tmp = analyzeModules(mb->getBarrelModuleCaps(), eta, theta, phi, track, sumComponentsRI, isPixelType);
      ractivebarrel.Fill(eta, tmp.radiation);
      iactivebarrel.Fill(eta, tmp.interaction);
      rbarrelall.Fill(   eta, tmp.radiation);
      ibarrelall.Fill(   eta, tmp.interaction);
      ractiveall.Fill(   eta, tmp.radiation);
      iactiveall.Fill(   eta, tmp.interaction);
      rglobal.Fill(      eta, tmp.radiation);
      iglobal.Fill(      eta, tmp.interaction);
  
      // Active volumes, endcap
      tmp = analyzeModules(mb->getEndcapModuleCaps(), eta, theta, phi, track, sumComponentsRI, isPixelType);
      ractiveendcap.Fill(eta, tmp.radiation);
      iactiveendcap.Fill(eta, tmp.interaction);
      rendcapall.Fill(   eta, tmp.radiation);
      iendcapall.Fill(   eta, tmp.interaction);
      ractiveall.Fill(   eta, tmp.radiation);
      iactiveall.Fill(   eta, tmp.interaction);
      rglobal.Fill(      eta, tmp.radiation);
      iglobal.Fill(      eta, tmp.interaction);
  
      // Create component histograms
      for (std::map<std::string, Material>::iterator it = sumComponentsRI.begin(); it != sumComponentsRI.end(); ++it) {
        if (rComponents[it->first]==nullptr) {
          rComponents[it->first] = new TH1D();
          rComponents[it->first]->SetBins(nTracks, 0.0, getEtaMaxMaterial());
        }
        rComponents[it->first]->Fill(eta, it->second.radiation);
        if (iComponents[it->first]==nullptr) {
          iComponents[it->first] = new TH1D();
          iComponents[it->first]->SetBins(nTracks, 0.0, getEtaMaxMaterial());
        }
        iComponents[it->first]->Fill(eta, it->second.interaction);
      }
  
      if (rComponents["Services"]==nullptr) {
        rComponents["Services"] = new TH1D();
        rComponents["Services"]->SetBins(nTracks, 0.0, getEtaMaxMaterial());
      }
      if (iComponents["Services"]==nullptr) {
        iComponents["Services"] = new TH1D();
        iComponents["Services"]->SetBins(nTracks, 0.0, getEtaMaxMaterial());
      }
      if (rComponents["Supports"]==nullptr) {
        rComponents["Supports"] = new TH1D();
        rComponents["Supports"]->SetBins(nTracks, 0.0, getEtaMaxMaterial());
      }
      if (iComponents["Supports"]==nullptr) {
        iComponents["Supports"] = new TH1D();
        iComponents["Supports"]->SetBins(nTracks, 0.0, getEtaMaxMaterial());
      }
      if (rComponents["Beam pipe"]==nullptr) {
        rComponents["Beam pipe"] = new TH1D();
        rComponents["Beam pipe"]->SetBins(nTracks, 0.0, getEtaMaxMaterial());
      }
      if (iComponents["Beam pipe"]==nullptr) {
        iComponents["Beam pipe"] = new TH1D();
        iComponents["Beam pipe"]->SetBins(nTracks, 0.0, getEtaMaxMaterial());
      }
  
      // Services, barrel
      tmp = analyzeInactiveSurfaces(mb->getInactiveSurfaces().getBarrelServices(), eta, theta, track, MaterialProperties::no_cat, isPixelType);
      rserfbarrel.Fill(eta, tmp.radiation);
      iserfbarrel.Fill(eta, tmp.interaction);
      rbarrelall.Fill( eta, tmp.radiation);
      ibarrelall.Fill( eta, tmp.interaction);
      rserfall.Fill(   eta, tmp.radiation);
      iserfall.Fill(   eta, tmp.interaction);
      rglobal.Fill(    eta, tmp.radiation);
      iglobal.Fill(    eta, tmp.interaction);
      rComponents["Services"]->Fill(eta, tmp.radiation);
      iComponents["Services"]->Fill(eta, tmp.interaction);
  
      // Services, endcap
      tmp = analyzeInactiveSurfaces(mb->getInactiveSurfaces().getEndcapServices(), eta, theta, track, MaterialProperties::no_cat, isPixelType);
      rserfendcap.Fill(eta, tmp.radiation);
      iserfendcap.Fill(eta, tmp.interaction);
      rendcapall.Fill( eta, tmp.radiation);
      iendcapall.Fill( eta, tmp.interaction);
      rserfall.Fill(   eta, tmp.radiation);
      iserfall.Fill(   eta, tmp.interaction);
      rglobal.Fill(    eta, tmp.radiation);
      iglobal.Fill(    eta, tmp.interaction);
      rComponents["Services"]->Fill(eta, tmp.radiation);
      iComponents["Services"]->Fill(eta, tmp.interaction);
  
      // Supports, barrel
      tmp = analyzeInactiveSurfaces(mb->getInactiveSurfaces().getSupports(), eta, theta, track, MaterialProperties::b_sup, isPixelType);
      rlazybarrel.Fill(eta, tmp.radiation);
      ilazybarrel.Fill(eta, tmp.interaction);
      rbarrelall.Fill( eta, tmp.radiation);
      ibarrelall.Fill( eta, tmp.interaction);
      rlazyall.Fill(   eta, tmp.radiation);
      ilazyall.Fill(   eta, tmp.interaction);
      rglobal.Fill(    eta, tmp.radiation);
      iglobal.Fill(    eta, tmp.interaction);
      rComponents["Supports"]->Fill(eta, tmp.radiation);
      iComponents["Supports"]->Fill(eta, tmp.interaction);
  
      // Supports, endcap
      tmp = analyzeInactiveSurfaces(mb->getInactiveSurfaces().getSupports(), eta, theta, track, MaterialProperties::e_sup, isPixelType);
      rlazyendcap.Fill(eta, tmp.radiation);
      ilazyendcap.Fill(eta, tmp.interaction);
      rendcapall.Fill( eta, tmp.radiation);
      iendcapall.Fill( eta, tmp.interaction);
      rlazyall.Fill(   eta, tmp.radiation);
      ilazyall.Fill(   eta, tmp.interaction);
      rglobal.Fill(    eta, tmp.radiation);
      iglobal.Fill(    eta, tmp.interaction);
      rComponents["Supports"]->Fill(eta, tmp.radiation);
      iComponents["Supports"]->Fill(eta, tmp.interaction);
  
      // Supports, tubes
      tmp = analyzeInactiveSurfaces(mb->getInactiveSurfaces().getSupports(), eta, theta, track, MaterialProperties::o_sup, isPixelType);
      rlazytube.Fill(eta, tmp.radiation);
      ilazytube.Fill(eta, tmp.interaction);
      rlazyall.Fill( eta, tmp.radiation);
      ilazyall.Fill( eta, tmp.interaction);
      rglobal.Fill(  eta, tmp.radiation);
      iglobal.Fill(  eta, tmp.interaction);
      rComponents["Supports"]->Fill(eta, tmp.radiation);
      iComponents["Supports"]->Fill(eta, tmp.interaction);
  
      // Supports, barrel tubes
      tmp = analyzeInactiveSurfaces(mb->getInactiveSurfaces().getSupports(), eta, theta, track, MaterialProperties::t_sup, isPixelType);
      rlazybtube.Fill(eta, tmp.radiation);
      ilazybtube.Fill(eta, tmp.interaction);
      rlazyall.Fill(  eta, tmp.radiation);
      ilazyall.Fill(  eta, tmp.interaction);
      rglobal.Fill(   eta, tmp.radiation);
      iglobal.Fill(   eta, tmp.interaction);
      rComponents["Supports"]->Fill(eta, tmp.radiation);
      iComponents["Supports"]->Fill(eta, tmp.interaction);
  
      // Supports, user defined
      tmp = analyzeInactiveSurfaces(mb->getInactiveSurfaces().getSupports(), eta, theta, track, MaterialProperties::u_sup, isPixelType);
      rlazyuserdef.Fill(eta, tmp.radiation);
      ilazyuserdef.Fill(eta, tmp.interaction);
      rlazyall.Fill(    eta, tmp.radiation);
      ilazyall.Fill(    eta, tmp.interaction);
      rglobal.Fill(     eta, tmp.radiation);
      iglobal.Fill(     eta, tmp.interaction);
      rComponents["Supports"]->Fill(eta, tmp.radiation);
      iComponents["Supports"]->Fill(eta, tmp.interaction);
  
      // TODO: BeamPipe --> needs to be finished,  Add the hit on the beam pipe - CURRENT WORKAROUND
      //tmp = analyzeInactiveSurfaces(mb->getInactiveSurfaces().getSupports(), eta, theta, track, MaterialProperties::beam_pipe);
      if (mb==*(mbVector.begin())) {

        double outerRadius = SimParms::getInstance()->bpRadius()+SimParms::getInstance()->bpThickness();
        double position    = SimParms::getInstance()->bpRadius()+SimParms::getInstance()->bpThickness()/2.;
        double radLength   = SimParms::getInstance()->bpRadLength()/100.;
        double intLength   = SimParms::getInstance()->bpIntLength()/100.;
        //double minRTracker = mb->getTracker().minR();

        //if (outerRadius>=minRTracker) {
        //
        //  std::stringstream message("");
        //  message << "Beam pipe not built -> outer radius: " << outerRadius << " bigger than tracker inner radius: " << minRTracker << "!!!" << std::endl;
        //  logERROR(message.str());
        //}
        //else
        if (radLength<=0 && intLength<=0) {

          std::stringstream message("");
          message << "Beam pipe not built, radiation length: " << radLength << " or interaction length: " << intLength << " zero or negative!!!" << std::endl;
          logERROR(message.str());
        }
        else {

          Hit* hit = new Hit(position/sin(theta));
          hit->setOrientation(Hit::Horizontal);
          hit->setObjectKind(Hit::Inactive);
          tmp.radiation   = radLength/sin(theta);
          tmp.interaction = intLength/sin(theta);
          hit->setCorrectedMaterial(tmp);
          track.addHit(hit);
  
          rbeampipe.Fill(eta, tmp.radiation);
          ibeampipe.Fill(eta, tmp.interaction);
          rglobal.Fill(  eta, tmp.radiation);
          iglobal.Fill(  eta, tmp.interaction);
          rComponents["Beam pipe"]->Fill(eta, tmp.radiation);
          iComponents["Beam pipe"]->Fill(eta, tmp.interaction);
        }
      }
      // Strip efficiency
      if (stripEfficiency!=1) track.addEfficiency(stripEfficiency, false);
      // Pixel efficiency
      if (pixelEfficiency!=1) track.addEfficiency(pixelEfficiency, true);

    } // Material budgets

    // Calculate efficiency plots
    if (!track.noHits()) {

      track.sort();

      // Get hits
      int nActive = track.nActiveHits(true);
      if (nActive>0) {

        hadronTotalHitsGraph.SetPoint(hadronTotalHitsGraph.GetN(), eta, nActive);
  
        double probability;
        std::vector<double> probabilities = track.hadronActiveHitsProbability(true);
  
        double averageHits  = 0;
        double exactProb    = 0;
        double moreThanProb = 0;
        for (int i=probabilities.size()-1; i>=0; --i) {

          //if (nActive==10) { // debug
          //  std::cerr << "probabilities.at("
          //  << i << ")=" << probabilities.at(i)
          //  << endl;
          //}
          exactProb     = probabilities.at(i)-moreThanProb;
          averageHits  += (i+1)*exactProb;
          moreThanProb += exactProb;
        }

        hadronAverageHitsGraph.SetPoint(hadronAverageHitsGraph.GetN(), eta, averageHits);
        //hadronAverageHitsGraph.SetPointError(hadronAverageHitsGraph.GetN()-1, 0, sqrt( averageSquaredHits - averageHits*averageHits) );
  
        unsigned int requiredHits;
        for (unsigned int i = 0; i<hadronNeededHitsFraction.size(); ++i) {

          requiredHits = int(ceil(double(nActive) * hadronNeededHitsFraction.at(i)));
          if      (requiredHits==0)                   probability = 1;
          else if (requiredHits>probabilities.size()) probability = 0;
          else                                        probability = probabilities.at(requiredHits-1);

          //if (probabilities.size()==10) { // debug
          //  std::cerr << "required " << requiredHits
          //              << " out of " << probabilities.size()
          //              << " == " << nActive
          //              << endl;
          // std::cerr << "      PROBABILITY = " << probability << endl << endl;
          //}
          hadronGoodTracksFraction.at(i).SetPoint(hadronGoodTracksFraction.at(i).GetN(), eta, probability);
        }
      }
    } // Calculate efficiency plots

  } // nTracks

#ifdef MATERIAL_SHADOW       
  // integration over eta
  for (unsigned int i = 0; i < cells.size(); i++) {
    for (unsigned int j = 1; j < cells.at(i).size(); j++) {
      cells.at(i).at(j).rlength = cells.at(i).at(j).rlength + cells.at(i).at(j - 1).rlength;
      cells.at(i).at(j).ilength = cells.at(i).at(j).ilength + cells.at(i).at(j - 1).ilength;
    }
  }
  // transformation from (eta, r) to (z, r) coordinates
  transformEtaToZ();
#endif // MATERIAL_SHADOW

}

void Analyzer::analyzePower(Tracker& tracker) {
  computeIrradiatedPowerConsumption(tracker);
  preparePowerHistograms();
  //fillPowerMap(tracker);
}




void Analyzer::computeTriggerFrequency(Tracker& tracker) {
  TriggerFrequencyVisitor v; 
  SimParms::getInstance()->accept(v);
  tracker.accept(v);

  triggerFrequencyTrueSummaries_ = v.triggerFrequencyTrueSummaries;
  triggerFrequencyFakeSummaries_ = v.triggerFrequencyFakeSummaries;
  triggerFrequencyMisfilteredSummaries_ = v.triggerFrequencyMisfilteredSummaries;
  triggerFrequencyCombinatorialSummaries_ = v.triggerFrequencyCombinatorialSummaries;
  triggerFrequencyInterestingSummaries_ = v.triggerFrequencyInterestingSummaries;
  triggerRateSummaries_ = v.triggerRateSummaries; 
  triggerEfficiencySummaries_ = v.triggerEfficiencySummaries; 
  triggerPuritySummaries_ = v.triggerPuritySummaries; 
  triggerDataBandwidthSummaries_ = v.triggerDataBandwidthSummaries;
  triggerFrequenciesPerEvent_ = v.triggerFrequenciesPerEvent;
  stripOccupancySummaries_ = v.stripOccupancySummaries;
  hitOccupancySummaries_ = v.hitOccupancySummaries;
}


  
    
void Analyzer::computeTriggerProcessorsBandwidth(Tracker& tracker) {
  TriggerProcessorBandwidthVisitor v(triggerDataBandwidths_, triggerFrequenciesPerEvent_);
  v.preVisit();
  SimParms::getInstance()->accept(v);
  tracker.accept(v);
  v.postVisit();

  processorConnectionSummary_ = v.processorConnectionSummary; 
  processorCommonConnectionSummary_ = v.processorCommonConnectionSummary;
  processorInboundBandwidthSummary_ = v.processorInboundBandwidthSummary; 
  processorInboundStubPerEventSummary_ = v.processorInboundStubPerEventSummary; 
  moduleConnectionsDistribution = v.moduleConnectionsDistribution; 
  moduleConnections_ = v.moduleConnections;
  triggerSectorMap_ = v.sectorMap;
  processorCommonConnectionMap_ = v.processorCommonConnectionMap;
  sampleTriggerPetal_ = v.sampleTriggerPetal;
  triggerPetalCrossoverR_ = v.crossoverR;
}


void Analyzer::computeIrradiatedPowerConsumption(Tracker& tracker) {
  IrradiationPowerVisitor v;
  SimParms::getInstance()->accept(v);
  tracker.accept(v);

  irradiatedPowerConsumptionSummaries_ = v.irradiatedPowerConsumptionSummaries;
}




// protected
/**
 * Looping on the layers, and picking only modules on the YZ section
 * For all these modules prepare a different table with materials
 * @param tracker a reference to the <i>ModuleCap</i> vector of vectors that sits on top of the tracker modules
 * @param result a map of summary tables to be filled
 */
void Analyzer::computeDetailedWeights(std::vector<std::vector<ModuleCap> >& tracker,std::map<std::string, SummaryTable>& result,
                                      bool byMaterial) {
  map<std::string, map<std::pair<int, int>, bool> > typeTaken;
  map<std::string, map<std::pair<int, int>, bool> > typeWritten;

  string tempString;
  ostringstream tempSS;

  std::vector<std::vector<ModuleCap> >::iterator layerIt;
  //std::vector<std::vector<ModuleCap> >::iterator layerGuard;
  std::vector<ModuleCap>::iterator moduleIt;
  //std::vector<ModuleCap>::iterator moduleGuard;

  ModuleCap* myModuleCap;
  Module* myModule;

  //  unsigned int nLocalMasses;
  //  unsigned int nExitingMasses;

  std::map<std::string, double>::const_iterator localmassesBegin;
  std::map<std::string, double>::const_iterator localmassesEnd;

  std::map<std::string, double>::const_iterator exitingmassesBegin;
  std::map<std::string, double>::const_iterator exitingmassesEnd;

  // First create a list of material used anywhere
  std::vector<std::string> materialTagV;
  std::vector<std::string>::iterator materialTagIt;

  double localMaterial;
  double exitingMaterial;
  string materialTag;

  // loop over layers
  for (layerIt = tracker.begin(); layerIt != tracker.end(); ++layerIt) {
    // Loop over modules
    for (moduleIt = layerIt->begin(); moduleIt != layerIt->end(); ++moduleIt) {
      // Check if the module is on the YZ section
      myModuleCap = &(*moduleIt);
      myModule = &(myModuleCap->getModule());
      if (myModule->posRef().phi==1) {
        pair<int, int> myIndex = make_pair(myModule->tableRef().row, myModule->tableRef().col);
        tempString = myModule->cntName();
        if (!typeTaken[tempString][myIndex]) {
          typeTaken[tempString][myIndex]=true;
          // TODO: put this in a better place
          // (and make a better module typing)
          struct Visitor : public ConstGeometryVisitor {
            std::map<string, SummaryTable>& result;

            Visitor(std::map<std::string, SummaryTable>& result_) : result(result_) {}
            void visit(const BarrelModule& m) {
              string s = m.cntName() + " (L" + any2str(m.layer()) + ")";
              result[s].setCell(0, m.ring(), TagMaker::makePosTag(m));
            }
            void visit(const EndcapModule& m) {
              string s = m.cntName() + " (D" + any2str(m.disk()) + ")";
              result[s].setCell(0, m.ring(), TagMaker::makePosTag(m));
            }
          };
          Visitor v(result);
          myModule->accept(v);
        }
        if (byMaterial) { // sort by Material tag
          //nLocalMasses = myModuleCap->localMassCount();
          //nExitingMasses = myModuleCap->exitingMassCount();
          localmassesBegin = myModuleCap->getLocalMasses().begin();
          localmassesEnd = myModuleCap->getLocalMasses().end();
          exitingmassesBegin = myModuleCap->getExitingMasses().begin();
          exitingmassesEnd = myModuleCap->getExitingMasses().end();
        } else { // sort by Component tag
          //nLocalMasses = myModuleCap->localMassCompCount();
          //nExitingMasses = myModuleCap->exitingMassCompCount();
          localmassesBegin = myModuleCap->getLocalMassesComp().begin();
          localmassesEnd = myModuleCap->getLocalMassesComp().end();
          exitingmassesBegin = myModuleCap->getExitingMassesComp().begin();
          exitingmassesEnd = myModuleCap->getExitingMassesComp().end();
        }
        for (std::map<std::string, double>::const_iterator it = localmassesBegin; it != localmassesEnd; ++it) {
          // if (byMaterial) materialTag = myModuleCap->getLocalTag(iLocalMasses); // sort by Material tag
          //  else materialTag = myModuleCap->getLocalTagComp(iLocalMasses);           // sort by Component tag
          materialTag = it->first;
          materialTagIt = find(materialTagV.begin(), materialTagV.end(), materialTag);
          if (materialTagIt==materialTagV.end()) {
            materialTagV.push_back(materialTag);
          }
        }
        for (std::map<std::string, double>::const_iterator it = exitingmassesBegin; it != exitingmassesEnd; ++it) {
          //  if (byMaterial) materialTag = myModuleCap->getExitingTag(iExitingMasses); // sort by Material tag
          //  else materialTag = myModuleCap->getExitingTagComp(iExitingMasses);           // sort by Component tag
          materialTag = it->first;
          materialTagIt = find(materialTagV.begin(), materialTagV.end(), materialTag);
          if (materialTagIt==materialTagV.end()) {
            materialTagV.push_back(materialTag);
          }
        }
      }
    }
  }

  // Alphabetically sort materials
  std::sort(materialTagV.begin(), materialTagV.end());

  // Prepare the columns of the tables
  for (map<string, SummaryTable>::iterator it=result.begin();
       it!=result.end(); ++it) {
    for (unsigned int materialTag_i=0; materialTag_i<materialTagV.size(); ++materialTag_i) {
      it->second.setCell(materialTag_i+1, 0, materialTagV[materialTag_i]);
    }
    it->second.setCell(materialTagV.size()+1, 0, "Total");
  }

  // Now fill the table
  // loop over layers
  for (layerIt = tracker.begin(); layerIt != tracker.end(); ++layerIt) { 
    // Loop over modules
    for (moduleIt = layerIt->begin(); moduleIt != layerIt->end(); ++moduleIt) { 
      // Check if the module is on the YZ section
      myModuleCap = &(*moduleIt);
      myModule = &(myModuleCap->getModule());
      TagMaker tmak(*myModule);

      if (byMaterial) {
        typeWeight[tmak.posTag]+=myModuleCap->getLocalMass();
        typeWeight[tmak.posTag]+=myModuleCap->getExitingMass();
        tagWeight[tmak.sensorGeoTag]+=myModuleCap->getLocalMass();
        tagWeight[tmak.sensorGeoTag]+=myModuleCap->getExitingMass();
      }
      if (myModule->posRef().phi == 1) {
        // If we did not write this module type yet
        pair<int, int> myIndex = make_pair(myModule->tableRef().row/*+myModule->getDisk()*/, myModule->tableRef().col);
        tempString = myModule->cntName();
        if (!typeWritten[tempString][myIndex]) {
          typeWritten[tempString][myIndex]=true;
          if (tempString!="") {
            // TODO: put this in a better place
            // (and make a better module typing)
            tempSS.str("");
            if (myModule->subdet() == BARREL) {
              tempSS << ((BarrelModule*)myModule)->layer();
              tempString+=" (L"+tempSS.str()+")";
            } else if (myModule->subdet() == ENDCAP) {
              tempSS << ((EndcapModule*)myModule)->disk(); //getDisk();
              tempString+=" (D"+tempSS.str()+")";
            } else {
              cerr << "ERROR in Analyzer::detailedWeights(): "
                  << "I found a module which is neither endcap nor barrel!" << std::endl;
            }
            //      cout << "Here's a module: id = " << myModule->getId()
            //           << ", tag = " << myModule->getTag()
            //           << ", type = " << myModule->getType()
            //           << ", ring = " << myModule->getRing()
            //           << endl;
            // std::cout << "Material\tLocal\texiting\n";

            // Then we fill in the proper column
            // Prepare the columns of the table
            for (unsigned int materialTag_i=0; materialTag_i<materialTagV.size(); ++materialTag_i) {
              materialTag = materialTagV[materialTag_i];
              if (byMaterial) { // table by materials
                try { localMaterial = myModuleCap->getLocalMass(materialTag); }
                catch (exception e) { localMaterial = 0; }
                try { exitingMaterial = myModuleCap->getExitingMass(materialTag); }
                catch (exception e) { exitingMaterial = 0; }
              } else { // table by components
                try { localMaterial = myModuleCap->getLocalMassComp(materialTag); }
                catch (exception e) { localMaterial = 0; }
                try { exitingMaterial = myModuleCap->getExitingMassComp(materialTag); }
                catch (exception e) { exitingMaterial = 0; }
              }
              //cout << materialTag << "\t"
              //<< localMaterial << "\t"
              //<< exitingMaterial << "\t" << endl;
              tempSS.str("");
              // TODO: move this to Vizard
              tempSS << std::dec << std::fixed << std::setprecision(1) << localMaterial << "+"
                << std::dec << std::fixed << std::setprecision(1) << exitingMaterial << "="
                << std::dec << std::fixed << std::setprecision(1) << localMaterial+exitingMaterial;
              result[tempString].setCell(materialTag_i+1, myModule->tableRef().col, tempSS.str());
            }
            localMaterial = myModuleCap->getLocalMass();
            exitingMaterial = myModuleCap->getExitingMass();
            tempSS.str("");
            tempSS << std::dec << std::fixed << std::setprecision(1) << localMaterial << "+"
              << std::dec << std::fixed << std::setprecision(1) << exitingMaterial << "="
              << std::dec << std::fixed << std::setprecision(1) << localMaterial+exitingMaterial;
            result[tempString].setCell(materialTagV.size()+1, myModule->tableRef().col, tempSS.str());
          } else {
            cerr << "ERROR in Analyzer::detailedWeights(): "
                << "I found a module with no reference to the container name." << endl;
          }
        }
      }
    }
  }
}

// public
/**
 * Produces a full material summary for the modules
 */
void Analyzer::computeWeightSummary(MaterialBudget& mb) {

  typeWeight.clear();
  tagWeight.clear();
  barrelWeights.clear();
  computeDetailedWeights(mb.getBarrelModuleCaps(), barrelWeights, true);
  endcapWeights.clear();
  computeDetailedWeights(mb.getEndcapModuleCaps(), endcapWeights, true);

  barrelComponentWeights.clear();
  computeDetailedWeights(mb.getBarrelModuleCaps(), barrelComponentWeights, false);
  endcapComponentWeights.clear();
  computeDetailedWeights(mb.getEndcapModuleCaps(), endcapComponentWeights, false);
}

// protected
/**
 * The layer-level analysis function for modules forms the frame for sending a single track through the active modules.
 * It loops through all layers and adds up the results returned from the analysis of each of them.
 * @param tr A reference to the <i>ModuleCap</i> vector of vectors that sits on top of the tracker modules
 * @param eta The pseudorapidity of the track
 * @param theta The track angle in the yz-plane
 * @param phi The track angle in the xy-plane
 * @param t A reference to the current track object
 * @param A boolean flag to indicate which set of active surfaces is analysed: true if the belong to a pixel detector, false if they belong to the tracker
 * @return The summed up radiation and interaction lengths for the given track, bundled into a <i>std::pair</i>
 */
Material Analyzer::analyzeModules(std::vector<std::vector<ModuleCap> >& tr,
                                  double eta, double theta, double phi, Track& t, 
                                  std::map<std::string, Material>& sumComponentsRI,
                                  bool isPixel) {
  std::vector<std::vector<ModuleCap> >::iterator iter = tr.begin();
  std::vector<std::vector<ModuleCap> >::iterator guard = tr.end();
  Material res, tmp;
  res.radiation= 0.0;
  res.interaction = 0.0;
  while (iter != guard) {
    tmp = findModuleLayerRI(*iter, eta, theta, phi, t, sumComponentsRI, isPixel);
    res.radiation= res.radiation+ tmp.radiation;
    res.interaction= res.interaction + tmp.interaction;
    iter++;
  }
  return res;
}

void printPosRefString(std::ostream& os, const Module& m, const string& delim = " ") {
  os << "cnt=" << m.posRef().cnt << delim << "z=" << m.posRef().z << delim << "rho=" << m.posRef().rho << " (" << m.center().Rho() << ")" << delim << "phi=" << m.posRef().phi << delim << "side=" << m.side();
} 

/**
 * The module-level analysis function loops through all modules of a given layer, checking for collisions with the given track.
 * If one is found, the radiation and interaction lengths are scaled with respect to theta, then summed up into a grand total,
 * which is returned. As phi is fixed at the moment and the tracks hit the modules orthogonally with respect to it, it is so far
 * not used to scale the results further.
 * @param layer A reference to the <i>ModuleCap</i> vector linking the collection of material properties to the current layer
 * @param eta The pseudorapidity of the current track
 * @param theta The track angle in the yz-plane
 * @param phi The track angle in the xy-plane
 * @param t A reference to the current track object
 * @param A boolean flag to indicate which set of active surfaces is analysed: true if the belong to a pixel detector, false if they belong to the tracker
 * @return The scaled and summed up radiation and interaction lengths for the given layer and track, bundled into a <i>std::pair</i>
 */
Material Analyzer::findModuleLayerRI(std::vector<ModuleCap>& layer,
                                     double eta, double theta, double phi, Track& t, 
                                     std::map<std::string, Material>& sumComponentsRI,
                                     bool isPixel) {
  std::vector<ModuleCap>::iterator iter = layer.begin();
  std::vector<ModuleCap>::iterator guard = layer.end();
  Material res, tmp;
  XYZVector origin, direction;
  Polar3DVector dir;
  double distance, r;
  int hits = 0;
  res.radiation = 0.0;
  res.interaction = 0.0;
  // set the track direction vector
  dir.SetCoordinates(1, theta, phi);
  direction = dir;
  while (iter != guard) {
    // collision detection: rays are in z+ only, so consider only modules that lie on that side
    // only consider modules that have type BarrelModule or EndcapModule
    if (iter->getModule().maxZ() > 0) {
        // same method as in Tracker, same function used
        // TODO: in case origin==0,0,0 and phi==0 just check if sectionYZ and minEta, maxEta
        //distance = iter->getModule().trackCross(origin, direction);
        auto h = iter->getModule().checkTrackHits(origin, direction);
        if (h.second != HitType::NONE) {
          distance = h.first.R();
          HitType type = h.second;
          // module was hit
          hits++;
          r = distance * sin(theta);
          tmp.radiation = iter->getRadiationLength();
          tmp.interaction = iter->getInteractionLength();

          Module& m = iter->getModule();
          double tiltAngle = m.tiltAngle();
          // 2D material maps
          fillMapRT(r, theta, tmp);
          // radiation and interaction length scaling for barrels
          if (iter->getModule().subdet() == BARREL) {
            tmp.radiation = tmp.radiation / sin(theta + tiltAngle);
            tmp.interaction = tmp.interaction / sin(theta + tiltAngle);
          }
          // radiation and interaction length scaling for endcaps
          else {
            tmp.radiation = tmp.radiation / cos(theta + tiltAngle - M_PI/2);
            tmp.interaction = tmp.interaction / cos(theta + tiltAngle - M_PI/2);
          }

          double tmpr = 0., tmpi = 0.;

          std::map<std::string, Material> moduleComponentsRI = iter->getComponentsRI();
          for (std::map<std::string, Material>::iterator cit = moduleComponentsRI.begin(); cit != moduleComponentsRI.end(); ++cit) {
            sumComponentsRI[cit->first].radiation += cit->second.radiation / (iter->getModule().subdet() == BARREL ? sin(theta + tiltAngle) : cos(theta + tiltAngle - M_PI/2));
            //if (cit->first == "SupportMechanics") std::cout << eta << " " << distance << " " << cit->second.radiation / sin(theta + tiltAngle) << " " << cit->second.radiation << std::endl;
            tmpr += sumComponentsRI[cit->first].radiation;
            sumComponentsRI[cit->first].interaction += cit->second.interaction / (iter->getModule().subdet() == BARREL ? sin(theta + tiltAngle) : cos(theta + tiltAngle - M_PI/2));
            tmpi += sumComponentsRI[cit->first].interaction;
          }
          // 2D plot and eta plot results
          if (!isPixel) fillCell(r, eta, theta, tmp);
          res += tmp;
          // create Hit object with appropriate parameters, add to Track t
          Hit* hit = new Hit(distance, &(iter->getModule()), type);
          //if (iter->getModule().getSubdetectorType() == Module::Barrel) hit->setOrientation(Hit::Horizontal); // should not be necessary
          //else if(iter->getModule().getSubdetectorType() == Module::Endcap) hit->setOrientation(Hit::Vertical); // should not be necessary
          //hit->setObjectKind(Hit::Active); // should not be necessary
          hit->setCorrectedMaterial(tmp);
          hit->setPixel(isPixel);
          t.addHit(hit);
        }
    }
    iter++;
  }
  return res;
}


// protected
/**
 * The layer-level function to find hits on modules and connect them to a track
 * It loops through all layers and adds up the results returned from the analysis of each of them.
 * @param tr A reference to the <i>ModuleCap</i> vector of vectors that sits on top of the tracker modules
 * @param eta The pseudorapidity of the track
 * @param theta The track angle in the yz-plane
 * @param phi The track angle in the xy-plane
 * @param t A reference to the current track object
 * @param A boolean flag to indicate which set of active surfaces is analysed: true if the belong to a pixel detector, false if they belong to the tracker
 * @return The summed up radiation and interaction lengths for the given track, bundled into a <i>std::pair</i>
 */
Material Analyzer::findHitsModules(std::vector<std::vector<ModuleCap> >& tr,
                                   // TODO: add z0 here and in the hit finder for inactive surfaces
                                   double eta, double theta, double phi, Track& t, bool isPixel) {
  std::vector<std::vector<ModuleCap> >::iterator iter = tr.begin();
  std::vector<std::vector<ModuleCap> >::iterator guard = tr.end();
  Material res, tmp;
  res.radiation= 0.0;
  res.interaction = 0.0;
  while (iter != guard) {
    tmp = findHitsModuleLayer(*iter, eta, theta, phi, t, isPixel);
    res.radiation = res.radiation + tmp.radiation;
    res.interaction = res.interaction + tmp.interaction;
    iter++;
  }
  return res;
}

int Analyzer::findHitsModules(Tracker& tracker, double z0, double eta, double theta, double phi, Track& t) {

  Material emptyMaterial;
  XYZVector origin(0,0,z0);
  XYZVector direction;
  Polar3DVector dir;
  double distance;

  int hits = 0;
  emptyMaterial.radiation = 0.0;
  emptyMaterial.interaction = 0.0;

  // set the track direction vector
  dir.SetCoordinates(1, theta, phi);
  direction = dir;

  for (auto aModule : tracker.modules()) {

    // collision detection: rays are in z+ only, so consider only modules that lie on that side
    if (aModule->maxZ() > 0) {

      // same method as in Tracker, same function used
      //distance = aModule->trackCross(origin, direction);
      auto ht = aModule->checkTrackHits(origin, direction);
      //if (distance > 0) {
      if (ht.second != HitType::NONE) {
        double distance = ht.first.R();
        // module was hit
        hits++;

        // create Hit object with appropriate parameters, add to Track t
        Hit* hit = new Hit(distance, aModule, ht.second);
        hit->setCorrectedMaterial(emptyMaterial);
        t.addHit(hit);
      }
    }
  }
  return hits;
}


/**
 * The module-level analysis function loops through all modules of a given layer, finds hits and connects them to the track.
 * If one is found, the radiation and interaction lengths are scaled with respect to theta, then summed up into a grand total,
 * which is returned. As phi is fixed at the moment and the tracks hit the modules orthogonally with respect to it, it is so far
 * not used to scale the results further.
 * @param layer A reference to the <i>ModuleCap</i> vector linking the collection of material properties to the current layer
 * @param eta The pseudorapidity of the current track
 * @param theta The track angle in the yz-plane
 * @param phi The track angle in the xy-plane
 * @param t A reference to the current track object
 * @param A boolean flag to indicate which set of active surfaces is analysed: true if the belong to a pixel detector, false if they belong to the tracker
 * @return The scaled and summed up radiation and interaction lengths for the given layer and track, bundled into a <i>std::pair</i>
 */
Material Analyzer::findHitsModuleLayer(std::vector<ModuleCap>& layer,
                                       double eta, double theta, double phi, Track& t, bool isPixel) {
  std::vector<ModuleCap>::iterator iter = layer.begin();
  std::vector<ModuleCap>::iterator guard = layer.end();
  Material res, tmp;
  XYZVector origin, direction;
  Polar3DVector dir;
  double distance;
  //double r;
  int hits = 0;
  res.radiation = 0.0;
  res.interaction = 0.0;
  // set the track direction vector
  dir.SetCoordinates(1, theta, phi);
  direction = dir;
  while (iter != guard) {
    // collision detection: rays are in z+ only, so consider only modules that lie on that side
    if (iter->getModule().maxZ() > 0) {
        // same method as in Tracker, same function used
        // TODO: in case origin==0,0,0 and phi==0 just check if sectionYZ and minEta, maxEta
        //distance = iter->getModule().trackCross(origin, direction);
        auto h = iter->getModule().checkTrackHits(origin, direction); 
        if (h.second != HitType::NONE) {
        //if (distance > 0) {
          double distance = h.first.R();
          // module was hit
          hits++;
          // r = distance * sin(theta);
          tmp.radiation = iter->getRadiationLength();
          tmp.interaction = iter->getInteractionLength();
          // radiation and interaction length scaling for barrels
          if (iter->getModule().subdet() == BARREL) {
            //std::cout << ">B>> " << tmp.radiation << " " << cos(theta) << std::endl;
            tmp.radiation = tmp.radiation / sin(theta);
            tmp.interaction = tmp.interaction / sin(theta);
          }
          // radiation and interaction length scaling for endcaps
          else {
            //std::cout << ">E>> " << tmp.radiation << " " << cos(theta) << std::endl;
            tmp.radiation = tmp.radiation / cos(theta);
            tmp.interaction = tmp.interaction / cos(theta);
          }
          res += tmp;
          // create Hit object with appropriate parameters, add to Track t
          Hit* hit = new Hit(distance, &(iter->getModule()), h.second);
          //if (iter->getModule().getSubdetectorType() == Module::Barrel) hit->setOrientation(Hit::Horizontal); // should not be necessary
          //else if(iter->getModule().getSubdetectorType() == Module::Endcap) hit->setOrientation(Hit::Vertical); // should not be necessary
          //hit->setObjectKind(Hit::Active); // should not be necessary
          hit->setCorrectedMaterial(tmp);
          hit->setPixel(isPixel);
          t.addHit(hit);
        }
    }
    iter++;
  }
  return res;
}

/**
 * The analysis function for inactive volumes loops through the given vector of elements, checking for collisions with
 * the given track. If one is found, the radiation and interaction lengths are scaled with respect to theta, then summed
 * up into a grand total, which is returned. As all inactive volumes are symmetric with respect to rotation around the
 * z-axis, the track angle phi is not necessary.
 * @param elements A reference to the collection of inactive surfaces that is to be checked for collisions with the track
 * @param eta The pseudorapidity of the current track
 * @param theta The track angle in the yz-plane
 * @param t A reference to the current track object
 * @param cat The category of inactive surfaces that need to be considered within the collection; none if the function is to look at all of them
 * @param A boolean flag to indicate which set of active surfaces is analysed: true if the belong to a pixel detector, false if they belong to the tracker
 * @return The scaled and summed up radiation and interaction lengths for the given collection of elements and track, bundled into a <i>std::pair</i>
 */

Material Analyzer::analyzeInactiveSurfaces(std::vector<InactiveElement>& elements, double eta,
                                           double theta, Track& t, MaterialProperties::Category cat, bool isPixel) {

  /*
  for (InactiveElement& currElem : elements) {
    currElem.calculateTotalMass();
    currElem.calculateRadiationLength();
    currElem.calculateInteractionLength();
  }
  */
  
  std::vector<InactiveElement>::iterator iter = elements.begin();
  std::vector<InactiveElement>::iterator guard = elements.end();
  Material res, corr;
  std::pair<double, double> tmp;
  double s = 0.0;
  while ((iter != guard)) {
    //if  ((iter->getInteractionLength() > 0) && (iter->getRadiationLength() > 0)) {
    // collision detection: rays are in z+ only, so only volumes in z+ need to be considered
    // only volumes of the requested category, or those without one (which should not exist) are examined
    if (((iter->getZOffset() + iter->getZLength()) > 0)
        && ((cat == MaterialProperties::no_cat) || (cat == iter->getCategory()))) {
      // collision detection: check eta range
      tmp = iter->getEtaMinMax();
      // volume was hit
      if ((tmp.first < eta) && (tmp.second > eta)) {
        double r, z;
        /*
        if (eta<0.01) {
          std::cout << "Hitting an inactive surface at z=("
                    << iter->getZOffset() << " to " << iter->getZOffset()+iter->getZLength()
                    << ") r=(" << iter->getInnerRadius() << " to " << iter->getInnerRadius()+iter->getRWidth() << ")" << std::endl;
          const std::map<std::string, double>& localMasses = iter->getLocalMasses();
          const std::map<std::string, double>& exitingMasses = iter->getExitingMasses();
          for (auto massIt : localMasses) std::cerr   << "       localMass" <<  massIt.first << " = " << any2str(massIt.second) << " g" << std::endl;
          for (auto massIt : exitingMasses) std::cerr << "     exitingMass" <<  massIt.first << " = " << any2str(massIt.second) << " g" << std::endl;
        }
        */
        // radiation and interaction lenth scaling for vertical volumes
        if (iter->isVertical()) {
          z = iter->getZOffset() + iter->getZLength() / 2.0;
          r = z * tan(theta);
          // 2D maps for vertical surfaces
          fillMapRZ(r,z,iter->getMaterialLengths());
          // special treatment for user-defined supports as they can be very close to z=0
          if (cat == MaterialProperties::u_sup) {
            s = iter->getZLength() / cos(theta);
            if (s > (iter->getRWidth() / sin(theta))) s = iter->getRWidth() / sin(theta);
            // add the hit if it's declared as inside the tracking volume, add it to 'others' if not
            if (iter->track()) {
              corr.radiation = iter->getRadiationLength() * s / iter->getZLength();
              corr.interaction = iter->getInteractionLength() * s / iter->getZLength();
              res += corr;
              if (!isPixel) {
                Material thisLength;
                thisLength.radiation = iter->getRadiationLength() * s / iter->getZLength();
                thisLength.interaction = iter->getInteractionLength() * s / iter->getZLength(); 
                fillCell(r, eta, theta, thisLength); 
              }
            }
            else {
              if (!isPixel) {
                rextrasupports.Fill(eta, iter->getRadiationLength() * s / iter->getZLength());
                iextrasupports.Fill(eta, iter->getInteractionLength() * s / iter->getZLength());
              }
            }
          }
          else {
            // add the hit if it's declared as inside the tracking volume, add it to 'others' if not
            if (iter->track()) {
              corr.radiation = iter->getRadiationLength() / cos(theta);
              corr.interaction = iter->getInteractionLength() / cos(theta);
              res += corr;
              if (!isPixel) {
                Material thisLength;
                thisLength.radiation = iter->getRadiationLength() / cos(theta); 
                thisLength.interaction = iter->getInteractionLength() / cos(theta);
                fillCell(r, eta, theta, thisLength);
              }
            }
            else {
              if (!isPixel) {
                if ((iter->getCategory() == MaterialProperties::b_ser)
                    || (iter->getCategory() == MaterialProperties::e_ser)) {
                  rextraservices.Fill(eta, iter->getRadiationLength() / cos(theta));
                  iextraservices.Fill(eta, iter->getInteractionLength() / cos(theta));
                }
                else if ((iter->getCategory() == MaterialProperties::b_sup)
                         || (iter->getCategory() == MaterialProperties::e_sup)
                         || (iter->getCategory() == MaterialProperties::o_sup)
                         || (iter->getCategory() == MaterialProperties::t_sup)) {
                  rextrasupports.Fill(eta, iter->getRadiationLength() / cos(theta));
                  iextrasupports.Fill(eta, iter->getInteractionLength() / cos(theta));
                }
              }
            }
          }
        }
        // radiation and interaction length scaling for horizontal volumes
        else {
          r = iter->getInnerRadius() + iter->getRWidth() / 2.0;
          // 2D maps for horizontal surfaces
          fillMapRT(r,theta,iter->getMaterialLengths());
          // special treatment for user-defined supports; should not be necessary for now
          // as all user-defined supports are vertical, but just in case...
          if (cat == MaterialProperties::u_sup) {
            s = iter->getZLength() / sin(theta);
            if (s > (iter->getRWidth() / cos(theta))) s = iter->getRWidth() / cos(theta);
            // add the hit if it's declared as inside the tracking volume, add it to 'others' if not
            if (iter->track()) {
              corr.radiation = iter->getRadiationLength() * s / iter->getZLength();
              corr.interaction = iter->getInteractionLength() * s / iter->getZLength();
              res += corr;
              if (!isPixel) {
                Material thisLength;
                thisLength.radiation = iter->getRadiationLength() * s / iter->getZLength(); 
                thisLength.interaction = iter->getInteractionLength() * s / iter->getZLength();
                fillCell(r, eta, theta, thisLength);
              }
            }
            else {
              if (!isPixel) {
                rextrasupports.Fill(eta, iter->getRadiationLength() * s / iter->getZLength());
                iextrasupports.Fill(eta, iter->getInteractionLength() * s / iter->getZLength());
              }
            }
          }
          else {
            // add the hit if it's declared as inside the tracking volume, add it to 'others' if not
            if (iter->track()) {
              corr.radiation = iter->getRadiationLength() / sin(theta);
              corr.interaction = iter->getInteractionLength() / sin(theta);
              res += corr;
              if (!isPixel) {
                Material thisLength;
                thisLength.radiation = iter->getRadiationLength() / sin(theta);
                thisLength.interaction =  iter->getInteractionLength() / sin(theta);
                fillCell(r, eta, theta, thisLength); 
              }
            }
            else {
              if (!isPixel) {
                if ((iter->getCategory() == MaterialProperties::b_ser)
                    || (iter->getCategory() == MaterialProperties::e_ser)) {
                  rextraservices.Fill(eta, iter->getRadiationLength() / sin(theta));
                  iextraservices.Fill(eta, iter->getInteractionLength() / sin(theta));
                }
                else if ((iter->getCategory() == MaterialProperties::b_sup)
                         || (iter->getCategory() == MaterialProperties::e_sup)
                         || (iter->getCategory() == MaterialProperties::o_sup)
                         || (iter->getCategory() == MaterialProperties::t_sup)) {
                  rextrasupports.Fill(eta, iter->getRadiationLength() / sin(theta));
                  iextrasupports.Fill(eta, iter->getInteractionLength() / sin(theta));
                }
              }
            }
          }
        }
        // create Hit object with appropriate parameters, add to Track t
        Hit* hit = new Hit((theta == 0) ? r : (r / sin(theta)));
        if (iter->isVertical()) hit->setOrientation(Hit::Vertical);
        else hit->setOrientation(Hit::Horizontal);
        hit->setObjectKind(Hit::Inactive);
        hit->setCorrectedMaterial(corr);
        hit->setPixel(isPixel);
        t.addHit(hit);
      }
    }
    iter++;
    //}
  }
  return res;
}

/**
 * The analysis function for inactive volumes loops through the given vector of elements, checking for collisions with
 * the given track. If one is found, the radiation and interaction lengths are scaled with respect to theta.
 * All hits are added to the given track
 * The total crossed material is returned.
 * As all inactive volumes are symmetric with respect to rotation around the z-axis, the track angle phi is not necessary.
 * @param elements A reference to the collection of inactive surfaces that is to be checked for collisions with the track
 * @param eta The pseudorapidity of the current track
 * @param theta The track angle in the yz-plane
 * @param t A reference to the current track object
 * @return The scaled and summed up crossed material amount
 */
Material Analyzer::findHitsInactiveSurfaces(std::vector<InactiveElement>& elements, double eta,
                                            double theta, Track& t, bool isPixel) {
  std::vector<InactiveElement>::iterator iter = elements.begin();
  std::vector<InactiveElement>::iterator guard = elements.end();
  Material res, corr;
  std::pair<double, double> tmp;
  double s_normal = 0;
  double s_alternate = 0;
  while (iter != guard) {
    // Collision detection: rays are in z+ only, so only volumes in z+ need to be considered
    // only volumes of the requested category, or those without one (which should not exist) are examined
    if ((iter->getZOffset() + iter->getZLength()) > 0) {
      // collision detection: check eta range
      tmp = iter->getEtaMinMax();
      // Volume was hit if:
      if ((tmp.first < eta) && (tmp.second > eta)) {
        double r, z;
        // radiation and interaction lenth scaling for vertical volumes
        if (iter->isVertical()) { // Element is vertical
          z = iter->getZOffset() + iter->getZLength() / 2.0;
          r = z * tan(theta);

          // In case we are crossing the material with a very shallow angle
          // we have to take into account its finite radial size
          s_normal = iter->getZLength() / cos(theta);
          s_alternate = iter->getRWidth() / sin(theta);
          if (s_normal > s_alternate) { 
            // Special case: it's easier to cross the material by going left-to-right than
            // by going bottom-to-top, so I have to rescale the material amount computation
            corr.radiation = iter->getRadiationLength() / iter->getZLength() * s_alternate;
            corr.interaction = iter->getInteractionLength() / iter->getZLength() * s_alternate;
            res += corr;
          } else {
            // Standard computing of the crossed material amount
            corr.radiation = iter->getRadiationLength() / cos(theta);
            corr.interaction = iter->getInteractionLength() / cos(theta);
            res += corr;
          }
        }
        // radiation and interaction length scaling for horizontal volumes
        else { // Element is horizontal
          r = iter->getInnerRadius() + iter->getRWidth() / 2.0;

          // In case we are crossing the material with a very shallow angle
          // we have to take into account its finite z length
          s_normal = iter->getRWidth() / sin(theta);
          s_alternate = iter->getZLength() / cos(theta);
          if (s_normal > s_alternate) { 
            // Special case: it's easier to cross the material by going left-to-right than
            // by going bottom-to-top, so I have to rescale the material amount computation
            corr.radiation = iter->getRadiationLength() / iter->getRWidth() * s_alternate;
            corr.interaction = iter->getInteractionLength() / iter->getRWidth() * s_alternate;
            res += corr;
          } else {
            // Standard computing of the crossed material amount
            corr.radiation = iter->getRadiationLength() / sin(theta);
            corr.interaction = iter->getInteractionLength() / sin(theta);
            res += corr;
          }
        }
        // create Hit object with appropriate parameters, add to Track t
        Hit* hit = new Hit((theta == 0) ? r : (r / sin(theta)));
        if (iter->isVertical()) hit->setOrientation(Hit::Vertical);
        else hit->setOrientation(Hit::Horizontal);
        hit->setObjectKind(Hit::Inactive);
        hit->setCorrectedMaterial(corr);
        hit->setPixel(isPixel);
        t.addHit(hit);
      }
    }
    iter++;
  }
  return res;
}
  
  
void Analyzer::clearGraphs_Pt(int graphAttributes, const std::string& graphTag) {
  std::map<int, TGraph>& thisRhoGraphs_Pt      = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::RhoGraph_Pt      ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::RhoGraph_Pt     , graphTag);
  std::map<int, TGraph>& thisPhiGraphs_Pt      = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::PhiGraph_Pt      ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::PhiGraph_Pt     , graphTag);
  std::map<int, TGraph>& thisDGraphs_Pt        = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::DGraph_Pt        ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::DGraph_Pt       , graphTag);
  std::map<int, TGraph>& thisCtgThetaGraphs_Pt = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::CtgthetaGraph_Pt ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::CtgthetaGraph_Pt, graphTag);
  std::map<int, TGraph>& thisZ0Graphs_Pt       = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::Z0Graph_Pt       ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::Z0Graph_Pt      , graphTag);
  std::map<int, TGraph>& thisPGraphs_Pt        = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::PGraph_Pt        ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::PGraph_Pt       , graphTag);
  std::map<int, TGraph>& thisPGraphsDipole_Pt  = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::PGraphDipole_Pt  ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::PGraphDipole_Pt , graphTag);


  thisRhoGraphs_Pt.clear();
  thisPhiGraphs_Pt.clear();
  thisDGraphs_Pt.clear();
  thisCtgThetaGraphs_Pt.clear();
  thisZ0Graphs_Pt.clear();
  thisPGraphs_Pt.clear();
  thisPGraphsDipole_Pt.clear();
}

void Analyzer::clearGraphs_P(int graphAttributes, const std::string& graphTag) {
  std::map<int, TGraph>& thisRhoGraphs_P      = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::RhoGraph_P      ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::RhoGraph_P     , graphTag);
  std::map<int, TGraph>& thisPhiGraphs_P      = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::PhiGraph_P      ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::PhiGraph_P     , graphTag);
  std::map<int, TGraph>& thisDGraphs_P        = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::DGraph_P        ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::DGraph_P       , graphTag);
  std::map<int, TGraph>& thisCtgThetaGraphs_P = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::CtgthetaGraph_P ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::CtgthetaGraph_P, graphTag);
  std::map<int, TGraph>& thisZ0Graphs_P       = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::Z0Graph_P       ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::Z0Graph_P      , graphTag);
  std::map<int, TGraph>& thisPGraphs_P        = graphTag.empty() ? myGraphBag.getGraphs(graphAttributes | GraphBag::PGraph_P        ) : myGraphBag.getTaggedGraphs(graphAttributes | GraphBag::PGraph_P       , graphTag);

  thisRhoGraphs_P.clear();
  thisPhiGraphs_P.clear();
  thisDGraphs_P.clear();
  thisCtgThetaGraphs_P.clear();
  thisZ0Graphs_P.clear();
  thisPGraphs_P.clear();
  
}
 
 
/**
 * Calculate the error graphs for case I with const Pt across eta: the radius curvature, the distance and the angle, for each momentum,
 * and store them internally for later visualisation.
 * @param parameter The list of different momenta that the error graphs are calculated for
 */
void Analyzer::calculateGraphsConstPt(const int& parameter,
                                      const TrackCollection& aTrackCollection,
                                      int graphAttributes,
                                      const string& graphTag) {

  // Get graphs from graphBag
  TGraph& thisRhoGraph_Pt       = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::RhoGraph_Pt     , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::RhoGraph_Pt       , graphTag, parameter);
  TGraph& thisPhiGraph_Pt       = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::PhiGraph_Pt     , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::PhiGraph_Pt       , graphTag, parameter);
  TGraph& thisDGraph_Pt         = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::DGraph_Pt       , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::DGraph_Pt         , graphTag, parameter);
  TGraph& thisCtgThetaGraph_Pt  = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::CtgthetaGraph_Pt, parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::CtgthetaGraph_Pt  , graphTag, parameter);
  TGraph& thisZ0Graph_Pt        = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::Z0Graph_Pt      , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::Z0Graph_Pt        , graphTag, parameter);
  TGraph& thisPGraph_Pt         = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::PGraph_Pt       , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::PGraph_Pt         , graphTag, parameter);

  // Variables
  double eta, R;
  std::ostringstream aName;

  // Prepare plot for const pt across eta
  double momentum = double(parameter)/1000.;
  double magField = SimParms::getInstance()->magneticField();

  // Prepare plots: pt
  thisRhoGraph_Pt.SetTitle("p_{T} resolution versus #eta - const P_{T} across #eta;#eta;#delta p_{T}/p_{T} [%]");
  aName.str(""); aName << "pt_vs_eta" << momentum << graphTag;
  thisRhoGraph_Pt.SetName(aName.str().c_str());
  // Prepare plots: phi
  thisPhiGraph_Pt.SetTitle("Track azimuthal angle error - const P_{T} across #eta;#eta;#delta #phi [deg]");
  aName.str(""); aName << "phi_vs_eta" << momentum << graphTag;
  thisPhiGraph_Pt.SetName(aName.str().c_str());
  // Prepare plots: d
  thisDGraph_Pt.SetTitle("Transverse impact parameter error - const P_{T} across #eta;#eta;#delta d_{0} [#mum]");
  aName.str(""); aName << "d_vs_eta" << momentum << graphTag;
  thisDGraph_Pt.SetName(aName.str().c_str());
  // Prepare plots: ctg(theta)
  thisCtgThetaGraph_Pt.SetTitle("Track polar angle error - const P_{T} across #eta;#eta;#delta ctg(#theta)");
  aName.str(""); aName << "ctgTheta_vs_eta" << momentum << graphTag;
  thisCtgThetaGraph_Pt.SetName(aName.str().c_str());
  // Prepare plots: z0
  thisZ0Graph_Pt.SetTitle("Longitudinal impact parameter error - const P_{T} across #eta;#eta;#delta z_{0} [#mum]");
  aName.str(""); aName << "z_vs_eta" << momentum << graphTag;
  thisZ0Graph_Pt.SetName(aName.str().c_str());
  // Prepare plots: p
  thisPGraph_Pt.SetTitle("p resolution versus #eta - const P_{T} across #eta;#eta;#delta p/p [%]");
  aName.str(""); aName << "p_vs_eta" << momentum << graphTag;
  thisPGraph_Pt.SetName(aName.str().c_str());
 
  // Add analytical resolution in dipole region
  TGraph* thisPGraphDipole_Pt = nullptr;
  TGraph* thisPGraphTotal_Pt  = nullptr;
  if (SimParms::getInstance()->dipoleMagneticField()!=0.0 && graphAttributes==GraphBag::RealGraph) {

    thisPGraphDipole_Pt = &(graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::PGraphDipole_Pt, parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::PGraphDipole_Pt, graphTag, parameter));
    thisPGraphTotal_Pt  = &(graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::PGraphTotal_Pt, parameter )  : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::PGraphTotal_Pt, graphTag, parameter));

    thisPGraphDipole_Pt->SetTitle("Dipole p_{T} resolution versus #eta - const P_{T} across #eta;#eta;#delta p_{T}/p_{T} [%]");
    aName.str(""); aName << "Dipole_pt_vs_eta" << momentum << graphTag;
    thisPGraphDipole_Pt->SetName(aName.str().c_str());

    thisPGraphTotal_Pt->SetTitle("Total (Dipole+Central) p_{T} resolution versus #eta - const P_{T} across #eta;#eta;#delta p_{T}/p_{T} [%]");
    aName.str(""); aName << "Total_pt_vs_eta" << momentum << graphTag;
    thisPGraphTotal_Pt->SetName(aName.str().c_str());
  }

  // track loop
  double graphValue;
  for ( const auto& myTrack : aTrackCollection ) {
    const double& drho = myTrack.getDeltaRho();
    const double& dphi = myTrack.getDeltaPhi();
    const double& dd   = myTrack.getDeltaD();
    const double& dctg = myTrack.getDeltaCtgTheta();
    const double& dz0  = myTrack.getDeltaZ0();
    const double& dp   = myTrack.getDeltaP();
    eta = myTrack.getEta();
    R = myTrack.getTransverseMomentum() / magField / 0.3 * 1E3; // radius in mm
    if (drho>0) {
      // deltaRho / rho = deltaRho * R
      graphValue = (drho * R) * 100; // in percent
      thisRhoGraph_Pt.SetPoint(thisRhoGraph_Pt.GetN(), eta, graphValue);
    }

    if (dphi>0) {
      graphValue = dphi/M_PI*180.; // in degrees
      thisPhiGraph_Pt.SetPoint(thisPhiGraph_Pt.GetN(), eta, graphValue);
    }

    if (dd>0) {
      graphValue = dd * 1000.; // in um
      thisDGraph_Pt.SetPoint(thisDGraph_Pt.GetN(), eta, graphValue );
    }

    if (dctg>0) {
      graphValue = dctg; // An absolute number
      thisCtgThetaGraph_Pt.SetPoint(thisCtgThetaGraph_Pt.GetN(), eta, graphValue);
    }

    if (dz0>0) {
      graphValue =  (dz0) * 1000.; // in um
      thisZ0Graph_Pt.SetPoint(thisZ0Graph_Pt.GetN(), eta, graphValue);
    }

    if ((dp>0)||true) {

      // Central region value
      double centralValue = dp * 100.; // in percent
      thisPGraph_Pt.SetPoint(thisPGraph_Pt.GetN(), eta, centralValue);

      if (SimParms::getInstance()->dipoleMagneticField()!=0.0 && thisPGraphDipole_Pt) {

        // Dipole region value
        double momentum   = parameter/1000.; // internally in MeV -> convert

        // At 10TeV, let's require deltaPl/Pl resolution at 10TeV -> calculate dTheta deltaPl/Pl = Pl/0.3/(B.l) * dTheta
        double deltaTheta = SimParms::getInstance()->dipoleDPlResAt10TeV()/10000*0.3*SimParms::getInstance()->dipoleMagneticField();

        // DeltaPt/Pt for multiple scattering
        double deltaMS = 0.0136/0.3/SimParms::getInstance()->dipoleMagneticField()*sqrt(SimParms::getInstance()->dipoleXToX0());

        // DeltaPt/Pt for tracking
        double deltaTrack = deltaTheta/0.3/SimParms::getInstance()->dipoleMagneticField()*momentum*sinh(eta);

        // DeltaPt/Pt for cotg theta variance due to MS
//        double matBudget[73] = {
//        0.997153,
//        0.891669,
//        1.02299 ,
//        1.17411 ,
//        1.06964 ,
//        1.0618  ,
//        1.13002 ,
//        1.2646  ,
//        1.12848 ,
//        1.29534 ,
//        1.37321 ,
//        1.14493 ,
//        1.34557 ,
//        1.2456  ,
//        1.42435 ,
//        1.56226 ,
//        1.43541 ,
//        0.902052,
//        1.24526 ,
//        0.998822,
//        1.03633 ,
//        1.24818 ,
//        0.954545,
//        1.04802 ,
//        0.927624,
//        1.12035 ,
//        1.05374 ,
//        0.95252 ,
//        0.942077,
//        0.820696,
//        0.525223,
//        0.587456,
//        0.525377,
//        0.566011,
//        0.559251,
//        0.603952,
//        0.542715,
//        0.547602,
//        0.556393,
//        0.558118,
//        0.600371,
//        0.580681,
//        0.572266,
//        0.611737,
//        0.560034,
//        0.596506,
//        0.622353,
//        0.549761,
//        0.558072,
//        0.574113,
//        0.557646,
//        0.54895 ,
//        0.573662,
//        0.543978,
//        0.566047,
//        0.577682,
//        0.542319,
//        0.536832,
//        0.550271,
//        0.549735,
//        0.549892,
//        0.565413,
//        0.578039,
//        0.565851,
//        0.558163,
//        0.576971,
//        0.593044,
//        0.591792,
//        0.609854,
//        0.632643,
//        0.630939,
//        0.667002,
//        0.678685};
//
//        double currentMB = 0.0;
//        double binWidth  = (6.000 - 2.496)/73;
//        int    iBin      = (eta - 2.496)/binWidth;
//        if (iBin>=0 && iBin<=72) currentMB = matBudget[iBin];
//
//        currentMB = 0.00286 + 2*0.03;
//
//        double deltaCTMS = 0.0136/momentum/cos(myTrack.getTheta())*sqrt(currentMB/sin(myTrack.getTheta()));
//
//        std::cout << "> " << deltaCTMS << " ";

        double deltaCTMS = sin(myTrack.getTheta())/cos(myTrack.getTheta())*myTrack.getDeltaCtgTheta();

        //std::cout << deltaCTMS << std::endl;
        // Dipole value
        double dipoleValue = sqrt(deltaMS*deltaMS + deltaTrack*deltaTrack + deltaCTMS*deltaCTMS) * 100.;

        //std::cout << "Dipole calc --> Theta: " << myTrack.getTheta() << " DipoleValue:"  << sqrt(deltaMS*deltaMS + deltaTrack*deltaTrack)*100 << " CotgContrib: " << sqrt(deltaMS*deltaMS + deltaTrack*deltaTrack + deltaCTMS*deltaCTMS) * 100 << std::endl;

        // Final
        graphValue = dipoleValue;
        thisPGraphDipole_Pt->SetPoint(thisPGraph_Pt.GetN(), eta, graphValue);

        // Beyond eta>=2.5 take advantage of both
        if (eta>=insur::geom_range_eta_regions[2]) graphValue = dipoleValue < centralValue ? dipoleValue : centralValue;
        //if (eta>=insur::geom_range_eta_regions[2]) graphValue = dipoleValue;
        // Just central works
        else graphValue = centralValue;

        // For debugging purposes
        //std::cout << ">>>> " << momentum << " " << eta << " " << deltaTheta << " " << deltaTrack << " " << deltaMS << " " << dipoleValue << " " << centralValue << " > " << graphValue << std::endl;

        thisPGraphTotal_Pt->SetPoint(thisPGraph_Pt.GetN(), eta, graphValue);
      }
    }
  }
}

/**
 * Calculate the error graphs for case II with const P across eta: the radius curvature, the distance and the angle, for each momentum,
 * and store them internally for later visualisation.
 * @param parameter The list of different momenta that the error graphs are calculated for
 */
void Analyzer::calculateGraphsConstP(const int& parameter,
                                     const TrackCollection& aTrackCollection,
                                     int graphAttributes,
                                     const string& graphTag) {

  // Get graphs from graphBag
  TGraph& thisRhoGraph_P       = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::RhoGraph_P     , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::RhoGraph_P       , graphTag, parameter);
  TGraph& thisPhiGraph_P       = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::PhiGraph_P     , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::PhiGraph_P       , graphTag, parameter);
  TGraph& thisDGraph_P         = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::DGraph_P       , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::DGraph_P         , graphTag, parameter);
  TGraph& thisCtgThetaGraph_P  = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::CtgthetaGraph_P, parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::CtgthetaGraph_P  , graphTag, parameter);
  TGraph& thisZ0Graph_P        = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::Z0Graph_P      , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::Z0Graph_P        , graphTag, parameter);
  TGraph& thisPGraph_P         = graphTag.empty() ? myGraphBag.getGraph(graphAttributes | GraphBag::PGraph_P       , parameter ) : myGraphBag.getTaggedGraph(graphAttributes | GraphBag::PGraph_P         , graphTag, parameter);

  // Variables
  double eta, R;
  std::ostringstream aName;

  // Prepare plot for const pt across eta
  double momentum = double(parameter)/1000.;
  double magField = SimParms::getInstance()->magneticField();

  // Prepare plots: pt
  thisRhoGraph_P.SetTitle("p_{T} resolution versus #eta - const P across #eta;#eta;#delta p_{T}/p_{T} [%]");
  aName.str(""); aName << "pt_vs_eta" << momentum << graphTag;
  thisRhoGraph_P.SetName(aName.str().c_str());
  // Prepare plots: phi
  thisPhiGraph_P.SetTitle("Track azimuthal angle error - const P across #eta;#eta;#delta #phi [deg]");
  aName.str(""); aName << "phi_vs_eta" << momentum << graphTag;
  thisPhiGraph_P.SetName(aName.str().c_str());
  // Prepare plots: d
  thisDGraph_P.SetTitle("Transverse impact parameter error - const P across #eta;#eta;#delta d_{0} [#mum]");
  aName.str(""); aName << "d_vs_eta" << momentum << graphTag;
  thisDGraph_P.SetName(aName.str().c_str());
  // Prepare plots: ctg(theta)
  thisCtgThetaGraph_P.SetTitle("Track polar angle error - const P across #eta;#eta;#delta ctg(#theta)");
  aName.str(""); aName << "ctgTheta_vs_eta" << momentum << graphTag;
  thisCtgThetaGraph_P.SetName(aName.str().c_str());
  // Prepare plots: z0
  thisZ0Graph_P.SetTitle("Longitudinal impact parameter error - const P across #eta;#eta;#delta z_{0} [#mum]");
  aName.str(""); aName << "z_vs_eta" << momentum << graphTag;
  thisZ0Graph_P.SetName(aName.str().c_str());
  // Prepare plots: p
  thisPGraph_P.SetTitle("p resolution versus #eta - const P across #eta;#eta;#delta p/p [%]");
  aName.str(""); aName << "p_vs_eta" << momentum << graphTag;
  thisPGraph_P.SetName(aName.str().c_str());

  // track loop
  double graphValue;
  for ( const auto& myTrack : aTrackCollection ) {
    const double& drho = myTrack.getDeltaRho();
    const double& dphi = myTrack.getDeltaPhi();
    const double& dd   = myTrack.getDeltaD();
    const double& dctg = myTrack.getDeltaCtgTheta();
    const double& dz0  = myTrack.getDeltaZ0();
    const double& dp   = myTrack.getDeltaP();
    eta = myTrack.getEta();
    R = myTrack.getTransverseMomentum() / magField / 0.3 * 1E3; // radius in mm
    if (drho>0) {
      // deltaRho / rho = deltaRho * R
      graphValue = (drho * R) * 100; // in percent
      thisRhoGraph_P.SetPoint(thisRhoGraph_P.GetN(), eta, graphValue);
    }

    if (dphi>0) {
      graphValue = dphi/M_PI*180; // in degrees
      thisPhiGraph_P.SetPoint(thisPhiGraph_P.GetN(), eta, graphValue);
    }

    if (dd>0) {
      graphValue = dd * 1000.; // in um
      thisDGraph_P.SetPoint(thisDGraph_P.GetN(), eta, graphValue );
    }

    if (dctg>0) {
      graphValue = dctg; // An absolute number
      thisCtgThetaGraph_P.SetPoint(thisCtgThetaGraph_P.GetN(), eta, graphValue);
    }

    if (dz0>0) {
      graphValue =  (dz0) * 1000.; // in um
      thisZ0Graph_P.SetPoint(thisZ0Graph_P.GetN(), eta, graphValue);
    }

    if ((dp>0)||true) {
      graphValue = dp * 100.; // in percent
      thisPGraph_P.SetPoint(thisPGraph_P.GetN(), eta, graphValue);
    }
  }
}
 
/**
 * This convenience function resets and empties all histograms for the
 * material budget, so they are ready for a new round of analysis.
 */
void Analyzer::clearMaterialBudgetHistograms() {
  // single category
  ractivebarrel.Reset();
  ractivebarrel.SetNameTitle("ractivebarrels", "Barrel Modules Radiation Length");
  ractiveendcap.Reset();
  ractiveendcap.SetNameTitle("ractiveendcap", "Endcap Modules Radiation Length");
  rserfbarrel.Reset();
  rserfbarrel.SetNameTitle("rserfbarrel", "Barrel Services Radiation Length");
  rserfendcap.Reset();
  rserfendcap.SetNameTitle("rserfendcap", "Endcap Services Radiation Length");
  rlazybarrel.Reset();
  rlazybarrel.SetNameTitle("rlazybarrel", "Barrel Supports Radiation Length");
  rlazyendcap.Reset();
  rlazyendcap.SetNameTitle("rlazyendcap", "Endcap Supports Radiation Length");
  rlazytube.Reset();
  rlazytube.SetNameTitle("rlazytube", "Support Tubes Radiation Length");
  rlazyuserdef.Reset();
  rlazyuserdef.SetNameTitle("rlazyuserdef", "Userdefined Supports Radiation Length");
  rbeampipe.Reset();
  rbeampipe.SetNameTitle("rbeampipe", "Beam pipe Radiation Length");
  iactivebarrel.Reset();
  iactivebarrel.SetNameTitle("iactivebarrel", "Barrel Modules Interaction Length");
  iactiveendcap.Reset();
  iactiveendcap.SetNameTitle("iactiveendcap", "Endcap Modules Interaction Length");
  iserfbarrel.Reset();
  iserfbarrel.SetNameTitle("iserfbarrel", "Barrel Services Interaction Length");
  iserfendcap.Reset();
  iserfendcap.SetNameTitle("iserfendcap", "Endcap Services Interaction Length");
  ilazybarrel.Reset();
  ilazybarrel.SetNameTitle("ilazybarrel", "Barrel Supports Interaction Length");
  ilazyendcap.Reset();
  ilazyendcap.SetNameTitle("ilazyendcap", "Endcap Supports Interaction Length");
  ilazytube.Reset();
  ilazytube.SetNameTitle("ilazytube", "Support Tubes Interaction Length");
  ilazyuserdef.Reset();
  ilazyuserdef.SetNameTitle("ilazyuserdef", "Userdefined Supports Interaction Length");
  ibeampipe.Reset();
  ibeampipe.SetNameTitle("ibeampipe", "Beam pipe Ineraction Length");
  // composite
  rbarrelall.Reset();
  rbarrelall.SetNameTitle("rbarrelall", "Barrel Radiation Length");
  rendcapall.Reset();
  rendcapall.SetNameTitle("rendcapall", "Endcap Radiation Length");
  ractiveall.Reset();
  ractiveall.SetNameTitle("ractiveall", "Modules Radiation Length");
  rserfall.Reset();
  rserfall.SetNameTitle("rserfall", "Services Radiation Length");
  rlazyall.Reset();
  rlazyall.SetNameTitle("rlazyall", "Supports Radiation Length");
  ibarrelall.Reset();
  ibarrelall.SetNameTitle("ibarrelall", "Barrel Interaction Length");
  iendcapall.Reset();
  iendcapall.SetNameTitle("iendcapall", "Endcap Interaction Length");
  iactiveall.Reset();
  iactiveall.SetNameTitle("iactiveall", "Modules Interaction Length");
  iserfall.Reset();
  iserfall.SetNameTitle("iserfall", "Services Interaction Length");
  ilazyall.Reset();
  ilazyall.SetNameTitle("ilazyall", "Supports Interaction Length");
  // outside tracking volume
  rextraservices.Reset();
  rextraservices.SetNameTitle("rextraservices", "Services Outside Tracking Volume: Radiation Length");
  rextrasupports.Reset();
  rextrasupports.SetNameTitle("rextrasupports", "Supports Outside Tracking Volume: Radiation Length");
  iextraservices.Reset();
  iextraservices.SetNameTitle("iextraservices", "Services Outside Tracking Volume: Interaction Length");
  iextrasupports.Reset();
  iextrasupports.SetNameTitle("iextrasupports", "Supports Outside Tracking Volume: Interaction Length");
  // global
  rglobal.Reset();
  rglobal.SetNameTitle("rglobal", "Overall Radiation Length");
  iglobal.Reset();
  iglobal.SetNameTitle("iglobal", "Overall Interaction Length");
  // isolines
  isor.Reset();
  isor.SetNameTitle("isor", "Radiation Length Contours");
  isoi.Reset();
  isoi.SetNameTitle("isoi", "Interaction Length Contours");
  mapRadiation.Reset();
  mapRadiation.SetName("mapRadiation");
  mapRadiation.SetTitle("Radiation length map (raw);z [mm];r [mm]");
  mapInteraction.Reset();
  mapInteraction.SetName("mapInteraction");
  mapInteraction.SetTitle("Interaction length map (raw);z [mm];r [mm]");
  mapRadiationCount.Reset();
  mapRadiationCount.SetName("mapRadiationCount");
  mapRadiationCount.SetTitle("Radiation length hit count map;z [mm];r [mm]");
  mapInteractionCount.Reset();
  mapInteractionCount.SetName("mapInteractionCount");
  mapInteractionCount.SetTitle("Interaction length hit count map;z [mm];r [mm]");
  mapRadiationCalib.Reset();
  mapRadiationCalib.SetName("mapRadiationCalib");
  mapRadiationCalib.SetTitle("Radiation length map;z [mm];r [mm]");
  mapInteractionCalib.Reset();
  mapInteractionCalib.SetName("mapInteractionCalib");
  mapInteractionCalib.SetTitle("Interaction length map;z [mm];r [mm]");

  // Nuclear interactions
  while (hadronTotalHitsGraph.GetN()) hadronTotalHitsGraph.RemovePoint(0);
  hadronTotalHitsGraph.SetName("hadronTotalHitsGraph");
  while (hadronAverageHitsGraph.GetN()) hadronAverageHitsGraph.RemovePoint(0);
  hadronAverageHitsGraph.SetName("hadronAverageHitsGraph");

  // Clear the list of requested good hadron hits
  hadronNeededHitsFraction.clear();
  hadronGoodTracksFraction.clear();

  hadronNeededHitsFraction.push_back(ZeroHitsRequired);
  hadronNeededHitsFraction.push_back(OneHitRequired);
  //hadronNeededHitsFraction.push_back(.33);
  hadronNeededHitsFraction.push_back(.66);
  hadronNeededHitsFraction.push_back(1);

  std::sort(hadronNeededHitsFraction.begin(),
            hadronNeededHitsFraction.end());

  // Prepare the plots for the track survival fraction
  ostringstream tempSS;
  string tempString;
  TGraph* myGraph;
  for (unsigned int i=0;
       i<hadronNeededHitsFraction.size();
       ++i) {
    tempSS.str("");
    tempSS << "hadronGoodTracksFraction_at"
      << hadronNeededHitsFraction.at(i);
    tempString = tempSS.str();
    myGraph = new TGraph();
    myGraph->SetName(tempString.c_str());
    hadronGoodTracksFraction.push_back(*myGraph);
  }
}


/**
 * This convenience function computes the trigger performance maps
 * the trigger performance, so they are ready for a new round of
 * analysis.
 */  
void Analyzer::fillTriggerPerformanceMaps(Tracker& tracker) {
  std::map<double, TH2D>& efficiencyMaps = myMapBag.getMaps(mapBag::efficiencyMap);
  std::map<double, TH2D>& thresholdMaps = myMapBag.getMaps(mapBag::thresholdMap);

  TH2D& thicknessMap = myMapBag.getMaps(mapBag::thicknessMap)[mapBag::dummyMomentum];
  TH2D& windowMap = myMapBag.getMaps(mapBag::windowMap)[mapBag::dummyMomentum];
  TH2D& suggestedSpacingMap = myMapBag.getMaps(mapBag::suggestedSpacingMap)[mapBag::dummyMomentum];
  TH2D& suggestedSpacingMapAW = myMapBag.getMaps(mapBag::suggestedSpacingMapAW)[mapBag::dummyMomentum];
  TH2D& nominalCutMap = myMapBag.getMaps(mapBag::nominalCutMap)[mapBag::dummyMomentum]; 

  // Compute the trigger efficiency maps. The pT values are given by
  // the maps
  for (std::map<double, TH2D>::iterator it = efficiencyMaps.begin();
       it!=efficiencyMaps.end(); ++it) {
    double myPt = it->first;
    TH2D& myMap = it->second;

    // Reset the our map, in case it is not empty
    for (int i=1; i<=myMap.GetNbinsX(); ++i)
      for (int j=1; j<=myMap.GetNbinsY(); ++j)
        myMap.SetBinContent(i,j,0);


    TriggerEfficiencyMapVisitor temv(myMap, myPt);
    SimParms::getInstance()->accept(temv);
    tracker.accept(temv);
    temv.postVisit();
  }

  // Compute the trigger threshold maps. The efficiency values are
  // given by the maps
  for (std::map<double, TH2D>::iterator it = thresholdMaps.begin();
       it!=thresholdMaps.end(); ++it) {
    double myEfficiency = it->first;
    TH2D& myMap = it->second;

    // Reset the our map, in case it is not empty
    for (int i=1; i<=myMap.GetNbinsX(); ++i)
      for (int j=1; j<=myMap.GetNbinsY(); ++j)
        myMap.SetBinContent(i,j,0);


    PtThresholdMapVisitor ptmv(myMap, myEfficiency);
    SimParms::getInstance()->accept(ptmv);
    tracker.accept(ptmv);
    ptmv.postVisit();

  }

  // Then: single maps

  for (int i=1; i<=suggestedSpacingMap.GetNbinsX(); ++i) {
    for (int j=1; j<=suggestedSpacingMap.GetNbinsY(); ++j) {
      suggestedSpacingMap.SetBinContent(i,j,0);
      suggestedSpacingMapAW.SetBinContent(i,j,0);
      nominalCutMap.SetBinContent(i,j,0);
    }
  }

  SpacingCutVisitor scv(suggestedSpacingMap, suggestedSpacingMapAW, nominalCutMap, moduleOptimalSpacings);
  SimParms::getInstance()->accept(scv);
  tracker.accept(scv);
  scv.postVisit();
}

/*
void Analyzer::fillPowerMap(Tracker& tracker) {
  TH2D& irradiatedPowerConsumptionMap = myMapBag.getMaps(mapBag::irradiatedPowerConsumptionMap)[mapBag::dummyMomentum];
  TH2D& totalPowerConsumptionMap = myMapBag.getMaps(mapBag::totalPowerConsumptionMap)[mapBag::dummyMomentum];

  for (int i=1; i<=irradiatedPowerConsumptionMap.GetNbinsX(); ++i) {
    for (int j=1; j<=irradiatedPowerConsumptionMap.GetNbinsY(); ++j) {
      irradiatedPowerConsumptionMap.SetBinContent(i,j,0);
      totalPowerConsumptionMap.SetBinContent(i,j,0);
    }
  }

  IrradiatedPowerMapVisitor v(irradiatedPowerConsumptionMap, totalPowerConsumptionMap);
  SimParms::getInstance()->accept(v);
  tracker.accept(v);
  v.postVisit();

}
*/
void Analyzer::prepareTrackerMap(TH2D& myMap, const std::string& name, const std::string& title) { 
  int mapBinsY = int( (geom_max_radius + geom_inactive_volume_width) * geom_safety_factor / 10.); // every cm
  int mapBinsX = int( (geom_max_length) * geom_safety_factor / 10.); // every cm
  myMap.SetName(name.c_str());
  myMap.SetTitle(title.c_str());
  myMap.SetXTitle("z [mm]");
  myMap.SetYTitle("r [mm]");
  myMap.SetBins(mapBinsX, 0.0, geom_max_length*geom_safety_factor, mapBinsY, 0.0, (geom_max_radius + geom_inactive_volume_width) * geom_safety_factor);
  myMap.Reset();
}

void Analyzer::prepareRadialTrackerMap(TH2D& myMap, const std::string& name, const std::string& title) { 
  int mapBinsY = int( (2*geom_max_radius) * geom_safety_factor / 10.); // every cm
  int mapBinsX = int( (2*geom_max_radius) * geom_safety_factor / 10.); // every cm
  myMap.SetName(name.c_str());
  myMap.SetTitle(title.c_str());
  myMap.SetXTitle("x [mm]");
  myMap.SetYTitle("y [mm]");
  myMap.SetBins(mapBinsX, -geom_max_radius*geom_safety_factor, geom_max_radius*geom_safety_factor, mapBinsY, -geom_max_radius*geom_safety_factor, geom_max_radius*geom_safety_factor);
  myMap.Reset();
}


/**
 * This convenience function resets and empties all histograms for
 * the trigger performance, so they are ready for a new round of
 * analysis.
 * @parameter triggerMomenta the vector of pt to test the trigger
 */

void Analyzer::prepareTriggerPerformanceHistograms(const int& nTracks, const double& myEtaMax, const std::vector<double>& triggerMomenta, const std::vector<double>& thresholdProbabilities) {
  // Clean-up and prepare the trigger performance maps
  myMapBag.clearMaps(mapBag::efficiencyMap);
  myMapBag.clearMaps(mapBag::thresholdMap);
  myMapBag.clearMaps(mapBag::thicknessMap);
  myMapBag.clearMaps(mapBag::windowMap);
  myMapBag.clearMaps(mapBag::suggestedSpacingMap);
  myMapBag.clearMaps(mapBag::suggestedSpacingMapAW);
  myMapBag.clearMaps(mapBag::nominalCutMap);

  std::map<double, TH2D>& thresholdMaps = myMapBag.getMaps(mapBag::thresholdMap);
  std::map<double, TH2D>& efficiencyMaps = myMapBag.getMaps(mapBag::efficiencyMap);

  // PT Threshold maps here
  ostringstream tempSS;
  // string tempString;
  for (std::vector<double>::const_iterator it = thresholdProbabilities.begin();
       it!=thresholdProbabilities.end(); ++it) {
    TH2D& myMap = thresholdMaps[(*it)];
    tempSS.str("");
    tempSS << "ptThresholdMap_" << std::dec << (*it) * 100 << "perc";
    prepareTrackerMap(myMap, tempSS.str(), tempSS.str());
  }

  // Efficiency maps here
  for (std::vector<double>::const_iterator it = triggerMomenta.begin();
       it!=triggerMomenta.end(); ++it) {
    TH2D& myMap = efficiencyMaps[(*it)];
    tempSS.str("");
    tempSS << "triggerEfficiencyMap_" << std::dec << (*it) << "GeV";
    prepareTrackerMap(myMap, tempSS.str(), tempSS.str());
  }

  // Single maps here
  TH2D& thicknessMap = myMapBag.getMaps(mapBag::thicknessMap)[mapBag::dummyMomentum];
  TH2D& windowMap = myMapBag.getMaps(mapBag::windowMap)[mapBag::dummyMomentum];
  TH2D& suggestedSpacingMap = myMapBag.getMaps(mapBag::suggestedSpacingMap)[mapBag::dummyMomentum];
  TH2D& suggestedSpacingMapAW = myMapBag.getMaps(mapBag::suggestedSpacingMapAW)[mapBag::dummyMomentum];
  TH2D& nominalCutMap = myMapBag.getMaps(mapBag::nominalCutMap)[mapBag::dummyMomentum];

  prepareTrackerMap(thicknessMap, "thicknessMap", "Module thickness map");
  //thicknessMap.SetMinimum(0);
  //thicknessMap.SetMaximum(6);
  prepareTrackerMap(windowMap, "windowMap", "Module window map");
  prepareTrackerMap(suggestedSpacingMap, "suggestedSpacingMap", "Map of nearest available spacing [using standard window]");
  prepareTrackerMap(suggestedSpacingMapAW, "suggestedSpacingMapAW", "Map of nearest available spacing [using selected windows]");
  prepareTrackerMap(nominalCutMap, "nominalCutMap", "Map of nominal pT cut");


  // Clear the graph (and profile) list
  myGraphBag.clearTriggerGraphs();
  myProfileBag.clearTriggerProfiles();

  // Prepare the graphs to record the number of triggered points
  // std::map<double, TGraph>& trigGraphs = myGraphBag.getGraphs(GraphBag::TriggerGraph|GraphBag::TriggeredGraph);
  std::map<double, TProfile>& trigProfiles = myProfileBag.getProfiles(profileBag::TriggerProfile|profileBag::TriggeredProfile);
  std::map<double, TProfile>& trigFractionProfiles = myProfileBag.getProfiles(profileBag::TriggerProfile|profileBag::TriggeredFractionProfile);
  std::map<double, TProfile>& trigPurityProfiles = myProfileBag.getProfiles(profileBag::TriggerProfile|profileBag::TriggerPurityProfile);

  // Prepare the graphs for the trigger performace
  std::ostringstream aName;

  // TODO: tune this parameter
  int nbins = int(nTracks/10.);
  for (vector<double>::const_iterator iter = triggerMomenta.begin();
       iter != triggerMomenta.end();
       ++iter) {
    //std::pair<double, TGraph> elemGraph;
    std::pair<double, TProfile> elemProfile;
    std::pair<double, TProfile> elemFractionProfile;
    std::pair<double, TProfile> elemPurityProfile;
    //TGraph graph;
    TProfile profile("dummyName", "dummyTitle", nbins, 0, myEtaMax);
    //elemGraph.first = *iter;
    //elemGraph.second = graph;
    elemProfile.first = *iter;
    elemProfile.second = profile;
    elemFractionProfile.first = *iter;
    elemFractionProfile.second = profile;
    elemPurityProfile.first = *iter;
    elemPurityProfile.second = profile;
    // Prepare plots: triggered graphs
    // trigGraphs.insert(elemGraph);
    trigProfiles.insert(elemProfile);
    trigFractionProfiles.insert(elemFractionProfile);
    trigPurityProfiles.insert(elemPurityProfile);
    // trigGraphs[*iter].SetTitle("Average triggered points;#eta;Triggered points <N>");
    trigProfiles[*iter].SetTitle("Average triggered points;#eta;Triggered points <N>");
    trigFractionProfiles[*iter].SetTitle("Average trigger efficiency;#eta;Efficiency [%]");
    trigPurityProfiles[*iter].SetTitle("Average stub purity;#eta;Purity [%]");
    aName.str(""); aName << "triggered_vs_eta" << *iter << "_profile";      
    trigProfiles[*iter].SetName(aName.str().c_str());
    aName.str(""); aName << "triggered_vs_eta" << *iter << "_fractionProfile";
    trigFractionProfiles[*iter].SetName(aName.str().c_str());
    aName.str(""); aName << "triggered_vs_eta" << *iter << "_purityProfile";
    trigPurityProfiles[*iter].SetName(aName.str().c_str());
  }

  //std::pair<double, TGraph> elemTotalGraph;
  std::pair<double, TProfile> elemTotalProfile;
  //TGraph totalGraph;
  char dummyName[256]; sprintf(dummyName, "dummyName%d", bsCounter++);
  TProfile totalProfile(dummyName, dummyName, nbins, 0, myEtaMax);
  //elemTotalGraph.first = GraphBag::Triggerable;
  //elemTotalGraph.second = totalGraph;
  elemTotalProfile.first = profileBag::Triggerable;
  elemTotalProfile.second = totalProfile;
  // Prepare plot: total trigger points
  //trigGraphs.insert(elemTotalGraph);
  trigProfiles.insert(elemTotalProfile);
  //trigGraphs[GraphBag::Triggerable].SetTitle("Average triggered points;#eta;Triggered points <N>");
  trigProfiles[profileBag::Triggerable].SetTitle("Average triggered points;#eta;Triggered points <N>");
  //aName.str(""); aName << "triggerable_vs_eta_graph";
  //trigGraphs[GraphBag::Triggerable].SetName(aName.str().c_str());
  aName.str(""); aName << "triggerable_vs_eta_profile";
  trigProfiles[profileBag::Triggerable].SetName(aName.str().c_str());

}


void Analyzer::preparePowerHistograms() {
  myMapBag.clearMaps(mapBag::irradiatedPowerConsumptionMap);
  myMapBag.clearMaps(mapBag::totalPowerConsumptionMap);
  TH2D& irradiatedPowerConsumptionMap = myMapBag.getMaps(mapBag::irradiatedPowerConsumptionMap)[mapBag::dummyMomentum]; // dummyMomentum is supplied because it is a single map. Multiple maps are indexed like arrays (see above efficiency maps)
  TH2D& totalPowerConsumptionMap = myMapBag.getMaps(mapBag::totalPowerConsumptionMap)[mapBag::dummyMomentum]; // dummyMomentum is supplied because it is a single map. Multiple maps are indexed like arrays (see above efficiency maps)
  prepareTrackerMap(irradiatedPowerConsumptionMap, "irradiatedPowerConsumptionMap", "Map of power dissipation in sensors (after irradiation)");
  prepareTrackerMap(totalPowerConsumptionMap, "irradiatedPowerConsumptionMap", "Map of power dissipation in modules (after irradiation)");
}

void Analyzer::prepareTriggerProcessorHistograms() {
  myMapBag.clearMaps(mapBag::moduleConnectionEtaMap);
  TH2D& moduleConnectionEtaMap = myMapBag.getMaps(mapBag::moduleConnectionEtaMap)[mapBag::dummyMomentum];
  prepareTrackerMap(moduleConnectionEtaMap, "moduleConnectionEtaMap", "Map");

  myMapBag.clearMaps(mapBag::moduleConnectionPhiMap);
  TH2D& moduleConnectionPhiMap = myMapBag.getMaps(mapBag::moduleConnectionPhiMap)[mapBag::dummyMomentum];
  prepareRadialTrackerMap(moduleConnectionPhiMap, "moduleConnectionPhiMap", "Map");

  myMapBag.clearMaps(mapBag::moduleConnectionEndcapPhiMap);
  TH2D& moduleConnectionEndcapPhiMap = myMapBag.getMaps(mapBag::moduleConnectionEndcapPhiMap)[mapBag::dummyMomentum];
  prepareRadialTrackerMap(moduleConnectionEndcapPhiMap, "moduleConnectionEndcapPhiMap", "Map");

  for (int i=1; i<=moduleConnectionEtaMap.GetNbinsX(); ++i) {
    for (int j=1; j<=moduleConnectionEtaMap.GetNbinsY(); ++j) {
      moduleConnectionEtaMap.SetBinContent(i,j,0);
    }
  }

  for (int i=1; i<=moduleConnectionPhiMap.GetNbinsX(); ++i) {
    for (int j=1; j<=moduleConnectionPhiMap.GetNbinsY(); ++j) {
      moduleConnectionPhiMap.SetBinContent(i,j,0);
    }
  }

  for (int i=1; i<=moduleConnectionEndcapPhiMap.GetNbinsX(); ++i) {
    for (int j=1; j<=moduleConnectionEndcapPhiMap.GetNbinsY(); ++j) {
      moduleConnectionEndcapPhiMap.SetBinContent(i,j,0);
    }
  }
}

/**
 * This convenience function resets and empties all histograms for the
 * geometry so they are ready for a new round of analysis.
 */
void Analyzer::clearGeometryHistograms() {
  // geometry analysis
  mapPhiEta.Reset();
  mapPhiEta.SetNameTitle("mapPhiEta", "Number of hits;#phi;#eta");
  etaProfileCanvas.SetName("etaProfileCanvas"); etaProfileCanvas.SetTitle("Eta Profiles");
  hitDistribution.Reset();
  hitDistribution.SetNameTitle("hitDistribution", "Hit distribution");
  //geomLite->SetName("geometryLite");   geomLite->SetTitle("Modules geometry");
  //geomLiteXY->SetName("geometryLiteXY"); geomLiteXY->SetTitle("Modules geometry (XY Section)");
  //geomLiteYZ->SetName("geometryLiteYZ"); geomLiteYZ->SetTitle("Modules geometry (EC Section)");
  //geomLiteEC->SetName("geometryLiteEC"); geomLiteEC->SetTitle("Modules geometry (Endcap)");

  // Power density
  while (powerDensity.GetN()) powerDensity.RemovePoint(0);
  powerDensity.SetName("powerdensity");
  powerDensity.SetTitle("Power density;Total area [m^{2}];Power density [kW / m^{2}]");
}

/**
 * This convenience function sets all values in the internal array <i>cells</i> to zero.
 */
void Analyzer::clearCells() {
  for (unsigned int i = 0; i < cells.size(); i++) {
    cells.at(i).clear();
  }
  cells.clear();
}

/**
 * This convenience function sets the number of bins and the lower and upper range for their contents for
 * each of the available histograms.
 * @param bins The number of bins in each 1D histogram
 * @param min The minimal eta value that should be plotted
 * @param max the maximal eta value that should be plotted
 */
void Analyzer::setHistogramBinsBoundaries(int bins, double min, double max) {
  // single category
  ractivebarrel.SetBins(bins, min, max);
  ractiveendcap.SetBins(bins, min, max);
  rserfbarrel.SetBins(bins, min, max);
  rserfendcap.SetBins(bins, min, max);
  rlazybarrel.SetBins(bins, min, max);
  rlazyendcap.SetBins(bins, min, max);
  rlazytube.SetBins(bins, min, max);
  rlazyuserdef.SetBins(bins, min, max);
  rbeampipe.SetBins(bins, min, max);
  iactivebarrel.SetBins(bins, min, max);
  iactiveendcap.SetBins(bins, min, max);
  iserfbarrel.SetBins(bins, min, max);
  iserfendcap.SetBins(bins, min, max);
  ilazybarrel.SetBins(bins, min, max);
  ilazyendcap.SetBins(bins, min, max);
  ilazytube.SetBins(bins, min, max);
  ilazyuserdef.SetBins(bins, min, max);
  ibeampipe.SetBins(bins, min, max);
  // composite
  rbarrelall.SetBins(bins, min, max);
  rendcapall.SetBins(bins, min, max);
  ractiveall.SetBins(bins, min, max);
  rserfall.SetBins(bins, min, max);
  rlazyall.SetBins(bins, min, max);
  ibarrelall.SetBins(bins, min, max);
  iendcapall.SetBins(bins, min, max);
  iactiveall.SetBins(bins, min, max);
  iserfall.SetBins(bins, min, max);
  ilazyall.SetBins(bins, min, max);
  // outside tracking volume
  rextraservices.SetBins(bins, min, max);
  rextrasupports.SetBins(bins, min, max);
  iextraservices.SetBins(bins, min, max);
  iextrasupports.SetBins(bins, min, max);
  // global
  rglobal.SetBins(bins, min, max);
  iglobal.SetBins(bins, min, max);
  // isolines
  isor.SetBins(bins, 0.0, geom_max_length, bins / 2, 0.0, geom_max_radius + geom_inactive_volume_width);
  isoi.SetBins(bins, 0.0, geom_max_length, bins / 2, 0.0, geom_max_radius + geom_inactive_volume_width);
  // Material distribution maps
  int materialMapBinsY = int( (geom_max_radius + geom_inactive_volume_width) * geom_safety_factor / 5.); // every half a cm
  int materialMapBinsX = int( (geom_max_length) * geom_safety_factor / 5.); // every half a cm
  mapRadiation.SetBins(       materialMapBinsX, 0.0, geom_max_length*geom_safety_factor, materialMapBinsY, 0.0, (geom_max_radius + geom_inactive_volume_width)*geom_safety_factor);
  mapInteraction.SetBins(     materialMapBinsX, 0.0, geom_max_length*geom_safety_factor, materialMapBinsY, 0.0, (geom_max_radius + geom_inactive_volume_width)*geom_safety_factor);
  mapRadiationCount.SetBins(  materialMapBinsX, 0.0, geom_max_length*geom_safety_factor, materialMapBinsY, 0.0, (geom_max_radius + geom_inactive_volume_width)*geom_safety_factor);
  mapInteractionCount.SetBins(materialMapBinsX, 0.0, geom_max_length*geom_safety_factor, materialMapBinsY, 0.0, (geom_max_radius + geom_inactive_volume_width)*geom_safety_factor);
  mapRadiationCalib.SetBins(  materialMapBinsX, 0.0, geom_max_length*geom_safety_factor, materialMapBinsY, 0.0, (geom_max_radius + geom_inactive_volume_width)*geom_safety_factor);
  mapInteractionCalib.SetBins(materialMapBinsX, 0.0, geom_max_length*geom_safety_factor, materialMapBinsY, 0.0, (geom_max_radius + geom_inactive_volume_width)*geom_safety_factor);
}

/**
 * This convenience function sets the number of bins and the lower and upper range for their contents for
 * each of the cells that make up the basis for the 2D histograms.
 * @param bins The number of bins in eta; the number of bins in r will be half that
 * @param minr The minimum radius that will be considered for tracking
 * @param maxr The maximum radius that will be considered for tracking
 * @param mineta The minimum value of eta that will be considered for tracking
 * @param maxeta The maximum value of eta that will be considered for tracking
 */
void Analyzer::setCellBoundaries(int bins, double minr, double maxr, double mineta, double maxeta) {
  double rstep, etastep;
  rstep = 2 * (maxr - minr) / bins;
  etastep = (maxeta - mineta) / bins;
  Cell c;
  c.rmin = 0.0;  // TODO: is this right?
  c.rmax = 0.0;
  c.etamin = 0.0;
  c.etamax = 0.0;
  c.rlength = 0.0;
  c.ilength = 0.0;
  cells.resize(bins);
  for (unsigned int i = 0; i < cells.size(); i++) if (bins != 1) cells.at(i).resize(bins / 2, c);
  for (unsigned int i = 0; i < cells.size(); i++) {
    for (unsigned int j = 0; j < cells.at(i).size(); j++) {
      cells.at(i).at(j).rmin = minr + j * rstep;
      cells.at(i).at(j).rmax = minr + (j+1) * rstep;
      cells.at(i).at(j).etamin = mineta + i * etastep;;
      cells.at(i).at(j).etamax = mineta + (i+1) * etastep;
    }
  }
}


/**
 * Fills the material distribution maps
 * @param r The radius at which the hit was detected
 * @param theta The angle of the track used for meterial detection
 * @param rl The local radiation length
 * @param il The local interaction length
 */
void Analyzer::fillMapRT(const double& r, const double& theta, const Material& mat) {
  double z = r /tan(theta);
  if (mat.radiation>0){
    mapRadiation.Fill(z,r,mat.radiation);
    mapRadiationCount.Fill(z,r);
  } 
  if (mat.interaction>0) {
    mapInteraction.Fill(z,r,mat.interaction);
    mapInteractionCount.Fill(z,r);
  }
}

/**
 * Fills the material distribution maps
 * @param r The radius at which the hit was detected
 * @param z The z coordinate of the hit
 * @param rl The local radiation length
 * @param il The local interaction length
 */
void Analyzer::fillMapRZ(const double& r, const double& z, const Material& mat) {
  if (mat.radiation>0){
    mapRadiation.Fill(z,r,mat.radiation);
    mapRadiationCount.Fill(z,r);
  } 
  if (mat.interaction>0) {
    mapInteraction.Fill(z,r,mat.interaction);
    mapInteractionCount.Fill(z,r);
  }
}

/**
 * @return a (hit-scaled) map of radiation length
 */
TH2D& Analyzer::getHistoMapRadiation() {
  int nBins = mapRadiation.GetNbinsX()*mapRadiation.GetNbinsY();
  double content;
  double count;
  for (int iBin=1; iBin<=nBins; ++iBin) {
    content = mapRadiation.GetBinContent(iBin);
    count = mapRadiationCount.GetBinContent(iBin);
    //mapRadiationCalib.SetBinContent(iBin,content);
    if (count==1) mapRadiationCalib.SetBinContent(iBin,content);
    else if (count>1) mapRadiationCalib.SetBinContent(iBin,content/double(count));
    //else if (count==0) mapRadiationCalib.SetBinContent(iBin, 0.);
  }
  return mapRadiationCalib;
}

/**
 * @return a (hit-scaled) map of interaction length
 */
TH2D& Analyzer::getHistoMapInteraction() {
  int nBins = mapInteraction.GetNbinsX()*mapInteraction.GetNbinsY();
  double content;
  double count;
  for (int iBin=1; iBin<=nBins; ++iBin) {
    content = mapInteraction.GetBinContent(iBin);
    count = mapInteractionCount.GetBinContent(iBin);
    //mapInteractionCalib.SetBinContent(iBin,content);
    if (count==1) mapInteractionCalib.SetBinContent(iBin,content);
    else if (count>1) mapInteractionCalib.SetBinContent(iBin,content/double(count));
    //else if (count==0) mapInteractionCalib.SetBinContent(iBin, 0.);
  }
  return mapInteractionCalib;
}


/**
 * This function assigns the local radiation and interaction lengths of a detected hit to their position in the
 * (eta, r) space.
 * @param r The radius at which the hit was detected
 * @param eta The eta value of the current track
 * @param rl The local radiation length
 * @param il The local interaction length
 */
void Analyzer::fillCell(double r, double eta, double theta, Material mat) {
  double rl = mat.radiation;
  double il = mat.interaction;
  int rindex, etaindex;
  if (cells.size() > 0) {
    for (rindex = 0; (unsigned int) rindex < cells.at(0).size(); rindex++) {
      if ((cells.at(0).at(rindex).rmin <= r) && (cells.at(0).at(rindex).rmax > r)) break;
    }
    if ((unsigned int) rindex < cells.at(0).size()) {
      for (etaindex = 0; (unsigned int) etaindex < cells.size(); etaindex++) {
        if ((cells.at(etaindex).at(rindex).etamin <= eta) && (cells.at(etaindex).at(rindex).etamax > eta)) break;
      }
      if ((unsigned int) etaindex < cells.size()) {
        cells.at(etaindex).at(rindex).rlength = cells.at(etaindex).at(rindex).rlength + rl;
        cells.at(etaindex).at(rindex).ilength = cells.at(etaindex).at(rindex).ilength + il;
      }
    }
  }
}

/**
 * The integrated radiation and interaction lengths in (eta, r)
 * are converted to (z, r) coordinates and stored in <i>isor</i>
 * and <i>isoi</i> in this function.
 */
void Analyzer::transformEtaToZ() {
  int size_z, size_r, rindex, etaindex;
  double z, r, eta, z_max, z_min, r_max, r_min, z_c, r_c;
  // init: sizes and boundaries
  size_z = isor.GetNbinsX();
  size_r = isor.GetNbinsY();
  z_max = isor.GetXaxis()->GetXmax();
  z_min = isor.GetXaxis()->GetXmin();
  r_max = isor.GetYaxis()->GetXmax();
  r_min = isor.GetYaxis()->GetXmin();
  // new number of bins in z and r
  z_c = (z_max - z_min) / (2 * size_z);
  r_c = (r_max - r_min) / (2 * size_r);
  // radiation length loop
  for (int i = 1; i <= size_z; i++) {
    // calculate current z bin
    z = z_min + 2 * (i - 1) * z_c + z_c;
    for (int j = 1; j <= size_r; j++) {
      // calculate current r bin
      r = r_min + 2 * (j - 1) * r_c + r_c;
      eta = -log(tan(atan(r / z) / 2.0));
      // find corresponding r and eta positions
      etaindex = findCellIndexEta(eta);
      rindex = findCellIndexR(r);
      // fill in radiation length in r and z
      if ((etaindex >= 0) && (rindex >= 0)) isor.Fill(z, r, cells.at(etaindex).at(rindex).rlength);
    }
  }
  // init: sizes and boundaries
  size_z = isoi.GetNbinsX();
  size_r = isoi.GetNbinsY();
  z_max = isoi.GetXaxis()->GetXmax();
  z_min = isoi.GetXaxis()->GetXmin();
  r_max = isoi.GetYaxis()->GetXmax();
  r_min = isoi.GetYaxis()->GetXmin();
  // new number of bins in z and r
  z_c = (z_max - z_min) / (2 * size_z);
  r_c = (r_max - r_min) / (2 * size_r);
  // interaction length loop
  for (int i = 0; i < size_z; i++) {
    // calculate current z bin
    z = z_min + 2 * i * z_c + z_c;
    for (int j = 0; j < size_r; j++) {
      // calculate current r bin
      r = r_min + 2 * j * r_c + r_c;
      eta = -log(tan(atan(r / z) / 2.0));
      // find corresponding r and eta positions
      etaindex = findCellIndexEta(eta);
      rindex = findCellIndexR(r);
      // fill in interaction length in r and z
      if ((etaindex >= 0) && (rindex >= 0)) isoi.Fill(z, r, cells.at(etaindex).at(rindex).ilength);
    }
  }
}

// private
/**
 * This is a convenience function that converts a radius to an index into the second dimension of <i>cells</i>.
 * @param r The given radius value
 * @return The corresponding index into the vector
 */
int Analyzer::findCellIndexR(double r) {
  int index = -1;
  if (r >= 0) {
    if (cells.size() > 0) {
      index = 0;
      while (index < (int)cells.at(0).size()) {
        if (cells.at(0).at(index).rmax < r) index++;
        else break;
      }
      if (index == (int)cells.at(0).size()) index = -1;
      return index;
    }
  }
  return index;
}

/**
 * This is a convenience function that converts an eta value to an index into the first dimension of <i>cells</i>.
 * @param eta The given eta value
 * @return the corresponding index into the vector
 */
int Analyzer::findCellIndexEta(double eta) {
  int index = -1;
  if (eta >= 0) {
    index = 0;
    while (index < (int)cells.size()) {
      if (cells.at(index).size() > 0) {
        if (cells.at(index).at(0).etamax < eta) index++;
        else break;
      }
      else {
        index = -1;
        break;
      }
    }
    if (index == (int)cells.size()) index = -1;
    return index;
  }
  return index;
}


std::pair<double, double> Analyzer::computeMinMaxTracksEta(const Tracker& t) const {
  std::pair <double, double> etaMinMax = t.computeMinMaxEta();
  if (SimParms::getInstance()->maxTracksEta.state()) etaMinMax.second = SimParms::getInstance()->maxTracksEta();
  if (SimParms::getInstance()->minTracksEta.state()) etaMinMax.first  = SimParms::getInstance()->minTracksEta();

  // TODO: DIRTY HACK TO SET ETA
  etaMinMax.first  = -1*geom_max_eta_coverage;
  etaMinMax.second = +1*geom_max_eta_coverage;

  return etaMinMax;
}

// public
/**
 * Creates the histograms to analyze the tracker coverage
 * @param tracker the tracker to be analyzed
 * @param nTracker the number of tracks to be used to analyze the coverage (defaults to 1000)
 */
void Analyzer::analyzeGeometry(std::vector<Tracker*> trackers, int nTracks /*=1000*/ ) {
  
  geometryTracksUsed = nTracks;
  savingGeometryV.clear();
  clearGeometryHistograms();

  // Initialize
  std::map <std::string, int>      moduleTypeCount;      // counts hit per module type -- if any of the sensors (or both) are hit, it counts 1
  std::map <std::string, int>      sensorTypeCount;      // counts hit per sensor on module type -- if one sensor is hit, it counts 1, if both sensors are hit, it counts 2
  std::map <std::string, int>      moduleTypeCountStubs; // counts stubs per module type -- if both sensors are hit and a stub is formed, it counts 1
  std::map <std::string, TProfile> etaProfileByType;
  std::map <std::string, TProfile> etaProfileByTypeSensors;
  std::map <std::string, TProfile> etaProfileByTypeStubs;
  std::string aType;

  // Optimize the track creation on the real tracker
  double etaMin = std::numeric_limits<double>::max();
  double etaMax = 0;

  for (auto tracker : trackers) {

    auto etaMinMax = computeMinMaxTracksEta(*tracker);
    if (etaMinMax.first <etaMin) etaMin = etaMinMax.first;
    if (etaMinMax.second>etaMax) etaMax = etaMinMax.second;
  }
  // Computing the margin of the tracks to shoot
  double randomPercentMargin = 0.04;
  double randomSpan = (etaMax - etaMin)*(1. + randomPercentMargin);
  double randomBase = etaMin - (etaMax - etaMin)*(randomPercentMargin)/2.;
  double maxEta     = etaMax * (1 + randomPercentMargin);

  // Initialize random number generator, counters and histograms
  myDice.SetSeed(MY_RANDOM_SEED);
  for (auto tracker : trackers) {

    createResetCounters(*tracker, moduleTypeCount);
    createResetCounters(*tracker, sensorTypeCount);
    createResetCounters(*tracker, moduleTypeCountStubs);
  }

  class LayerNameVisitor : public ConstGeometryVisitor {
    string m_id;
  public:
    std::set<string> data;
    LayerNameVisitor(const std::vector<Tracker*>& trackers) { for (auto tracker : trackers) tracker->accept(*this); }
    virtual ~LayerNameVisitor() {};
    void visit(const Barrel& b) { m_id = b.myid(); }
    void visit(const Endcap& e) { m_id = e.myid(); }
    void visit(const Layer& l)  { data.insert(m_id + " " + any2str(l.myid())); }
    void visit(const Disk& d)   { data.insert(m_id + " " + any2str(d.myid())); }
  };

  LayerNameVisitor layerNames(trackers);

  // Creating the layer hit coverage profiles
  layerEtaCoverageProfile.clear();
  layerEtaCoverageProfileStubs.clear();
  for (auto it = layerNames.data.begin(); it!=layerNames.data.end(); ++it) { // CUIDADO this is horrible code (not mine). refactor!
    TProfile* aProfile      = new TProfile(Form("layerEtaCoverageProfileHits%s" , it->c_str()), it->c_str(), 200, maxEta, maxEta);
    TProfile* aProfileStubs = new TProfile(Form("layerEtaCoverageProfileStubs%s", it->c_str()), it->c_str(), 200, maxEta, maxEta);
    layerEtaCoverageProfile[*it]      = (*aProfile);
    layerEtaCoverageProfileStubs[*it] = (*aProfileStubs);
    delete aProfile;
    delete aProfileStubs;
  }

  etaProfileByType.clear();
  etaProfileByTypeSensors.clear();
  etaProfileByTypeStubs.clear();

  for (std::map <std::string, int>::iterator it = moduleTypeCount.begin(); it!=moduleTypeCount.end(); it++) {

    TProfile& aProfile = etaProfileByType[(*it).first];
    aProfile.SetBins(100, 0, maxEta);
    aProfile.SetName((*it).first.c_str());
    aProfile.SetTitle((*it).first.c_str());
  }

  for (auto mel : sensorTypeCount) {
    TProfile& aProfileStubs = etaProfileByTypeSensors[mel.first];
    aProfileStubs.SetBins(100, 0, maxEta);
    aProfileStubs.SetName(mel.first.c_str());
    aProfileStubs.SetTitle(mel.first.c_str());
  }

  for (auto mel : moduleTypeCountStubs) {
    TProfile& aProfileStubs = etaProfileByTypeStubs[mel.first];
    aProfileStubs.SetBins(100, 0, maxEta);
    aProfileStubs.SetName(mel.first.c_str());
    aProfileStubs.SetTitle(mel.first.c_str());
  }

  //  ModuleVector allModules;
  double zError = SimParms::getInstance()->zErrorCollider();

  // The real simulation
  std::pair <XYZVector, double> aLine;

  int nTracksPerSide = int(pow(nTracks, 0.5));
  int nBlocks        = int(nTracksPerSide/2.);
  nTracks            = nTracksPerSide*nTracksPerSide;

  mapPhiEta.SetBins(nBlocks, -1*M_PI, M_PI, nBlocks, -maxEta, maxEta);
  TH2I mapPhiEtaCount("mapPhiEtaCount ", "phi Eta hit count", nBlocks, -1*M_PI, M_PI, nBlocks, -maxEta, maxEta);
  totalEtaProfile.Reset();
  totalEtaProfile.SetName("totalEtaProfile");
  totalEtaProfile.SetMarkerStyle(8);
  totalEtaProfile.SetMarkerColor(1);
  totalEtaProfile.SetMarkerSize(1.5);
  totalEtaProfile.SetTitle("Number of modules with at least one hit;#eta;Number of hit modules");
  totalEtaProfile.SetBins(100, 0, maxEta);

  totalEtaProfileSensors.Reset();
  totalEtaProfileSensors.SetName("totalEtaProfileSensors");
  totalEtaProfileSensors.SetMarkerStyle(8);
  totalEtaProfileSensors.SetMarkerColor(1);
  totalEtaProfileSensors.SetMarkerSize(1.5);
  totalEtaProfileSensors.SetTitle("Number of hits;#eta;Number of hits");
  totalEtaProfileSensors.SetBins(100, 0, maxEta);

  totalEtaProfileStubs.Reset();
  totalEtaProfileStubs.SetName("totalEtaProfileStubs");
  totalEtaProfileStubs.SetMarkerStyle(8);
  totalEtaProfileStubs.SetMarkerColor(1);
  totalEtaProfileStubs.SetMarkerSize(1.5);
  totalEtaProfileStubs.SetTitle("Number of modules with a stub;#eta;Number of stubs");
  totalEtaProfileStubs.SetBins(100, 0, maxEta);

  std::map<std::string, int> modulePlotColors; // CUIDADO quick and dirty way of creating a map with all the module colors (a cleaner way would be to have the map already created somewhere else)

  //XYZVector dir(0, 1, 0);
  // Shoot nTracksPerSide^2 tracks
  double angle = M_PI/2/(double)nTracksPerSide;

  for (int i=0; i<nTracksPerSide; i++) {
    for (int j=0; j<nTracksPerSide; j++) {

      // Generate a straight track and collect the list of hit modules
      aLine = shootDirection(randomBase, randomSpan);
      std::vector<std::pair<Module*, HitType>> hitModules = trackHit( XYZVector(0, 0, ((myDice.Rndm()*2)-1)* zError), aLine.first, trackers);

      // Reset the per-type hit counter and fill it
      resetTypeCounter(moduleTypeCount);
      resetTypeCounter(sensorTypeCount);
      resetTypeCounter(moduleTypeCountStubs);
      int numStubs = 0;
      int numHits  = 0;

      for (auto& hitModule : hitModules) {
        moduleTypeCount[hitModule.first->moduleType()]++;
        if (hitModule.second & HitType::INNER) {
          sensorTypeCount[hitModule.first->moduleType()]++;
          numHits++;
        }
        if (hitModule.second & HitType::OUTER) {
          sensorTypeCount[hitModule.first->moduleType()]++;
          numHits++;
        }
        if (hitModule.second == HitType::STUB) {
          moduleTypeCountStubs[hitModule.first->moduleType()]++;
          numStubs++;
        }
        modulePlotColors[hitModule.first->moduleType()] = hitModule.first->plotColor();
      }

      // Fill the module type hit plot
      for (std::map <std::string, int>::iterator it = moduleTypeCount.begin(); it!=moduleTypeCount.end(); it++) {
        etaProfileByType[(*it).first].Fill(fabs(aLine.second), (*it).second);
      }

      for (auto& mel : sensorTypeCount) {
        etaProfileByTypeSensors[mel.first].Fill(fabs(aLine.second), mel.second);
      }

      for (auto& mel : moduleTypeCountStubs) {
        etaProfileByTypeStubs[mel.first].Fill(fabs(aLine.second), mel.second);
      }

      // Fill other plots
      mapPhiEta.Fill(aLine.first.Phi(), aLine.second, hitModules.size()); // phi, eta 2d plot
      mapPhiEtaCount.Fill(aLine.first.Phi(), aLine.second);               // Number of shot tracks

      totalEtaProfile.Fill(fabs(aLine.second), hitModules.size());        // Total number of hits
      totalEtaProfileSensors.Fill(fabs(aLine.second), numHits);
      totalEtaProfileStubs.Fill(fabs(aLine.second), numStubs); 

      for (auto layerName : layerNames.data) {
        int layerHit = 0;
        int layerStub = 0;
        for (auto mh : hitModules) {
          UniRef ur = mh.first->uniRef();
          if (layerName == (ur.cnt + " " + any2str(ur.layer))) {
            layerHit=1;
            if (mh.second == HitType::STUB) layerStub=1;
            if (layerHit && layerStub) break;
          }
        }
        layerEtaCoverageProfile[layerName].Fill(aLine.second, layerHit);
        layerEtaCoverageProfileStubs[layerName].Fill(aLine.second, layerStub);
      }

    }
  }

  // Create and archive for saving our 2D map of hits
  double hitCount;
  double trackCount;
  for (int nx=0; nx<=mapPhiEtaCount.GetNbinsX()+1; nx++) {
    for (int ny=0; ny<=mapPhiEtaCount.GetNbinsY()+1; ny++) {
      trackCount=mapPhiEtaCount.GetBinContent(nx, ny);
      if (trackCount>0) {
        hitCount=mapPhiEta.GetBinContent(nx, ny);
        mapPhiEta.SetBinContent(nx, ny, hitCount/trackCount);
      }
    }
  }

  savingGeometryV.push_back(mapPhiEta);

  etaProfileCanvas.cd();
  savingGeometryV.push_back(etaProfileCanvas);

  //TProfile* total = total2D.ProfileX("etaProfileTotal");
  char profileName_[256];
  sprintf(profileName_, "etaProfileTotal%d", bsCounter++);
  // totalEtaProfile = TProfile(*total2D.ProfileX(profileName_));
  savingGeometryV.push_back(totalEtaProfile);
  if (totalEtaProfile.GetMaximum()<maximum_n_planes)        totalEtaProfile.SetMaximum(maximum_n_planes);
  if (totalEtaProfileSensors.GetMaximum()<maximum_n_planes) totalEtaProfileSensors.SetMaximum(maximum_n_planes);
  if (totalEtaProfileStubs.GetMaximum()<maximum_n_planes)   totalEtaProfileStubs.SetMaximum(maximum_n_planes);
  totalEtaProfile.Draw();
  totalEtaProfileSensors.Draw();
  totalEtaProfileStubs.Draw();
  for (std::map <std::string, TProfile>::iterator it = etaProfileByType.begin();
       it!=etaProfileByType.end(); it++) {
    TProfile* myProfile=(TProfile*)it->second.Clone();
    savingGeometryV.push_back(*myProfile); // TODO: remove savingGeometryV everywhere :-) [VERY obsolete...]
    myProfile->SetMarkerStyle(8);
    myProfile->SetMarkerColor(Palette::color(modulePlotColors[it->first]));
    myProfile->SetMarkerSize(1);
    std::string profileName = "etaProfile"+(*it).first;
    myProfile->SetName(profileName.c_str());
    myProfile->SetTitle((*it).first.c_str());
    myProfile->GetXaxis()->SetTitle("eta");
    myProfile->GetYaxis()->SetTitle("Number of hit modules");
    myProfile->Draw("same");
    typeEtaProfile.push_back(*myProfile);
  }

  for (std::map <std::string, TProfile>::iterator it = etaProfileByTypeSensors.begin();
       it!=etaProfileByTypeSensors.end(); it++) {
    TProfile* myProfile=(TProfile*)it->second.Clone();
    myProfile->SetMarkerStyle(8);
    myProfile->SetMarkerColor(Palette::color(modulePlotColors[it->first]));
    myProfile->SetMarkerSize(1);
    std::string profileName = "etaProfileSensors"+(*it).first;
    myProfile->SetName(profileName.c_str());
    myProfile->SetTitle((*it).first.c_str());
    myProfile->GetXaxis()->SetTitle("eta");
    myProfile->GetYaxis()->SetTitle("Number of hits");
    myProfile->Draw("same");
    typeEtaProfileSensors.push_back(*myProfile);
  }

  for (std::map <std::string, TProfile>::iterator it = etaProfileByTypeStubs.begin();
       it!=etaProfileByTypeStubs.end(); it++) {
    TProfile* myProfile=(TProfile*)it->second.Clone();
    myProfile->SetMarkerStyle(8);
    myProfile->SetMarkerColor(Palette::color(modulePlotColors[it->first]));
    myProfile->SetMarkerSize(1);
    std::string profileName = "etaProfileStubs"+(*it).first;
    myProfile->SetName(profileName.c_str());
    myProfile->SetTitle((*it).first.c_str());
    myProfile->GetXaxis()->SetTitle("eta");
    myProfile->GetYaxis()->SetTitle("Number of stubs");
    myProfile->Draw("same");
    typeEtaProfileStubs.push_back(*myProfile);
  }


  // Record the fraction of hits per module, power, ...
  hitDistribution.SetBins(nTracks, 0 , 1);
  savingGeometryV.push_back(hitDistribution);

  std::map<std::string, int>    typeToCount;
  std::map<std::string, double> typeToPower;
  std::map<std::string, double> typeToSurface;

  for (auto tracker : trackers) {
    for (auto module : tracker->modules()) {

      hitDistribution.Fill(module->numHits()/double(nTracks));

      string aSensorType          = module->moduleType();
      typeToCount[aSensorType]++;
      typeToSurface[aSensorType] += module->area() / 1e6; // in mq
      typeToPower[aSensorType]   += module->sensorPowerConsumption() / 1e3; // in kW
    }
  }

  int iPoints = 0;
  for (std::map<std::string, int>::iterator typesIt = typeToCount.begin();
       typesIt != typeToCount.end(); ++typesIt ) {
    string aSensorType = typesIt->first;
    powerDensity.SetPoint(iPoints++,
                          typeToSurface[aSensorType],
                          typeToPower[aSensorType] / typeToSurface[aSensorType] );
  }
  return;
}

// public
// TODO!!!
// Creates the geometry objects geomLite
// (now they are canvases, but in the future
// they could be just lists of 3d poly objects)
// Moreover now we create them by calling the tracker object, which seems improper
void Analyzer::createGeometryLite(Tracker& tracker) {
  if (!(geomLiteCreated &&
        geomLiteXYCreated &&
        geomLiteYZCreated &&
        geomLiteECCreated)) {
    tracker.createGeometry(true);
    geomLiteCreated=true;
    geomLiteXYCreated=true;
    geomLiteYZCreated=true;
    geomLiteECCreated=true;
  }
  geomLite = tracker.getGeomLite();
  geomLiteXY = tracker.getGeomLiteXY();
  geomLiteYZ = tracker.getGeomLiteYZ();
  geomLiteEC = tracker.getGeomLiteEC();
}

    // private
    /**
     * Creates a module type map
     * It sets a different integer for each one
     * @param tracker the tracker to be analyzed
     * @param moduleTypeCount the map to count the different module types
     * @return the total number of module types
     */
    int Analyzer::createResetCounters(Tracker& tracker, std::map <std::string, int> &moduleTypeCount) {
      ModuleVector result;
      LayerVector::iterator layIt;
      ModuleVector* moduleV;
      ModuleVector::iterator modIt;

      std::string aType;
      int typeCounter=0;

      for (auto m : tracker.modules()) {
          aType = m->moduleType();
          m->resetHits();
          if (moduleTypeCount.find(aType)==moduleTypeCount.end()) {
            moduleTypeCount[aType]=typeCounter++;
          }
      }

      return(typeCounter);
    }

    // private
    /**
     * Shoots directions with random (flat) phi, random (flat) pseudorapidity
     * gives also the direction's eta
     * @param minEta minimum eta to shoot tracks
     * @param spanEta difference between minimum and maximum eta
     * @return the pair of value: pointing XYZVector and eta of the track
     */
    std::pair <XYZVector, double > Analyzer::shootDirection(double minEta, double spanEta) {
      std::pair <XYZVector, double> result;

      double eta;
      double phi;
      double theta;

      // phi is random [0, 2pi)
      phi = myDice.Rndm() * 2 * M_PI; // debug

      // eta is random (-range, range]
      eta = myDice.Rndm() * spanEta + minEta;
      theta=2*atan(exp(-1*eta));

      // Direction
      result.first  = XYZVector(cos(phi)*sin(theta), sin(phi)*sin(theta), cos(theta));
      result.second = eta;
      return result;
    }

    // private
    /**
     * Checks whether a track would hit a module
     * @param origin XYZVector of origin of the track
     * @param direction pointing XYZVector of the track
     * @param moduleV vector of modules to be checked
     * @return the vector of hit modules
     */
    std::vector<std::pair<Module*, HitType>> Analyzer::trackHit(const XYZVector& origin, const XYZVector& direction, const std::vector<Tracker*>& trackers) {

      std::vector<std::pair<Module*, HitType>> result;

      double distance;
      static const double zErrorSafetyMargin = 5. ; // track origin shift in units of zError to compute boundaries

      // Go through all trackers and their modules and save only those that were hit
      for (auto tracker : trackers) {
        for (auto& module : tracker->modules()) {

          // CouldHit -> save CPU time by calculating a cross-section of hit and given module within track phi and eta
          // (assuming origin error within +-5 sigma). Beware that module phi extends from -pi to +3*pi to avoid troubles
          // with -pi & +pi cross-line
          if (module->couldHit(direction, SimParms::getInstance()->zErrorCollider()*zErrorSafetyMargin)) {

            // Get hit
            auto hit = module->checkTrackHits(origin, direction);
            if (hit.second != HitType::NONE) result.push_back(std::make_pair(module,hit.second));
          }
        }
      }
      return result;
    }

    // Resets a module type counter
    void Analyzer::resetTypeCounter(std::map <std::string, int> &modTypes) {
      for (std::map <std::string, int>::iterator it = modTypes.begin();
           it!=modTypes.end(); it++) {
        (*it).second = 0;
      }
    }

    double Analyzer::diffclock(clock_t clock1, clock_t clock2) {
      double diffticks=clock1-clock2;
      double diffms=(diffticks*1000)/CLOCKS_PER_SEC;
      return diffms;
    }

    std::vector<TObject> Analyzer::getSavingVector() {
      std::vector<TObject> result;
      std::vector<TObject>::iterator it;

      for (it=savingGeometryV.begin(); it!=savingGeometryV.end(); ++it) {
        result.push_back(*it);
      }
      for (it=savingMaterialV.begin(); it!=savingMaterialV.end(); ++it) {
        result.push_back(*it);
      }
      return result;

    }


    void Analyzer::computeBandwidth(Tracker& tracker) {
      BandwidthVisitor bv(chanHitDistribution, bandwidthDistribution, bandwidthDistributionSparsified);
      SimParms::getInstance()->accept(bv);
      tracker.accept(bv);
    }

// Calculate average value in the given cut regions
std::vector<double> Analyzer::average(TGraph& myGraph, std::vector<double> cuts) {

  std::vector<double> averages;
  if (cuts.size()<2) return averages;

  std::sort(cuts.begin(), cuts.end());
  int iBorder;
  int nBoarders=cuts.size();
  double valuesCount, valuesSum;
  double vx, vy;

  for (iBorder=0; iBorder<nBoarders-1; ++iBorder) {

    // Here we have a cut between
    // cuts[iBorder] and cuts[iBorder+1]
    //std::cerr << myGraph.GetTitle() << std::endl;
    //std::cerr << cuts[iBorder] << "< x <= "<< cuts[iBorder+1] << std::endl;

    // Average on points within the cut
    valuesSum=0;
    valuesCount=0;
    for (int iPoint=0; iPoint<myGraph.GetN(); ++iPoint) {
      myGraph.GetPoint(iPoint, vx, vy);
      if ((vx>=cuts[iBorder]) && (vx<cuts[iBorder+1])) {
        valuesCount++;
        valuesSum+=vy;
      }
    }
    averages.push_back(valuesSum/valuesCount);
  }
  return averages;
}



std::map<int, TGraph>& Analyzer::getRhoGraphs(bool ideal, bool isTrigger) {
  int attribute = GraphBag::buildAttribute(ideal, isTrigger);
  attribute |= GraphBag::RhoGraph_Pt;
  return myGraphBag.getGraphs(attribute);
}

std::map<int, TGraph>& Analyzer::getPhiGraphs(bool ideal, bool isTrigger) {
  int attribute = GraphBag::buildAttribute(ideal, isTrigger);
  attribute |= GraphBag::PhiGraph_Pt;
  return myGraphBag.getGraphs(attribute);
}

std::map<int, TGraph>& Analyzer::getDGraphs(bool ideal, bool isTrigger) {
  int attribute = GraphBag::buildAttribute(ideal, isTrigger);
  attribute |= GraphBag::DGraph_Pt;
  return myGraphBag.getGraphs(attribute);
}

std::map<int, TGraph>& Analyzer::getCtgThetaGraphs(bool ideal, bool isTrigger) {
  int attribute = GraphBag::buildAttribute(ideal, isTrigger);
  attribute |= GraphBag::CtgthetaGraph_Pt;
  return myGraphBag.getGraphs(attribute);
}

std::map<int, TGraph>& Analyzer::getZ0Graphs(bool ideal, bool isTrigger) {
  int attribute = GraphBag::buildAttribute(ideal, isTrigger);
  attribute |= GraphBag::Z0Graph_Pt;
 return myGraphBag.getGraphs(attribute);
}

std::map<int, TGraph>& Analyzer::getPGraphs(bool ideal, bool isTrigger) {
  int attribute = GraphBag::buildAttribute(ideal, isTrigger);
  attribute |= GraphBag::PGraph_Pt;
  return myGraphBag.getGraphs(attribute);
}

TH1D& Analyzer::getHistoOptimalSpacing(bool actualWindow) {
  if (actualWindow) return optimalSpacingDistributionAW;
  else return optimalSpacingDistribution;
}

  }

