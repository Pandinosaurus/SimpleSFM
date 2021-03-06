/*
 * Main file for SimpleSFM project. Run the entire SfM pipeline on a set of images and save the 
 * results to a *.ply file for viewing in MeshLab.
 */

#include <iostream>

#include "opencv2/core/core.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/calib3d/calib3d.hpp"
#include <opencv2/imgproc/imgproc.hpp>

#include "BasicFunctions.hpp"

using namespace std;
namespace bf = BasicFunctions;

#define NUM_FEATURES 10000
#define LOWE_RATIO 0.8
#define MIN_MATCHES 25
#define MIN_INLIERS 15
#define VISUALIZE true
#define RATIO 0.25
#define F_DIST 3.0
#define F_CONF 0.95

void usage(char *argv[]) {
  cerr << "Usage: " << argv[0] << " PATH_TO_IMAGES" << endl;
}

int main(int argc, char *argv[]) {

  // ensure proper utilization
  if (argc != 2) {
    usage(argv);
    return 1;
  }

  // get image file names
  vector<string> img_files = bf::getImgFiles(argv[1]);
  const int num_files = img_files.size();

  // iterate through each pair of consecutive images and find keypoints
  for (int i = 1; i < num_files; i++) {
    cout << "\nNow analyzing frames " << i-1 << " and " << i << " of " << num_files-1 << ".\n";
    
    cv::Mat img1_big = cv::imread(img_files[i-1], CV_LOAD_IMAGE_GRAYSCALE);
    cv::Mat img2_big = cv::imread(img_files[i], CV_LOAD_IMAGE_GRAYSCALE);

    // ensure non-zero size (no read errors)
    if (!img1_big.cols || !img2_big.cols) {
      cerr << "Error reading files... skipping this pair." << endl;
      continue;
    }

    cv::Mat img1; cv::Mat img2;
    cv::resize(img1_big, img1, cv::Size(), RATIO, RATIO);
    cv::resize(img2_big, img2, cv::Size(), RATIO, RATIO);

    if (!img1.data || !img2.data) {
      cerr << "Error reading images." << endl; 
      return -1; 
    }

    // detect and compute keypoints and descriptors
    cv::ORB orb(NUM_FEATURES);

    vector<cv::KeyPoint> kp1, kp2;
    cv::Mat des1, des2;

    orb.detect(img1, kp1); orb.compute(img1, kp1, des1);
    orb.detect(img2, kp2); orb.compute(img2, kp2, des2);

    // do keypoint matching
    cv::BFMatcher matcher;
    vector< vector<cv::DMatch> > matches;
    matcher.knnMatch(des1, des2, matches, 2);

    // apply Lowe's ratio test and filter out only good matches
    vector<cv::DMatch> good_matches;
    for (int i = 0; i < matches.size(); i++) {
      vector<cv::DMatch> matches_entry = matches[i];

      // ensure that there are two matches for this keypoint
      if (matches_entry.size() != 2)
	cerr << "Keypoint " << i << " does not have enough match candidates." << endl;

      // handle default case
      else {
	cv::DMatch m1 = matches_entry[0];
	cv::DMatch m2 = matches_entry[1];

	if (m1.distance < LOWE_RATIO * m2.distance)
	  good_matches.push_back(m1);
      }
    }
   
    // print out number of good matches
    cout << "Found " << good_matches.size() << " good matches." << endl;
    
    // skip if there are not enough good matches
    if (good_matches.size() < MIN_MATCHES) {
      cerr << "Not enough good matches." << endl;
      continue;
    }

    
    // estimate fundamental matrix F
    vector<cv::Point2f> pts1(good_matches.size());
    vector<cv::Point2f> pts2(good_matches.size());
    
    for (int i = 0; i < good_matches.size(); i++) {
      cv::DMatch m = good_matches[i];
      pts1[i] = kp1[m.queryIdx].pt;
      pts2[i] = kp2[m.trainIdx].pt;
    }
    
    cv::Mat mask;
    cv::Mat F = cv::findFundamentalMat(pts1, pts2, cv::FM_RANSAC, F_DIST, F_CONF, mask);

    vector<cv::DMatch> inliers;
    for (int i = 0; i < good_matches.size(); i++) {
      if (mask.at<bool>(i, 0)) inliers.push_back(good_matches[i]);
    }

    // visualize if desired
    if (VISUALIZE) {
      cv::Mat visual;
      cv::drawMatches(img1, kp1, img2, kp2, inliers, visual);

      cv::imshow("Inliers after estimating F", visual);
      cv::waitKey(0);
    }

    // skip these frames if insufficient inliers
    if (inliers.size() < MIN_INLIERS) {
      cerr << "Not enough inliers." << endl;
      continue;
    }

    cout << "Found " << inliers.size() << " inliers." << endl;

  }

  return 0;
}
