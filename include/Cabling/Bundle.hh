#ifndef BUNDLE_HH
#define BUNDLE_HH

#include <vector>
#include <string>

#include "Property.hh"
#include "Module.hh"


//#include "CablingVisitable.h"
//#include "CablingVisitor.h"

/*using std::string;
using std::vector;
using std::pair;
using std::unique_ptr;*/

namespace insur { class Cable; }
using insur::Cable;



//class Bundle : public PropertyObject, public Buildable, public Identifiable<int>, public CablingVisitable {
class Bundle : public PropertyObject, public Buildable, public Identifiable<int> {
  std::string type_;
  std::string subDetectorName_;
  int layerDiskNumber_;

  double phiSegmentWidth_;
  int phiSegmentRef_;
  double phiRegionStart_;
  double phiRegionWidth_;
  int phiRegionRef_;
  double phiSectorWidth_;
  int phiSectorRef_;
  bool isPositiveCablingSide_;

  int plotColor_;

  bool isTiltedPart_ = false;
  Cable* cable_ = NULL;


  typedef PtrVector<Module> Container;
  //typedef PtrSet<Module> Container;
  //typedef std::vector<Module*> Container;
  Container modules_;

  //Property<int, Default> nModulesPerBundle;

public:

  //~Bundle();
  Bundle(int id, std::string type, std::string subDetectorName, int layerDiskNumber, double phiSegmentWidth, int phiSegmentRef, double phiRegionStart, double phiRegionWidth, int phiRegionRef, const double phiSectorWidth, int phiSectorRef, bool isPositiveCablingSide) {
    myid(id);

    type_ = type;
    subDetectorName_ = subDetectorName;
    layerDiskNumber_ = layerDiskNumber;

    phiSegmentWidth_ = phiSegmentWidth;
    phiSegmentRef_ = phiSegmentRef;
    phiRegionStart_ = phiRegionStart;
    phiRegionWidth_ = phiRegionWidth;
    phiRegionRef_ = phiRegionRef;
    phiSectorWidth_ = phiSectorWidth;
    phiSectorRef_ = phiSectorRef;
    isPositiveCablingSide_ = isPositiveCablingSide;

    // Calculate plotColor_
    int plotId = fabs(id);
    int plotType = 2 + plotId % 2;  // Barrel : Identifies Flat vs Tilted. Endcap : Identifies PS10G vs PG5GA vs PS5GB vs 2S type.
    int dizaine = plotId / 10;
    int plotPhi = dizaine % 3;  // Barrel : Identifies phiSegmentRef. Endcap : Identifies phiRegionRef.
    plotColor_ = plotType * 3 + plotPhi;  
  };

  const std::string type() const { return type_; }
  const std::string subDetectorName() const { return subDetectorName_; }
  const int layerDiskNumber() const { return layerDiskNumber_; }

  const double phiSegmentWidth() const { return phiSegmentWidth_; }
  const int phiSegmentRef() const { return phiSegmentRef_; }
  const double phiRegionStart() const { return phiRegionStart_; }
  const double phiRegionWidth() const { return phiRegionWidth_; }
  const int phiRegionRef() const { return phiRegionRef_; }
  const double phiSectorWidth() const { return phiSectorWidth_; }
  const int phiSectorRef() const { return phiSectorRef_; }
  const bool isPositiveCablingSide() const { return isPositiveCablingSide_; }

  const int plotColor() const { return plotColor_; }



  const bool isTiltedPart() const { return isTiltedPart_; }
  void setIsTiltedPart(bool isTiltedPart) { isTiltedPart_ = isTiltedPart; }

  const Cable* getCable() const { return cable_; }
  void setCable(Cable* cable) { cable_ = cable; }


  
  const Container& modules() const { return modules_; }
  void addModule(Module* m) { modules_.push_back(m); }
  /*void removeModule(Module* m) {
    int detId = m->myDetId();
    modules_.erase_if([detId](Module& m) { return (m.myDetId() == detId); });
    }*/

  Container& modules() { return modules_; }

  void moveMaxPhiModuleFromOtherBundle(Bundle* otherBundle) {
    Container& otherBundleModules = otherBundle->modules();
    auto maxPhiModuleIt = std::max_element(otherBundleModules.begin(), otherBundleModules.end(), [](const Module& a, const Module& b) {
	return (femod(a.center().Phi(), 2. * M_PI) <= femod(b.center().Phi(), 2. * M_PI));
      });

    modules_.transfer(modules_.end(), 
		      maxPhiModuleIt,
		      otherBundleModules);
  }

  void moveMinPhiModuleFromOtherBundle(Bundle* otherBundle) {
    Container& otherBundleModules = otherBundle->modules();
    auto minPhiModuleIt = std::min_element(otherBundleModules.begin(), otherBundleModules.end(), [](const Module& a, const Module& b) {
	return (femod(a.center().Phi(), 2. * M_PI) <= femod(b.center().Phi(), 2. * M_PI));
      });

    modules_.transfer(modules_.end(), 
		      minPhiModuleIt,
		      otherBundleModules);
  }





  int numModules() const { return modules_.size(); }

  const double minPhi() const { 
    double min = std::numeric_limits<double>::max();
    for (const auto& m : modules_) { min = MIN(min, femod(m.center().Phi(), 2. * M_PI) ); } return min;
  }

  const double maxPhi() const { 
    double max = 0.;
    for (const auto& m : modules_) { max = MAX(max, femod(m.center().Phi(), 2. * M_PI) ); } return max;
  }

  Module* minPhiModule() const {
    const Module* mod = &(*std::min_element(modules_.begin(), modules_.end(), [](const Module& a, const Module& b) {
	return (femod(a.center().Phi(), 2. * M_PI) <= femod(b.center().Phi(), 2. * M_PI));
	}));
    Module* mod2 = const_cast<Module*>(mod);  // TO DO : Ugly, completely delete this ! actually, PtrSet should be defined and used as a modules container
    return mod2;
  }

  Module* maxPhiModule() const {
    const Module* mod = &(*std::max_element(modules_.begin(), modules_.end(), [](const Module& a, const Module& b) {
	  return (femod(a.center().Phi(), 2. * M_PI) <= femod(b.center().Phi(), 2. * M_PI));
	}));
    Module* mod2 = const_cast<Module*>(mod);  // TO DO : Ugly, completely delete this ! actually, PtrSet should be defined and used as a modules container
    return mod2;
  }
  


  /*Bundle() :
            nModulesPerBundle      ("nModulesPerBundle"      , parsedAndChecked(), 6)
  {}
  void setup() {
  }

  Container& modules() { return detectormodules_; }
  const Container& modules() const { return detectormodules_; }
  int nModules() const { return detectormodules_.size(); }
  int maxModules() {return nModulesPerBundle(); }
  
  void check() override;
  void build();

  void addModule(Module& m) {}

  void accept(CablingVisitor& v) { 
    v.visit(*this); 
    for (Module& m : detectormodules_) { m.accept(v); }
  }
  void accept(ConstCablingVisitor& v) const { 
    v.visit(*this); 
    for (const auto& m : detectormodules_) { m.accept(v); }
    }*/

};



#endif
