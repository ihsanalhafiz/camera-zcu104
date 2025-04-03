#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <vector>

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

    cv::Mat frame;
    int counter = 0;
    auto last_tick = std::chrono::steady_clock::now();

    while (true) {
        cap >> frame;
        if (frame.empty()) {
            std::cerr << "ERROR: Blank frame grabbed\n";
            break;
        }

        // --- Detect square on white paper ---
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
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
            // Check for quadrilateral and convexity
            if (approx.size() == 4 && cv::isContourConvex(approx)) {
                cv::Rect r = cv::boundingRect(approx);
                float aspectRatio = (float)r.width / r.height;
                if (aspectRatio >= 0.8 && aspectRatio <= 1.2) {
                    square = approx;
                    break; // take the first acceptable square found
                }
            }
        }

        if (!square.empty()) {
            // Draw the detected square on the original frame
            for (int i = 0; i < 4; i++) {
                cv::line(frame, square[i], square[(i+1)%4], cv::Scalar(255, 0, 0), 2);
            }

            // Order the square's corners: top-left, top-right, bottom-right, bottom-left
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

            // Destination points for a 28x28 image
            std::vector<cv::Point2f> dst = {
                cv::Point2f(0, 0),
                cv::Point2f(27, 0),
                cv::Point2f(27, 27),
                cv::Point2f(0, 27)
            };

            // Compute the perspective transform and apply it
            cv::Mat M = cv::getPerspectiveTransform(ordered, dst);
            cv::Mat warp;
            cv::warpPerspective(gray, warp, M, cv::Size(28, 28));

            // Convert the warped (grayscale) image to BGR so it can be overlaid on the original frame
            cv::Mat warpColor;
            cv::cvtColor(warp, warpColor, cv::COLOR_GRAY2BGR);

            // Overlay the small image onto the main frame (top-right corner with a 10-pixel margin)
            int x_offset = frame.cols - warpColor.cols - 10;
            int y_offset = 10;
            warpColor.copyTo(frame(cv::Rect(x_offset, y_offset, warpColor.cols, warpColor.rows)));
        }

        // --- Update counter every second ---
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_tick).count() >= 1) {
            counter++;
            last_tick = now;
        }

        // Overlay counter text on the frame
        std::string text = "Counter: " + std::to_string(counter);
        int fontFace = cv::FONT_HERSHEY_SIMPLEX;
        double fontScale = 1.0;
        int thickness = 2;
        cv::Point textOrg(20, 50);
        cv::putText(frame, text, textOrg, fontFace, fontScale, cv::Scalar(0, 255, 0), thickness);

        // Display the final frame with overlayed processed image and counter
        cv::imshow("USB Camera", frame);

        // Exit if ESC is pressed
        if (cv::waitKey(1) == 27) break;
    }

    cap.release();
    cv::destroyAllWindows();
    return 0;
}
