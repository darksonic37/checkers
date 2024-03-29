#include <opencv2/opencv.hpp>
#include <iostream>
#include "game.hpp"

void sharpen(cv::Mat src, cv::Mat dst)
{
    cv::GaussianBlur(src, dst, cv::Size(0, 0), 3);
    cv::addWeighted(src, 1.5, dst, -0.5, 0, dst);
}

void tile(const std::vector<cv::Mat> &src, cv::Mat &dst, int grid_x, int grid_y)
{
    // patch size
    int width  = dst.cols/grid_x;
    int height = dst.rows/grid_y;

    // iterate through grid
    int k = 0;
    for(int i = 0; i < grid_y; i++) {
        for(int j = 0; j < grid_x; j++) {
            cv::Mat s = src[k++];
            resize(s, s, cv::Size(width, height));
            s.copyTo(dst(cv::Rect(j*width, i*height, width, height)));
        }
    }
}

std::vector<cv::Point2f> completeBoardCorners(std::vector<cv::Point2f> insideCorners)
{
    cv::Size patternsize(7, 7);
    int insideCornerDim = patternsize.width;

    cv::Point2f vertex1 = cv::Point2f(
        insideCorners[0].x + abs(insideCorners[0].x - insideCorners[insideCornerDim].x),
        insideCorners[0].y - abs(insideCorners[0].y - insideCorners[1].y)
    );

    cv::Point2f vertex2 = cv::Point2f(
        insideCorners[insideCornerDim-1].x + abs(insideCorners[insideCornerDim-1].x - insideCorners[insideCornerDim*2-1].x),
        insideCorners[insideCornerDim-1].y + abs(insideCorners[insideCornerDim-1].y - insideCorners[insideCornerDim-2].y)
    );

    cv::Point2f vertex3 = cv::Point2f(
        insideCorners[insideCorners.size()-insideCornerDim].x - abs(insideCorners[insideCorners.size()-insideCornerDim].x - insideCorners[insideCorners.size()-insideCornerDim*2].x),
        insideCorners[insideCorners.size()-insideCornerDim].y - abs(insideCorners[insideCorners.size()-insideCornerDim].y - insideCorners[insideCorners.size()-insideCornerDim+1].y)
    );

    cv::Point2f vertex4 = cv::Point2f(
        insideCorners[insideCorners.size()-1].x - abs(insideCorners[insideCorners.size()-1].x - insideCorners[insideCorners.size()-1-insideCornerDim].x),
        insideCorners[insideCorners.size()-1].y + abs(insideCorners[insideCorners.size()-1].y - insideCorners[insideCorners.size()-2].y)
    );


    std::vector<cv::Point2f> outterCorners= std::vector<cv::Point2f>();

    // first column
    outterCorners.push_back(vertex1);
    outterCorners.push_back(vertex2);
    outterCorners.push_back(vertex3);
    outterCorners.push_back(vertex4);


    for(int i = 0; i < insideCornerDim; ++i){
        outterCorners.push_back(cv::Point2f(
            insideCorners[i].x + abs(insideCorners[i].x - insideCorners[insideCornerDim+i].x),
            insideCorners[i].y
        ));
    }

    for(int i = 0; i < insideCornerDim; ++i){
        int idx = insideCornerDim*(i+1)-1;
        outterCorners.push_back(cv::Point2f(
            insideCorners[idx].x,
            insideCorners[idx].y + abs(insideCorners[idx].y - insideCorners[idx-1].y))
        );

    }

    for(int i = 0; i < insideCornerDim; ++i){
        int idx = insideCorners.size()-insideCornerDim + i;
        outterCorners.push_back(cv::Point2f(
            insideCorners[idx].x - abs(insideCorners[idx].x - insideCorners[idx-insideCornerDim].x),
            insideCorners[idx].y
        ));
    }


    for(int i = 0; i < insideCornerDim; ++i){
        int idx = insideCornerDim*i;
        outterCorners.push_back(cv::Point2f(
            insideCorners[idx].x,
            insideCorners[idx].y - abs(insideCorners[idx].y - insideCorners[idx+1].y)
        ));
    }

    std::vector<cv::Point2f> corners = insideCorners;

    corners.insert(corners.end(), outterCorners.begin(), outterCorners.end());

    std::sort(corners.begin(), corners.end(), [](const cv::Point2f &a, const cv::Point2f &b) { return a.x < b.x; });
    for (auto i = corners.begin(); i != corners.end(); i += 9)
        std::sort(i, i+9, [](const cv::Point2f &a, const cv::Point2f &b) { return a.y < b.y; });

    return std::vector<cv::Point2f>(corners);
}

