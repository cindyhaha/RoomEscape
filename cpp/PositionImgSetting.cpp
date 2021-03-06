#include "../h/PositionImgSetting.h"
#include "../h/PyramidBlending.h"
#include "../h/ChooseImageSSIM.h"
#include "../h/Define.h"
#include "../h/ssim.h"

#include <opencv2/contrib/contrib.hpp>	//要用timer就需要
#include "opencv2/highgui/highgui.hpp"
#include <iostream>
#include <windows.h>
#include <opencv2/legacy/legacy.hpp>
#include <tchar.h> 

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

int img_amount_top, img_amount_mid, img_amount_bot, img_amount;
int interval, central_img, left_img, right_img;
Mat img_mid, img_l, img_r, stitch_ref;
TickMeter  t;

PositionImgSetting::PositionImgSetting() {
	_room_name = "noname";
	_number = 1;
}

PositionImgSetting::PositionImgSetting(string& room_name, int number) {
	_room_name = room_name;
	_number = number;
}

void PositionImgSetting::set_room_name(int room_name) {
	_room_name = room_name;
}

string& PositionImgSetting::get_room_name() {
	return _room_name;
}

void PositionImgSetting::set_position_number(int number) {
	_number = number;
}

int PositionImgSetting::get_position_number() {
	return _number;
}

int PositionImgSetting::img_capture(Mat img_original, int x, int y, int width, int height, Image& img_result)
{
	if (!img_original.data)
	{
		cout << "ERROR: Input image is null. Cannot capture image!" << endl;
		return -1;
	}

	Rect rect;
	Mat img_temp;
	vector<KeyPoint> img_keypoints;

	rect = Rect(x, y, width, height);
	img_original(rect).copyTo(img_temp);
	img_result.set_image(img_temp);

	if (x > 0 || y > 0)
	{
		img_keypoints = img_result.keypoints();
		cout << img_keypoints.size() << endl;
		for (int i = 0; i < img_keypoints.size(); i++)
		{
			img_keypoints[i].pt.x += x;
			img_keypoints[i].pt.y += y;
		}
		img_result.set_keypoints(img_keypoints);
	}
	return 1;
}

void PositionImgSetting::symmetryTest(const vector<DMatch> matches1, const vector<DMatch> matches2, vector<DMatch> &symMatches)
{
	//matches1是img1->img2，matches2是img2->img1
	symMatches.clear();
	for (vector<DMatch>::const_iterator matchIterator1 = matches1.begin(); matchIterator1 != matches1.end(); ++matchIterator1)
	{
		for (vector<DMatch>::const_iterator matchIterator2 = matches2.begin(); matchIterator2 != matches2.end(); ++matchIterator2)
		{
			if ((*matchIterator1).queryIdx == (*matchIterator2).trainIdx && (*matchIterator2).queryIdx == (*matchIterator1).trainIdx)
			{
				symMatches.push_back(DMatch((*matchIterator1).queryIdx, (*matchIterator1).trainIdx, (*matchIterator1).distance));
				break;
			}
		}
	}
}

vector< DMatch > PositionImgSetting::get_good_dist_matches(vector< DMatch > matches, int min_scale){
	double max_dist = 0;
	double min_dist = 100;
	double distance;

	//-- Quick calculation of max and min distances between keypoints
	for (int i = 0; i < matches.size(); i++)
	{
		distance = matches[i].distance;
		if (distance < min_dist) min_dist = distance;
		if (distance > max_dist) max_dist = distance;
	}
	//printf("-- Max dist : %f \n", max_dist);
	//printf("-- Min dist : %f \n", min_dist);

	//-- Use only "good" matches (i.e. whose distance is less than min_scale*min_dist )
	vector< DMatch > good_matches;

	for (int i = 0; i < matches.size(); i++)
	{
		if (matches[i].distance < min_scale * min_dist)
			good_matches.push_back(matches[i]);
	}
	return good_matches;
}

void PositionImgSetting::draw_matches(Mat img1, Mat img2, vector< KeyPoint > keypoints1, vector< KeyPoint > keypoints2, vector< DMatch > matches, int number){
	Mat img_matches;
	drawMatches(img1, keypoints1, img2, keypoints2,
		matches, img_matches, Scalar::all(-1), Scalar::all(-1),
		vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);

	imwrite("D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\Good Matches\\good_match" + to_string(number) + ".jpg", img_matches);
}

void PositionImgSetting::draw_2img_matches(Mat img_1, Mat img_2, int number){
	Image img1(img_1), img2(img_2);
	
	FlannBasedMatcher matcher;
	vector<DMatch>matches1, matches1_1, matches2_1;
	matcher.match(img1.descriptors(), img2.descriptors(), matches1);
	matcher.match(img2.descriptors(), img1.descriptors(), matches1_1);
	draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), matches1, 11);

	//look for symmetry matches
	vector< DMatch > symm_matches1;
	symmetryTest(matches1, matches1_1, symm_matches1);
	draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), symm_matches1, 12);
	cout << "symmetryTest OK" << endl;

	//-- Use only "good" matches (i.e. whose distance is less than 6*min_dist )
	vector< DMatch > good_dist_matches1;
	good_dist_matches1 = get_good_dist_matches(symm_matches1, METHOD2);
	draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), good_dist_matches1, 13);
	cout << "GoodDistTest OK" << endl;

	//look if the match is inside a defined area of the image
	double tresholdDist_x = 0.55*img2.mat().cols;
	double tresholdDist_y = 0.55*img2.mat().rows;

	//vector< DMatch > good_matches;
	vector< DMatch > best_matches1;
	best_matches1.reserve(good_dist_matches1.size());

	for (int i = 0; i < good_dist_matches1.size(); i++)
	{
		//calculate local distance for each possible match
		vector < KeyPoint > keypoints1 = img1.keypoints();
		vector < KeyPoint > keypoints2 = img2.keypoints();

		Point2f from = keypoints2[good_dist_matches1[i].trainIdx].pt;
		Point2f to = keypoints1[good_dist_matches1[i].queryIdx].pt;

		if (from.x < tresholdDist_x && to.x > img2.mat().cols - tresholdDist_x)
		{
			best_matches1.push_back(good_dist_matches1[i]);
		}
	}
	cout << "GoodRangeTest OK" << endl;
	
	Mat img_matches;
	drawMatches(img1.mat(), img1.keypoints(), img2.mat(), img2.keypoints(),
		best_matches1, img_matches, Scalar::all(-1), Scalar::all(-1),
		vector<char>(), DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);

	imwrite("D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\Good Matches\\good_match" + to_string(number) + ".jpg", img_matches);
	cout<<"D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\Good Matches\\good_match" + to_string(number) + ".jpg"<<endl;
}

