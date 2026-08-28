#pragma once

namespace planar_stitcher {

struct Point2f {
    double x = 0.0;
    double y = 0.0;
};

struct RectI {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct Mat3 {
    double v[9] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0};

    Point2f Map(Point2f point) const;
    bool Invert(Mat3* inverse) const;
};

}
