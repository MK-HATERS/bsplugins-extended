#ifndef BSPLUGINLIST_UPDATECHECKER_H
#define BSPLUGINLIST_UPDATECHECKER_H

// UpdateChecker has been replaced with a simple browser-open approach.
// Qt6::Network is NOT linked — creating a QNetworkAccessManager inside a
// MO2 plugin DLL initialises WinHTTP which disrupts MO2's NXM download
// reception.  Users can check for updates manually via the Nexus link in
// the Settings tab.

#endif  // BSPLUGINLIST_UPDATECHECKER_H
