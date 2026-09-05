#pragma once

#include <atomic>

//-----------------------------------------------------------------------------

// Joins the scanner's SID_*.txt databases with the HVSC documents and packs
// them into ultraSID's Data/ultraSID.db. Returns an exit code: 0 = success.
// progress, when given, is updated
// with the build's completed fraction, reaching 1 once the file is on disk
int buildDatabase ( std::atomic<float>* progress = nullptr );
//-----------------------------------------------------------------------------