void PositionImgSetting::get_homography_matrix_by_matches(Image img1, Image img2, vector< DMatch > matches1, Mat& H1)
{
	vector< Point2f > img_good1, img_good2;		//img1是不變的(central)，img2要貼到img1上

	for (size_t i = 0; i < matches1.size(); i++)
	{
		//-- Get the keypoints from the good matches
		vector < KeyPoint > keypoints1 = img1.keypoints();
		vector < KeyPoint > keypoints2 = img2.keypoints();
		img_good1.push_back(keypoints1[matches1[i].queryIdx].pt);
		img_good2.push_back(keypoints2[matches1[i].trainIdx].pt);
	}

	// Find the Homography Matrix
	H1 = findHomography(img_good2, img_good1, CV_RANSAC);
}

Mat PositionImgSetting::get_blending_image(Mat left, Mat right, Mat mask)
{
	//做pyramid blending讓光線柔和
	Mat_<Vec3f> l, r;
	left.convertTo(l, CV_32F, 1.0 / 255.0);//Vec3f表示有三通道，即 l[row][column][depth]
	right.convertTo(r, CV_32F, 1.0 / 255.0);//Vec3f表示有三通道，即 l[row][column][depth]

	Mat_<Vec3f> blend;
	Mat blend_mat;

	blend = LaplacianBlend(l, r, mask);

	blend.convertTo(blend_mat, CV_8UC3, 255);
	cout << "blend success" << endl;
	return blend_mat;
}

Mat PositionImgSetting::get_stitch_image(Mat img1, Mat img2, int direction, Mat H1, vector<int> &fill)
{
	// Use the Homography Matrix to warp the images
	Mat left, right, result;
	Mat mask(STITCH_SCREEN_HEIGHT, STITCH_SCREEN_WIDTH, CV_8U, Scalar(0));
	Mat point1(3, 1, CV_64F), point2(3, 1, CV_64F), point3(3, 1, CV_64F), point4(3, 1, CV_64F);

	point1.at<double>(0, 0) = 0.0;		//img2 左上角
	point1.at<double>(1, 0) = 0.0;
	point1.at<double>(2, 0) = 1.0;

	point2.at<double>(0, 0) = img2.cols;	//img2右上角
	point2.at<double>(1, 0) = 0.0;
	point2.at<double>(2, 0) = 1.0;

	point3.at<double>(0, 0) = img2.cols;	//img2右下角
	point3.at<double>(1, 0) = img2.rows;
	point3.at<double>(2, 0) = 1.0;

	point4.at<double>(0, 0) = 0.0;			//img2左下角
	point4.at<double>(1, 0) = img2.rows;
	point4.at<double>(2, 0) = 1.0;

	Mat point1_1 = H1*point1;
	Mat point2_1 = H1*point2;
	Mat point3_1 = H1*point3;
	Mat point4_1 = H1*point4;

	int x1 = (int)round(point1_1.at<double>(0, 0) / point1_1.at<double>(2, 0));
	int x2 = (int)round(point2_1.at<double>(0, 0) / point2_1.at<double>(2, 0));
	int x3 = (int)round(point3_1.at<double>(0, 0) / point3_1.at<double>(2, 0));
	int x4 = (int)round(point4_1.at<double>(0, 0) / point4_1.at<double>(2, 0));
	int y1 = (int)round(point1_1.at<double>(1, 0) / point1_1.at<double>(2, 0));
	int y2 = (int)round(point2_1.at<double>(1, 0) / point2_1.at<double>(2, 0));
	int y3 = (int)round(point3_1.at<double>(1, 0) / point3_1.at<double>(2, 0));
	int y4 = (int)round(point4_1.at<double>(1, 0) / point4_1.at<double>(2, 0));

	//cout << "x1=" << x1 << endl << "x2=" << x2 << endl << "x3=" << x3 << endl << "x4=" << x4 << endl;
	/** Create some points */
	Point rook_points[1][4];
	int lineType = 8;
	int npt[] = { 4 };


	if (direction == STITCH2_LEFT)
	{
		warpPerspective(img2, left, H1, Size(STITCH_SCREEN_WIDTH, STITCH_SCREEN_HEIGHT));
		mask(Range::all(), Range(0, x2-300)) = 1.0;

		if (x1 < 0 && x4 < 0) fill[0] = 1;
		cout <<"fill[0] = "<< fill[0] << endl;
		result = get_blending_image(left, img1, mask);
	}
	else if (direction == STITCH2_RIGHT)
	{
		warpPerspective(img2, right, H1, Size(STITCH_SCREEN_WIDTH, STITCH_SCREEN_HEIGHT));
		mask(Range::all(), Range(0, x1 + 300)) = 1.0;

		if (x2 > STITCH_SCREEN_WIDTH && x3 >STITCH_SCREEN_WIDTH) fill[1] = 1;
		cout << "fill[1] = " << fill[1] << endl;

		result = get_blending_image(img1, right, mask);
	}
	else if (direction == STITCH2_UP)
	{
		warpPerspective(img2, left, H1, Size(STITCH_SCREEN_WIDTH, STITCH_SCREEN_HEIGHT));
		rook_points[0][0] = Point(x1 + 20, y1-10);
		rook_points[0][1] = Point(x2 - 20, y2-10);
		rook_points[0][2] = Point(x3 - 60, y3 - 200);
		rook_points[0][3] = Point(x4 + 60, y4 - 200);

		const Point* ppt[1] = { rook_points[0] };
		fillPoly(mask, ppt, npt, 1, Scalar(1), lineType);

		result = get_blending_image(left, img1, mask);
	}
	else if (direction == STITCH2_DOWN)
	{
		warpPerspective(img2, left, H1, Size(STITCH_SCREEN_WIDTH, STITCH_SCREEN_HEIGHT));
		rook_points[0][0] = Point(x1 + 60, y1 + 200);
		rook_points[0][1] = Point(x2 - 60, y2 + 200);
		rook_points[0][2] = Point(x3 - 20, y3);
		rook_points[0][3] = Point(x4 + 20, y4);

		const Point* ppt[1] = { rook_points[0] };
		fillPoly(mask, ppt, npt, 1, Scalar(1), lineType);

		result = get_blending_image(left, img1, mask);
	}
	else if (direction == STITCH2_LEFT_UP)
	{
		warpPerspective(img2, left, H1, Size(STITCH_SCREEN_WIDTH, STITCH_SCREEN_HEIGHT));
		rook_points[0][0] = Point(x1, y1);
		rook_points[0][1] = Point(x2 - 200, y2);
		rook_points[0][2] = Point(x3 - 200, y3 - 200);
		rook_points[0][3] = Point(x4 + 10, y4 - 200);

		const Point* ppt[1] = { rook_points[0] };
		fillPoly(mask, ppt, npt, 1, Scalar(1), lineType);

		if (x1 < 0 && x4 <0) fill[2] = 1;
		cout << "fill[2] = " << fill[2] << endl;

		result = get_blending_image(left, img1, mask);
	}
	else if (direction == STITCH2_RIGHT_UP)
	{
		warpPerspective(img2, left, H1, Size(STITCH_SCREEN_WIDTH, STITCH_SCREEN_HEIGHT));
		rook_points[0][0] = Point(x1 + 200, y1);
		rook_points[0][1] = Point(x2, y2);
		rook_points[0][2] = Point(x3 - 10, y3 - 200);
		rook_points[0][3] = Point(x4 + 200, y4 - 200);

		const Point* ppt[1] = { rook_points[0] };
		fillPoly(mask, ppt, npt, 1, Scalar(1), lineType);

		if (x2 > STITCH_SCREEN_WIDTH && x3 >STITCH_SCREEN_WIDTH) fill[3] = 1;
		cout << "fill[3] = " << fill[3] << endl;

		result = get_blending_image(left, img1, mask);
	}
	else if (direction == STITCH2_LEFT_DOWN)
	{
		warpPerspective(img2, left, H1, Size(STITCH_SCREEN_WIDTH, STITCH_SCREEN_HEIGHT));
		rook_points[0][0] = Point(x1 + 10, y1 + 200);
		rook_points[0][1] = Point(x2 - 200, y2 + 200);
		rook_points[0][2] = Point(x3 - 200, y3);
		rook_points[0][3] = Point(x4, y4);

		const Point* ppt[1] = { rook_points[0] };
		fillPoly(mask, ppt, npt, 1, Scalar(1), lineType);

		if (x1 < 0 && x4 <0) fill[4] = 1;
		cout << "fill[4] = " << fill[4] << endl;

		result = get_blending_image(left, img1, mask);
	}
	else if (direction == STITCH2_RIGHT_DOWN)
	{
		warpPerspective(img2, left, H1, Size(STITCH_SCREEN_WIDTH, STITCH_SCREEN_HEIGHT));
		rook_points[0][0] = Point(x1 + 200, y1 + 200);
		rook_points[0][1] = Point(x2 - 10, y2 + 200);
		rook_points[0][2] = Point(x3 - 10, y3);
		rook_points[0][3] = Point(x4 + 200, y4 );

		const Point* ppt[1] = { rook_points[0] };
		fillPoly(mask, ppt, npt, 1, Scalar(1), lineType);
		if (x2 > STITCH_SCREEN_WIDTH && x3 >STITCH_SCREEN_WIDTH) fill[5] = 1;
		cout << "fill[5] = " << fill[5] << endl;

		result = get_blending_image(left, img1, mask);
	}

	//imshow("get_stitch_matrix", result2);
	return result;
}


