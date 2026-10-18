// Local-only original-bytecode oracle; never installed or included in CI archive.
#include "script_runtime.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Expected local workspace root");
    for(const char* language:{"en"}){
        const auto a=fs15portable::load_localization(std::filesystem::u8path(argv[1]),language);
        const auto b=fs15portable::reference_localization_for_test(std::filesystem::u8path(argv[1]),language);
        if(a.texts!=b.texts||a.descriptions!=b.descriptions||a.labels!=b.labels||a.fluid!=b.fluid||a.power_hp!=b.power_hp||a.power_kw!=b.power_kw)throw std::runtime_error("Recovered/original localization outputs differ");
        std::cout<<language<<": texts="<<a.texts.size()<<" descriptions="<<a.descriptions.size()<<" labels="<<a.labels.size()<<"; exact original-bytecode parity\n";
    }return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
