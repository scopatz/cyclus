#ifndef OsiCbcSolverInterface_HPP
#define OsiCbcSolverInterface_HPP

#include "CbcConfig.h"

// headers in this file below this pragma have all warnings shushed
#pragma GCC system_header

// CBC_VERSION_MAJOR defined for Cbc > 2.5
#ifndef CBC_VERSION_MAJOR
#include "OsiCbcSolverInterface_2_5.hpp"
#elif CBC_VERSION_MAJOR == 2 && CBC_VERSION_MINOR <= 9
#include "OsiCbcSolverInterface_2_9.hpp"
#else
#pragma message("WARNING: Cbc version " + CBC_VERSION_MAJOR + "." +     \
                CBC_VERSION_MINOR + \
                " detected. The interface may be unstable and could " + \
                "be a possible source of segfaults.")
#include "OsiCbcSolverInterface_2_9.hpp"
#endif

#endif
