#pragma once
#include <filesystem>
#include <map>
#include <string>
namespace fs15portable {
struct Localization {
    std::map<std::string,std::string> texts,descriptions,labels;
    double fluid=0,power_hp=0,power_kw=0;
    size_t loaded_sources=0;
};
// Authored host around the shared XML bridge and external source. Original
// bytecode is accepted ONLY by the separate parity-test entry, never by CLI.
Localization load_localization(const std::filesystem::path&,const std::string&);
Localization reference_localization_for_test(const std::filesystem::path&,const std::string&);
const char* xml_backend();
}
