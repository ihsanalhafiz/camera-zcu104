#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

// Global variables to hold the latest frame and control synchronization.
cv::Mat latestFrame;
std::mutex frameMutex;
std::atomic<bool> keepRunning(true);

// Capture thread function: continuously reads frames from the camera.
void captureThread(cv::VideoCapture& cap) {
    cv::Mat frame;
    while (keepRunning) {
        cap >> frame;
        if (frame.empty())
            continue;
        // Lock and update the shared latest frame.
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            latestFrame = frame.clone();
        }
    }
}

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
    cap.set(cv::CAP_PROP_FPS, 60);  // requested FPS

    // Start the capture thread.
    std::thread capThread(captureThread, std::ref(cap));

    int counter = 0;
    auto last_tick = std::chrono::steady_clock::now();

    while (true) {
        cv::Mat frame;
        // Retrieve the most recent frame safely.
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            if (!latestFrame.empty())
                frame = latestFrame.clone();
        }
        if (frame.empty()) {
            // If no frame is available yet, wait a bit and continue.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(600));

        // Convert captured frame to grayscale for processing and display.
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        // --- Detect square on white paper ---
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);
        cv::Mat edged;
        cv::Canny(blurred, edged, 50, 150);

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edged, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        std::vector<cv::Point> square;
        for (const auto &cnt : contours) {
            double area = cv::contourArea(cnt);
            if (area < 1000) continue; // filter out small contours

            std::vector<cv::Point> approx;
            cv::approxPolyDP(cnt, approx, 0.02 * cv::arcLength(cnt, true), true);
            // Check for quadrilateral and convexity.
            if (approx.size() == 4 && cv::isContourConvex(approx)) {
                cv::Rect r = cv::boundingRect(approx);
                float aspectRatio = static_cast<float>(r.width) / r.height;
                if (aspectRatio >= 0.8 && aspectRatio <= 1.2) {
                    square = approx;
                    break; // take the first acceptable square found.
                }
            }
        }

        cv::Mat warp; // 28x28 warped grayscale image.
        // Vectors to hold the flattened warp image and its binarized version.
        std::vector<float> warpVec;
        std::vector<float> wrap_binarize;

        if (!square.empty()) {
            // Draw the detected square on the grayscale image (using a dark line).
            for (int i = 0; i < 4; i++) {
                cv::line(gray, square[i], square[(i + 1) % 4], cv::Scalar(0), 2);
            }

            // Order the square's corners: top-left, top-right, bottom-right, bottom-left.
            std::vector<cv::Point2f> pts;
            for (auto pt : square) {
                pts.push_back(cv::Point2f(pt.x, pt.y));
            }
            cv::Point2f tl, tr, br, bl;
            float sumMin = 1e9, sumMax = -1e9;
            float diffMin = 1e9, diffMax = -1e9;
            for (auto p : pts) {
                float sum = p.x + p.y;
                float diff = p.y - p.x;
                if (sum < sumMin) { sumMin = sum; tl = p; }
                if (sum > sumMax) { sumMax = sum; br = p; }
                if (diff < diffMin) { diffMin = diff; tr = p; }
                if (diff > diffMax) { diffMax = diff; bl = p; }
            }
            std::vector<cv::Point2f> ordered = {tl, tr, br, bl};

            // Destination points for a 28x28 image.
            std::vector<cv::Point2f> dst = {
                cv::Point2f(0, 0),
                cv::Point2f(27, 0),
                cv::Point2f(27, 27),
                cv::Point2f(0, 27)
            };

            // Compute the perspective transform and apply it.
            cv::Mat M = cv::getPerspectiveTransform(ordered, dst);
            cv::warpPerspective(gray, warp, M, cv::Size(28, 28));

            // Flatten the 28x28 warp image into a vector of floats normalized between 0 and 1.
            warpVec.resize(28 * 28);
            for (int i = 0; i < warp.rows; i++) {
                for (int j = 0; j < warp.cols; j++) {
                    warpVec[i * warp.cols + j] = warp.at<uchar>(i, j) / 255.0f;
                }
            }

            // Create wrap_binarize with size 2x28x28 using a simple binarization process.
            wrap_binarize.resize(2 * warpVec.size());
            for (size_t i = 0; i < warpVec.size(); i++) {
                wrap_binarize[2 * i] = 1 - warpVec[i];
                wrap_binarize[2 * i + 1] = warpVec[i];
            }
        }

        // --- Update counter every second ---
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick).count() >= 1) {
            counter++;
            last_tick = now;
        }

        // Overlay counter text on the grayscale image.
        std::string text = "Counter: " + std::to_string(counter);
        int fontFace = cv::FONT_HERSHEY_SIMPLEX;
        double fontScale = 1.0;
        int thickness = 2;
        cv::Point textOrg(20, 50);
        cv::putText(gray, text, textOrg, fontFace, fontScale, cv::Scalar(255), thickness);

        // Display the grayscale camera feed.
        cv::imshow("Grayscale Camera", gray);
        // Display the 28x28 warped grayscale image if available.
        if (!warp.empty()) {
            cv::imshow("Warped (28x28 Grayscale)", warp);
        }

        // Exit if ESC is pressed.
        if (cv::waitKey(1) == 27) break;
    }

    // Clean up: stop the capture thread and release resources.
    keepRunning = false;
    capThread.join();
    cap.release();
    cv::destroyAllWindows();
    return 0;
}
