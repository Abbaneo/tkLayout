/*
 * AnalyzerModule.cpp
 *
 *  Created on: 27. 11. 2015
 *      Author: drasal
 */

#include "AnalyzerModule.h"

AnalyzerModule::AnalyzerModule(std::vector<Tracker*> trackers)
{
  // Set geometry, i.e. individual trackers
  for (auto it : trackers) m_trackers.push_back(it);
}

AnalyzerModule::~AnalyzerModule() {}

