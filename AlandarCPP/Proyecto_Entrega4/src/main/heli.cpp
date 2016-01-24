#include <iostream>
#include <stdio.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <queue>    //queue
#include <math.h>    //sqrt()
#include <time.h>    //time()
#include <vector>
#include "CHeli.h"
#include <unistd.h>
#include <stdlib.h>
#include "SDL/SDL.h"

using namespace std;
using namespace cv;

/*Blob Detection Definitions */
#define OBJECT 255
#define BACKGROUND 0
#define areaThreshold 300

#define mu11(i) (sumXY[i] - ( (sumX[i]*sumY[i])/((double)area[i]) ))
#define mu20(i) (sumX2[i] - ( pow(sumX[i],2)/area[i] ))
#define mu02(i) (sumY2[i] - ( pow(sumY[i],2)/area[i] ))

#define eta11(i) (mu11(i)/pow(area[i],2))
#define eta20(i) (mu20(i)/pow(area[i],2))
#define eta02(i) (mu02(i)/pow(area[i],2))

#define phi1(i) (eta20(i) + eta02(i))
#define phi2(i) (pow(eta20(i) - eta02(i),2) + 4*pow(eta11(i),2))

#define theta(i) (0.5*atan2(-2*mu11(i), mu20(i)-mu02(i)))

#define SIDE_TIME 1180000
#define SIDE_RECOVERY_TIME 198000
#define CENTER_RECOVERY_TIME 30000
#define BACK_RECOVERY_TIME 60000
#define CENTER_REVERSE_TIME 800000
#define BACK_REVERSE_TIME   1550000

/*
    This program was designed to fit any image, and is optimized for images of size 480*320.
    The program may find a maximum of 254 blobs.
    16 is a common divisor of 480 and 320.
    16² = 256.
*/
#define SECTION_NUMS 16 //Number of sections is in fact SECTION_NUMS*SECTION_NUMS.

Mat colorizeSegments(Mat& mat);
void detectBlobs(void);
void DrawGraph(void);

/* Path planning definitions */
void expandObstacles();
void calculateDistance(Point p);
bool findPath(Point p, bool right_preference);
void drawRobot();

Mat originalImage, binaryPathImage, distanceImage, pathImage;
Point origin, destination;
bool originSet, destSet;

#define PATHCOLOR 0
#define MAX_DISTANCE 0xFFFF
#define ROBOT_WIDTH 70
#define ROBOT_HEIGHT 70
#define ORIGIN_POINT Point(357, 179)
#define DESTINATION_CENTER Point(357, 407)
#define DESTINATION_BACK Point(357, 652)


/* End Path planning definitions */

int currentIndex;

double area[256] = {0},
    sumX[256] = {0},
    sumY[256] ={0},
    sumX2[256] = {0},
    sumY2[256] = {0},
    sumXY[256] = {0},
    centroidX[256] = {0},
    centroidY[256] = {0};
double  phi1[256] = {0},
        phi2[256] = {0},
        theta[256] = {0};
double 	phix,
	phiy;
/*End Blob Detection Definitions */

/*Graph Definitions*/
#define phi2x(p) ( ((p - 0.16) / ( 0.25 - 0.16)) * 480 )
#define phi2y(p) ( 310 - (p / 0.04) * 320 )
#define max(a, b) (a > b ? a : b)
Mat graphs(320, 480, CV_8UC3, Scalar(0, 0, 0));

/*End Graph Definitions*/

CRawImage *image;
CHeli *heli;
float pitch, roll, yaw, height;
int hover;
// Joystick related
SDL_Joystick* m_joystick;
bool useJoystick;
int joypadRoll, joypadPitch, joypadVerticalSpeed, joypadYaw;
bool navigatedWithJoystick, joypadTakeOff, joypadLand, joypadHover, joypadNoHover;

/* Autopilot definitions */
bool cuadro, barra, triangulo, letraT, autopilot;
#define autoRight    5000
#define autoLeft    -5000
#define autoForward    -5000
#define autoBackward    5000

#define recoverRight    -32000
#define recoverLeft     32000
#define recoverForward  32000
#define recoverBackward -32000
/* End Autopilot definitions */

/* Identification definitions */
#define tp1m 0.24087936
#define tp1v 0.0027
#define tp2m 0.0203858
#define tp2v 0.033

#define sp1m 0.16667915
#define sp1v 0.015
#define sp2m 0.00042615
#define sp2v 0.00025

#define bp1m 0.24569233
#define bp1v 0.02
#define bp2m 0.03282002
#define bp2v 0.025

#define rp1m 0.19532852
#define rp1v 0.018
#define rp2m 0.00183952
#define rp2v 0.0028

#define isLetterT(a, b)     ( (tp1m - tp1v <= a) && /*  No upper bound */ (tp2m - tp2v <= b) && (b <= tp2m + tp2v) )
#define isSquare(a, b)      ( (sp1m - sp1v <= a) && (a <= sp1m + sp1v) && /*  No lower bound */ (b <= sp2m + sp2v) )
#define isBar(a, b)         ( (bp1m - bp1v <= a) && /*  No upper bound */ (bp2m - bp2v <= b) /*  No upper bound */ )
#define isTriangle(a, b)    ( (rp1m - rp1v <= a) && (a <= rp1m + rp1v) && (rp2m - rp2v <= b) && (b <= rp2m + rp2v) )

/* End Identification Definitions */

// Convert CRawImage to Mat
void rawToMat( Mat &destImage, CRawImage* sourceImage)
{    
    uchar *pointerImage = destImage.ptr(0);
    
    for (int i = 0; i < 240*320; i++)
    {
        pointerImage[3*i] = sourceImage->data[3*i+2];
        pointerImage[3*i+1] = sourceImage->data[3*i+1];
        pointerImage[3*i+2] = sourceImage->data[3*i];
    }
}

bool mouseButtonLDown, mouseButtonRDown, pauseVideo, selectingColor;

Mat currentImage, hsvImage, binaryImage, segmentedImage, coloredImage;

