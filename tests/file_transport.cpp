#include "file_transport.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs=std::filesystem;
void check(bool result){if(!result)throw std::runtime_error("Read-only file transport assertion failed");}
int main(){
    fs::path root;
    try{
        root=fs::current_path()/("file-transport-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(fs::create_directory(root));
        const auto path=root/fs::u8path("resource space-\xc3\xa9.dat");
        const std::vector<uint8_t> expected{0,1,2,3,0,255,128,42};
        {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(expected.data()),expected.size());}
        {
            fs15platform::ReadOnlyFile file(path);unsigned char data[12]{};
            check(file.full_size()==8&&file.size()==8&&file.tell()==0&&!file.eof());
            check(file.read(data,3)==3&&file.tell()==3&&!file.eof()&&data[2]==2);
            check(file.read(data,12)==5&&file.tell()==8&&file.eof()&&data[0]==3);
            check(file.read(data,1)==0&&file.eof());
            check(file.read(nullptr,0)==0&&!file.eof());
            check(file.seek(4)&&file.tell()==4&&!file.eof());
            check(file.read(data,4)==4&&!file.eof()&&data[0]==0&&data[3]==42);
            check(file.seek(20)&&file.tell()==20&&!file.eof());
            check(file.read(data,1)==0&&file.eof());
            check(!file.seek(0x80000000)&&file.tell()==20&&file.eof());
            check(file.seek(0)&&!file.eof());
        }
        check(fs15platform::read_file_bounded(path,8)==expected);
        {
            fs15platform::BufferedReadOnlyFile file(path);unsigned char data[12]{};
            check(file.status()==0&&file.read(data,3)==3&&data[2]==2);
            check(file.seek(1,0)==0&&file.read(data,2)==2&&data[0]==1&&data[1]==2);
            check(file.seek(2,2)==0&&file.read(data,12)==2&&data[0]==128&&data[1]==42&&file.status()==0);
            check(file.seek(0,0)==0&&file.read(data,8)==8&&file.status()==0);
        }
        bool bounded=false,missing=false,directory=false;
        try{fs15platform::read_file_bounded(path,7);}catch(const std::runtime_error&){bounded=true;}
        try{fs15platform::ReadOnlyFile file(root/"missing");}catch(const std::runtime_error&){missing=true;}
        try{fs15platform::ReadOnlyFile file(root);}catch(const std::runtime_error&){directory=true;}
        check(bounded&&missing&&directory);
        const auto empty=root/"empty";{std::ofstream out(empty);}
        check(fs15platform::read_file_bounded(empty,0).empty());
        check(fs15platform::read_file_bounded(path,8)==expected);
        fs::remove(path);fs::remove(empty);fs::remove(root);
        std::cout<<"Read-only Windows/POSIX file transport checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
