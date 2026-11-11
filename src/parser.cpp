#include <winsock2.h>
#include "parser.h"
#include <sstream>
#include <cwctype>
#include <ws2tcpip.h>

namespace josts {
bool valid_ip(const std::wstring& ip) {
    IN_ADDR a4{}; IN6_ADDR a6{};
    return InetPtonW(AF_INET,ip.c_str(),&a4)==1||InetPtonW(AF_INET6,ip.c_str(),&a6)==1;
}
bool valid_domain(const std::wstring& domain) {
    if(domain.empty()||domain.size()>253||domain.front()==L'.'||domain.back()==L'.') return false;
    size_t label=0;
    for(wchar_t c:domain) {
        if(c==L'.') { if(!label||label>63) return false; label=0; continue; }
        if(!(c>=L'a'&&c<=L'z')&&!(c>=L'A'&&c<=L'Z')&&!(c>=L'0'&&c<=L'9')&&c!=L'-'&&c!=L'_') return false;
        ++label;
    }
    return label>0&&label<=63;
}
static std::wstring visible_markdown(std::wstring s) {
    // Convert [domain](URL) to the visible domain; remove simple Markdown emphasis.
    for(size_t p=0;(p=s.find(L'[',p))!=std::wstring::npos;) {
        size_t close=s.find(L']',p+1);
        if(close==std::wstring::npos||close+1>=s.size()||s[close+1]!=L'(') { ++p; continue; }
        size_t end=s.find(L')',close+2);
        if(end==std::wstring::npos) { ++p; continue; }
        s.replace(p,end-p+1,s.substr(p+1,close-p-1));
    }
    for(wchar_t& c:s) if(c==L'`'||c==L'*') c=L' ';
    return s;
}
ParseResult parse_list(const std::wstring& text,const std::set<std::wstring>& existing) {
    ParseResult result;
    std::set<std::wstring> seen=existing;
    bool fence=false;
    std::wistringstream stream(text);
    std::wstring line;
    while(std::getline(stream,line)) {
        line=trim(line);
        if(line.compare(0,3,L"```")==0||line.compare(0,3,L"~~~")==0) { fence=!fence; continue; }
        if(fence||line.empty()||line[0]==L'#'||line[0]==L';'||line.compare(0,2,L"//")==0||line.compare(0,4,L"<!--")==0) continue;
        if(line.size()>1&&(line[0]==L'-'||line[0]==L'+'||line[0]==L'*')&&iswspace(line[1])) line=trim(line.substr(2));
        line=visible_markdown(line);
        size_t comment=line.find(L'#'); if(comment!=std::wstring::npos) line=line.substr(0,comment);
        line=trim(line); if(line.empty()) continue;
        std::wistringstream words(line);
        std::vector<std::wstring> tokens;
        std::wstring word;
        while(words>>word) tokens.push_back(word);
        if(tokens.empty()) continue;
        std::wstring ip=L"0.0.0.0";
        size_t first=0;
        if(valid_ip(tokens[0])) { ip=tokens[0]; first=1; }
        if(first==tokens.size()) { ++result.errors; continue; }
        for(size_t i=first;i<tokens.size();++i) {
            std::wstring domain=lower(tokens[i]);
            while(!domain.empty()&&(domain.back()==L','||domain.back()==L';')) domain.pop_back();
            if(!valid_domain(domain)) { ++result.errors; continue; }
            if(!seen.insert(domain).second) { ++result.duplicates; continue; }
            result.entries.push_back({ip,domain,false});
        }
    }
    return result;
}
}