int delay;

Point selectedPoint;

char keyPressed;

void hsvCallback(int event, int x, int y, int flags, void* param);

void filterImage(Mat& imgsrc, int a, int b, int c, int A, int B, int C, Mat& dest);
void updateColorLimits(unsigned char x, unsigned char y, unsigned char z);
void associateColorLimits(int* a, int* b, int* c, int* A, int* B, int* C);

struct sixIntPointers{
    int* a;
    int* b;
    int* c;
    int* A;
    int* B;
    int* C;
}colorLimits;


int main(int argc, char* argv[]){
    
    char menuOption;
    bool runProgram = true;
    bool parrotCam;
    
    if(argc < 2){
        printf("Usage: %s parrot | webcam\n", argv[0]);
        return -1;
    }
    else if(!strcmp(argv[1], "parrot"))
        parrotCam = true;
    else if(!strcmp(argv[1], "webcam"))
        parrotCam = false;
    else{
        printf("Error: parameter %s not recognized.\n", argv[1]);
        printf("Usage: %s parrot | webcam\n", argv[0]);
        return -1;
    }
        
    cvStartWindowThread();
    srand (time(NULL));
    
    if(parrotCam){
        //establishing connection with the quadcopter
        heli = new CHeli();

        //this class holds the image from the drone    
        image = new CRawImage(320,240);
    }
    
    VideoCapture camera = VideoCapture(0);
    if(!camera.isOpened()){
        cout << "CRITICAL ERROR: Failed to open camera!" << endl;
        return -1;
    }
    delay = 30;
        
    // Initial values for control    
    pitch = roll = yaw = height = 0.0;
    joypadPitch = joypadRoll = joypadYaw = joypadVerticalSpeed = 0.0;
    
    // Destination OpenCV Mat    
    currentImage = Mat(240, 320, CV_8UC3);
    
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
    useJoystick = SDL_NumJoysticks() > 0;
    if (useJoystick){
        SDL_JoystickClose(m_joystick);
        m_joystick = SDL_JoystickOpen(0);
    }
    
    
    while(runProgram){
        cout << "\nEnter the letter of the demo you would like to see." << endl
             << "In some demos, playback may be (p)aused, (u)npaused or (q)uit." << endl
             << "Press shift + S to enter filtering mode. While in filtering mode, click on the image to allow certain colors to show."  << endl
             << "Press shift + D to exit filtering mode.\nPress shift + R to reset the filter."  << endl
             << "(i) View HSV video." << endl
             << "(z) Parrot control mode." << endl
             << "(q) Quit." << endl;
        cin >> menuOption;
        pauseVideo = false;
        switch(menuOption){
            case 'i':
            case 'I':
                {
                Mat filteredImage;
                int lowH(0), lowS(0), lowV(0), highH(179), highS(255), highV(255);
                
                lowH = 179;
                        lowS = lowV = 255;
                        highH = highS = highV = 0;
                
                associateColorLimits(&lowH, &lowS, &lowV, &highH, &highS, &highV);
                
                cvNamedWindow("Original colorspace (BGR)");
                cvNamedWindow("Filtered image");
                cvNamedWindow("Colored image");
                
                cvNamedWindow("Filtering controls", CV_WINDOW_AUTOSIZE);
                
                cvCreateTrackbar("h", "Filtering controls", &lowH, 179); //Hue (0 - 179)
                cvCreateTrackbar("H", "Filtering controls", &highH, 179);

                cvCreateTrackbar("s", "Filtering controls", &lowS, 255); //Saturation (0 - 255)
                cvCreateTrackbar("S", "Filtering controls", &highS, 255);

                cvCreateTrackbar("v", "Filtering controls", &lowV, 255); //Value (0 - 255)
                cvCreateTrackbar("V", "Filtering controls", &highV, 255);
                
                setMouseCallback("Original colorspace (BGR)", hsvCallback);
                setMouseCallback("Filtered image", hsvCallback);
                
                keyPressed = waitKey(delay)%256;
                while(keyPressed != 'q'){
                    keyPressed = waitKey(delay)%256;
                    if(keyPressed == 'p')
                        pauseVideo = true;
                    else if(keyPressed == 'u')
                        pauseVideo = false;
                    else if(keyPressed == 'S'){ // Ctrl + S
                        cout << "Selecting color to filter." << endl;
                        lowH = 179;
                        lowS = lowV = 255;
                        highH = highS = highV = 0;
                        selectingColor = true;
                    }
                    else if(keyPressed == 'D'){ // Ctrl + D
                        cout << "Done selecting color to filter." << endl;
                        selectingColor = false;
                    }
                    else if(keyPressed == 'R'){ // Ctrl + R
                        cout << "All color limits restored to defaults." << endl;
                        selectingColor = false;
                        lowH = lowS = lowV = 0;
                        highH = 179;
                        highS = highV = 255;
                    }
                    
                    if(selectingColor){
                        cvSetTrackbarPos("h", "Filtering controls", lowH);
                        cvSetTrackbarPos("H", "Filtering controls", highH);

                        cvSetTrackbarPos("s", "Filtering controls", lowS);
                        cvSetTrackbarPos("S", "Filtering controls", highS);

                        cvSetTrackbarPos("v", "Filtering controls", lowV);
                        cvSetTrackbarPos("V", "Filtering controls", highV);
                    }
                        
                    if(!pauseVideo){
                        if(parrotCam){
                            heli->renewImage(image);
                            rawToMat(currentImage, image);
                        }
                        else{
                            camera.read(currentImage);
                        }
                    }
                    
                    cvtColor(currentImage, hsvImage, CV_BGR2HSV);
                    
                    filteredImage = currentImage.clone();
                    filterImage(hsvImage, lowH, lowS, lowV, highH, highS, highV, filteredImage);
                    
                    detectBlobs();
                    
                    imshow("Original colorspace (BGR)", currentImage);
                    imshow("Filtered image", filteredImage);
                    imshow("Colored image", coloredImage);
                }
                setMouseCallback("New colorspace (HSV)", NULL);
                setMouseCallback("Original colorspace (BGR)", NULL);
                setMouseCallback("Filtered image", NULL);
                }
                break;
            case 'z':
            case 'Z':
                {
                Mat filteredImage;
                int lowH(96), lowS(130), lowV(60), highH(179), highS(255), highV(185);
                
                associateColorLimits(&lowH, &lowS, &lowV, &highH, &highS, &highV);
                
                cvNamedWindow("ParrotCam");
                cvNamedWindow("Filtered image");
                setMouseCallback("ParrotCam", hsvCallback);
                setMouseCallback("Filtered image", hsvCallback);
	            cvNamedWindow("Path");
                
                cvNamedWindow("Filtering controls", CV_WINDOW_AUTOSIZE);
                cvNamedWindow("Colored image");
        		cvNamedWindow("Image moments");
                
                cvCreateTrackbar("h", "Filtering controls", &lowH, 179); //Hue (0 - 179)
                cvCreateTrackbar("H", "Filtering controls", &highH, 179);

                cvCreateTrackbar("s", "Filtering controls", &lowS, 255); //Saturation (0 - 255)
                cvCreateTrackbar("S", "Filtering controls", &highS, 255);

                cvCreateTrackbar("v", "Filtering controls", &lowV, 255); //Value (0 - 255)
                cvCreateTrackbar("V", "Filtering controls", &highV, 255);
                
                bool stop = false;
                
                
                originalImage = imread("bin/Exercise1.png", CV_LOAD_IMAGE_GRAYSCALE);
                //imshow("Path", originalImage);
                //waitKey(0);
	            pathImage = originalImage.clone();
	            queue<Point> processing;
	            
	            Point p;
            	
	            originSet = destSet = false;
	            
	            threshold(originalImage, binaryPathImage, 200, 255, 0);
	            
	            expandObstacles();
	
	            binaryPathImage.convertTo(distanceImage, CV_16U, 255); //This conversion is needed to store distances greater than 255, which wouldn't fit in an 8-bit char.
                
                origin = ORIGIN_POINT;
                //Calculating distances
                distanceImage = Scalar(MAX_DISTANCE);
                calculateDistance(origin);
                //Distance calculated
                originSet = true;
                drawRobot();
                imshow("Path", pathImage);
                while (!stop)
                {

                    // Clear the console
                    printf("\033[2J\033[1;1H");
                    cuadro = barra = triangulo = letraT = false;

                    if (useJoystick)
                    {
                        SDL_Event event;
                        SDL_PollEvent(&event);
                            
                        /* //PS4
                        joypadRoll = SDL_JoystickGetAxis(m_joystick, 2);
                        joypadPitch = SDL_JoystickGetAxis(m_joystick, 5);
                        joypadVerticalSpeed = SDL_JoystickGetAxis(m_joystick, 1);
                        joypadYaw = SDL_JoystickGetAxis(m_joystick, 0);
                        joypadTakeOff = SDL_JoystickGetButton(m_joystick, 1);
                        joypadLand = SDL_JoystickGetButton(m_joystick, 2);
                        joypadHover = SDL_JoystickGetButton(m_joystick, 0);
                        joypadNoHover = SDL_JoystickGetButton(m_joystick, 3);
                        autopilot = SDL_JoystickGetButton(m_joystick, 4);
                        */
                        
                        //Logitech
                        joypadRoll = SDL_JoystickGetAxis(m_joystick, 2);
                        joypadPitch = SDL_JoystickGetAxis(m_joystick, 3);
                        joypadVerticalSpeed = SDL_JoystickGetAxis(m_joystick, 1);
                        joypadYaw = SDL_JoystickGetAxis(m_joystick, 0);
                        joypadTakeOff = SDL_JoystickGetButton(m_joystick, 1);
                        joypadLand = SDL_JoystickGetButton(m_joystick, 2);
                        joypadHover = SDL_JoystickGetButton(m_joystick, 0);
                        joypadNoHover = SDL_JoystickGetButton(m_joystick, 3);
                        autopilot = SDL_JoystickGetButton(m_joystick, 9);
                        
                    }

                    keyPressed = waitKey(5)%256;
                    switch (keyPressed) {
                        case 'a': yaw = -20000.0; break;
                        case 'd': yaw = 20000.0; break;
                        case 'w': height = -20000.0; break;
                        case 's': height = 20000.0; break;
                        case 'q': heli->takeoff(); break;
                        case 'e': heli->land(); break;
                        case 'z': heli->switchCamera(0); break;
                        case 'x': heli->switchCamera(1); break;
                        case 'c': heli->switchCamera(2); break;
                        case 'v': heli->switchCamera(3); break;
                        case 'j': roll = -20000.0; break;
                        case 'l': roll = 20000.0; break;
                        case 'i': pitch = -20000.0; break;
                        case 'k': pitch = 20000.0; break;
                        case 'h': hover = (hover + 1) % 2; break;
                        case  27: stop = true; break;
                        case 'p': pauseVideo = true; break;
                        case 'u': pauseVideo = false; break;
                        case 'S': 
                                  lowH = 179;
                                  lowS = lowV = 255;
                                  highH = highS = highV = 0;
                                  selectingColor = true;
                                  break;
                        case 'D': selectingColor = false; break;
                        case 'R':
                                  selectingColor = false;
                                  lowH = lowS = lowV = 0;
                                  highH = 179;
                                  highS = highV = 255;
                                  break;
                        case 'f':
                            autopilot = true;
                            break;
                        default: pitch = roll = yaw = height = 0.0;
                    }
                    
                    if(selectingColor){
                        cvSetTrackbarPos("h", "Filtering controls", lowH);
                        cvSetTrackbarPos("H", "Filtering controls", highH);

                        cvSetTrackbarPos("s", "Filtering controls", lowS);
                        cvSetTrackbarPos("S", "Filtering controls", highS);

                        cvSetTrackbarPos("v", "Filtering controls", lowV);
                        cvSetTrackbarPos("V", "Filtering controls", highV);
                    }

                    if (joypadTakeOff) {
                        heli->takeoff();
                    }
                    if (joypadLand) {
                        heli->land();
                    }
                    if(joypadHover){
                        hover = 1;
                    }
                    if(joypadNoHover){
                        hover = 0;
                    }
                    
                    
                    //image is captured
                    if(!pauseVideo){
                        heli->renewImage(image);
                        rawToMat(currentImage, image);
                    }
                    
                                cvtColor(currentImage, hsvImage, CV_BGR2HSV);
                    
                    filteredImage = currentImage.clone();
                    filterImage(hsvImage, lowH, lowS, lowV, highH, highS, highV, filteredImage);
                    
                    //Blobs are detected and statistical data for them is created
                    detectBlobs();

                    if(autopilot){
                        if(barra){
                            //Center
                            if(cuadro){
                                //Right
                                destination = DESTINATION_CENTER;
		                        destSet = true;
		                        
                                pathImage = originalImage.clone();
                                findPath(destination, true);
                                drawRobot();
                                imshow("Path", pathImage);
                                
                                roll = autoRight;
                                pitch = 0;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(SIDE_TIME);
                                heli->setAngles(pitch, recoverRight, yaw, height, hover);
                                usleep(SIDE_RECOVERY_TIME);
                                
                                roll = 0;
                                pitch = autoBackward;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(CENTER_REVERSE_TIME);
                                heli->setAngles(recoverBackward, roll, yaw, height, hover);
                                usleep(CENTER_RECOVERY_TIME);
                                
                                pitch = 0;
                                roll = autoLeft;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(SIDE_TIME);
                                heli->setAngles(pitch, recoverLeft, yaw, height, hover);
                                usleep(SIDE_RECOVERY_TIME);
                                
                                roll = 0;
                                
                                heli->setAngles(0, 0, 0, 0, hover);
                                
                            }
                            else if(triangulo){
                                //Left
                                destination = DESTINATION_CENTER;
		                        destSet = true;
		                        
                                pathImage = originalImage.clone();
                                findPath(destination, false);
                                drawRobot();
                                imshow("Path", pathImage);
                                
                                pitch = 0;
                                roll = autoLeft;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(SIDE_TIME);
                                heli->setAngles(pitch, recoverLeft, yaw, height, hover);
                                usleep(SIDE_RECOVERY_TIME);
                                
                                roll = 0;
                                pitch = autoBackward;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(CENTER_REVERSE_TIME);
                                heli->setAngles(recoverBackward, roll, yaw, height, hover);
                                usleep(CENTER_RECOVERY_TIME);
                                
                                roll = autoRight;
                                pitch = 0;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(SIDE_TIME);
                                heli->setAngles(pitch, recoverRight, yaw, height, hover);
                                usleep(SIDE_RECOVERY_TIME);
                                
                                roll = 0;
                                
                                heli->setAngles(0, 0, 0, 0, hover);
                                
                            }
                        }
                        else if(letraT){
                            //Back
                            if(cuadro){
                                //Right
                                destination = DESTINATION_BACK;
		                        destSet = true;
		                        
                                pathImage = originalImage.clone();
                                findPath(destination, true);
                                drawRobot();
                                imshow("Path", pathImage);
                                
                                roll = autoRight;
                                pitch = 0;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(SIDE_TIME);
                                heli->setAngles(pitch, recoverRight, yaw, height, hover);
                                usleep(SIDE_RECOVERY_TIME);
                                
                                roll = 0;
                                pitch = autoBackward;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(BACK_REVERSE_TIME);
                                heli->setAngles(recoverBackward, roll, yaw, height, hover);
                                usleep(BACK_RECOVERY_TIME);
                                
                                roll = autoLeft;
                                pitch = 0;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(SIDE_TIME);
                                heli->setAngles(pitch, recoverLeft, yaw, height, hover);
                                usleep(SIDE_RECOVERY_TIME);
                                
                                roll = 0;
                                
                                heli->setAngles(0, 0, 0, 0, hover);
                                
                            }
                            else if(triangulo){
                                //Left
                                destination = DESTINATION_BACK;
		                        destSet = true;
		                        
                                pathImage = originalImage.clone();
                                findPath(destination, false);
                                drawRobot();
                                imshow("Path", pathImage);
                                
                                pitch = 0;
                                roll = autoLeft;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(SIDE_TIME);
                                heli->setAngles(pitch, recoverLeft, yaw, height, hover);
                                usleep(SIDE_RECOVERY_TIME);
                                
                                roll = 0;
                                pitch = autoBackward;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(BACK_REVERSE_TIME*1.1);
                                heli->setAngles(recoverBackward, roll, yaw, height, hover);
                                usleep(BACK_RECOVERY_TIME);
                                
                                roll = autoRight;
                                pitch = 0;
                                heli->setAngles(pitch, roll, yaw, height, hover);
                                usleep(SIDE_TIME);
                                heli->setAngles(pitch, recoverRight, yaw, height, hover);
                                usleep(SIDE_RECOVERY_TIME);
                                
                                roll = 0;
                                
                                heli->setAngles(0, 0, 0, 0, hover);
                                
                            }
                        }
                        
                        autopilot = false;
			            heli->land();
                    }

                    //setting the drone angles
                    if (joypadRoll != 0 || joypadPitch != 0 || joypadVerticalSpeed != 0 || joypadYaw != 0)
                    {
                        heli->setAngles(joypadPitch, joypadRoll, joypadYaw, joypadVerticalSpeed, hover);
                        navigatedWithJoystick = true;
                    }
                    else
                    {
                        heli->setAngles(pitch, roll, yaw, height, hover);
                        navigatedWithJoystick = false;
                    }
                    DrawGraph();
                    // Copy to OpenCV Mat
                    imshow("ParrotCam", currentImage);
                    imshow("Filtered image", filteredImage);
                    imshow("Colored image", coloredImage);

                    usleep(15000);
                }

                heli->land();
                SDL_JoystickClose(m_joystick);
                setMouseCallback("New colorspace (HSV)", NULL);
                setMouseCallback("Original colorspace (BGR)", NULL);
                setMouseCallback("Filtered image", NULL);
                }
                break;
            case 'q':
            case 'Q':
                runProgram = false;
                break;
            default:
                cout << "Option (" << menuOption << ") is invalid. Please enter a valid option." << endl << endl;
        }
        
        destroyAllWindows();        
    }
    camera.release();
    if(parrotCam){
        delete heli;
        delete image;
    }
    return 0;
}