Mat PositionImgSetting::get_H(Mat img_1, Mat img_2, int method)
{
	Image img1(img_1), img2(img_2);

	FlannBasedMatcher matcher;
	vector<DMatch> matches1, matches2, good_matches;

	matcher.match(img1.descriptors(), img2.descriptors(), matches1);
	matcher.match(img2.descriptors(), img1.descriptors(), matches2);
	draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), matches1, 1);
	draw_matches(img1.mat(), img2.mat(), img1.keypoints(), img2.keypoints(), matches1, 2);

	if (method == METHOD1 || method == METHOD2)
	{
		//look for symmetry matches
		vector< DMatch > sym_matches;

		symmetryTest(matches1, matches2, sym_matches);
		draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), sym_matches, 3);
		draw_matches(img1.mat(), img2.mat(), img1.keypoints(), img2.keypoints(), sym_matches, 4);

		//-- Use only "good" matches (i.e. whose distance is less than 3*min_dist )
		if (method == METHOD1)
			good_matches = get_good_dist_matches(sym_matches, 4);
		else
			good_matches = get_good_dist_matches(sym_matches, 7);

		draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), good_matches, 5);
		draw_matches(img1.mat(), img2.mat(), img1.keypoints(), img2.keypoints(), good_matches, 6);
	}
	else
	{
		//-- Use only "good" matches (i.e. whose distance is less than 3*min_dist )
		good_matches = get_good_dist_matches(matches1, 6);
		draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), good_matches, 7);
		draw_matches(img1.mat(), img2.mat(), img1.keypoints(), img2.keypoints(), good_matches, 8);
	}

	// Find the Homography Matrix from best matches
	Mat H1;
	get_homography_matrix_by_matches(img1, img2, good_matches, H1);

	return H1;
}


/* stitch2: Stitch image2 to image1 and return the result.
*  
*  img_1 - image that won't change
*  img_2 - image that will stitch to img_1
*  method - choose one of the stitching method.
*  (method1 - symmetric test + distance <= 6*min_dist
*   method2 - symmetric test + distance <= 7*min_dist
*   method3 - distance <= 6*min_dist)
*  direction - the direction of stitching img_2 to img_1, according to the center of img_1
*  fill - has 6 numbers according to 6 directions. Number change from 0 to 1 if the edge of that direction is filled.
*
*/

