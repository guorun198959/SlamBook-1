#include<iostream>
#include <fstream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <Eigen/Geometry>
#include <boost/format.hpp>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/filters/statistical_outlier_removal.h>

using namespace std;

/**
 * 本程序演示了RGBD点云地图稠密重建
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char **argv) {
    // 彩色图和深度图
    vector<cv::Mat> colorImgs, depthImgs;
    // 相机位姿
    vector<Eigen::Isometry3d> poses;

    ifstream fin("../../ch13/data/pose.txt");
    if (!fin) {
        cerr << "can't find pose file" << endl;
        return 1;
    }

    for (int i = 0; i < 5; i++) {
        //图像文件格式
        boost::format fmt("../../ch13/data/%s/%d.%s");
        colorImgs.push_back(cv::imread((fmt % "color" % (i + 1) % "png").str()));
        // 使用-1读取原始图像
        depthImgs.push_back(cv::imread((fmt % "depth" % (i + 1) % "pgm").str(), -1));

        double data[7] = {0};
        for (int j = 0; j < 7; j++) {
            fin >> data[j];
        }
        Eigen::Quaterniond q(data[6], data[3], data[4], data[5]);
        Eigen::Isometry3d T(q);
        T.pretranslate(Eigen::Vector3d(data[0], data[1], data[2]));
        poses.push_back(T);
    }

    // 计算点云并拼接
    // 相机内参
    double cx = 325.5;
    double cy = 253.5;
    double fx = 518.0;
    double fy = 519.0;
    double depthScale = 1000.0;

    cout << "正在将图像转换为点云..." << endl;
    // 定义点云使用的格式：这里用的是XYZRGB
    typedef pcl::PointXYZRGB PointT;
    typedef pcl::PointCloud<PointT> PointCloud;

    // 新建一个点云
    PointCloud::Ptr pointCloud(new PointCloud);
    for (int i = 0; i < 5; i++) {
        PointCloud::Ptr current(new PointCloud);
        cout << "转换图像中: " << i + 1 << endl;
        cv::Mat color = colorImgs[i];
        cv::Mat depth = depthImgs[i];
        Eigen::Isometry3d T = poses[i];
        for (int v = 0; v < color.rows; v++)
            for (int u = 0; u < color.cols; u++) {
                // 深度值
                unsigned int d = depth.ptr<unsigned short>(v)[u];
                // 为0表示没有测量到
                if (d == 0) continue;
                // 深度太大时不稳定，去掉
                if (d >= 7000) continue;
                Eigen::Vector3d point;
                point[2] = double(d) / depthScale;
                point[0] = (u - cx) * point[2] / fx;
                point[1] = (v - cy) * point[2] / fy;
                Eigen::Vector3d pointWorld = T * point;

                PointT p;
                p.x = pointWorld[0];
                p.y = pointWorld[1];
                p.z = pointWorld[2];
                p.b = color.data[v * color.step + u * color.channels()];
                p.g = color.data[v * color.step + u * color.channels() + 1];
                p.r = color.data[v * color.step + u * color.channels() + 2];
                current->points.push_back(p);
            }
        // depth filter and statistical removal
        PointCloud::Ptr tmp(new PointCloud);
        pcl::StatisticalOutlierRemoval<PointT> statistical_filter;
        statistical_filter.setMeanK(50);
        statistical_filter.setStddevMulThresh(1.0);
        statistical_filter.setInputCloud(current);
        statistical_filter.filter(*tmp);
        (*pointCloud) += *tmp;
    }

    pointCloud->is_dense = false;
    cout << "点云共有" << pointCloud->size() << "个点." << endl;

    // voxel filter
    pcl::VoxelGrid<PointT> voxel_filter;
    // resolution
    voxel_filter.setLeafSize(0.01, 0.01, 0.01);
    PointCloud::Ptr tmp(new PointCloud);
    voxel_filter.setInputCloud(pointCloud);
    voxel_filter.filter(*tmp);
    tmp->swap(*pointCloud);

    cout << "滤波之后，点云共有" << pointCloud->size() << "个点." << endl;

    pcl::io::savePCDFileBinary("map.pcd", *pointCloud);
    return 0;
}