void detectBlobs(){
    srand (time(NULL));
    queue<Point> processing;
    
    segmentedImage = binaryImage.clone();    //Make segmentedImage same size and type as binaryImage
    segmentedImage = Scalar(BACKGROUND);    //Set segmentedImage to the background value
    
    const int sectionCols = binaryImage.cols/SECTION_NUMS;
    const int sectionRows = binaryImage.rows/SECTION_NUMS;
    const int maxTries = (int)sqrt(sectionRows * sectionCols)/2; //Arbitrarily defined number of maximum tries.
    currentIndex = 0;
    Point p, p2;

	memset(area, 0, sizeof(area));
	memset(sumX, 0, sizeof(sumX));
	memset(sumY, 0, sizeof(sumY));
	memset(sumX2, 0, sizeof(sumX2));
	memset(sumY2, 0, sizeof(sumY2));
	memset(sumXY, 0, sizeof(sumXY));
	memset(centroidX, 0, sizeof(centroidX));
	memset(centroidY, 0, sizeof(centroidY));
	memset(phi1, 0, sizeof(phi2));
	memset(phi2, 0, sizeof(phi2));
	memset(theta, 0, sizeof(theta));
    
    for(int j=0; j<SECTION_NUMS; j++){ //Iterate through columns
        for(int i=0; i<SECTION_NUMS; i++){ //Iterate through rows
            for(int k=0; k<maxTries; k++){ //Set a maximum number of seeds per section
                double y = rand() % (sectionRows-1) + (sectionRows*j) + 1;
                double x = rand() % (sectionCols-1) + (sectionCols*i) + 1;
                
                p.y = y, p.x = x;
                
                if(binaryImage.at<uchar>(p) == OBJECT && segmentedImage.at<uchar>(p) == BACKGROUND){
                    processing.push(p);
                    binaryImage.at<uchar>(p) = BACKGROUND;
                    
                    currentIndex++;
                    if(currentIndex>255){
                        cout << "Error! Too many blobs found!" << endl;
                        break;    //Error. Too many blobs found!
                    }
                    while(!processing.empty()){
                        p = processing.front();
                        processing.pop();
                            /*  Begin region characterization   */
                            area[currentIndex] ++;
                            sumX[currentIndex] += p.x;
                            sumY[currentIndex] += p.y;
                            sumX2[currentIndex]+= pow(p.x,2);
                            sumY2[currentIndex]+= pow(p.y,2);
                            sumXY[currentIndex]+= p.x * p.y;
                            /*  End region characterization   */
                            
                        segmentedImage.at<uchar>(p) = currentIndex;
                        
                        if(p.x+1 < binaryImage.cols){
                            p2.x = p.x+1, p2.y = p.y;
                            if(binaryImage.at<uchar>(p2) == OBJECT){
                                processing.push(p2);
                                binaryImage.at<uchar>(p2) = BACKGROUND;
                            }
                        }
                        
                        if(p.x-1 >= 0){
                            p2.x = p.x-1, p2.y = p.y;
                            if(binaryImage.at<uchar>(p2) == OBJECT){
                                processing.push(p2);
                                binaryImage.at<uchar>(p2) = BACKGROUND;
                            }
                        }
                        
                        if(p.y+1 < binaryImage.rows){
                            p2.x = p.x, p2.y = p.y+1;
                            if(binaryImage.at<uchar>(p2) == OBJECT){
                                processing.push(p2);
                                binaryImage.at<uchar>(p2) = BACKGROUND;
                            }
                        }
                        
                        if(p.y-1 >=0){    
                            p2.x = p.x, p2.y = p.y-1;
                            if(binaryImage.at<uchar>(p2) == OBJECT){
                                processing.push(p2);
                                binaryImage.at<uchar>(p2) = BACKGROUND;
                            }
                        }
                    }
                    break; //If seed was found, go to next section.
                }
            }
        }
    }    

    coloredImage = colorizeSegments(segmentedImage);
    
    printf("===================== Parrot Basic Example ======ESC to quit====\n\n");
    fprintf(stdout, "Angles  : %.2lf %.2lf %.2lf \n", helidata.phi, helidata.psi, helidata.theta);
    fprintf(stdout, "Speeds  : %.2lf %.2lf %.2lf \n", helidata.vx, helidata.vy, helidata.vz);
    fprintf(stdout, "Battery : %.0lf \n", helidata.battery);
    fprintf(stdout, "Hover   : %d \n", hover);
    fprintf(stdout, "Joypad  : %d \n", useJoystick ? 1 : 0);
    fprintf(stdout, "  Roll    : %d \n", joypadRoll);
    fprintf(stdout, "  Pitch   : %d \n", joypadPitch);
    fprintf(stdout, "  Yaw     : %d \n", joypadYaw);
    fprintf(stdout, "  V.S.    : %d \n", joypadVerticalSpeed);
    fprintf(stdout, "  TakeOff : %d \n", joypadTakeOff);
    fprintf(stdout, "  Land    : %d \n", joypadLand);
    fprintf(stdout, "Navigating with Joystick: %d \n", navigatedWithJoystick ? 1 : 0);

    for(int i=1; i<=currentIndex; i++){
        phi1[i] = phi1(i);
        phi2[i] = phi2(i);
        centroidX[i] = sumX[i] / area[i];
        centroidY[i] = sumY[i] / area[i];
        theta[i] = theta(i);
	
	
        if(area[i] <= areaThreshold)
            continue;

        if ( isSquare(phi1[i], phi2[i]) ){
            printf("Cuadro\n");
            cuadro = true;
        }
        else if( isBar(phi1[i], phi2[i]) ){
            printf("Barra\n");
            barra = true;
        }
        else if( isTriangle(phi1[i], phi2[i]) ){
            printf("Triangulo\n");
            triangulo = true;
        }
        else if( isLetterT(phi1[i], phi2[i]) ){
            printf("Letra 'T'\n");
            letraT = true;
        }
        else{
            printf("Objeto desconocido\n");
        }
        
        line(coloredImage, Point(centroidX[i] - 20*cos(theta[i]), centroidY[i] + 20*sin(theta[i])), Point(centroidX[i] + 20*cos(theta[i]),
        centroidY[i] - 20*sin(theta[i])), Scalar(255, 255, 255), 2);
        line(coloredImage, Point(centroidX[i] - 5*cos(theta[i]), centroidY[i] - 5*sin(theta[i])), Point(centroidX[i] + 5*cos(theta[i]), 
        centroidY[i] + 5*sin(theta[i])), Scalar(255, 255, 255), 2);
        printf("%3s %6s %11s %11s %11s %11s %11s %9s %9s %9s %8s %9s %5s %5s %6s\n", "n", "area", "m10", "m01", "m20", "m02", "m11", "mu11",
          "mu20", "mu02", "phi1", "phi2", "x", "y", "theta");
        printf("%3i %6g %11.3g %11.3g %11.3g %11g %11g %9.2g %9.2g %9.2g %8.6g %9.6g %5.3g %5.3g %6.4g\n", i, area[i], sumX[i], sumY[i], 
        sumX2[i], sumY2[i], sumXY[i], mu11(i), mu20(i), mu02(i), phi1[i], phi2[i], centroidX[i], centroidY[i], theta[i]);
        
        /*  Used in creating the phi1 vs phi2 tables
            printf("%f\t%f\n", phi1[i], phi2[i]); */

    }


}