std::vector<cv::Point2f> getBoardCorners(cv::Mat frame)
{
    static int count = 0;
    std::vector<cv::Point2f> result = std::vector<cv::Point2f>();

    if (++count == 1) {
        count = 0;
        cv::Size patternsize(7, 7); //interior number of corners
        std::vector<cv::Point2f> tmp = std::vector<cv::Point2f>();
        bool patternfound = cv::findChessboardCorners(frame, patternsize, tmp, cv::CALIB_CB_ADAPTIVE_THRESH + cv::CALIB_CB_NORMALIZE_IMAGE + cv::CALIB_CB_FAST_CHECK);
        if(patternfound) {
            // improve accuracy
            cv::cornerSubPix(frame, tmp, cv::Size(11, 11), cv::Size(-1, -1), cv::TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));
            result = std::vector<cv::Point2f>(tmp);
            std::sort(result.begin(), result.end(), [](const cv::Point2f &a, const cv::Point2f &b) { return a.x > b.x; });
            for (auto i = result.begin(); i != result.end(); i += 7)
                std::sort(i, i+7, [](const cv::Point2f &a, const cv::Point2f &b) { return a.y < b.y; });
            result = completeBoardCorners(result);
            cv::cornerSubPix(frame, result, cv::Size(11, 11), cv::Size(-1, -1), cv::TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));
        }
    }

    return result;
}

std::vector<cv::Point2f> getPositionCorners(std::vector<cv::Point2f> corners, int x, int y)
{
    int stride = BOARD_SIZE+1;
    std::vector<cv::Point2f> quad;
    quad.push_back(corners.at(stride*x + y));
    quad.push_back(corners.at(stride*x + (y + 1)));
    quad.push_back(corners.at(stride*(x + 1) + y));
    quad.push_back(corners.at(stride*(x + 1) + (y + 1)));
    return quad;
}

bool isPointInsideQuad(cv::Point2f c, std::vector<cv::Point2f> quad)
{
    cv::Point2f p0 = quad.at(0);
    cv::Point2f p3 = quad.at(3);

    float thresh_x = abs(p3.x - p0.x)/4;
    float thresh_y = abs(p3.y - p0.y)/4;

    return p0.x-thresh_x < c.x && c.x < p3.x+thresh_x &&
           p0.y-thresh_y < c.y && c.y < p3.y+thresh_y;
}

