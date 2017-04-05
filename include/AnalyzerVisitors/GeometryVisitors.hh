#ifndef _GEOMETRYVISITORS_HH
#define	_GEOMETRYVISITORS_HH

#include <fstream>
#include <sstream>
// ROOT objects
#include <TGeoManager.h>
#include <TGeoMedium.h>
#include <TGeoMaterial.h>
#include <TGeoVolume.h>
#include <TGeoArb8.h>
#include <TGeoMatrix.h>
#include <TFile.h>
#include <THStack.h>
#include <TStyle.h>
#include <TText.h>
#include <TLatex.h>
#include <TColor.h>
#include <TLine.h>
#include <TView.h>
#include <TLegend.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TPaletteAxis.h>
#include <TProfile.h>
#include <TPolyLine3D.h>
#include <TArc.h>
// Program constants
#include <global_constants.hh>
// Custom objects
#include <Tracker.hh>
#include <Analyzer.hh>
#include <TagMaker.hh>

#include <InactiveSurfaces.hh>
#include "Module.hh"
#include <RootWeb.hh>
#include <vector>
#include <set>
#include <Palette.hh>

#include <PlotDrawer.hh>
#include "VizardTools.hh"





    //**************************************//
    //*             Visitor                *//
    //*  Layers and disks : global info    *//
    //*                                    *//
    //**************************************//

class LayerDiskSummaryVisitor : public ConstGeometryVisitor {
public:
  // info
  RootWTable* layerTable = new RootWTable();
  RootWTable* diskTable = new RootWTable();
  std::vector<RootWTable*> diskNames;
  std::vector<RootWTable*> ringTables;
  std::map<std::string, std::set<std::string> > tagMapPositions;
  std::map<std::string, int> tagMapCount;
  std::map<std::string, long> tagMapCountChan;
  std::map<std::string, double> tagMapMaxStripOccupancy;
  std::map<std::string, double> tagMapAveStripOccupancy;
  std::map<std::string, double> tagMapMaxHitOccupancy;
  std::map<std::string, double> tagMapAveHitOccupancy;
  std::map<std::string, double> tagMapAveRphiResolution;
  std::map<std::string, double> tagMapAveRphiResolutionRmse;
  std::map<std::string, double> tagMapSumXResolution;
  std::map<std::string, double> tagMapSumSquaresXResolution;
  std::map<std::string, double> tagMapCountXResolution;
  std::map<std::string, double> tagMapIsParametrizedXResolution;
  std::map<std::string, double> tagMapAveYResolution;
  std::map<std::string, double> tagMapAveYResolutionRmse;
  std::map<std::string, double> tagMapSumYResolution;
  std::map<std::string, double> tagMapSumSquaresYResolution;
  std::map<std::string, double> tagMapCountYResolution;
  std::map<std::string, double> tagMapIsParametrizedYResolution;
  std::map<std::string, double> tagMapAveRphiResolutionTrigger;
  std::map<std::string, double> tagMapAveYResolutionTrigger;
  std::map<std::string, double> tagMapSensorPowerAvg;
  std::map<std::string, double> tagMapSensorPowerMax;
  std::map<std::string, const DetectorModule*> tagMap;

  // counters
  int nBarrelLayers=0;
  int nEndcaps=0;
  int nDisks=0;
  int nRings=0;
  int totalBarrelModules = 0;
  int totalEndcapModules = 0;

  double totArea = 0;
  int totCountMod = 0;
  int totCountSens = 0;
  long totChannel = 0;
  double totalSensorPower = 0;

  double nMB;

  void preVisit();
  void visit(const Barrel& b) override;
  void visit(const Layer& l) override;
  void visit(const Endcap& e) override;
  void visit(const Disk& d) override;
  void visit(const Ring& r) override;
  void visit(const Module& m) override;
  void visit(const EndcapModule& m) override;
  void postVisit();
};


    //***************************************//
    //*                Visitor              *//
    //* Automatic-placement tilted layers : *//
    //*            Additional info          *//
    //*                                     *//
    //***************************************//

class TiltedLayersVisitor : public ConstGeometryVisitor {
public:
  // tilted info
  std::vector<RootWTable*> tiltedLayerNames;
  std::vector<RootWTable*> flatPartNames;
  std::vector<RootWTable*> tiltedPartNames;
  std::vector<RootWTable*> flatPartTables;
  std::vector<RootWTable*> tiltedPartTables;

  // counter
  int numTiltedLayers = 0;

  void visit(const Layer& l) override;     
};

#endif // _GEOMETRYVISITORS_HH