Mat PositionImgSetting::stitch2(Mat img_1, Mat img_2, int method, int direction, vector<int> &fill)
{
	assert(img_1.rows > 0 && img_2.rows > 0);
	Image img1, img2;
	
	if (direction == STITCH2_LEFT)	//img1.x<(2/3)*img1 wdith, img2.x>0.3*img2 width
	{
		img_capture(img_1, 0, 0, int(STITCH_SCREEN_WIDTH * 2 / 3), STITCH_SCREEN_HEIGHT, img1);
		//img_capture(img_2, int(STITCH_IMG_WIDTH * 0.3), 0, int(STITCH_IMG_WIDTH * 0.7), STITCH_IMG_HEIGHT, img2);
		img2.set_image(img_2);
	}
	else if (direction == STITCH2_RIGHT)	//img1.x > (1/3)*img1 width, img2.x < 0.7 * img2 width
	{
		img_capture(img_1, int(STITCH_SCREEN_WIDTH * 1 / 3), 0, int(STITCH_SCREEN_WIDTH * 2 / 3), STITCH_SCREEN_HEIGHT, img1);
		//img_capture(img_2, 0, 0, int(STITCH_IMG_WIDTH * 0.7), STITCH_IMG_HEIGHT, img2);
		img2.set_image(img_2);
	}
	else if (direction == STITCH2_UP)	//(1/6)*img1 width<img1.x < (5/6)*img1 width, img1.y < (7/12)*img1 height, img2.y > 0.4 * img2 height
	{
		img_capture(img_1, int(STITCH_SCREEN_WIDTH * 1 / 6), 0, int(STITCH_SCREEN_WIDTH * 4 / 6), int(STITCH_SCREEN_HEIGHT * 7 / 12), img1);
		img_capture(img_2, 0, int(STITCH_IMG_HEIGHT*0.4), STITCH_IMG_WIDTH, int(STITCH_IMG_HEIGHT*0.6), img2);
	}
	else if (direction == STITCH2_DOWN)	// img1.y > (5/12)*img1 height, img2.y < 0.6 * img2 height
	{
		img_capture(img_1, int(STITCH_SCREEN_WIDTH * 1 / 6), int(STITCH_SCREEN_HEIGHT * 5 / 12), int(STITCH_SCREEN_WIDTH * 4 / 6), int(STITCH_SCREEN_HEIGHT * 7 / 12), img1);
		img_capture(img_2, 0, 0, STITCH_IMG_WIDTH, int(STITCH_IMG_HEIGHT*0.6), img2);
	}
	else if (direction == STITCH2_LEFT_UP)	//img1.x<(2/3)*img1 width, img1.y<(7/12)*img1 height
	{
		img_capture(img_1, 0, 0, int(STITCH_SCREEN_WIDTH * 2 / 3), int(STITCH_SCREEN_HEIGHT * 7 / 12), img1);
		img2.set_image(img_2);
	}
	else if (direction == STITCH2_LEFT_DOWN)	//img1.x<(2/3)*img1 width, img1.y > (5/12)*img1 height
	{
		img_capture(img_1, 0, int(STITCH_SCREEN_HEIGHT * 5 / 12), int(STITCH_SCREEN_WIDTH * 2 / 3), int(STITCH_SCREEN_HEIGHT * 7 / 12), img1);
		img2.set_image(img_2);
	}
	else if (direction == STITCH2_RIGHT_UP)	//img1.x > (1/3)*img1 width, img1.y<(7/12)*img1 height
	{
		img_capture(img_1, int(STITCH_SCREEN_WIDTH * 1 / 3), 0, int(STITCH_SCREEN_WIDTH * 2 / 3), int(STITCH_SCREEN_HEIGHT * 7 / 12), img1);
		img2.set_image(img_2);
	}
	else if (direction == STITCH2_RIGHT_DOWN)	//	//img1.x > (1/3)*img1 width, img1.y > (5/12)*img1 height
	{
		img_capture(img_1, int(STITCH_SCREEN_WIDTH * 1 / 3), int(STITCH_SCREEN_HEIGHT * 5 / 12), int(STITCH_SCREEN_WIDTH * 2 / 3), int(STITCH_SCREEN_HEIGHT * 7 / 12), img1);
		img2.set_image(img_2);
	}

	imwrite("D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\Good Matches\\capture1.jpg", img1.mat());
	imwrite("D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\Good Matches\\capture2.jpg", img2.mat());

	FlannBasedMatcher matcher;
	vector<DMatch> matches1, matches2, good_matches;

	matcher.match(img1.descriptors(), img2.descriptors(), matches1);
	matcher.match(img2.descriptors(), img1.descriptors(), matches2);
	draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), matches1, 1);
	draw_matches(img1.mat(), img2.mat(), img1.keypoints(), img2.keypoints(), matches1, 2);

	if (method == METHOD1 || method == METHOD2)
	{
		//look for symmetry matches
		vector< DMatch > sym_matches;

		symmetryTest(matches1, matches2, sym_matches);
		draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), sym_matches, 3);
		draw_matches(img1.mat(), img2.mat(), img1.keypoints(), img2.keypoints(), sym_matches, 4);

		//-- Use only "good" matches (i.e. whose distance is less than 3*min_dist )
		if (method == METHOD1)
			good_matches = get_good_dist_matches(sym_matches, 6);
		else
			good_matches = get_good_dist_matches(sym_matches, 7);

		draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), good_matches, 5);
		draw_matches(img1.mat(), img2.mat(), img1.keypoints(), img2.keypoints(), good_matches, 6);
	}
	else
	{
		//-- Use only "good" matches (i.e. whose distance is less than 3*min_dist )
		good_matches = get_good_dist_matches(matches1, 6);
		draw_matches(img_1, img_2, img1.keypoints(), img2.keypoints(), good_matches, 7);
		draw_matches(img1.mat(), img2.mat(), img1.keypoints(), img2.keypoints(), good_matches, 8);
	}

	// Find the Homography Matrix from best matches
	Mat H1;
	get_homography_matrix_by_matches(img1, img2,  good_matches, H1);
	//cout << H1 << endl;

	// Use the Homography Matrix to warp the images
	Mat stitch2_result;
	stitch2_result = get_stitch_image(img_1, img_2,  direction, H1, fill);

	cout << "stitch2 success" << endl<<"----------------------------"<<endl;
	//imshow("stitch3_result",stitch3_result);
	return stitch2_result;
}


