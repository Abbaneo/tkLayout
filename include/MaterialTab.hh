#ifndef MATERIALTAB_H
#define MATERIALTAB_H

//#include <map>
#include <tuple>

#include <global_constants.hh>
#include <global_funcs.hh>

namespace material {

  //class TableChemicalElement : public TableMaterial {


  class ChemicalBase {
  public:
    ChemicalBase(const double density);
    const double getDensity() const { return density_; }
    const double getRadiationLength() const { return radiationLength_; }
    const double getInteractionLength() const { return interactionLength_; }

    virtual const bool isChemicalElement() const = 0;

  protected:
    double density_;
    double radiationLength_;
    double interactionLength_;
  };
  

  class ChemicalElement : public ChemicalBase {
  public:
    ChemicalElement(const double density, const int atomicNumber, const double atomicMass);
    const int getAtomicNumber() const { return atomicNumber_; } 
    const double getAtomicMass() const { return atomicMass_; } 
    const bool isChemicalElement() const override { return true; }

  private:
    // TO DO: should probably set more accurate RL and IL values directly in the cfg file?
    const double computeRadiationLength(const int atomicNumber, const int atomicMass); 
    const double computeInteractionLength(const int atomicMass);

    int atomicNumber_;  // atome's Z
    double atomicMass_;    // atome's A    
  };


  typedef std::map<std::string, ChemicalBase*> ChemicalBaseMap; // to do: define ChemicalBase directly inside MaterialsTable to avoid this?
  typedef std::map<std::string, ChemicalElement*> ChemicalElementMap;

  typedef std::vector< std::pair<std::string, int> > ChemicalFormula;
  typedef std::vector< std::pair<std::string, double> > MassicComposition;

  class ChemicalMixture : public ChemicalBase {
  public:
    ChemicalMixture(const double density, const ChemicalFormula& formula, const ChemicalElementMap& allChemicalElements);
    ChemicalMixture(const double density, const MassicComposition& ratios, const ChemicalBaseMap& alreadyDefinedMaterials);

    const bool isChemicalElement() const override { return false; }
    const bool hasChemicalFormula() const { return (formula_.size() != 0); }
    const ChemicalFormula getChemicalFormula() const { return formula_; }
    const MassicComposition getMassicComposition() const { return ratios_; }

  private:
    // TO DO: should probably place more accurate RL and IL values directly in the file?
    const MassicComposition computeMassicComposition(const ChemicalFormula& formula, const ChemicalElementMap& allChemicalElements) const;
    const std::pair<double, double> computeRadiationAndInteractionLengths(const MassicComposition& ratios, const ChemicalBaseMap& alreadyDefinedMaterials) const;

    ChemicalFormula formula_;
    MassicComposition ratios_;
  };

  typedef std::map<std::string, ChemicalMixture*> ChemicalMixtureMap;



  typedef std::pair<ChemicalElementMap, ChemicalMixtureMap > MaterialsTableType;
  class MaterialsTable : public MaterialsTableType {
  private:
    MaterialsTable();
 
  public:
    static const MaterialsTable& instance();

    double density(const std::string materialName) const;  // TO DO: should return const double!!!
    double radiationLength(const std::string materialName) const;
    double interactionLength(const std::string materialName) const;

    const ChemicalElementMap getAllChemicalElements() const { return first; }
    const ChemicalMixtureMap getAllChemicalMixtures() const { return second; }
  };












  typedef std::map<std::string, std::tuple<double, double, double> > MaterialTabType;
  class MaterialTab : public MaterialTabType {
  private:
    MaterialTab();
    static const std::string msg_no_mat_file;
    static const std::string msg_no_mat_file_entry1;
    static const std::string msg_no_mat_file_entry2;

  public:
    static const MaterialTab& instance();

    double density(std::string material) const;
    double radiationLength(std::string material) const;
    double interactionLength(std::string material) const;
  };
} /* namespace material */

#endif /* MATERIALTAB_H */
