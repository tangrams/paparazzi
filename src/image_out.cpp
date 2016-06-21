#include "image_out.h"

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

bool savePixels(char const* _path, unsigned char* _pixels, int _width, int _height) {
    // Flip the image on Y
    int depth = 4;
    unsigned char *result = new unsigned char[_width*_height*depth];
    memcpy(result, _pixels, _width*_height*depth);
    int row,col,z;
    stbi_uc temp;

    for (row = 0; row < (_height>>1); row++) {
        for (col = 0; col < _width; col++) {
            for (z = 0; z < depth; z++) {
                temp = result[(row * _width + col) * depth + z];
                result[(row * _width + col) * depth + z] = result[((_height - row - 1) * _width + col) * depth + z];
                result[((_height - row - 1) * _width + col) * depth + z] = temp;
            }
        }
    }
    stbi_write_png(_path, _width, _height, 4, result, _width * 4);
    delete result;
    
    return true;
}