/*void PositionImgSetting::stitch_one_scene()
{
	string path = "D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\";
	Mat img_top = imread(path+"0_choosen\\small\\0.jpg");
	Mat img_central = imread(path + "1_choosen\\small\\0.jpg");
	Mat img_bottom = imread(path + "2_choosen\\small\\0.jpg");
	vector<Mat> img_top_left, img_top_right, img_central_left, img_central_right, img_bottom_left, img_bottom_right;
	vector<int> fill(8, 0);

	img_top_left.push_back(imread(path + "0_choosen\\small\\625.jpg"));
	img_top_left.push_back(imread(path + "0_choosen\\small\\616.jpg"));
	img_top_left.push_back(imread(path + "0_choosen\\small\\607.jpg"));

	img_top_right.push_back(imread(path + "0_choosen\\small\\9.jpg"));
	img_top_right.push_back(imread(path + "0_choosen\\small\\18.jpg"));
	img_top_right.push_back(imread(path + "0_choosen\\small\\27.jpg"));
	img_top_right.push_back(imread(path + "0_choosen\\small\\36.jpg"));
	img_top_right.push_back(imread(path + "0_choosen\\small\\45.jpg"));
	img_top_right.push_back(imread(path + "0_choosen\\small\\54.jpg"));

	img_central_left.push_back(imread(path + "1_choosen\\small\\275.jpg"));
	img_central_left.push_back(imread(path + "1_choosen\\small\\271.jpg"));
	img_central_left.push_back(imread(path + "1_choosen\\small\\267.jpg"));
	img_central_left.push_back(imread(path + "1_choosen\\small\\263.jpg"));
	img_central_left.push_back(imread(path + "1_choosen\\small\\259.jpg"));
	img_central_left.push_back(imread(path + "1_choosen\\small\\255.jpg"));

	img_central_right.push_back(imread(path + "1_choosen\\small\\4.jpg"));
	img_central_right.push_back(imread(path + "1_choosen\\small\\8.jpg"));
	img_central_right.push_back(imread(path + "1_choosen\\small\\12.jpg"));
	img_central_right.push_back(imread(path + "1_choosen\\small\\16.jpg"));
	img_central_right.push_back(imread(path + "1_choosen\\small\\20.jpg"));

	img_bottom_left.push_back(imread(path + "2_choosen\\small\\323.jpg"));
	img_bottom_left.push_back(imread(path + "2_choosen\\small\\318.jpg"));
	img_bottom_left.push_back(imread(path + "2_choosen\\small\\313.jpg"));
	img_bottom_left.push_back(imread(path + "2_choosen\\small\\308.jpg"));
	img_bottom_left.push_back(imread(path + "2_choosen\\small\\303.jpg"));
	img_bottom_left.push_back(imread(path + "2_choosen\\small\\298.jpg"));

	img_bottom_right.push_back(imread(path + "2_choosen\\small\\5.jpg"));
	img_bottom_right.push_back(imread(path + "2_choosen\\small\\10.jpg"));
	img_bottom_right.push_back(imread(path + "2_choosen\\small\\15.jpg"));
	img_bottom_right.push_back(imread(path + "2_choosen\\small\\20.jpg"));
	img_bottom_right.push_back(imread(path + "2_choosen\\small\\25.jpg"));

	Mat stitch_ref(STITCH_SCREEN_HEIGHT, STITCH_SCREEN_WIDTH, img_central.type());
	Rect rect_ref(int(STITCH_SCREEN_WIDTH / 2 - STITCH_IMG_WIDTH / 2), int(STITCH_SCREEN_HEIGHT / 2 - STITCH_IMG_HEIGHT / 2), STITCH_IMG_WIDTH, STITCH_IMG_HEIGHT);
	img_central.copyTo(stitch_ref(rect_ref));		//先把正中間的圖放到screen大小的矩陣

	Mat stitch(STITCH_SCREEN_HEIGHT, STITCH_SCREEN_WIDTH, img_central.type());
	Rect rect(int(STITCH_SCREEN_WIDTH / 2 - STITCH_IMG_WIDTH / 2), int(STITCH_SCREEN_HEIGHT / 2 - STITCH_IMG_HEIGHT / 2), STITCH_IMG_WIDTH, STITCH_IMG_HEIGHT);
	img_central.copyTo(stitch(rect));		//先把正中間的圖放到screen大小的矩陣
	imwrite(path + "stitch\\" + to_string(1) + "\\stitch" + to_string(0) + ".jpg", stitch);

	Mat H_top = get_H(stitch_ref, img_top, METHOD1);
	Mat H_bottom = get_H(stitch_ref, img_bottom, METHOD1);
	stitch = get_stitch_matrix(stitch, img_top, STITCH2_UP, H_top, fill);
	imwrite(path + "stitch\\" + to_string(1) + "\\stitch" + to_string(0) + ".jpg", stitch);
	stitch = get_stitch_matrix(stitch, img_bottom, STITCH2_DOWN, H_bottom, fill);
	imwrite(path + "stitch\\" + to_string(1) + "\\stitch" + to_string(0) + ".jpg", stitch);


	vector<Mat> H_top_left, H_top_right, H_central_left, H_central_right, H_bottom_left, H_bottom_right;


	//top_left
	H_top_left.push_back(H_top);
	H_top_left.push_back(get_H(img_top, img_top_left[0], METHOD1));
	for (int i = 0; i <img_top_left.size()-1; i++)
	{
		H_top_left.push_back(get_H(img_top_left[i], img_top_left[i+1], METHOD1));
	}
	

	double h[] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
	Mat H = Mat(3, 3, CV_64F, h).clone();
	int i, j;
	for (i = 0 ; i < img_top_left.size(); i++)
	{
		for (j = i+1; j > -1; j--)
		{
			H = H_top_left[j] * H;
		}
		stitch = get_stitch_matrix(stitch, img_top_left[i], STITCH2_LEFT_UP, H, fill);
		imwrite(path + "stitch\\" + to_string(1) + "\\stitch" + to_string(0) + ".jpg", stitch);
		H = Mat(3, 3, CV_64F, h).clone();
	}



	//central_right
	H_central_right.push_back(get_H(stitch_ref, img_central_right[0], METHOD1));
	for (int i = 0; i <img_central_right.size() - 1; i++)
	{
		H_central_right.push_back(get_H(img_central_right[i], img_central_right[i + 1], METHOD1));
	}
	
	for (i = 0; i < img_central_right.size(); i++)
	{
		for (j = i; j > -1; j--)
		{
			H = H_central_right[j] * H;
		}
		stitch = get_stitch_matrix(stitch, img_central_right[i], STITCH2_RIGHT, H, fill);
		imwrite(path + "stitch\\" + to_string(1) + "\\stitch" + to_string(0) + ".jpg", stitch);
		H = Mat(3, 3, CV_64F, h).clone();
	}


	//bottom_right
	H_bottom_right.push_back(H_bottom);
	H_bottom_right.push_back(get_H(img_bottom, img_bottom_right[0], METHOD1));
	for (int i = 0; i <img_bottom_right.size() - 1; i++)
	{
		H_bottom_right.push_back(get_H(img_bottom_right[i], img_bottom_right[i + 1], METHOD1));
	}

	for (i = 0; i < img_bottom_right.size(); i++)
	{
		for (j = i + 1; j > -1; j--)
		{
			H = H_bottom_right[j] * H;
		}
		stitch = get_stitch_matrix(stitch, img_bottom_right[i], STITCH2_RIGHT_DOWN, H, fill);
		imwrite(path + "stitch\\" + to_string(1) + "\\stitch" + to_string(0) + ".jpg", stitch);
		H = Mat(3, 3, CV_64F, h).clone();
	}

	//return stitch2_result;
}*/