int main(int argc, char* argv[])
{
    cv::VideoCapture cap;
    if(argc == 2) {
        cap.open(argv[1]);
    } else {
        cap.open(0);
    }

    if(!cap.isOpened()) {
        return -1;
    }

    cv::Mat frame;
    while(cap.read(frame)) {
        // Normalize image for finding board positions
        cv::Mat gray;
        cv::cvtColor(frame, gray, CV_BGR2GRAY);
        cv::equalizeHist(gray, gray);
        cv::Mat grayClone = gray.clone();
        sharpen(grayClone, gray);

        // Find board corners
        cv::Mat boardCorners = gray.clone();
        std::vector<cv::Point2f> corners = getBoardCorners(gray);
        cv::drawChessboardCorners(boardCorners, cv::Size(9, 9), cv::Mat(corners), !corners.empty());

        // Threshold the HSV image, keep only the red pixels
        cv::Mat hsv;
        cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
        cv::medianBlur(hsv, hsv, 11);
        cv::Mat reds;
        cv::inRange(hsv, cv::Scalar(160, 100, 100), cv::Scalar(179, 255, 255), reds);
        cv::GaussianBlur(reds, reds, cv::Size(9, 9), 2, 2);

        // Threshold the HSV image, keep only the yellow pixels
        cv::Mat yellows;
        cv::inRange(hsv, cv::Scalar(25, 95, 95), cv::Scalar(32, 255, 255), yellows);
        cv::GaussianBlur(yellows, yellows, cv::Size(9, 9), 2, 2);

        // Find red and yellow circles
        std::vector<cv::Vec3f> redCircles;
        cv::HoughCircles(reds, redCircles, CV_HOUGH_GRADIENT, 1, reds.rows/8, 100, 20, 0, 0);
        std::vector<cv::Vec3f> yellowCircles;
        cv::HoughCircles(yellows, yellowCircles, CV_HOUGH_GRADIENT, 1, yellows.rows/8, 100, 20, 0, 0);

        cv::namedWindow("corners", cv::WINDOW_NORMAL);
        cv::imshow("corners", boardCorners);

        if (!corners.empty()) {
            Game game;
            for (int x = 0; x < BOARD_SIZE; x++) {
                for (int y = 0; y < BOARD_SIZE; y++) {
                    if ((x + y) % 2 == 0) {
                        for(size_t i = 0; i < redCircles.size(); i++) {
                            cv::Point2f center(redCircles[i][0], redCircles[i][1]);
                            std::vector<cv::Point2f> quad = getPositionCorners(corners, x, y);
                            if (isPointInsideQuad(center, quad)) {
                                game.set_red(x, y);
                                continue;
                            }
                        }

                        for(size_t j = 0; j < yellowCircles.size(); j++) {
                            cv::Point2f center(yellowCircles[j][0], yellowCircles[j][1]);
                            std::vector<cv::Point2f> quad = getPositionCorners(corners, x, y);
                            if (isPointInsideQuad(center, quad)) {
                                game.set_yellow(x, y);
                                continue;
                            }
                        }
                    }
                }
            }

            // Draw the game
            game.print();
            size_t boardx = 8;
            size_t boardy = 8;
            size_t dim = 400;
            cv::Mat white = cv::Mat(dim, dim, CV_8UC3, cv::Scalar(255,255,255));
            cv::Mat black = cv::Mat::ones(dim, dim, CV_8UC3);

            cv::Mat blackPiece = cv::Mat::ones(dim, dim, CV_8UC3);
            cv::circle(blackPiece, cv::Point(dim/2,dim/2),dim/4,cv::Scalar(0,0,255), -1, 8, 0);

            cv::Mat whitePiece = cv::Mat::ones(dim, dim, CV_8UC3);
            cv::circle(whitePiece, cv::Point(dim/2,dim/2),dim/4,cv::Scalar(0,255,255), -1, 8, 0);
            std::vector<cv::Mat> board;

            for(int i = 0, j = -1; board.size() != boardx*boardy; ++i) {
                if(i % 8 == 0) {
                    ++j;
                    i=0;
                }

                if((i+j) % 2 == 0){
                    int pieceType = game.get(i,j);
                    if(pieceType == 1) board.push_back(blackPiece);
                    else if(pieceType == 2) board.push_back(whitePiece);
                    else board.push_back(black);
                } else {
                    board.push_back(white);
                }
            }
            cv::Mat boardRes = cv::Mat(dim, dim, CV_8UC3);
            tile(board, boardRes, boardx, boardy);
            cv::imshow("board", boardRes);
        }

        if(cv::waitKey(1) == 'p') while(cv::waitKey(1) != 'p');
    }

    return 0;
}

