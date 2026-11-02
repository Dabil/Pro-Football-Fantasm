#pragma once

#include "CoreManifest.h"
#include "GCE/Database/WorldDatabase.h"
#include <sqlite3.h>
#include <charconv>
#include <memory>

// PFF-facing discovery and setup policy. World construction/snapshot ownership
// remains with GCE; inspecting a Core never writes or repairs it.
namespace LeagueSetup
{
    struct Core
    {
        std::filesystem::path path;
        std::map<std::string,std::string> metadata;
        std::vector<std::string> details;
        int season = 0;
        std::string error;
        std::filesystem::file_time_type modified;
        std::uintmax_t size = 0;
    };

    inline int integer(const std::string& text)
    {
        int value = 0;
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
        return parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size() ? value : 0;
    }

    inline Core inspect(const std::filesystem::path& path)
    {
        Core result;
        result.path=path;
        sqlite3* raw=nullptr;
        const int opened=sqlite3_open_v2(path.u8string().c_str(),&raw,SQLITE_OPEN_READONLY,nullptr);
        std::unique_ptr<sqlite3,decltype(&sqlite3_close)> db(raw,sqlite3_close);
        if(opened!=SQLITE_OK) throw std::runtime_error("Cannot read Core: "+path.u8string());
        sqlite3_stmt* query=nullptr;
        if(sqlite3_prepare_v2(raw,"SELECT MetadataKey,MetadataValue FROM CoreMetadata",-1,&query,nullptr)!=SQLITE_OK)
            throw std::runtime_error("No readable Core metadata: "+path.u8string());
        std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> statement(query,sqlite3_finalize);
        int step;
        while((step=sqlite3_step(query))==SQLITE_ROW)
        {
            const auto key=sqlite3_column_text(query,0), value=sqlite3_column_text(query,1);
            if(key) result.metadata[reinterpret_cast<const char*>(key)]=value?reinterpret_cast<const char*>(value):"";
        }
        if(step!=SQLITE_DONE) throw std::runtime_error("Core metadata read failed: "+path.u8string());
        auto value=[&](const std::string& key){auto found=result.metadata.find(key);return found==result.metadata.end()||found->second.empty()?std::string("Unknown / not recorded"):found->second;};
        result.season=integer(result.metadata["Season"]);
        result.details={"Core: "+path.filename().u8string(),"Ruleset: "+value("Ruleset"),
            "Season: "+value("Season"),"Core schema: "+value("CoreSchemaVersion")};
        if(result.season<=0) result.error="Core has no usable recorded season. Choose another Core or rebuild it.";
        std::map<std::string,std::map<std::string,std::string>> packages;
        for(const auto& entry:result.metadata)
            if(entry.first.rfind("ManifestPackage.",0)==0)
            {
                const auto dot=entry.first.find('.',16);
                if(dot!=std::string::npos) packages[entry.first.substr(16,dot-16)][entry.first.substr(dot+1)]=entry.second;
            }
        if(packages.empty()) result.details.push_back("Packages: Unknown / not recorded");
        for(const auto& item:packages)
        {
            auto field=[&](const char* key){auto f=item.second.find(key);return f==item.second.end()||f->second.empty()?std::string("Unknown"):f->second;};
            result.details.push_back("Package: "+field("Key")+" @ "+field("Version"));
            result.details.push_back("  Source manifest: "+field("Path"));
        }
        if(!result.metadata.count("BuildWarningCount")) result.details.push_back("Build warnings: Unknown / history not recorded by this Core");
        else
        {
            const auto text=result.metadata["BuildWarningCount"];
            const int count=integer(text);
            if(count<0 || count>10000 || (count==0 && text!="0")) result.details.push_back("Build warning history is incomplete or unreadable.");
            else if(count==0) result.details.push_back("Recorded build warnings: None");
            else for(int i=0;i<count;++i)
            {
                const auto prefix="BuildWarning."+std::to_string(i)+".";
                result.details.push_back("WARNING: "+value(prefix+"Message"));
                result.details.push_back("  Source: "+value(prefix+"File")+":"+value(prefix+"Line"));
            }
        }
        result.details.push_back("Chart execution readiness: "+value("ChartExecutionReadiness"));
        result.details.push_back("Creation preserves a private Core snapshot with the league.");
        result.modified=std::filesystem::last_write_time(path);
        result.size=std::filesystem::file_size(path);
        return result;
    }

    inline std::vector<Core> discover(const std::filesystem::path& directory,std::vector<std::string>& errors)
    {
        std::vector<Core> cores;
        try
        {
            if(!std::filesystem::exists(directory)) return cores;
            for(const auto& entry:std::filesystem::directory_iterator(directory))
            {
                auto ext=entry.path().extension().string();
                std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                if(!entry.is_regular_file()||ext!=".db") continue;
                try {cores.push_back(inspect(entry.path()));}
                catch(const std::exception& ex){errors.push_back(ex.what());}
            }
        }
        catch(const std::exception& ex){errors.push_back(ex.what());}
        std::sort(cores.begin(),cores.end(),[](const auto& a,const auto& b){return a.path.filename()<b.path.filename();});
        return cores;
    }

    inline std::filesystem::path destination(const std::filesystem::path& root,const std::string& name)
    {return root/std::filesystem::u8path(name)/"World.db";}

    inline std::string nameError(const std::filesystem::path& root,const std::string& name)
    {
        auto error=CoreManifest::basenameError(name);
        if(!error.empty()) return error;
        std::error_code ec;
        const bool exists=std::filesystem::exists(destination(root,name).parent_path(),ec);
        if(ec) return "Cannot check save location: "+ec.message();
        return exists?"That league/save folder already exists. Choose another name.":"";
    }

    inline WorldDatabaseCreateResult create(const Core& core,const std::filesystem::path& root,const std::string& name)
    {
        WorldDatabaseCreateResult result;
        try
        {
            const auto error=nameError(root,name);
            if(!error.empty()) throw std::runtime_error(error);
            if(!core.error.empty()) throw std::runtime_error(core.error);
            if(std::filesystem::last_write_time(core.path)!=core.modified || std::filesystem::file_size(core.path)!=core.size)
                throw std::runtime_error("Selected Core changed. Cancel and reopen New League to refresh its details.");
            std::filesystem::create_directories(root);
            const auto path=destination(root,name);
            // Atomic reservation prevents a racing creation from sharing the
            // same snapshot/temp files or overwriting an existing save folder.
            if(!std::filesystem::create_directory(path.parent_path()))
                throw std::runtime_error("Save folder already exists. Choose another name.");
            WorldDatabaseCreateRequest request;
            request.coreDatabase=core.path; request.outputDatabase=path;
            request.worldName=name; request.season=core.season;
            result=createWorldDatabase(request);
            // A failed attempt may leave its own empty folder. Keep it visible
            // rather than recursively deleting any save material.
        }
        catch(const std::exception& ex)
        {result.diagnostics.push_back({DatabaseDiagnosticSeverity::Error,ex.what(),{},0});}
        return result;
    }
}
