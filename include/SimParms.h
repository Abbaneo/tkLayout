#ifndef SIMPARMS_H
#define SIMPARMS_H

#include <map>
#include <string>
#include <sstream>
#include <fstream>

#include "Property.h"
#include "Visitable.h"

// Forward declaration
class GeometryVisitor;
class ConstGeometryVisitor;
class IrradiationMapsManager;

/*
 * @class SimParms
 * @brief A singleton class containing generic information needed across the tkLayout toolkit.
 * @details A singleton class containing generic information needed across the tkLayout toolkit. Various generic
 * parameters (geometry layout name, main geometry config file, etc) as well as generic environment/geometry
 * parameters (not directly related to individual sub-trackers) are held here. Two mechanisms are used to
 * fill-in the SimParms container: the environment variables are read-in from SimParms config file, the rest
 * are saved through setter methods at different level of processing (e.g. after full geometry is read-in).
 */
class SimParms : public PropertyObject, public Buildable, public Visitable {

 public:

  //! SimParms access method -> get instance of singleton class SimParms
  static SimParms* getInstance();
  
  //! Destructor
  ~SimParms();

  //! SimParms visitable -> implemented accept method to call visitor pattern
  void accept(GeometryVisitor& v);

  //! SimParms visitable -> implememented const accept method to call visitor pattern
  void accept(ConstGeometryVisitor& v) const;

  //! Cross-check that Sim parameters correctly read-in from the file
  void crosscheck();

  //! Set command line options passed over to program to analyze data
  //! @param[in] commandLine    Content of command line
  void setCommandLine(int argc, char* argv[]);

  //! Set geometry layout name
  //! @param[in] layout name
  void setLayoutName(std::string layoutName) {m_layoutName = layoutName;}

  //! Set default html directory, where all results are saved
  //! @param[in] directory address
  void setWebDir(std::string htmlDir) {m_htmlDir = htmlDir;}

  //! Set name of main geometry config file
  //! @param[in] file name
  void setBaseGeomFileName(std::string geomFileName) {m_geomFile = geomFileName;}

  //! Set path of tkLayout run directory
  //! @param[in] run directory path
  void setRunDirPath(std::string baseDir) {m_baseDir = baseDir;}

  //! Set list of all configuration files obtained from the base geometry file using @include command
  //! @param [in] set of all include files (strings)
  void setListOfConfFiles(std::set<std::string> includeSet) {m_includeSet = includeSet;}

  //! Get command line content
  //! @return line content as one string
  std::string getCommandLine() const {return m_commandLine;}

  //! Get geometry layout name
  //! @return layout name
  std::string getLayoutName() const {return m_layoutName;}

  //! Get default html directory, where all results are saved
  //! @return directory address
  std::string getWebDir() const {return m_htmlDir;}

  //! Get name of main geometry config file
  //! @return file name
  std::string getBaseGeomFileName() const {return m_geomFile;}

  //! Get path of tkLayout run directory
  //! @return run directory path
  std::string getRunDirPath() const {return m_baseDir;}

  //! Get list of all configuration files obtained from the base geometry file using @include command
  //! @return set of all include files (strings)
  std::set<std::string> getListOfConfFiles() const {return m_includeSet;}

  //! Get reference to irradiation maps manager
  const IrradiationMapsManager& irradiationMapsManager() const { return *m_irradiationMapsManager;}

  // Variables to be read in by SimParms class from a configuration file
  ReadonlyProperty<int   , NoDefault> numMinBiasEvents;
  ReadonlyProperty<int   , NoDefault> zErrorCollider;
  ReadonlyProperty<int   , NoDefault> rError;
  ReadonlyProperty<bool  , NoDefault> useIPConstraint;
  ReadonlyProperty<int   , NoDefault> ptCost;
  ReadonlyProperty<int   , NoDefault> stripCost;
  ReadonlyProperty<double, NoDefault> efficiency;
  ReadonlyProperty<double, NoDefault> pixelEfficiency;

  ReadonlyProperty<int   , NoDefault> bunchSpacingNs;

  ReadonlyProperty<double, Default>   triggerEtaCut;
  ReadonlyProperty<double, Default>   triggerPtCut;
  ReadonlyProperty<int   , Default>   numTriggerTowersEta, numTriggerTowersPhi;

