#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    cv::VideoCapture cap(0); // 0 is usually /dev/video0

    if (!cap.isOpened()) {
        std::cerr << "ERROR: Could not open camera." << std::endl;
        return 1;
    }

    cv::Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        cv::imshow("USB Camera", frame);

        if (cv::waitKey(10) == 27) break; // exit on 'ESC'
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