int PositionImgSetting::getImgAmount(string target_folder)
{
	DIR *dp;
	struct dirent *ep;
	char target_char[100];

	strcpy(target_char, target_folder.c_str());
	dp = opendir(target_char);
	int count = 0;
	if (dp != NULL)
	{
		while (ep = readdir(dp)){
			puts(ep->d_name);
			count++;
		}

		(void)closedir(dp);
	}
	else
		perror("Couldn't open the directory");

	//cout << "count = " << count << endl;
	return count-2;
}


/* Initial3Img: Initial 3 images to start stitching(central image, right image and left image).
*
*  img_number - the half angle of the central image(0~179)
*  vertical_angle - up 20 degree, 0 degree, down 20 degree or down 40 degree
*  stitch_part - the part of the scene (top, central or bottom)
*
*/

void PositionImgSetting::Initial3Img(int img_number, int vertical_angle, int stitch_part)
{
	string path = "D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\";

	if (stitch_part == STITCH_SCREEN_TOP)
	{
		vertical_angle = vertical_angle - 1;
		img_amount = img_amount_top;	
	}
	else if (stitch_part == STITCH_SCREEN_BOTTOM)
	{
		vertical_angle = vertical_angle + 1;
		img_amount =img_amount_bot;	
	}
	else
	{
		img_amount = img_amount_mid ;
	}

	assert(image_amount > 0);
	interval = (int)round(img_amount * 3.0 / 360.0);	//img next(left or right) number interval

	central_img = (int)round((float)img_number * 2.0 / 360.0 * (float)img_amount);
	if (central_img - interval >= 0)		//img max number is (image_amount - 1), min number is 0
		left_img = central_img - interval;
	else
		left_img = img_amount + central_img - interval;		

	if (central_img + interval < img_amount)
		right_img = central_img + interval;
	else
		right_img = central_img + interval - img_amount;

	cout << "central_img = " << central_img << endl << "left_img = " << left_img << endl << "right_img = " << right_img << endl;

	cout << path + to_string(vertical_angle) + "_choosen\\small\\" + to_string(left_img) + ".jpg" << endl;
	img_l = imread(path + to_string(vertical_angle) + "_choosen\\small\\" + to_string(left_img) + ".jpg");
	img_mid = imread(path + to_string(vertical_angle) + "_choosen\\small\\" + to_string(central_img) + ".jpg");
	img_r = imread(path + to_string(vertical_angle) + "_choosen\\small\\" + to_string(right_img) + ".jpg");

}


void PositionImgSetting::StitchPart(int img_number, int vertical_angle, int method, int stitch_part, vector<int> &fill, Mat& stitch)
{
	string path = "D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\";
	int stitch_direction_l, stitch_direction_r, check_fill_l, check_fill_r, img_vertical_angle;

	if (stitch_part == STITCH_SCREEN_TOP)
	{
		stitch_direction_l = STITCH2_LEFT_UP;
		stitch_direction_r = STITCH2_RIGHT_UP;
		check_fill_l = 2;
		check_fill_r = 3;
		img_vertical_angle = vertical_angle - 1;
	}
	else if (stitch_part == STITCH_SCREEN_MID)
	{
		stitch_direction_l = STITCH2_LEFT;
		stitch_direction_r = STITCH2_RIGHT;
		check_fill_l = 0;
		check_fill_r = 1;
		img_vertical_angle = vertical_angle;
	}
	else
	{
		stitch_direction_l = STITCH2_LEFT_DOWN;
		stitch_direction_r = STITCH2_RIGHT_DOWN;
		check_fill_l = 4;
		check_fill_r = 5;
		img_vertical_angle = vertical_angle + 1;
	}

	while (1)	//stitch left
	{
		cout << "left_img_num = " << left_img << endl;
		stitch = stitch2(stitch, img_l, method, stitch_direction_l, fill);
		imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(img_number) + ".jpg", stitch);

		if (fill[check_fill_l] == 1) break;

		if (left_img - interval >= 0)
			left_img -= interval;
		else
			left_img = img_amount + left_img - interval;

		img_l = imread(path + to_string(img_vertical_angle) + "_choosen\\small\\" + to_string(left_img) + ".jpg");
	}

	while (1)	//stitch right
	{
		cout << "right_img_num = " << right_img << endl;
		stitch = stitch2(stitch, img_r, method, stitch_direction_r, fill);
		imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(img_number) + ".jpg", stitch);
		
		if (fill[check_fill_r] == 1) break;

		if (right_img + interval < img_amount)
			right_img += interval;
		else
			right_img = right_img + interval - img_amount;

		img_r = imread(path + to_string(img_vertical_angle) + "_choosen\\small\\" + to_string(right_img) + ".jpg");
	}
}


