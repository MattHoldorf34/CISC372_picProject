#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "image-pthread.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

//An array of kernel matrices to be used for image convolution.  
//The indexes of these match the enumeration from the header file. ie. algorithms[BLUR] returns the kernel corresponding to a box blur.
Matrix algorithms[]={
    {{0,-1,0},{-1,4,-1},{0,-1,0}},
    {{0,-1,0},{-1,5,-1},{0,-1,0}},
    {{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0},{1/9.0,1/9.0,1/9.0}},
    {{1.0/16,1.0/8,1.0/16},{1.0/8,1.0/4,1.0/8},{1.0/16,1.0/8,1.0/16}},
    {{-2,-1,0},{-1,1,1},{0,1,2}},
    {{0,0,0},{0,1,0},{0,0,0}}
};

//Define variable for number of threads, used in main.
int num_threads = 10;

//Struct to track the input image, destination image, kernel type, rank, and thread count.
struct convolute_t {
	Image* src_image; 
	Image* dest_image; 
	enum KernelTypes type;
	long rank;
	int thread_count;
};

//minimum - simple function to return the minimum between two integers.
//Paramters:
//      a: An integer for comparison
//      b: a second integer for comparison.
//Returns: The lower of the two integers.
int minimum(int a, int b){
	if (a < b) {
		return a;
	} 
	else {
		return b;
	}
}

//getPixelValue - Computes the value of a specific pixel on a specific channel using the selected convolution kernel
//Paramters: srcImage:  An Image struct populated with the image being convoluted
//           x: The x coordinate of the pixel
//          y: The y coordinate of the pixel
//          bit: The color channel being manipulated
//          algorithm: The 3x3 kernel matrix to use for the convolution
//Returns: The new value for this x,y pixel and bit channel
uint8_t getPixelValue(Image* srcImage,int x,int y,int bit,Matrix algorithm){
    int px,mx,py,my,i,span;
    span=srcImage->width*srcImage->bpp;
    // for the edge pixes, just reuse the edge pixel
    px=x+1; py=y+1; mx=x-1; my=y-1;
    if (mx<0) mx=0;
    if (my<0) my=0;
    if (px>=srcImage->width) px=srcImage->width-1;
    if (py>=srcImage->height) py=srcImage->height-1;
    uint8_t result=
        algorithm[0][0]*srcImage->data[Index(mx,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][1]*srcImage->data[Index(x,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[0][2]*srcImage->data[Index(px,my,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][0]*srcImage->data[Index(mx,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][1]*srcImage->data[Index(x,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[1][2]*srcImage->data[Index(px,y,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][0]*srcImage->data[Index(mx,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][1]*srcImage->data[Index(x,py,srcImage->width,bit,srcImage->bpp)]+
        algorithm[2][2]*srcImage->data[Index(px,py,srcImage->width,bit,srcImage->bpp)];
    return result;
}

//convolute_p:  Applies a kernel matrix to an image, p-thread edition
//Parameters:
//      img_data: The image being convoluted.
//Returns: Nothing
void *convolute_p(void* img_data){
    struct convolute_t *args = img_data;
    
	Image* srcImage = args->src_image;
    Image* destImage = args->dest_image;    
    long my_rank = args->rank;
    int thread_count = args->thread_count;

    int row,pix,bit,span, start, end;
    start = (srcImage->height / thread_count) * my_rank + 1;
    
	end = minimum((srcImage->height / thread_count) * (my_rank + 1), srcImage->height); 
	
    span=srcImage->bpp*srcImage->bpp;
    for (row=start;row<=end;row++){
        for (pix=0;pix<srcImage->width;pix++){
            for (bit=0;bit<srcImage->bpp;bit++){
                destImage->data[Index(pix,row,srcImage->width,bit,srcImage->bpp)]=getPixelValue(srcImage,pix,row,bit,algorithms[args->type]);
            }
        }
    }
    free(img_data);
}

//Usage: Prints usage information for the program
//Returns: -1
int Usage(){
    printf("Usage: image <filename> <type>\n\twhere type is one of (edge,sharpen,blur,gauss,emboss,identity)\n");
    return -1;
}

//GetKernelType: Converts the string name of a convolution into a value from the KernelTypes enumeration
//Parameters: type: A string representation of the type
//Returns: an appropriate entry from the KernelTypes enumeration, defaults to IDENTITY, which does nothing but copy the image.
enum KernelTypes GetKernelType(char* type){
    if (!strcmp(type,"edge")) return EDGE;
    else if (!strcmp(type,"sharpen")) return SHARPEN;
    else if (!strcmp(type,"blur")) return BLUR;
    else if (!strcmp(type,"gauss")) return GAUSE_BLUR;
    else if (!strcmp(type,"emboss")) return EMBOSS;
    else return IDENTITY;
}

//main:
//argv is expected to take 2 arguments.  First is the source file name (can be jpg, png, bmp, tga).  Second is the lower case name of the algorithm.
int main(int argc,char** argv){
    double t1,t2;
    t1 = time(NULL);
    stbi_set_flip_vertically_on_load(0);

    if (argc!=3) return Usage();
    char* fileName=argv[1];

    if (!strcmp(argv[1],"pic4.jpg")&&!strcmp(argv[2],"gauss"))
    {
        printf("You have applied a gaussian filter to Gauss which has caused a tear in the time-space continum.\n");
    }

    enum KernelTypes type=GetKernelType(argv[2]);
    Image srcImage,destImage,bwImage;   
    srcImage.data=stbi_load(fileName,&srcImage.width,&srcImage.height,&srcImage.bpp,0);

    if (!srcImage.data)
    {
        printf("Error loading file %s.\n",fileName);
        return -1;
    }

    destImage.bpp=srcImage.bpp;
    destImage.height=srcImage.height;
    destImage.width=srcImage.width;
    destImage.data=malloc(sizeof(uint8_t)*destImage.width*destImage.bpp*destImage.height);
    
    pthread_t* thread_handles;
    thread_handles = (pthread_t*) malloc(num_threads * sizeof(pthread_t));

    for (long thread = 0; thread < num_threads; thread++)
    {
        struct convolute_t *args = malloc(sizeof(struct convolute_t));
		args->src_image = &srcImage;
		args->dest_image = &destImage;
		args->type = type; 
		args->rank = thread;
		args->thread_count = num_threads;
		pthread_create(&thread_handles[thread], NULL, convolute_p, args);
    }

    t2 = time(NULL);
    stbi_write_png("output.png",destImage.width,destImage.height,destImage.bpp,destImage.data,destImage.bpp*destImage.width);
    stbi_image_free(srcImage.data);
    
    for (long thread = 0; thread < num_threads; thread++)
    {
    	pthread_join(thread_handles[thread], NULL);
    }

    free(thread_handles);
    free(destImage.data);
    
    printf("Completed in: %f seconds\n",t2-t1);
    return 0;
}
