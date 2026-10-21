// Authored screenshot utility: reads actual X11 pixels, never draws game content.
#include "window_services.h"
#include <exception>
#include <sys/stat.h>
int main(int argc,char** argv){
    if(argc!=2){std::fprintf(stderr,"Usage: FS15GE6X11Snapshot output.png\n");return 2;}
    try{
        ge6_display::trace=[](const char*){};
        ge6_display::set_error=[](uint32_t){};
        if(!ge6_display::connect()||setenv("GE6_X11_CAPTURE",argv[1],1))return 1;
        if(!ge6_windows::capture_on_stop())return 1;
        struct stat result{};return stat(argv[1],&result)||result.st_size<24?1:0;
    }catch(const std::exception& error){std::fprintf(stderr,"X11 snapshot failed: %s\n",error.what());return 1;}
}
