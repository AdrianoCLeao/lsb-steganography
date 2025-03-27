#ifndef COMMON_H
#define COMMON_H

typedef enum {
    STATUS_OK = 0,              
    STATUS_ERROR = 1,          
    STATUS_FILE_NOT_FOUND,    
    STATUS_INVALID_FORMAT,     
    STATUS_OUT_OF_MEMORY,     
    STATUS_INVALID_ARGUMENT,   
    STATUS_IO_ERROR,          
    STATUS_IMAGE_TOO_SMALL,   
    STATUS_MESSAGE_TOO_LARGE,  
    STATUS_NULL_POINTER       
} StatusCode;

#endif 
