extern "C"
{
    #include "lsd.h"
};
#include "VPDetection.h"
#include <iostream>
#include <fstream>
#include <experimental/filesystem>

using namespace std;
using namespace cv;
namespace fs = std::experimental::filesystem;


// LSD line segment detection
void LineDetect( cv::Mat image, double thLength, std::vector<std::vector<double> > &lines )
{
	cv::Mat grayImage;
	if ( image.channels() == 1 )
		grayImage = image;
	else
		cv::cvtColor(image, grayImage, CV_BGR2GRAY);

	image_double imageLSD = new_image_double( grayImage.cols, grayImage.rows );
	unsigned char* im_src = (unsigned char*) grayImage.data;

	int xsize = grayImage.cols;
	int ysize = grayImage.rows;
	for ( int y = 0; y < ysize; ++y )
	{
		for ( int x = 0; x < xsize; ++x )
		{
			imageLSD->data[y * xsize + x] = im_src[y * xsize + x];
		}
	}

	ntuple_list linesLSD = lsd( imageLSD );
	free_image_double( imageLSD );

	int nLines = linesLSD->size;
	int dim = linesLSD->dim;
	std::vector<double> lineTemp( 4 );
	for ( int i = 0; i < nLines; ++i )
	{
		double x1 = linesLSD->values[i * dim + 0];
		double y1 = linesLSD->values[i * dim + 1];
		double x2 = linesLSD->values[i * dim + 2];
		double y2 = linesLSD->values[i * dim + 3];

		double l = sqrt( ( x1 - x2 ) * ( x1 - x2 ) + ( y1 - y2 ) * ( y1 - y2 ) );
		if ( l > thLength )
		{
			lineTemp[0] = x1;
			lineTemp[1] = y1;
			lineTemp[2] = x2;
			lineTemp[3] = y2;

			lines.push_back( lineTemp );
		}
	}

	free_ntuple_list(linesLSD);
}

void drawClusters( cv::Mat &img, std::vector<std::vector<double> > &lines, std::vector<std::vector<int> > &clusters )
{
	int cols = img.cols;
	int rows = img.rows;

	//draw lines
	std::vector<cv::Scalar> lineColors( 3 );
	lineColors[0] = cv::Scalar( 0, 0, 255 );
	lineColors[1] = cv::Scalar( 0, 255, 0 );
	lineColors[2] = cv::Scalar( 255, 0, 0 );

	for ( int i=0; i<lines.size(); ++i )
	{
		int idx = i;
		cv::Point pt_s = cv::Point( lines[idx][0], lines[idx][1]);
		cv::Point pt_e = cv::Point( lines[idx][2], lines[idx][3]);
		cv::Point pt_m = ( pt_s + pt_e ) * 0.5;

		cv::line( img, pt_s, pt_e, cv::Scalar(0,0,0), 2, CV_AA );
	}

	for ( int i = 0; i < clusters.size(); ++i )
	{
		for ( int j = 0; j < clusters[i].size(); ++j )
		{
			int idx = clusters[i][j];

			cv::Point pt_s = cv::Point( lines[idx][0], lines[idx][1] );
			cv::Point pt_e = cv::Point( lines[idx][2], lines[idx][3] );
			cv::Point pt_m = ( pt_s + pt_e ) * 0.5;

			cv::line( img, pt_s, pt_e, lineColors[i], 2, CV_AA );
		}
	}
}

int 
main(int argc, char *argv[])
{
	string inPutImage = "./P1020171.jpg";
	string filename;
	string outputFilename;
	string dataoutputFilename;

	if (argc == 2){
		inPutImage = argv[1];
	}

	filename = fs::path(inPutImage).stem();
	outputFilename = filename + string("_vpoutput.jpg");
	dataoutputFilename = filename + string("_vpoutput.txt");

	cv::Mat image= cv::imread( inPutImage );
	if ( image.empty() )
	{
		printf( "Load image error : %s\n", inPutImage );
	}

	// LSD line segment detection
	double thLength = 30.0;
	std::vector<std::vector<double> > lines;
	LineDetect( image, thLength, lines );

	// Camera internal parameters
	cv::Point2d pp( image.cols/2, image.rows/2);        // Principle point (in pixel)
	//double f = 6.053 / 0.009;          // Focal length (in pixel)
	double f = 1.2*(std::max(image.cols,image.rows));          // Focal length (in pixel)

	// Vanishing point detection
	std::vector<cv::Point3d> vps;              // Detected vanishing points (in pixel)
	std::vector<std::vector<int> > clusters;   // Line segment clustering results of each vanishing point
	VPDetection detector;
	detector.run( lines, pp, f, vps, clusters );

	// Computing homography from vanishing points
	// Pag 6 and 8 of:
	// https://www.cs.cmu.edu/~ph/869/www/notes/criminisi.pdf
	//
	ofstream outputfile;
	outputfile.open(dataoutputFilename);

	cv::Mat homography = cv::Mat::zeros(3,3,CV_64F);
	outputfile << "total vps " << vps.size() << endl;
	for (int i = 0; i < vps.size(); ++i){
		outputfile << "v" << i << " = " << vps[i] << endl;
		homography.at<double>(0,i) = vps[i].x;
		homography.at<double>(1,i) = vps[i].y;
		homography.at<double>(2,i) = vps[i].z;
	}
	//compute third colum
	cv::Point3d l;
	l = vps[0].cross(vps[1])/cv::norm(vps[0].cross(vps[1]));
	outputfile << "l = " << l << endl;
	homography.at<double>(0,2) = l.x;
	homography.at<double>(1,2) = l.y;
	homography.at<double>(2,2) = l.z;
	outputfile << homography << endl;
	outputfile.close();
	//
	
	drawClusters( image, lines, clusters );
    imwrite(outputFilename, image);   // Read the file
	imshow("",image);
	cv::waitKey( 0 );
	return 0;
}
