#ifndef BUNDLE_HH
#define BUNDLE_HH

#include <vector>
#include <string>

#include "Property.hh"
#include "Module.hh"
#include "Cabling/PhiPosition.hh"


namespace insur { class Cable; }
using insur::Cable;


class Bundle : public PropertyObject, public Buildable, public Identifiable<int> {
  typedef PtrVector<Module> Container; 

public:
  Bundle(const int id, const Category& type, const std::string subDetectorName, const int layerDiskNumber, const PhiPosition& phiPosition, const bool isPositiveCablingSide, const bool isTiltedPart);
  ~Bundle();

  // MODULES CONNECTED TO THE BUNDLE.
  const Container& modules() const { return modules_; }
  Container& modules() { return modules_; }
  const int numModules() const { return modules_.size(); }
  void addModule(Module* m) { modules_.push_back(m); }

  // CABLE THE BUNDLE IS CONNECTED TO.
  const Cable* getCable() const { return cable_; }
  void setCable(Cable* cable) { cable_ = cable; }

  void moveMaxPhiModuleFromOtherBundle(Bundle* otherBundle);
  void moveMinPhiModuleFromOtherBundle(Bundle* otherBundle);

  const double minPhi() const;
  const double maxPhi() const;
  const double meanPhi() const;

  Module* minPhiModule() const;
  Module* maxPhiModule() const;

  const Category& type() const { return type_; }
  const std::string subDetectorName() const { return subDetectorName_; }
  const int layerDiskNumber() const { return layerDiskNumber_; }
  const PhiPosition& phiPosition() const { return phiPosition_; }  
  const bool isPositiveCablingSide() const { return isPositiveCablingSide_; }
  const bool isTiltedPart() const { return isTiltedPart_; }

  const int plotColor() const { return plotColor_; }

  const int powerServicesChannel() const { return powerServicesChannel_; }
  const ChannelSection& powerServicesChannelSection() const { return powerServicesChannelSection_; }
  const int powerServicesChannelPlotColor() const { return powerServicesChannelPlotColor_; }

  void setPowerServicesChannel(std::pair<int, ChannelSection>& powerServicesChannel) {
    powerServicesChannel_ = powerServicesChannel.first;
    powerServicesChannelSection_ = powerServicesChannel.second;
    powerServicesChannelPlotColor_ = computePowerServicesChannelPlotColor(powerServicesChannel);
  }


private:
  const int computePlotColor(const int id, const bool isPositiveCablingSide) const;
  int computePowerServicesChannelPlotColor(std::pair<int, ChannelSection>& powerServicesChannel) const;

  Container modules_;

  Cable* cable_ = nullptr;

  Category type_;
  std::string subDetectorName_;
  int layerDiskNumber_;
  PhiPosition phiPosition_;
  bool isPositiveCablingSide_;
  bool isTiltedPart_;

  int plotColor_;

  int powerServicesChannel_;
  ChannelSection powerServicesChannelSection_;
  int powerServicesChannelPlotColor_;
};



#endif
