// Authored text-only subset loader. No complete GE6 bootstrap or renderer.
#include "script_runtime.h"
extern "C" {
#include <luajit.h>
}
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv){
    try{
        std::filesystem::path root;std::string language="en";bool probe=false;
        for(int i=1;i<argc;++i){const std::string a=argv[i];
            if(a=="--help"){std::cout<<"FS15ScriptHost --i18n-probe --root PATH [--language en]\nText-only localization subset; no menu window or full engine startup.\n";return 0;}
            if(a=="--i18n-probe"){if(probe)throw std::invalid_argument("Duplicate probe option");probe=true;}
            else if((a=="--root"||a=="--language")&&i+1<argc){if(a=="--root")root=std::filesystem::u8path(argv[++i]);else language=argv[++i];}
            else throw std::invalid_argument("Unknown or incomplete option");
        }
        if(!probe||root.empty())throw std::invalid_argument("Expected --i18n-probe and --root");
        const auto data=fs15portable::load_localization(root,language);
        std::cout<<"{\"status\":\"passed\",\"lua_runtime\":\""<<LUAJIT_VERSION<<"\",\"xml_backend\":\""<<fs15portable::xml_backend()
            <<"\",\"source_origin\":\"caller_supplied_text\",\"source_files\":"<<data.loaded_sources<<",\"text_entries\":"<<data.texts.size()<<",\"description_entries\":"<<data.descriptions.size()
            <<",\"main_menu_labels\":"<<data.labels.size()<<",\"retained_xml_documents\":0,\"native_engine_started\":false,\"renderer_started\":false,\"playable\":false}\n";return 0;
    }catch(const std::invalid_argument& e){std::cerr<<e.what()<<'\n';return 2;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
