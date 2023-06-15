#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

#include <iostream>
#include <thread>
#include <fstream>
#include <cstring>
#include <ctime>
#include <unistd.h>

#include "Object.h"

using namespace cv;
using namespace std;

// Test variables for counting bees
int in = 0;
int out = 0;
int imageCount = 0;
int objscale = 0;

// Variable for defining hive entrance and area of focus
Rect entrance;
Rect focus;

time_t currentTime;
tm current;

std::ofstream myfile;

double distanceBetweenPoints(cv::Point point1, cv::Point point2) { // Calculates distance between 2 points

	int intX = abs(point1.x - point2.x);
	int intY = abs(point1.y - point2.y);

	return(sqrt(pow(intX, 2) + pow(intY, 2)));
}

void addToExisting(Object& currentFrameObject, deque<Object>& existingObjects, int& intIndex) { // Updates existing object in deque

	existingObjects[intIndex].currentContour = currentFrameObject.currentContour;
	existingObjects[intIndex].currentBoundingRect = currentFrameObject.currentBoundingRect;
	existingObjects[intIndex].diagonalSize = currentFrameObject.diagonalSize;

	existingObjects[intIndex].centerPositions.push_back(currentFrameObject.centerPositions.back()); // Append new center position

	existingObjects[intIndex].tracking = true;
	existingObjects[intIndex].matchNew = true;
}

void addNewObject(Object& currentFrameObject, deque<Object>& existingObjects) { // Adds new object to deque

	currentFrameObject.matchNew = true;
	existingObjects.push_back(currentFrameObject);
}

double calculateDist(Point& currentPoint) { // Calculates distance of object from entrance

	double distX = max(entrance.x - currentPoint.x, currentPoint.x - (entrance.x + entrance.width));
	distX = max(distX, 0.0);

	double distY = max(entrance.y - currentPoint.y, currentPoint.y - (entrance.y + entrance.height));
	distY = max(distY, 0.0);

	return sqrt((distX * distX) + (distY * distY));
}

bool insideFocus(Point& currentPoint) { // Check if object is inside focus rectangle when called

	Point tl = focus.tl();
	Point br = focus.br();

	if (currentPoint.x > tl.x && currentPoint.x < br.x && currentPoint.y > tl.y && currentPoint.y < br.y) return true;

	return false;
}

bool outsideFocus(Point& currentPoint) { // Check if object is outside focus rectangle when called

	Point tl = focus.tl();
	Point br = focus.br();

	if (currentPoint.x < tl.x || currentPoint.x > br.x || currentPoint.y < tl.y || currentPoint.y > br.y) return true;

	return false;
}

void calculatePositionDiff(Object& currentObject) {  // Calculate difference in distance for objects no longer being tracked

	if (currentObject.direction == 0 && currentObject.lifetime > 1) {
		Point front = currentObject.centerPositions.front();
		Point back = currentObject.centerPositions.back();

		double firstDist = calculateDist(front);
		double lastDist = calculateDist(back);
		double dist = lastDist - firstDist;

		if (abs(dist) > objscale) { // if change in distance from entrance > objscale
			if (dist < 0 && (insideFocus(back) && outsideFocus(front))) { // Object is further from entrance than it was when first tracked
				in++;
				currentObject.direction = 1;
			}
			else if (dist > 0 && (insideFocus(front) && outsideFocus(back))) { // Object is closer to entrance than it was when first tracked
				out++;
				currentObject.direction = 2;
			}
			//else if (insideFocus(front) && lastDist > 100) { // Cases when bee flies out too quickly to track, better for low fps but somewhat inconsistent
			//	out++;
			//	currentObject.direction = 2;
			//}

		}
	}
}

