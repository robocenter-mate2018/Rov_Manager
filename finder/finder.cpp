#include "finder.hpp"
#include <QRgb>
#include <algorithm>
#include <iostream>

#define sqr(x) ((x)*(x))

Finder::Finder(Type *type, QRect *rect, QPixmap pixmap, QObject *parent) : QObject(parent)
{
    _type = type;
    _rect = rect;
    _pixmap = pixmap;
}

void Finder::process()
{
    _detectFigure(_pixmap);
    if (*_type == NONE) _detectText(_pixmap);
    emit(finished());
}

void Finder::_detectFigure(QPixmap pixmap)
{
    cv::Mat image = _getGrauScaleMat(pixmap.toImage());
    std::vector<std::vector<cv::Point> > contours;

    double cannyParams = cv::threshold(image, image, 0, 255, CV_THRESH_BINARY_INV + CV_THRESH_OTSU);
    cv::Canny(image, image, cannyParams, cannyParams / 2.0);
    cv::findContours(image, contours, CV_RETR_TREE, CV_CHAIN_APPROX_NONE);
    cv::drawContours(image, contours, -1, cv::Scalar(100,200,255));

    for (std::size_t i = 0; i < contours.size(); i++)
    {
        if (contours.at(i).size() < 5) continue;
        if (std::fabs(cv::contourArea(contours.at(i))) < 300.0) continue;

        static std::vector<cv::Point> hull;
        cv::convexHull(contours.at(i), hull, true);
        cv::approxPolyDP(hull, hull, 15, true);
        if (!cv::isContourConvex(hull)) continue;

        cv::RotatedRect bEllipse = cv::fitEllipse(contours.at(i));
        FigureColor color = _getFigureColor(pixmap.toImage().pixel(static_cast<int>(bEllipse.center.x), static_cast<int>(bEllipse.center.y)));
        if (color == FigureColor::OTHER || hull.size() > 4) continue;

        QPoint min = {INT_MAX, INT_MAX};
        QPoint max = {0, 0};
        for (auto i : hull){
            min.setX(qMin(min.x(), i.x));
            min.setY(qMin(min.y(), i.y));
            max.setX(qMax(max.x(), i.x));
            max.setY(qMax(max.y(), i.y));
        }

        *_rect = QRect(min, max);
        *_type = static_cast<Type>((hull.size() - 3) * 3 + color);
    }

}

