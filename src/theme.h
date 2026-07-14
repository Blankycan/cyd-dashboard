#pragma once
// =============================================================================
// CYD Dashboard — color theme selector
//
// Pick the active theme via CYD_THEME in config.h. Each theme is a standalone
// palette file under src/themes/ — see src/themes/forest.h for the full set
// of PAL_*/COL_*/TFT_COL_* names a theme must define.
// =============================================================================

#include "config.h"

#if CYD_THEME == CYD_THEME_GRAPE_EMBER
    #include "themes/grape-ember.h"
#else
    #include "themes/forest.h"
#endif
