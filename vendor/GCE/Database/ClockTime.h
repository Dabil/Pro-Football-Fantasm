#pragma once
#include "GCE/Database/TeamDataSql.h"
#include <cstdio>
#include <stdexcept>

// Named historical conversion capability, not the machine's local timezone.
// No installed IANA database is claimed. Unsupported zones/years fail explicitly.
namespace ClockTime
{
inline constexpr const char* Policy = "GCE_NAMED_CLOCK_V1";
inline std::string digest(const std::string& value)
{
    std::uint64_t h=14695981039346656037ULL;
    for(unsigned char c:value) { h^=c; h*=1099511628211ULL; }
    char out[32]; std::snprintf(out,sizeof(out),"FNV1A64:%016llx",static_cast<unsigned long long>(h));
    return out;
}
inline std::int64_t days(int y,unsigned m,unsigned d)
{
    y-=m<=2; const int era=(y>=0?y:y-399)/400; const unsigned yo=y-era*400;
    const unsigned dy=(153*(m+(m>2?-3:9))+2)/5+d-1;
    return era*146097+yo*365+yo/4-yo/100+dy-719468;
}
inline std::string format(std::int64_t seconds)
{
    auto d=seconds/86400,r=seconds%86400; if(r<0){r+=86400;--d;}
    auto z=d+719468; const auto era=(z>=0?z:z-146096)/146097;
    const unsigned de=static_cast<unsigned>(z-era*146097),yo=(de-de/1460+de/36524-de/146096)/365;
    int y=static_cast<int>(yo+era*400); const unsigned dy=de-(yo*365+yo/4-yo/100),mp=(5*dy+2)/153;
    const unsigned day=dy-(153*mp+2)/5+1,month=mp+(mp<10?3:-9); y+=month<=2;
    char out[32]; std::snprintf(out,sizeof(out),"%04d-%02u-%02uT%02d:%02d:%02dZ",y,month,day,int(r/3600),int(r/60%60),int(r%60));
    if(!TeamDataSql::utc(out)) throw std::runtime_error("INVALID_TIME: unsupported date range");
    return out;
}
inline int firstSunday(int y,int month)
{int weekday=int((days(y,month,1)+4)%7); if(weekday<0)weekday+=7; return 1+(7-weekday)%7;}
inline std::string normalize(const std::string& local,const std::string& zone)
{
    if(local.size()!=19 || !TeamDataSql::utc(local+"Z"))
        throw std::runtime_error("INVALID_TIME: expected valid whole-second local date/time");
    const int y=std::stoi(local.substr(0,4)),m=std::stoi(local.substr(5,2)),d=std::stoi(local.substr(8,2)),h=std::stoi(local.substr(11,2));
    int offset=0;
    if(zone!="Etc/UTC" && zone!="UTC")
    {
        if(zone!="America/New_York" || y<1966)
            throw std::runtime_error("MISSING_TIMEZONE: no installed historical converter for "+zone+" in "+std::to_string(y));
        int sm=4,em=10,sd=firstSunday(y,4)+((30-firstSunday(y,4))/7)*7,ed=firstSunday(y,10)+((31-firstSunday(y,10))/7)*7;
        // Same named New York rule eras as the existing schedule normalizer.
        if(y>=2007){sm=3;sd=firstSunday(y,3)+7;em=11;ed=firstSunday(y,11);}
        else if(y>=1987)sd=firstSunday(y,4);
        else if(y==1974){sm=1;sd=6;}
        else if(y==1975){sm=2;sd=23;}
        if((m==sm&&d==sd&&h==2)||(m==em&&d==ed&&h==1))
            throw std::runtime_error("AMBIGUOUS_LOCAL_TIME: nonexistent/repeated local hour requires an explicit conversion policy");
        const bool dst=(m>sm||(m==sm&&(d>sd||(d==sd&&h>=3))))&&(m<em||(m==em&&(d<ed||(d==ed&&h<1))));
        offset=dst?-240:-300;
    }
    const auto epoch=days(y,m,d)*86400+h*3600+std::stoi(local.substr(14,2))*60+std::stoi(local.substr(17,2));
    return format(epoch-offset*60);
}
}