void DrawGraph(void){
	graphs = Scalar(0, 0, 0);    

    /* Plot ranges */
	/*circle(Mat& img, Point center, 		int radius, 		  const Scalar& color)*/
        circle(graphs, Point(phi2x(tp1m), phi2y(tp2m/10)), (int) ( max(tp1v, tp2v) * 1000), Scalar(255, 255, 255));//Leter T
        circle(graphs, Point(phi2x(sp1m), phi2y(sp2m)), (int) ( max(sp1v, sp2v) * 1000), Scalar(255, 255, 255));//Square
        circle(graphs, Point(phi2x(bp1m), phi2y(bp2m)), (int) ( max(bp1v, bp2v) * 1000), Scalar(255, 255, 255));//Bar
        circle(graphs, Point(phi2x(rp1m), phi2y(rp2m)), (int) ( max(rp1v, rp2v) * 1000), Scalar(255, 255, 255));//Triangle
    /* End plot ranges */

    /* Lines */
	line(graphs, Point(phi2x(0.1857175),0), Point(phi2x(0.1857175),10*100), Scalar(255, 255, 255), 1, 8, 0);
	line(graphs, Point(phi2x(0.214576),0), Point(phi2x(0.214576),10*100), Scalar(255, 255, 255), 1, 8, 0);
	line(graphs, Point(0,phi2y(0.0145925)), Point(10*100,phi2y(0.0145925)), Scalar(255, 255, 255), 1, 8, 0);	
    /*End Lines*/

    /* Text*/
	putText(graphs, string("Bar"), Point(phi2x(bp1m)-10,phi2y(bp2m)), CV_FONT_HERSHEY_SIMPLEX, 0.3, Scalar(255, 255, 255), 1, 8);
	putText(graphs, string("Letter T"), Point(phi2x(tp1m)-15,phi2y(tp2m/10)), CV_FONT_HERSHEY_SIMPLEX, 0.3, Scalar(255, 255, 255), 1, 8);
	putText(graphs, string("Square"), Point(phi2x(sp1m)-15,phi2y(sp2m)), CV_FONT_HERSHEY_SIMPLEX, 0.3, Scalar(255, 255, 255), 1, 8);
	putText(graphs, string("Triangle"), Point(phi2x(rp1m)-15,phi2y(rp2m)), CV_FONT_HERSHEY_SIMPLEX, 0.3, Scalar(255, 255, 255), 1, 8);	
	putText(graphs, string("Unknown Object"), Point(phi2x(sp1m)-15,phi2y(bp2m)), CV_FONT_HERSHEY_SIMPLEX, 0.3, Scalar(255, 255, 255), 1, 8);
	putText(graphs, string("Unknown Object"), Point(phi2x(rp1m)-15,phi2y(bp2m)), CV_FONT_HERSHEY_SIMPLEX, 0.3, Scalar(255, 255, 255), 1, 8);
    /* End Text*/
    
	for(int i =1; i<=currentIndex; i++){
	phix=phi1[i];
	phiy=phi2[i];
	printf("%f\t%f\n",phi2x(phix),phi2y(phiy));
	/* Plot phi values */
	if(phi2x(phix) > 480)
		circle(graphs, Point(480, phi2y(phiy)), 2, Scalar(0, 255, 0), 2);
	if(phi2y(phiy) > 320)
		circle(graphs, Point(phi2x(phix), 320), 2, Scalar(0, 255, 0), 2);
        circle(graphs, Point(phi2x(phix), phi2y(phiy)), 2, Scalar(0, 255, 0), 2);
	/*End Plot phi values*/
    
    /* End plot phi values */
	}
    
    imshow("Image moments", graphs);
}