void PositionImgSetting::StitchPartByHPath(int img_number, int vertical_angle, int method, int stitch_part, vector<int> &fill, Mat& stitch)
{
	string path = "D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\";
	int stitch_direction_l, stitch_direction_r, check_fill_l, check_fill_r, img_vertical_angle;

	double identity_matrix[] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 };
	Mat H_first, H;
	Mat img_first, img_last;

	if (stitch_part == STITCH_SCREEN_TOP)
	{
		stitch_direction_l = STITCH2_LEFT_UP;
		stitch_direction_r = STITCH2_RIGHT_UP;
		check_fill_l = 2;
		check_fill_r = 3;
		img_vertical_angle = vertical_angle - 1;

		H_first = get_H(stitch_ref, img_mid, method);
		stitch = get_stitch_image(stitch, img_mid, STITCH2_UP, H_first, fill);
		img_first = img_mid.clone();
	}
	else if (stitch_part == STITCH_SCREEN_MID)
	{
		stitch_direction_l = STITCH2_LEFT;
		stitch_direction_r = STITCH2_RIGHT;
		check_fill_l = 0;
		check_fill_r = 1;
		img_vertical_angle = vertical_angle;

		H_first = Mat(3, 3, CV_64F, identity_matrix).clone();
		cout << H_first << endl;
		img_first = stitch_ref;
	}
	else
	{
		stitch_direction_l = STITCH2_LEFT_DOWN;
		stitch_direction_r = STITCH2_RIGHT_DOWN;
		check_fill_l = 4;
		check_fill_r = 5;
		img_vertical_angle = vertical_angle + 1;

		H_first = get_H(stitch_ref, img_mid, method);
		stitch = get_stitch_image(stitch, img_mid, STITCH2_DOWN, H_first, fill);
		img_first = img_mid.clone();
	}

	H = H_first.clone();
	img_last = img_first.clone();
	while (1)	//stitch left
	{
		cout << "left_img_num = " << left_img << endl;
		H = H * get_H(img_last, img_l, method);
		stitch = get_stitch_image(stitch, img_l, stitch_direction_l, H, fill);
		imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(img_number) + ".jpg", stitch);

		if (fill[check_fill_l] == 1) break;

		if (left_img - interval >= 0)
			left_img -= interval;
		else
			left_img = img_amount + left_img - interval;

		img_last = img_l;
		img_l = imread(path + to_string(img_vertical_angle) + "_choosen\\small\\" + to_string(left_img) + ".jpg");
	}


	H = H_first.clone();
	cout << H_first << endl;
	cout << H << endl;
	img_last = img_first.clone();
	while (1)	//stitch right
	{
		cout << "right_img_num = " << right_img << endl;
		H = H * get_H(img_last, img_r, method);
		stitch = get_stitch_image(stitch, img_r, stitch_direction_r, H, fill);
		imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(img_number) + ".jpg", stitch);

		if (fill[check_fill_r] == 1) break;

		if (right_img + interval < img_amount)
			right_img += interval;
		else
			right_img = right_img + interval - img_amount;

		img_last = img_r;
		img_r = imread(path + to_string(img_vertical_angle) + "_choosen\\small\\" + to_string(right_img) + ".jpg");
	}
}


void PositionImgSetting::StitchScene(int vertical_angle, int img_number, int method)	//0~179
{	
	string path = "D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\";
	FILE * fp;
	char filename[200];
	
	t.reset();
	t.start();

	cout <<endl<< "*****************  Stitch Screen " << img_number << "  ******************" << endl;
	vector<int> fill(8, 0);
	
	Initial3Img(img_number, vertical_angle, STITCH_SCREEN_MID);

	Mat stitch(STITCH_SCREEN_HEIGHT, STITCH_SCREEN_WIDTH, img_mid.type());
	Rect rect(int(STITCH_SCREEN_WIDTH / 2 - STITCH_IMG_WIDTH / 2), int(STITCH_SCREEN_HEIGHT / 2 - STITCH_IMG_HEIGHT / 2), STITCH_IMG_WIDTH, STITCH_IMG_HEIGHT);
	img_mid.copyTo(stitch(rect));		//先把正中間的圖放到screen大小的矩陣
	imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(img_number) + ".jpg", stitch);

	StitchPart(img_number, vertical_angle, method, STITCH_SCREEN_MID, fill, stitch);

	//stitch up------------------------------------------------------------------------------------------------
	Initial3Img(img_number, vertical_angle, STITCH_SCREEN_TOP);

	stitch = stitch2(stitch, img_mid, method, STITCH2_UP, fill);
	imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(img_number) + ".jpg", stitch);

	StitchPart(img_number, vertical_angle, method, STITCH_SCREEN_TOP, fill, stitch);

	//stitch down----------------------------------------------------------------------------------------------
	Initial3Img(img_number, vertical_angle, STITCH_SCREEN_BOTTOM);

	stitch = stitch2(stitch, img_mid, method, STITCH2_DOWN, fill);
	imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(img_number) + ".jpg", stitch);

	StitchPart(img_number, vertical_angle, method, STITCH_SCREEN_BOTTOM, fill, stitch);

	cout << path + "stitch\\"<<vertical_angle<<"\\stitch" + to_string(img_number) + ".jpg" << endl;
	imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(img_number) + ".jpg", stitch);

	t.stop();
	cout << "process time for stitch screen " << img_number << " = " << t.getTimeSec() << " sec." << endl;

	sprintf(filename, "%sstitch_time.txt", path);
	fp = fopen(filename, "a");
	if (!fp)
		cout << "Error: Open stitch_time.txt error!" << endl;
	else
		fprintf(fp, "Process time for stitch screen %d = %f sec.\n", img_number, t.getTimeSec());
	
	fclose(fp);
}