void matchCurrentFrameToExisting(deque<Object>& existingObjects, deque<Object>& currentFrameObjects, Mat& frame) {

	for (int i = 0; i < existingObjects.size(); i++) {

		existingObjects[i].matchNew = false;
		existingObjects[i].lifetime++;

		existingObjects[i].predictNextPosition();
		
		// If an object is tracked for more than 10 frames, save jpg of tracking rectangle
		/* if (existingObjects[i].lifetime == 10) {
			Mat section = frame(existingObjects[i].currentBoundingRect);
			imwrite(("C:\\Users\\Name\\" + to_string(imageCount) + ".jpg"), section);
			imageCount++;
		} */
	}

	for (auto& currentFrameObject : currentFrameObjects) {

		int intIndexOfLeastDistance = 0;
		double dblLeastDistance = 100000.0;

		currentFrameObject.sizeVector.push_back(currentFrameObject.diagonalSize);

		for (unsigned int i = 0; i < existingObjects.size(); i++) {
			if (existingObjects[i].tracking == true) {
				double dblDistance = distanceBetweenPoints(currentFrameObject.centerPositions.back(), existingObjects[i].predictedNextPosition);

				if (dblDistance < dblLeastDistance) {
					dblLeastDistance = dblDistance;
					intIndexOfLeastDistance = i;
				}
			}
		}

		if (dblLeastDistance < currentFrameObject.diagonalSize * 1.15) {
			addToExisting(currentFrameObject, existingObjects, intIndexOfLeastDistance);
		}
		else {
			addNewObject(currentFrameObject, existingObjects);
		}

	}

	// For objects no longer being tracked
	for (auto& existingObject : existingObjects) {

		if (existingObject.matchNew == false) {
			existingObject.framesWithoutMatch++;
		}

		if (existingObject.framesWithoutMatch >= 3) { // If object is not tracked for x frames, call calculatePositionDiff()
			calculatePositionDiff(existingObject);
			existingObject.tracking = false;
		}

	}
}

void drawInfo(deque<Object>& objects, cv::Mat& framecopy) {

	int intFontFace = 0;
	double dblFontScale = 1;
	int intFontThickness = 2;

	string in_string = "in: " + to_string(in);
	string out_string = "out: " + to_string(out);

	cv::putText(framecopy, in_string, Point(5, 30), 2, dblFontScale, Scalar(0, 200, 0), intFontThickness);
	cv::putText(framecopy, out_string, Point(5, 70), 2, dblFontScale, Scalar(0, 200, 0), intFontThickness);

	for (unsigned int i = 0; i < objects.size(); i++) {

		if (objects[i].tracking == true) {
			cv::rectangle(framecopy, objects[i].currentBoundingRect, Scalar(0, 0, 200), 2);

			cv::putText(framecopy, std::to_string(i), objects[i].centerPositions.back(), intFontFace, dblFontScale, Scalar(0, 200.0, 0), intFontThickness);
		}
	}
}

void cleanDeque(deque<Object>& objects) {
	if (!objects.empty()) {
		while (objects.front().tracking == false) { // Clears top of deque

			// If object existed for more than 5 frames, note direction/size in myfile
			// Requires specifying myfile beforehand
			/* if (objects.front().lifetime > 5 && objects.front().direction != 0) {
				Object temp = objects.front();
				double average = 0;
				int size = temp.sizeVector.size();

				while (!temp.sizeVector.empty()) {
					average += temp.sizeVector.back();
					temp.sizeVector.pop_back();
				}

				average = average / size;

				myfile << to_string(temp.direction) << ", " << to_string(average) << endl;

			} */

			objects.pop_front();
			if (objects.empty()) break;
		}
	}
}

void callPython(int x, int y) {
	// sleep(5);
	string cmd = "python3 process_data.py " + to_string(x) + " " + to_string(y); // # of bees entering as argv[1], exiting as argv[2]
	system(cmd.c_str()); // call python script
	cout << "Python script completed" << endl;
}