void hsvCallback(int event, int x, int y, int flags, void* param){
    switch(event){
        case CV_EVENT_LBUTTONDOWN:{
            Point3_<uchar>* p = hsvImage.ptr<Point3_<uchar> >(y,x);
            mouseButtonLDown = true;
            selectedPoint = Point(x, y);
            printf("Mouse (X,Y): (%d,%d)\nHSV(H,S,V): (%u,%u,%u)\n", x, y, p->x, p->y, p->z);
            
            if(selectingColor){
                updateColorLimits(p->x, p->y, p->z);
            }
            
            }
            break;
        case CV_EVENT_LBUTTONUP:
            mouseButtonLDown = false;
            break;
        case CV_EVENT_MOUSEMOVE:
            if(mouseButtonLDown){
                Point3_<uchar>* p = hsvImage.ptr<Point3_<uchar> >(y,x);
                selectedPoint = Point(x, y);
                printf("Mouse (X,Y): (%d,%d)\nHSV(H,S,V): (%u,%u,%u)\n", x, y, p->x, p->y, p->z);
                
                if(selectingColor){
                    updateColorLimits(p->x, p->y, p->z);
                }
            }
            break;
        default:
            break;
    }
}

void filterImage(Mat& imgsrc, int a, int b, int c, int A, int B, int C, Mat& dest){
    inRange(imgsrc, Scalar(a, b, c), Scalar(A, B, C), binaryImage);
    
    //morphological opening (remove small objects from the foreground)
    erode(binaryImage, binaryImage, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) );
    dilate( binaryImage, binaryImage, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) ); 

    //morphological closing (fill small holes in the foreground)
    dilate( binaryImage, binaryImage, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) ); 
    erode( binaryImage, binaryImage, getStructuringElement(MORPH_ELLIPSE, Size(5, 5)) );
        
    Mat temp[] = {binaryImage.clone(), binaryImage.clone(), binaryImage.clone()};
    Mat mask;
    merge(temp, 3, mask);
    bitwise_and(dest, mask, dest);
}

