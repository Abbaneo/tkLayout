/*
 * AnalysiManager.cc
 *
 *  Created on: 27. 4. 2016
 *      Author: drasal
 */
#include <AnalysisManager.h>

// List of units
#include <AnalyzerUnit.h>
#include <AnalyzerGeometry.h>
#include <AnalyzerMatBudget.h>
#include <AnalyzerResolution.h>

// Other include files
#include "Detector.h"
#include <TH2D.h>
#include <TProfile.h>
#include <TH2I.h>
#include "RootWPage.h"
#include "SimParms.h"
#include <StopWatch.h>
#include <GitRevision.h>
#include "RootWContent.h"
#include "RootWInfo.h"
#include "RootWSite.h"
#include "RootWBinaryFileList.h"
#include "RootWTextFile.h"

//
// Constructor - create instances of all available analyzer units & prepare web container
//
AnalysisManager::AnalysisManager(const Detector& detector) :
 m_webSitePrepared(false)
{
  std::unique_ptr<AnalyzerUnit> unit;

  // Create AnalyzerGeometry
  unit = std::unique_ptr<AnalyzerUnit>(new AnalyzerGeometry(detector));
  m_units[unit->getName()] = std::move(unit);

  // Create AnalyzerMatBudget
  unit = std::unique_ptr<AnalyzerUnit>(new AnalyzerMatBudget(detector));
  m_units[unit->getName()] = std::move(unit);

  // Create AnalyzerMatBudget
  unit = std::unique_ptr<AnalyzerUnit>(new AnalyzerResolution(detector));
  m_units[unit->getName()] = std::move(unit);

  // Prepare Web site
  auto simParms = SimParms::getInstance();
  m_webSitePrepared = prepareWebSite(simParms->getLayoutName(), simParms->getWebDir());
}

//
// Destructor - clean out memory
//
AnalysisManager::~AnalysisManager()
{
  m_units.clear();
  m_webSite.reset();
}

//
// Initialize required analyzer unit
//
bool AnalysisManager::initUnit(int nTracks, std::string analyzerName)
{
  if (m_units.find(analyzerName)!=m_units.end()) {

    logINFO("Initializing " + analyzerName + " unit");
    return m_units[analyzerName]->init(nTracks);
  }
  else {

    logERROR("AnalysisManager::initModule: Module ("+analyzerName+") failed, no such unit.");
    return false;
  }
}

//
// Analyze required analyzer unit
//
bool AnalysisManager::analyzeUnit(std::string analyzerName)
{
  if (m_units.find(analyzerName)!=m_units.end()) {

    logINFO("Analyzing tracker by " + analyzerName + " unit");
    return m_units[analyzerName]->analyze();
  }
  else {

    logERROR("AnalysisManager::analyzerModule: Module ("+analyzerName+") failed, no such unit.");
    return false;
  }
}

//
// Visualize required analyzer unit
//
bool AnalysisManager::visualizeUnit(std::string analyzerName)
{
  if (m_units.find(analyzerName)!=m_units.end()) {

    logINFO("Visualizing output from " + analyzerName + " unit");
    return m_units[analyzerName]->visualize(*m_webSite);
  }
  else {

    logERROR("AnalysisManager::visualizeModule: Module ("+analyzerName+") failed, no such unit.");
    return false;
  }
}

//
// Make web site - publish all results in a html format using the results of visualize method of all used units
//
bool AnalysisManager::makeWebSite(bool addInfoPage, bool addLogPage) {

  if (!m_webSitePrepared) return false;
  else {

    startTaskClock(any2str("Creating website in: "+ SimParms::getInstance()->getWebDir()) );

    // Add info webPage
    if (addInfoPage) makeWebInfoPage();

    // Add log webPage
    if (addLogPage) makeWebLogPage();

    // Create web-site (set if verbosity on/off)
    bool webOK = m_webSite->makeSite(true);
    stopTaskClock();

    return webOK;
  }
}

//
// Prepare web site (html container for all results)
//
bool AnalysisManager::prepareWebSite(std::string layoutName, std::string webDir)
{
  m_webSite = std::unique_ptr<RootWSite>(new RootWSite("TkLayout results"));

  // Set directory
  if (webDir!="") m_webSite->setTargetDirectory(webDir);
  else {

    logERROR("AnalysisManager::prepareWebSite -> Web site directory not set!");
    return false;
  }

  // Set layout title
  if (layoutName!="") m_webSite->setTitle(layoutName);
  else {

    logERROR("AnalysisManager: Layout name not set!");
    return false;
  }

  // Set authors
  m_webSite->setComment("Layouts");
  m_webSite->setCommentLink("../");
  m_webSite->addAuthor("Giovanni Bianchi");
  m_webSite->addAuthor("Nicoletta De Maio");
  m_webSite->addAuthor("Stefano Martina");
  m_webSite->addAuthor("Stefano Mersi");
  m_webSite->setRevision(GitRevision::revisionNumber);
  return true;
}

