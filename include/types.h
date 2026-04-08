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

// Result of a linear regression fit within one ROI
// Represents the line  z = slope * x + intercept
struct FitLine {
    double slope       = 0.0;
    double intercept   = 0.0;
    double xMin        = 0.0;   // draw range (= ROI x-bounds)
    double xMax        = 0.0;
    double phi         = 0.0;   // angle in degrees (atan(slope))
    double rmsResidual = 0.0;   // root-mean-square residual [mm]
    double maxResidual = 0.0;   // maximum absolute residual [mm]
    bool   valid       = false;
};