void updateColorLimits(unsigned char x, unsigned char y, unsigned char z){
    *(colorLimits.a) = min((unsigned char)(*(colorLimits.a)), x);
    *(colorLimits.b) = min((unsigned char)(*(colorLimits.b)), y);
    *(colorLimits.c) = min((unsigned char)(*(colorLimits.c)), z);
    *(colorLimits.A) = max((unsigned char)(*(colorLimits.A)), x);
    *(colorLimits.B) = max((unsigned char)(*(colorLimits.B)), y);
    *(colorLimits.C) = max((unsigned char)(*(colorLimits.C)), z);
}

void associateColorLimits(int* a, int* b, int* c, int* A, int* B, int* C){
    colorLimits.a = a;
    colorLimits.b = b;
    colorLimits.c = c;
    colorLimits.A = A;
    colorLimits.B = B;
    colorLimits.C = C;
}

/*
    This method is meant to be called only once, as it is inefficient.
    If more than one image's segments are to be colorized, the LUT color
    table should not be regenerated for each.
    
    WARNING: IT DOES NOT GUARANTEE DIFFERENT RGB COLORS.
*/
Mat colorizeSegments(Mat& mat){
    Mat B, G, R, BGR;
    vector<Mat> v;
    uchar pb[] = {0, 148,20,212,69,239,150,80,232,198,11,62,189,218,156,220,57,136,231,133,116,147,134,200,154,211,66,20,17,112,167,109,5,187,65,74,170,215,155,147,158,166,209,91,128,109,55,185,245,30,62,105,177,197,49,75,152,115,96,170,228,7,23,233,194,88,51,108,47,206,255,205,116,208,40,245,61,95,174,50,126,237,155,47,178,205,123,74,64,219,244,36,226,11,13,164,99,65,16,147,15,16,96,132,224,137,121,30,232,39,80,102,20,236,150,198,185,17,17,249,236,5,30,206,17,43,114,116,108,130,7,124,146,104,0,115,241,121,145,217,160,225,64,181,205,214,123,134,231,140,128,211,146,158,161,163,201,19,23,54,149,31,178,40,135,178,155,120,43,44,81,203,13,145,128,219,103,252,97,78,136,225,33,26,127,194,189,73,213,213,127,107,244,49,147,123,227,46,243,14,90,68,217,103,214,90,66,61,86,164,140,222,133,173,249,5,112,182,78,69,139,205,176,127,254,67,250,225,113,237,239,203,50,200,51,8,34,117,69,120,25,209,87,159,127,80,164,239,6,242,52,146,191,229,17,0},
          pg[] = {0, 106,230,199,79,61,168,200,170,176,14,166,77,212,39,48,146,99,72,221,69,118,142,89,222,21,224,134,250,86,196,148,192,170,91,15,231,3,216,145,180,230,56,1,187,95,49,77,194,121,43,7,239,185,96,205,207,65,83,201,151,23,93,87,193,185,103,168,188,63,58,112,37,114,113,224,209,162,46,147,27,89,154,10,18,251,216,225,60,43,171,211,67,8,42,4,193,145,173,126,208,231,238,246,89,96,214,42,2,4,189,30,93,87,40,112,82,0,81,142,44,252,97,111,5,140,115,198,29,32,68,238,7,51,228,96,147,186,138,149,191,71,179,28,159,220,140,241,220,222,128,8,218,225,119,223,109,235,166,139,11,234,121,19,29,93,115,176,23,254,70,214,69,249,243,228,213,127,214,178,93,86,186,56,55,50,23,165,29,189,48,40,168,169,59,197,6,175,118,29,173,188,244,242,181,231,215,139,102,173,61,196,3,247,252,58,41,19,223,70,209,15,111,121,184,170,62,190,89,180,220,6,112,208,249,38,183,208,177,29,125,238,225,128,229,221,186,15,241,154,85,194,169,196,59,0},
          pr[] = {0, 55,74,144,225,203,68,124,120,161,198,105,189,159,136,132,132,207,17,57,99,181,89,24,78,183,36,248,228,247,225,203,46,43,91,16,247,159,140,111,64,82,216,253,241,97,129,118,48,147,175,148,72,8,172,151,192,208,143,164,199,113,111,245,156,202,5,147,106,145,2,170,227,219,168,213,60,41,75,108,188,250,0,5,2,172,156,194,124,43,102,67,156,213,57,57,160,62,204,10,208,207,180,179,170,92,136,230,134,211,82,66,205,83,71,208,255,227,146,124,15,249,191,171,206,248,228,110,55,177,120,7,128,45,186,42,137,67,16,15,22,98,82,228,181,153,180,181,125,70,49,140,63,240,55,14,233,28,124,32,205,245,39,77,34,225,119,171,36,135,187,59,233,13,31,159,166,211,84,35,25,133,175,89,117,231,103,94,3,227,126,208,216,165,29,250,135,148,166,171,27,97,230,4,110,5,163,20,216,247,56,242,124,231,75,242,206,178,80,209,149,207,161,110,116,190,104,251,82,14,167,109,111,141,114,221,147,21,242,107,13,42,93,137,17,168,123,224,90,204,177,240,155,83,94,0};
    const Mat tableB(1, 256, CV_8U, pb), tableG(1, 256, CV_8U, pg), tableR(1, 256, CV_8U, pr);

    LUT(mat, tableB, B);
    LUT(mat, tableG, G);
    LUT(mat, tableR, R);
    v.push_back(B);
    v.push_back(G);
    v.push_back(R);
    merge(v, BGR);
    return BGR;
}