void PositionImgSetting::StitchSceneByHPath(int vertical_angle, int scene_num)
{
	string path = "D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\";
	vector<int> fill(6, 0);

	//stitch middle
	Initial3Img(scene_num, vertical_angle, STITCH_SCREEN_MID);

	stitch_ref.create(STITCH_SCREEN_HEIGHT, STITCH_SCREEN_WIDTH, img_mid.type());
	Rect rect_ref(int(STITCH_SCREEN_WIDTH / 2 - STITCH_IMG_WIDTH / 2), int(STITCH_SCREEN_HEIGHT / 2 - STITCH_IMG_HEIGHT / 2), STITCH_IMG_WIDTH, STITCH_IMG_HEIGHT);
	img_mid.copyTo(stitch_ref(rect_ref));		//先把正中間的圖放到screen大小的矩陣

	Mat stitch = stitch_ref.clone();
	imwrite(path + "stitch\\" + to_string(vertical_angle) + "\\stitch" + to_string(scene_num) + ".jpg", stitch);

	StitchPart(scene_num, vertical_angle, METHOD1, STITCH_SCREEN_MID, fill, stitch);

	//stitch top
	Initial3Img(scene_num, vertical_angle, STITCH_SCREEN_TOP);
	StitchPart(scene_num, vertical_angle, METHOD1, STITCH_SCREEN_TOP, fill, stitch);

	//stitch bottom
	Initial3Img(scene_num, vertical_angle, STITCH_SCREEN_BOTTOM);
	StitchPart(scene_num, vertical_angle, METHOD1, STITCH_SCREEN_BOTTOM, fill, stitch);
}


/* StitchSceneRange: Create the stitched scenes from img_number1 to img_number2.
*
*  vertical_angle - choose depression angle(0 degree or 20 degree)
*  img_number1 - the scene you want to start. (0~179)
*  img_number2 - the scene you want to end. (0~179)
*  methed - choose one of the stitching method.
*  (method1 - symmetric test + distance <= 6*min_dist
*   method2 - symmetric test + distance <= 7*min_dist
*   method3 - distance <= 6*min_dist)
*
*/
int PositionImgSetting::StitchSceneRange(int vertical_angle, int img_number1, int img_number2, int method){
	string path = "D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\";
	cout << "_room_naem = " << _room_name << endl;

	if (img_number1 < 0 || img_number1 > 179 || img_number2 < 0 || img_number2 > 179)
	{
		cout << "ERROR: The number of scene is out of range!" << endl;
		return -1;
	}

	if (vertical_angle == VERTICAL_CENTRAL)
	{
		img_amount_top = getImgAmount(path + to_string(VERTICAL_UP_20) + "_choosen\\small\\");
		img_amount_mid = getImgAmount(path + to_string(VERTICAL_CENTRAL) + "_choosen\\small\\");
		img_amount_bot = getImgAmount(path + to_string(VERTICAL_DOWN_20) + "_choosen\\small\\");
	}
	else
	{
		img_amount_top = getImgAmount(path + to_string(VERTICAL_CENTRAL) + "_choosen\\small\\");
		img_amount_mid = getImgAmount(path + to_string(VERTICAL_DOWN_20) + "_choosen\\small\\");
		img_amount_bot = getImgAmount(path + to_string(VERTICAL_DOWN_40) + "_choosen\\small\\");
	}

	for (int n = img_number1; n < img_number2 + 1; n++)
	{
		if (method == METHOD4)
			StitchSceneByHPath(vertical_angle, n);
		else
			StitchScene(vertical_angle, n, method);
	}
	return 1;
}


/* StitchSceneAll: Create all the stitched scenes. Total 360 images.
*  
*  methed - choose one of the stitching method.
*  (method1 - symmetric test + distance <= 6*min_dist
*   method2 - symmetric test + distance <= 7*min_dist
*   method3 - distance <= 6*min_dist)
*
*/
void PositionImgSetting::StitchSceneAll(int method){
	string path = "D:\\image\\" + _room_name + "\\position" + to_string(_number) + "\\";
	cout <<"_room_naem = "<< _room_name << endl;
	img_amount_top = getImgAmount(path + to_string(VERTICAL_UP_20) + "_choosen\\small\\");
	img_amount_mid = getImgAmount(path + to_string(VERTICAL_CENTRAL) + "_choosen\\small\\");
	img_amount_bot = getImgAmount(path + to_string(VERTICAL_DOWN_20) + "_choosen\\small\\");

	for (int n = 0; n < 180; n++)
	{
		if (method == METHOD4)
			StitchSceneByHPath(VERTICAL_CENTRAL, n);
		else
			StitchScene(VERTICAL_CENTRAL, n, method);
	}

	img_amount_top = img_amount_mid;
	img_amount_mid = img_amount_bot;
	img_amount_bot = getImgAmount(path + to_string(VERTICAL_DOWN_40) + "_choosen\\small\\");

	for (int n = 0; n < 180; n++)
	{
		if (method == METHOD4)
			StitchSceneByHPath(VERTICAL_DOWN_20, n);
		else
			StitchScene(VERTICAL_DOWN_20, n, method);
	}
}


/*Mat PositionImgSetting::get_screen(int angle_horizon, int angle_vertical){
int stitch9_number;
string path = "D://repo/image/position" + to_string(_number) + "/img";
Mat screen;

if (angle_horizon % 30 == 0 || angle_horizon % 30 == 10)
{
if (angle_vertical % 30 == 0 || angle_vertical % 30 == 10)
{
stitch9_number = angle_horizon / 30 + (angle_vertical / 30 - 1)*(-12);
screen = make_screen(stitch9_number, angle_horizon % 30, angle_vertical % 30);
}
else if (angle_vertical % 30 == 20)
{
stitch9_number = angle_horizon / 30 + (angle_vertical / 30 )*(-12);
screen = make_screen(stitch9_number, angle_horizon % 30, -10);
}
}
else if (angle_horizon % 30 == 20)
{
if (angle_vertical % 30 == 0 || angle_vertical % 30 == 10)
{
stitch9_number = angle_horizon / 30 + 1 + (angle_vertical / 30 - 1)*(-12);
screen = make_screen(stitch9_number, -10, angle_vertical % 30);
}
else if (angle_vertical % 30 == 20)
{
stitch9_number = angle_horizon / 30 + 1 + (angle_vertical / 30)*(-12);
screen = make_screen(stitch9_number, -10, -10);
}
}
return screen;
}*/