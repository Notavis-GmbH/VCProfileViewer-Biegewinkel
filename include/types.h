#pragma once

// Shared data types – no Windows headers included here
// so this file is safe to include from Qt widget headers

struct ProfilePoint {
    float x_mm;
    float z_mm;
};

// Region of interest (in sensor world coordinates, mm)
struct RoiRect {
    double xMin = 0.0;
    double xMax = 0.0;
    double zMin = -9999.0;  // unused for line-fit ROI
    double zMax =  9999.0;
    bool valid = false;
};
