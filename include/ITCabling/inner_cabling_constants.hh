#ifndef INNER_CABLING_CONSTANTS_HH
#define INNER_CABLING_CONSTANTS_HH

#include <math.h>
#include <string>
#include "global_funcs.hh"
#include "Cabling/cabling_constants.hh"


// DESIGN
// Maximum number of modules per serial power chain
static const int inner_cabling_maxNumModulesPerPowerChain = 10;
// Maximum number of ELinks per GBT
static const int inner_cabling_maxNumELinksPerGBT = 7;
// Maximum number of GBTs per bundle
static const int inner_cabling_maxNumGBTsPerBundle = 12;
// Maximum number of bundles per cable
static const int inner_cabling_maxNumBundlesPerCable = 6;


// READOUT 1.28 GB/s E-LINKS
static const int inner_cabling_numELinksPerModuleBarrelLayer1 = 6;
static const int inner_cabling_numELinksPerModuleBarrelLayer2 = 2;
static const int inner_cabling_numELinksPerModuleBarrelLayer3 = 2;
static const int inner_cabling_numELinksPerModuleBarrelLayer4 = 1;

static const int inner_cabling_numELinksPerModuleForwardRing1 = 3;
static const int inner_cabling_numELinksPerModuleForwardRing2 = 2;
static const int inner_cabling_numELinksPerModuleForwardRing3 = 2;
static const int inner_cabling_numELinksPerModuleForwardRing4 = 1;

static const int inner_cabling_numELinksPerModuleEndcap = 1;


// ROUNDING
static const double inner_cabling_roundingTolerance = cabling_roundingTolerance;


// GEOMETRY NAMES
// It is unavailable by design to use the geometry blocks names for the inner_cabling map.
// At least, they are placed here to ease renaming if needed.
static const std::string inner_cabling_tbpx = "PXB";
static const std::string inner_cabling_tfpx = "FPIX_1";
static const std::string inner_cabling_tepx = "FPIX_2";


// NEGATIVE INNER_CABLING SIDE
static const std::string inner_cabling_negativePrefix = "neg";


// INNER_CABLING TYPE
// PS or SS stands for the module type.
// 10G or 5G stands for the speed in the optical fibers (Gb/s).
// Modules with the same inner_cabling type have to be conected to the same DTC type (PS10G, PS5G, or SS).

// A or B is an extra distinction for PS10G modules, used in TEDD only. 
// PS10GA is used for modules which should be connected to 10G links (because of their location in the Tracker).
// PS10GB is used for modules which could actually in theory be connected to 5G links. These modules are connected to 10G links for data rates reduction purposes only.
enum PowerChainType { IUNDEFINED, I4A, I8A };


// SERVICES CHANNELS INFO
// There are 3 sections for cables in PP1: A, B, and C.
// The optical bundles are always placed in section B.
// Though, the inner_cabling map is also used for power cables. 1 optical bundle = 1 power cable. 
// As a result, the full optical inner_cabling map can be used for power cables mapping.
// The only difference is that power cables are assigned to section A or C in PP1.
// This additional info is dealt with by ChannelSlot.

//enum ChannelSlot { UNKNOWN, A, B, C };

// Offsets used in TEDD to split power cables coming from the same Phi nonant.
//static const double inner_cabling_powerChannelsTeddStripStripSemiNonantBoundaryShift = 5. * M_PI / 180.;
//static const double inner_cabling_powerChannelsTeddPixelStripSemiNonantBoundaryShift = -5. * M_PI / 180.;

// Number of outer tracker services channels (per inner_cabling side)
//static const int inner_cabling_numServicesChannels = 12;
// Maximum number of power cables per channel (used for cross-checking)
//static const int inner_cabling_maxNumPowerCablesPerChannel = 48;
// Maximum number of optical fiber bundles per channel (used for cross-checking)
//static const int inner_cabling_maxNumOpticalBundlesPerChannel = 72;


#endif  // INNER_CABLING_CONSTANTS_HH
