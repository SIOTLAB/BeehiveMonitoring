#ifndef MY_OBJECT
#define MY_OBJECT

#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>


class Object {
public:
    // member variables
    std::vector<cv::Point> currentContour;
    std::vector<cv::Point> centerPositions;

    cv::Rect currentBoundingRect;

    std::vector<double> sizeVector;
    double diagonalSize;

    bool matchNew;
    bool tracking;

    int framesWithoutMatch;
    int lifetime;
    int direction; // 1 if exiting hive, 2 if entering

    cv::Point predictedNextPosition;

    // functions
    Object(std::vector<cv::Point> _contour);
    void predictNextPosition(void);

};

#endif
