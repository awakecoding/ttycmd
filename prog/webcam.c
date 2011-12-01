/**
 * Display video from webcam
 *
 * Author  Nash
 * License GPL
 * Website http://nashruddin.com
 */
 
#include <stdio.h>
#include <string.h> 
#include "cv.h"
#include "highgui.h"
 
#define DEBUGMODE
#define QUALIFY_THRESHOLD (83)
#define VICTORY_THRESHOLD (95)
#define DIRECTION_THRESHOLD (59) 

int main( int argc, char **argv )
{
    CvCapture *capture = 0;
    IplImage *frame = 0;
    int key = 0;
    int loop; 
    
    int arg_index; 

    /* Mainly for loops */
    int i;
    int j;
    int k;
    unsigned char screen_section; 
    unsigned int screen_segment; 

    int step; 
    int height;
    int width; 
    int channels;
    
    unsigned long count_red; 

    /* 
      Depending on these percentages, and the defined threshold,
      we will know and tell the vehicle appropriately which way to
      go. 
    */
    unsigned int total_section1 = 0;
    unsigned int total_section2 = 0;
    unsigned int total_section3 = 0;
    double percent_r_section1 = 0.0f; 
    double percent_r_section2 = 0.0f; 
    double percent_r_section3 = 0.0f; 
    char direction[10];
    
    /* Controls what color we're looking for */
    unsigned char channel_checking = 0; 

    unsigned char *data; 
 
    channel_checking = 2; // red

#if 0
    for (arg_index=0; arg_index<argc; ++arg_index)
    {
      if (!strcmp(argv[arg_index],"--color=blue"))
      {
        channel_checking = 0; 
        printf("Detecting blue\n");
      }
      if (!strcmp(argv[arg_index],"--color=green")) 
      {
        channel_checking = 1; 
        printf("Detecting green\n");
      }
      if (!strcmp(argv[arg_index],"--color=red")) 
      {
        channel_checking = 2; 
        printf("Detecting red\n");
      }
    }
#endif 

    /* initialize camera */
    capture = cvCaptureFromCAM( 0 );
 
    /* always check */
    if ( !capture ) {
        fprintf( stderr, "Cannot open initialize webcam!\n" );
        return 1;
    }
 
    /* create a window for the video */
    // cvNamedWindow( "result", CV_WINDOW_AUTOSIZE );
 
    while( key != 'q' ) {
        /* get a frame */
        frame = cvQueryFrame( capture );

        /* always check */
        if( !frame ) break;
       
        data = (unsigned char*) frame->imageData; 

        height = frame->height; 
        width = frame->width; 
        step = frame->widthStep;
        channels = frame->nChannels;
        
        screen_segment = width / 3; 

        /* Cool inversion trick. Also a nice way to see how to individually
        mess with each byte. */ 
        
        /*
        for(i=0;i<height;i++) for(j=0;j<width;j++) for(k=0;k<channels;k++)
            data[i*step+j*channels+k]=255-data[i*step+j*channels+k];
        */ 

        count_red = 0 ;
        total_section1 = 0;       
        total_section2 = 0;       
        total_section3 = 0;       
        
        for(i=0;i<height;i++) 
        {
          for(j=0;j<width;j++) 
          {
            
            /* Check if white */
            if (data[i*step+j*channels+2] >= QUALIFY_THRESHOLD &&
                data[i*step+j*channels+0] >= QUALIFY_THRESHOLD && 
                data[i*step+j*channels+1] >= QUALIFY_THRESHOLD   ) 
            {
              ++count_red;

              /* Right Segment */
              if (j>=screen_segment*2)
              {
                ++total_section3;
              }
              /* Middle Segment */
              else if (j>=screen_segment)
              {
                ++total_section2; 
              }
              /* Left Segment */
              else 
              {
                ++total_section1; 
              }
  
            }

            // Uncomment if you want terminator vision :D
            //data[i*step+j*channels+0] = 0; 
            //data[i*step+j*channels+1] = 0; 
          } 
        }

        // printf("countred: %d\n", count_red);

        percent_r_section1 = 100 * (double) total_section1 / (screen_segment * height); 
        percent_r_section2 = 100 * (double) total_section2 / (screen_segment * height); 
        percent_r_section3 = 100 * (double) total_section3 / (screen_segment * height); 

#ifdef DEBUGMODE

        /*
            Note - This is your intelligence!
            Watch it crumble! When adding to the
            beagle board, you should just set
            the global flag here so that the thread
            edits said variable. 
        */

        /* left case */ 
        if( percent_r_section1 >= VICTORY_THRESHOLD )
        {
          strcpy(direction, "[VICTORY]");
        }
        else if 
        ( percent_r_section3 > percent_r_section2 && 
          percent_r_section3 > percent_r_section1 && 
          percent_r_section3 >= DIRECTION_THRESHOLD) 
        {
          strcpy(direction,"[left]");      
        }

        /* right case */
        else if 
        ( percent_r_section1 > percent_r_section2 && 
          percent_r_section1 > percent_r_section3 && 
          percent_r_section1 >= DIRECTION_THRESHOLD)
        {
          strcpy(direction,"[right]");      
        }

        /* forward case */
        else if 
        ( percent_r_section2 > percent_r_section1 &&
          percent_r_section2 > percent_r_section3 && 
          percent_r_section2 >= DIRECTION_THRESHOLD)
        {
          strcpy(direction,"[forward]");      
        }

        /* default */
        else 
        {
          strcpy(direction,"[default]");      
        }

        printf
        (
          "x:%d,y:%d,[r%%:%f][r%%:%f][r%%:%f] : I want to go... %s    ", 
          frame->width
          ,frame->height
          ,percent_r_section1 
          ,percent_r_section2 
          ,percent_r_section3 
          ,direction
        );

        for(loop=0; loop<1000; ++loop)
          printf("\b"); 

#endif 
        /* display current frame - have disabled when using beagle */
        // cvShowImage( "result", frame );
 
        /* exit if user press 'q' */
        key = cvWaitKey( 1 );
    }
 
    /* free memory */
    // cvDestroyWindow( "result" );
    cvReleaseCapture( &capture );
 
    return 0;
}