void Finder::_detectText(QPixmap pixmap)
{
//    QImage image = pixmap.toImage();
//    for (int i = 0; i < image.width(); i++)
//        for (int j = 0; j < image.height(); j++){
//            QColor color = image.pixel(i, j);
//            double distance = std::sqrt(sqr(color.green()) +
//                                        sqr(color.red()) +
//                                        sqr(color.blue()));
//            if (distance < 200)
//                image.setPixel(i, j, qRgb(0, 0, 0));
//            else
//                image.setPixel(i, j, qRgb(255, 255, 255));
//        }
//    cv::Mat im = _getGrauScaleMat(image);
//    cv::imshow("test", im);
//    cv::waitKey(2);

    try {
        cv::Mat img_scene = _getGrauScaleMat(pixmap.toImage());
        std::vector<cv::KeyPoint> keypoints_scene;

        int minHessian = 400;
        cv::SurfFeatureDetector detector(minHessian);
        detector.detect(img_scene, keypoints_scene);

        cv::SurfDescriptorExtractor extractor;
        cv::Mat descriptors_scene;
        extractor.compute(img_scene, keypoints_scene, descriptors_scene);

        for (int i = 0; i < 6; i++){
            cv::Mat img_object = cv::imread("C://MATE//Rov_front//" + TEMPLATESPATH[i].toStdString(), CV_LOAD_IMAGE_GRAYSCALE);
//            cv::Mat img_object = cv::imread(TEMPLATESPATH[i].toStdString(), CV_LOAD_IMAGE_GRAYSCALE);



            std::vector<cv::KeyPoint> keypoints_object;
            detector.detect(img_object, keypoints_object);

            cv::Mat descriptors_object;
            extractor.compute(img_object, keypoints_object, descriptors_object);

            cv::FlannBasedMatcher matcher;
            cv::vector< cv::DMatch > matches;
            matcher.match( descriptors_object, descriptors_scene, matches );

            float max_dist = 0; float min_dist = 100;

            for(size_t i = 0; i < static_cast<size_t>(descriptors_object.rows); i++ )
            {
                float dist = matches[i].distance;
                if( dist < min_dist ) min_dist = dist;
                if( dist > max_dist ) max_dist = dist;
            }

            std::vector< cv::DMatch > good_matches;

            for(size_t i = 0; i < static_cast<size_t>(descriptors_object.rows); i++ )
            {
                if( matches[i].distance < 3 * min_dist )
                {
                    good_matches.push_back( matches[i]);
                }
            }

            std::vector<cv::Point2f> obj;
            std::vector<cv::Point2f> scene;

            for(size_t i = 0; i < good_matches.size(); i++ )
            {
                obj.push_back( keypoints_object[static_cast<size_t>(good_matches[i].queryIdx)].pt );
                scene.push_back( keypoints_scene[static_cast<size_t>(good_matches[i].trainIdx)].pt );
            }

            if (obj.size() * scene.size() <= 16) return;
            cv::Mat H = cv::findHomography(obj, scene, CV_RANSAC);
            std::vector<cv::Point2f> obj_corners(4);
            obj_corners[0] = cvPoint(0,0);
            obj_corners[1] = cvPoint( img_object.cols, 0 );
            obj_corners[2] = cvPoint( img_object.cols, img_object.rows );
            obj_corners[3] = cvPoint( 0, img_object.rows );

            std::vector<cv::Point2f> scene_corners(4);
            cv::perspectiveTransform(obj_corners, scene_corners, H);

            float square = _getSquare(scene_corners);
            if (cv::isContourConvex(scene_corners) && square > MINSQUARE && square < MAXSQUARE){
                *_rect = _getRect(scene_corners);
                *_type = static_cast<Type>(i + 1);
                return;
            }
        }
    } catch (...) {
    }
}

FigureColor Finder::_getFigureColor(QColor color)
{
    QColor colors[3] = { QColor(Qt::red), QColor(Qt::yellow), QColor(Qt::blue)};

    for (int i = 0; i < 3; i++){
        double distance = std::sqrt(sqr(colors[i].green() - color.green()) +
                                    sqr(colors[i].red() - color.red()) +
                                    sqr(colors[i].blue() - color.blue()));
        if (distance < COLORTOLERANCE)
            return static_cast<FigureColor>(i + 1);
    }
    return FigureColor::OTHER;
}

float Finder::_getSquare(std::vector<cv::Point2f> poitns)
{
    float result = 0;
    for (size_t i = 0; i < poitns.size(); i++){
        result += poitns[i].x * poitns[(i + 1) % poitns.size()].y;
        result -= poitns[i].y * poitns[(i + 1) % poitns.size()].x;
    }
    return qAbs(result);
}

QRect Finder::_getRect(std::vector<cv::Point2f> poitns)
{
    QPoint min = {INT_MAX, INT_MAX};
    QPoint max = {0, 0};
    for (auto i : poitns){
        min.setX(qMin(min.x(), static_cast<int>(i.x)));
        min.setY(qMin(min.y(), static_cast<int>(i.y)));
        max.setX(qMax(max.x(), static_cast<int>(i.x)));
        max.setY(qMax(max.y(), static_cast<int>(i.y)));
    }
    return QRect(min, max);
}

cv::Mat Finder::_getGrauScaleMat(QImage image)
{
    cv::Mat mat(image.height(),image.width(),CV_8UC4,
                static_cast<void*>(const_cast<uchar *>(image.constBits())),
                static_cast<size_t>(image.bytesPerLine()));
    cv::cvtColor(mat, mat, CV_BGR2GRAY);
    cv::GaussianBlur(mat, mat, cv::Size(5, 5), cv::BORDER_REFLECT);
    return mat;
}