//
// Prepare extra info web page
//
bool AnalysisManager::makeWebInfoPage()
{
  // Create web page: Log info
  RootWPage& myPage       = m_webSite->addPage("Info");

  // Summary of tkLayout parameters
  RootWContent& myContentParms = myPage.addContent("Summary of tkLayout parameters");

  // Command line arguments
  RootWInfo& myInfo = myContentParms.addInfo("Command line arguments");
  myInfo.setValue(SimParms::getInstance()->getCommandLine());

  // Summary of used geometry & simulation tracks
  if (m_units.find("AnalyzerGeometry")!=m_units.end()) {

    const AnalyzerGeometry* unit = dynamic_cast<const AnalyzerGeometry*>(m_units["AnalyzerGeometry"].get());
    RootWInfo& myInfo = myContentParms.addInfo("Number of tracks - geometry studies: ");
    myInfo.setValue(unit->getNGeomTracks());
  }
  if (m_units.find("AnalyzerResolution")!=m_units.end()) {

    const AnalyzerResolution* unit = dynamic_cast<const AnalyzerResolution*>(m_units["AnalyzerResolution"].get());
    RootWInfo& myInfo = myContentParms.addInfo("Number of tracks - resolution studies: ");
    myInfo.setValue(unit->getNSimTracks());
  }

  // Summary of geometry config files
  auto simParms   = SimParms::getInstance();
  auto includeSet = simParms->getListOfConfFiles();
  if (!includeSet.empty()) {
    std::vector<std::string> includeNameSet;
    std::transform(includeSet.begin(), includeSet.end(), std::back_inserter(includeNameSet), [](const std::string& s) {

      auto pos = s.find_last_of('/');
      return (pos != string::npos ? s.substr(pos+1) : s);
    });

    auto iterNameBegin = includeNameSet.begin();
    auto iterNameEnd   = includeNameSet.end();
    auto iterInclBegin = includeSet.begin();
    auto iterInclEnd   = includeSet.end();
    std::unique_ptr<RootWBinaryFileList> myBinaryFileList(new RootWBinaryFileList(iterNameBegin, iterNameEnd,
                                                                                  "Geometry configuration file(s)",
                                                                                  iterInclBegin, iterInclEnd));
    myContentParms.addItem(std::move(myBinaryFileList));
  }

  // Summary of Csv files
  RootWContent& myContentCsv = myPage.addContent("Summary list of csv files");

  // Csv files - resolution files
  if (m_units.find("AnalyzerResolution")!=m_units.end()) {

    const AnalyzerResolution* unit = dynamic_cast<const AnalyzerResolution*>(m_units["AnalyzerResolution"].get());

    // Pt option
    std::string fileName    = "ResolutionSummaryPt.csv";
    std::string webFileName = "Resolution summary (pt option)";
    RootWTextFile& myCsvResFilePt = myContentCsv.addTextFile(fileName, webFileName);

    for (auto it=unit->getCsvResPt().getCsvTextBegin(); it!=unit->getCsvResPt().getCsvTextEnd(); ++it) {
      myCsvResFilePt.addText(unit->getCsvResPt().getCsvText(it->first));
    }

    // P option
    fileName    = "ResolutionSummaryP.csv";
    webFileName = "Resolution summary (p option)";
    RootWTextFile& myCsvResFileP = myContentCsv.addTextFile(fileName, webFileName);

    for (auto it=unit->getCsvResP().getCsvTextBegin(); it!=unit->getCsvResP().getCsvTextEnd(); ++it) {
      myCsvResFileP.addText(unit->getCsvResP().getCsvText(it->first));
    }
  }

  return true;
}

//
// Prepare web log page for information
//
bool AnalysisManager::makeWebLogPage()
{
  // Found any log
  bool anyLogFound=false;

  // Create web page: Log info
  RootWPage& myPage = m_webSite->addPage("Log");

  // Check logs
  if (!MessageLogger::hasEmptyLog(MessageLogger::ERROR))        myPage.setAlert(1);
  else if (!MessageLogger::hasEmptyLog(MessageLogger::WARNING)) myPage.setAlert(0.5);

  // Print summary logs for each verbosity level
  for (auto iLevel=0; iLevel < MessageLogger::NumberOfLevels; ++iLevel) {

    if (!MessageLogger::hasEmptyLog(iLevel)) {

      bool defaultOpen = false;
      if (iLevel<=MessageLogger::WARNING) defaultOpen=true;

      anyLogFound = true;
      RootWContent& newContent = myPage.addContent(MessageLogger::getLevelName(iLevel), defaultOpen);
      newContent.addText("<pre>"+MessageLogger::getSummaryLog(iLevel)+"</pre>");
    }
  }

  return anyLogFound;
}