int video()
{

	// Open video feed
	cout << "calling video" << endl;
	VideoCapture cap(0);
	if (!cap.isOpened())
	{
		myfile.close();
		cout << "not opened" << endl;
		return -1; // Return -1 if unable to open webcam
	}

	int downscale = 2;

	double videoHeight = cap.get(CAP_PROP_FRAME_HEIGHT) / downscale;
	double videoWidth = cap.get(CAP_PROP_FRAME_WIDTH) / downscale;

	objscale = min(videoHeight, videoWidth) / 16;

	Mat frame, region, fgMask, thresh, boxes;

	// Define variables for contours
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	// Initialize background subtractor (history, threshold, shadows)
	Ptr<BackgroundSubtractorMOG2> bg = createBackgroundSubtractorMOG2(50, 15, true);

	// Create deque of objects to match
	deque<Object> objects;

	bool firstFrame = true;

	int hour = 0;

	while (cap.isOpened())
	{
		// Read frame from the video, break if end of video
		cap.grab();
		cap.retrieve(frame);
		if (frame.empty()) break;

		cv::resize(frame, frame, cv::Size(videoWidth, videoHeight));

		entrance = Rect(170/downscale, 350/downscale, 280/downscale, 60/downscale); // Rect(x, y, width, length) creates rectangle, x and y specify top left corner coordinates
		focus = Rect(60/downscale, 190/downscale, 530/downscale, 230/downscale); // Image scaled down to reduce pixels scanned

		// Code for manually selecting entrance and focus areas
		/* if (firstFrame && (entrance.height == 0)) {
			entrance = selectROI("Frame", frame);
			destroyWindow("Frame");
			focus = selectROI("Frame2", frame);
			destroyWindow("Frame2");
			cout << "entrance  x: " << entrance.tl().x << ", y: " << entrance.tl().y << ", width: " << entrance.br().x-entrance.tl().x << ", height: " << entrance.br().y-entrance,tl().y << endl;
			cout << "focus  x: " << focus.tl().x << ", y: " << focus.tl().y <<  ", width: " << focus.br().x-focus.tl().x << ", height: " << focus.br().y-focus.tl().y << endl;
		} */

		// Check time
		currentTime = time(0);
		localtime_r(&currentTime, &current);

		// Select interval, can change tm_min to tm_hour or add other logic for different intervals
		if (current.tm_min != hour) {

			// Call python script w/ parameters in a thread
			if (in != 0 && out != 0) {
				cout << "Calling python script with in: " << in << ", out: " << out << endl;
				std::thread (callPython,in,out).detach();
			}

			cout << "Camera resumed"<< endl;

			// Reset variables
			hour = current.tm_min;
			in = 0;
			out = 0;
		}

		deque<Object> currentObjects;

		//Update the tracking result with new frame
		region = frame.clone();

		// Process video for better detection
		bg->apply(region, fgMask);

		// Set background threshold
		threshold(fgMask, thresh, 200, 255, THRESH_BINARY);
		boxes = frame.clone();

		// Draw objects
		cv::findContours(thresh, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE);
		//cv::drawContours(boxes, contours, -1, Scalar(255, 100, 100), 2);


		for (int i = 0; i < contours.size(); i++) { // for objects on screen, add to currentObjects if above size threshold
			if (((boundingRect(contours[i]).height) < objscale || (boundingRect(contours[i]).width) < objscale)) continue;
			Object newObject(contours[i]);
			currentObjects.push_back(newObject);
		}

		if (firstFrame == true) {
			for (auto& currentObject : currentObjects) {
				objects.push_back(currentObject);
			}
		}
		else {
			matchCurrentFrameToExisting(objects, currentObjects, frame);
		}

		drawInfo(currentObjects, boxes);

		// Opens two windows for camera preview and moving objects
		/* cv::rectangle(boxes, entrance, Scalar(0, 200, 0), 2);
		cv::rectangle(boxes, focus, Scalar(0, 200, 0), 2);
		cv::imshow("Mask", fgMask);
		cv::imshow("Boxes", boxes); */

		firstFrame = false;
		currentObjects.clear();
		//std::cout << objects.size() << endl;
		cleanDeque(objects);

		// Quit on esc button
		if (cv::waitKey(1) == 27) break;

	}

	myfile.close();
	frame.release();

	return 0;

}

int main() {

	video();

	return 0;
}
