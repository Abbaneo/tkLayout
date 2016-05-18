#include "Tracker.h"
#include "global_constants.h"

#include <Barrel.h>
#include <DetectorModule.h>
#include <Disk.h>
#include <Endcap.h>
#include <Layer.h>
#include <Ring.h>
#include <RodPair.h>
#include <SupportStructure.h>

//
// Helper class: Name visitor - methods
//
void HierarchicalNameVisitor::visit(Barrel& b)       { cnt = b.myid(); cntId++; }
void HierarchicalNameVisitor::visit(Endcap& e)       { cnt = e.myid(); cntId++; }
void HierarchicalNameVisitor::visit(Layer& l)        { c1 = l.myid(); }
void HierarchicalNameVisitor::visit(Disk& d)         { c1 = d.myid(); }
void HierarchicalNameVisitor::visit(RodPair& r)      { c2 = r.myid(); }
void HierarchicalNameVisitor::visit(Ring& r)         { c2 = r.myid(); }
void HierarchicalNameVisitor::visit(Module& m)       { m.cntNameId(cnt, cntId); }
void HierarchicalNameVisitor::visit(BarrelModule& m) { m.layer(c1); m.rod(c2); }
void HierarchicalNameVisitor::visit(EndcapModule& m) { m.disk(c1); m.ring(c2); }

//
// Constructor - parse geometry config file using boost property tree & read-in Barrel, Endcap & Support nodes
//
Tracker::Tracker(const PropertyTree& treeProperty) :
 m_barrelNode(    "Barrel"          , parsedOnly()),
 m_endcapNode(    "Endcap"          , parsedOnly()),
 m_supportNode(   "Support"         , parsedOnly()),
 etaCut(          "etaCut"          , parsedOnly(), insur::geom_max_eta_coverage),
 isPixelType(     "isPixelType"     , parsedOnly(), true),
 servicesForcedUp("servicesForcedUp", parsedOnly(), true),
 skipAllServices( "skipAllServices" , parsedOnly(), false),
 skipAllSupports( "skipAllSupports" , parsedOnly(), false),
 m_containsOnly(  "containsOnly"    , parsedOnly())
{
  // Set the geometry config parameters
  this->myid(treeProperty.data());
  this->store(treeProperty);

  // Build & setup tracker
  this->build();
  this->setup();
}

//
// Build recursively individual subdetector systems: Barrels, Endcaps  -> private method called by constructor
//
void Tracker::build() {

  try {
    check();

    double barrelMaxZ = 0;

    // Build barrel tracker
    for (auto& mapel : m_barrelNode) {
      if (!m_containsOnly.empty() && m_containsOnly.count(mapel.first) == 0) continue;
      Barrel* b = GeometryFactory::make<Barrel>();
      b->myid(mapel.first);
      b->store(propertyTree());
      b->store(mapel.second);
      b->build();
      b->cutAtEta(etaCut());
      barrelMaxZ = MAX(b->maxZ(), barrelMaxZ);
      m_barrels.push_back(b);
    }

    // Build end-cap tracker
    for (auto& mapel : m_endcapNode) {
      if (!m_containsOnly.empty() && m_containsOnly.count(mapel.first) == 0) continue;
      Endcap* e = GeometryFactory::make<Endcap>();
      e->myid(mapel.first);
      e->barrelMaxZ(barrelMaxZ);
      e->store(propertyTree());
      e->store(mapel.second);
      e->build();
      e->cutAtEta(etaCut());
      m_endcaps.push_back(e);
    }

    // Build support structures within tracker
    for (auto& mapel : m_supportNode) {
      SupportStructure* s = new SupportStructure();
      s->store(propertyTree());
      s->store(mapel.second);
      s->buildInTracker();
      m_supportStructures.push_back(s);
    }
  }
  catch (PathfulException& pe) {

    pe.pushPath(fullid(*this));
    throw;
  }

  // Search tracker and get its properties
  accept(m_modulesSetVisitor);
  accept(m_cntNameVisitor);

  cleanup();
  builtok(true);
}

//
// Calculate various tracker related properties -> private method called by constructor
//
void Tracker::setup() {

  maxR.setup([this]() {
    double max = 0;
    for (const auto& b : m_barrels) max = MAX(max, b.maxR());
    for (const auto& e : m_endcaps) max = MAX(max, e.maxR());
    return max;
  });
  minR.setup([this]() {
    double min = std::numeric_limits<double>::max();
    for (const auto& b : m_barrels) min = MIN(min, b.minR());
    for (const auto& e : m_endcaps) min = MIN(min, e.minR());
    return min;
  });
  maxZ.setup([this]() {
    double max = 0;
    for (const auto& b : m_barrels) max = MAX(max, b.maxZ());
    for (const auto& e : m_endcaps) max = MAX(max, e.maxZ());
    return max;
  });
  minEta.setup([this]() {
    double min = std::numeric_limits<double>::max();
    for (const auto& m : m_modulesSetVisitor.modules()) min = MIN(min, m->minEta());
    return min;
  });
  maxEta.setup([this]() {
    double max = -std::numeric_limits<double>::max();
    for (const auto& m : m_modulesSetVisitor.modules()) max = MAX(max, m->maxEta());
    return max;
  });
}

//
// GeometryVisitor pattern -> tracker visitable
//
void Tracker::accept(GeometryVisitor& v) {
  v.visit(*this);
  for (auto& b : m_barrels) { b.accept(v); }
  for (auto& e : m_endcaps) { e.accept(v); }
}

//
// GeometryVisitor pattern -> tracker visitable (const. option)
//
void Tracker::accept(ConstGeometryVisitor& v) const {
  v.visit(*this);
  for (const auto& b : m_barrels) { b.accept(v); }
  for (const auto& e : m_endcaps) { e.accept(v); }
}
