// //////////////////////////////////////////////////////////
// how to use TooJpeg: creating a JPEG file
// see https://create.stephan-brumme.com/toojpeg/
// compile: g++ example.cpp toojpeg.cpp -o example -std=c++11

#include "toojpeg.h"

// //////////////////////////////////////////////////////////
// use a C++ file stream
#include <fstream>

// output file


// write a single byte compressed by tooJpeg



//std::ofstream myFile("example.jpg", std::ios_base::out | std::ios_base::binary);

std::ofstream myFile;

void myOutput(unsigned char byte)
{
    myFile << byte;
}

// //////////////////////////////////////////////////////////
int main(int arg_c, char *arg_v[])
{

    const char *pSrc_filename;
    const char *pDst_filename;
    const char *level;


    pSrc_filename = arg_v[1];
    pDst_filename = arg_v[2];
    level = arg_v[3];

    myFile.open(pDst_filename, std::ios_base::out | std::ios_base::binary);

    unsigned char quality;

    sscanf(level, "%d", &quality);

    //printf("%d \n", quality);
/*
    FILE * file1, *file2, *file3;
    file1 = fopen("coefficients1", "rb");
    file2 = fopen("coefficients2", "rb");
    file3 = fopen("test", "wb");

    int16_t buf1[64], buf2[64];

    while(fread(buf1, sizeof(int16_t), 64, file1) == 64 && fread(buf2, sizeof(int16_t), 64, file2) == 64)
    {
        for(int i = 0; i < 64; i++)
        {
            buf1[i] = buf1[i] - buf2[i];
        }
        fwrite(buf1, sizeof(int16_t), 64, file3);
    }

    fclose(file1); fclose(file2); fclose(file3);
*/
    // 800x600 image
    const auto width  = 1920;
    const auto height = 1080;
    // RGB: one byte each for red, green, blue
    const auto bytesPerPixel = 3;

    // allocate memory
    auto image = new unsigned char[width * height * bytesPerPixel];

    // create a nice color transition (replace with your code)
    for (auto y = 0; y < height; y++)
        for (auto x = 0; x < width; x++)
        {
            // memory location of current pixel
            auto offset = (y * width + x) * bytesPerPixel;

            // red and green fade from 0 to 255, blue is always 127
            image[offset    ] = 255 * x / width;
            image[offset + 1] = 255 * y / height;
            image[offset + 2] = 127;
          //  image[offset    ] = 0;
          //  image[offset + 1] = 0;
         //   image[offset + 2] = 0;
        }

    // start JPEG compression
    // note: myOutput is the function defined in line 18, it saves the output in example.jpg
    // optional parameters:
    const bool isRGB      = true;  // true = RGB image, else false = grayscale
   // const auto quality    = 95;    // compression quality: 0 = worst, 100 = best, 80 to 90 are most often used
    const bool downsample = true; // false = save as YCbCr444 JPEG (better quality), true = YCbCr420 (smaller file)
    const char* comment = "TooJpeg example image"; // arbitrary JPEG comment

    FILE * coeffs;
    //coeffs = fopen("coefficients1", "rb");
   //coeffs = fopen("diff", "rb");
   coeffs = fopen(pSrc_filename, "rb");

    auto ok = TooJpeg::writeJpeg(myOutput, image, width, height, isRGB, quality, downsample, comment, coeffs);

    delete[] image;
    fclose(coeffs);

    // error => exit code 1
    return ok ? 0 : 1;
}