//This method should either erode or dilate, depending on whether objects are black or white.
void expandObstacles(){
    erode(binaryPathImage, binaryPathImage, getStructuringElement(MORPH_ELLIPSE, Size(ROBOT_WIDTH, ROBOT_HEIGHT)));
}

void calculateDistance(Point p){
    Mat temp = Mat::ones(binaryPathImage.size(), binaryPathImage.type())*255 - binaryPathImage;
    Point p2;
    int d;
    queue<int> distance;
    queue<Point> pendingPoints;
    
    distance.push(0);
    pendingPoints.push(p);
    
    while(!pendingPoints.empty()){
        p = pendingPoints.front();
        pendingPoints.pop();
        d = distance.front();
        distance.pop();
        temp.at<uchar>(p) = OBJECT;
        distanceImage.at<uint16_t>(p) = d << 5;
        
        if(p.x + 1 < temp.cols){            
		    p2.x = p.x + 1, p2.y = p.y;
            if(temp.at<uchar>(p2) == BACKGROUND){
                pendingPoints.push(p2);
                distance.push(d + 1);
                temp.at<uchar>(p2) = OBJECT;
            }
        }
        
        if(p.x - 1 >= 0){
		    p2.x = p.x - 1, p2.y = p.y;
            if(temp.at<uchar>(p2) == BACKGROUND){
                pendingPoints.push(p2);
                distance.push(d + 1);
                temp.at<uchar>(p2) = OBJECT;
            }
        }
        
        if(p.y + 1 < temp.rows){
		    p2.x = p.x, p2.y = p.y + 1;
            if(temp.at<uchar>(p2) == BACKGROUND){
                pendingPoints.push(p2);
                distance.push(d + 1);
                temp.at<uchar>(p2) = OBJECT;
            }
        }
                
        if(p.y - 1 >= 0){
		    p2.x = p.x, p2.y = p.y - 1;
            if(temp.at<uchar>(p2) == BACKGROUND){
                pendingPoints.push(p2);
                distance.push(d + 1);
                temp.at<uchar>(p2) = OBJECT;
            }
        }
    }
}

