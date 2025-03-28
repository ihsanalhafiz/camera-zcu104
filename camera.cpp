#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) {
        std::cerr << "ERROR: Could not open camera." << std::endl;
        return 1;
    }

    // Request MJPG format for better performance
    cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M','J','P','G'));
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    cap.set(cv::CAP_PROP_FPS, 60);  // request 15 FPS

    cv::Mat frame;
    int counter = 0;

    // Time tracking
    auto last_tick = std::chrono::steady_clock::now();

    while (true) {
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "ERROR: Blank frame grabbed\n";
            break;
        }

        // Check if 1 second has passed
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick).count() >= 1) {
            counter++;
            last_tick = now;
        }

        // Overlay counter text onto the frame
        std::string text = "Counter: " + std::to_string(counter);
        int fontFace = cv::FONT_HERSHEY_SIMPLEX;
        double fontScale = 1.0;
        int thickness = 2;
        cv::Point textOrg(20, 50);  // x, y position of the text

        cv::putText(frame, text, textOrg, fontFace, fontScale,
                    cv::Scalar(0, 255, 0), thickness);  // Green text

        cv::imshow("USB Camera", frame);

        if (cv::waitKey(1) == 27) break; // ESC to exit
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