  ReadonlyProperty<double, Default>   timeIntegratedLumi;
  ReadonlyProperty<double, Default>   operatingTemp;
  ReadonlyProperty<double, Default>   chargeDepletionVoltage;
  ReadonlyProperty<double, Default>   alphaParm;
  ReadonlyProperty<double, Default>   referenceTemp;

  ReadonlyProperty<double, Default>   magneticField;      //!< Central magnetic field in Tesla

  // To include dipole region in a quick way
  ReadonlyProperty<double, Default>   dipoleMagneticField;//!< Dipole integral magnetic field in [Tm]
  ReadonlyProperty<double, Default>   dipoleDPlResAt10TeV;//!< Dipole deltaPl/Pl resolution of dipole tracker at 10 TeV
  ReadonlyProperty<double, Default>   dipoleXToX0;        //  [%]

  // Beam pipe radius, thickness, thickness in rad. length, in int. length
  ReadonlyProperty<double, Default>   bpRadius;           //!< Beam pipe radius
  ReadonlyProperty<double, Default>   bpThickness;        //!< Beam pipe thickness
  ReadonlyProperty<double, Default>   bpRadLength;        //!< Beam pipe rad. length in [%] @ 90 deg
  ReadonlyProperty<double, Default>   bpIntLength;        //!< Beam pipe int. lenght in [%] @ 90 deg

  PropertyVector<std::string, ','>    irradiationMapFiles;
  //std::vector<Property<std::string, NoDefault>> irradiationMapFiles;

  Property<std::string, Default> bFieldMapFile;           // Map of b field - not currently currently for tracking
  Property<std::string, Default> chargedMapFile;          // Map of charged hadron fluxes, segmented in R x Z
  Property<std::string, Default> chargedMapLowThFile;     // Map of charged hadron fluxes - electrons with lower threshold, segmented in R x Z
  Property<std::string, Default> chargedMapBOffMatOnFile; // Map of charged hadron fluxes, segmented in R x Z, no mag. field applied
  Property<std::string, Default> chargedMapBOnMatOffFile; // Map of charged hadron fluxes, segmented in R x Z, no material
  Property<std::string, Default> chargedMapBOffMatOffFile;// Map of charged hadron fluxes, segmented in R x Z, no mag. field applied, no material
  Property<std::string, Default> chargedMapBOffTrkOffFile;// Map of charged hadron fluxes, segmented in R x Z, no mag. field applied, no tracker material
  Property<std::string, Default> photonsMapFile;          // Map of photon fluxes, segmented in R x Z
  Property<std::string, Default> photonsMapLowThFile;     // Map of photon fluxes - electrons with lower threshold, segmented in R x Z
  Property<std::string, Default> photonsMapBOffMatOnFile; // Map of photon fluxes, segmented in R x Z, no mag. field applied
  Property<std::string, Default> photonsMapBOnMatOffFile; // Map of photon fluxes, segmented in R x Z, no material
  Property<std::string, Default> photonsMapBOffMatOffFile;// Map of photon fluxes, segmented in R x Z, no mag. field applied, no material
  Property<std::string, Default> photonsMapBOffTrkOffFile;// Map of photon fluxes, segmented in R x Z, no mag. field applied, no tracker material

  Property<        double, NoDefault> minTracksEta, maxTracksEta;
  PropertyNode<std::string>           taggedTracking;

 private:

  //! Singleton private constructor -> initialize all variables to be read-out from configuration file & define if checker should be called after parsing
  SimParms();

  static SimParms*       s_instance;     //!< An instance of SimParms class - singleton pattern

  std::string            m_commandLine;  //!< Command line options passed over to program to analyze data
  std::string            m_geomFile;     //!< Name of main geometry config file
  std::string            m_baseDir;      //!< Base tkLayout run directory
  std::string            m_htmlDir;      //!< Default html directory, where all results are saved
  std::string            m_layoutName;   //!< Geometry layout name
  std::set<std::string>  m_includeSet;   //!< List of all configuration files obtained from the base geometry file using @include command

  IrradiationMapsManager* m_irradiationMapsManager; //!< Irradiation maps manager



};

#endif