bool findPath(Point p, bool right_preference){
    Point p2;
    int currentDistance, direction;
    bool infiniteLoop = false;
    pathImage.at<uchar>(p) = PATHCOLOR;
    currentDistance = distanceImage.at<uint16_t>(p);
    direction = 0;
    
    while(p.x != origin.x || p.y != origin.y){
        switch(direction){
            case0:
            case 0:
                if(right_preference){
                    //Right
                    if (p.x + 1 < pathImage.cols && currentDistance > distanceImage.at<uint16_t>(Point(p.x + 1, p.y))){
                        p = Point(p.x + 1, p.y);
                        infiniteLoop = false;
                        break;
                    }
                }
                else{
                    //Left
                    if(p.x - 1 >= 0 && currentDistance > distanceImage.at<uint16_t>(Point(p.x - 1, p.y))){
                        p = Point(p.x - 1, p.y);
                        infiniteLoop = false;
                        break;
                    }
                }
                direction = 1;
            case 1:
                //Up
                if(p.y - 1 >= 0  && currentDistance > distanceImage.at<uint16_t>(Point(p.x, p.y - 1))){
                    p = Point(p.x, p.y - 1);
                    infiniteLoop = false;
                    break;
                }
                direction = 2;
            case 2:
                if(!right_preference){
                    //Right
                    if (p.x + 1 < pathImage.cols && currentDistance > distanceImage.at<uint16_t>(Point(p.x + 1, p.y))){
                        p = Point(p.x + 1, p.y);
                        infiniteLoop = false;
                        break;
                    }
                }
                else{
                    //Left
                    if(p.x - 1 >= 0 && currentDistance > distanceImage.at<uint16_t>(Point(p.x - 1, p.y))){
                        p = Point(p.x - 1, p.y);
                        infiniteLoop = false;
                        break;
                    }
                }
                direction = 3;
            case 3:
                //Down
                if(p.y + 1 < pathImage.rows && currentDistance > distanceImage.at<uint16_t>(Point(p.x, p.y + 1))){
                    p = Point(p.x, p.y + 1);
                    infiniteLoop = false;
                    break;
                }
                direction = 1;
                if(infiniteLoop){
                    printf("No solution found!\n \
                        The robot crashes on its way to the specified destination!\n");
                    return false;
                }   
                else{
                    infiniteLoop = true;
                    goto case0;
                }
        }
        currentDistance = distanceImage.at<uint16_t>(p);
        pathImage.at<uchar>(p) = PATHCOLOR;
    }
    return true;
}

void drawRobot(){
    if(originSet)
        circle(pathImage, origin, ROBOT_WIDTH, 0, 3);
    if(destSet)
        circle(pathImage, destination, ROBOT_WIDTH, 0, 3);
}

