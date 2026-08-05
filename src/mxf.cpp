#include "mxf.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;

namespace {
constexpr array<uint8_t, 13> kPartitionPrefix = {
    0x06,0x0e,0x2b,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01
};
constexpr array<uint8_t, 16> kRipKey = {
    0x06,0x0e,0x2b,0x34,0x02,0x05,0x01,0x01,0x0d,0x01,0x02,0x01,0x01,0x11,0x01,0x00
};
constexpr uint64_t kMaxPrototypeBytes = uint64_t(2) << 30;

struct Partition {
    uint64_t offset;
    uint8_t kind;
    size_t value_offset;
    uint64_t value_length;
    uint32_t index_sid;
    uint32_t body_sid;
};

uint32_t readBe32(const vector<uint8_t>& d, size_t p) {
    if (p + 4 > d.size()) throw runtime_error("truncated MXF partition pack");
    uint32_t v = 0;
    for (size_t i=0;i<4;++i) v=(v<<8)|d[p+i];
    return v;
}
void writeBe32(vector<uint8_t>& d, size_t p, uint32_t v) {
    if (p + 4 > d.size()) throw runtime_error("truncated MXF output");
    for (int i=3;i>=0;--i) { d[p+i]=uint8_t(v); v>>=8; }
}
void writeBe64(vector<uint8_t>& d, size_t p, uint64_t v) {
    if (p + 8 > d.size()) throw runtime_error("truncated MXF output");
    for (int i=7;i>=0;--i) { d[p+i]=uint8_t(v); v>>=8; }
}
bool readBer(const vector<uint8_t>& d, size_t p, uint64_t& v, size_t& n) {
    if (p >= d.size()) return false;
    uint8_t first=d[p];
    if (!(first&0x80)) { v=first; n=1; return true; }
    size_t count=first&0x7f;
    if (!count || count>8 || p+1+count>d.size()) return false;
    v=0;
    for (size_t i=0;i<count;++i) v=(v<<8)|d[p+1+i];
    n=1+count;
    return true;
}
vector<uint8_t> readAll(const string& path) {
    ifstream f(path,ios::binary|ios::ate);
    if (!f) throw runtime_error("could not open MXF file: "+path);
    auto end=f.tellg();
    if (end<0 || uint64_t(end)>kMaxPrototypeBytes)
        throw runtime_error("MXF prototype currently supports files up to 2 GiB");
    vector<uint8_t> d(static_cast<size_t>(end));
    f.seekg(0);
    if (!d.empty() && !f.read(reinterpret_cast<char*>(d.data()),d.size()))
        throw runtime_error("could not read MXF file: "+path);
    return d;
}
vector<Partition> partitions(const vector<uint8_t>& d) {
    vector<Partition> out;
    for (size_t p=0;p+17<=d.size();++p) {
        if (!equal(kPartitionPrefix.begin(),kPartitionPrefix.end(),d.begin()+p)) continue;
        uint8_t kind=d[p+13];
        if (kind<2 || kind>4) continue;
        uint64_t len=0; size_t ber=0;
        if (!readBer(d,p+16,len,ber) || len<80) continue;
        size_t value=p+16+ber;
        if (len>d.size()-value) continue;
        out.push_back({p,kind,value,len,readBe32(d,value+48),readBe32(d,value+60)});
        p=value+size_t(len)-1;
    }
    return out;
}
const Partition& firstKind(const vector<Partition>& ps,uint8_t kind,const char* what) {
    for (const auto& p:ps) if (p.kind==kind) return p;
    throw runtime_error(string("MXF has no valid ")+what+" partition");
}
bool sameOperationalPattern(const vector<uint8_t>& a,const Partition& pa,
                            const vector<uint8_t>& b,const Partition& pb) {
    return equal(a.begin()+pa.value_offset+64,a.begin()+pa.value_offset+80,
                 b.begin()+pb.value_offset+64);
}
size_t ripOffset(const vector<uint8_t>& d) {
    if (d.size()<20) return d.size();
    size_t begin=d.size()>65536?d.size()-65536:0;
    for (size_t p=d.size()-16;p>=begin;--p) {
        if (equal(kRipKey.begin(),kRipKey.end(),d.begin()+p)) return p;
        if (p==0) break;
    }
    return d.size();
}
void append32(vector<uint8_t>& d,uint32_t v) {
    size_t p=d.size(); d.resize(p+4); writeBe32(d,p,v);
}
void append64(vector<uint8_t>& d,uint64_t v) {
    size_t p=d.size(); d.resize(p+8); writeBe64(d,p,v);
}
void appendRip(vector<uint8_t>& out,const vector<Partition>& ps,int64_t delta) {
    vector<pair<uint32_t,uint64_t>> entries;
    entries.push_back({0,0});
    for (const auto& p:ps)
        entries.push_back({p.body_sid,uint64_t(int64_t(p.offset)+delta)});
    uint32_t value_len=uint32_t(entries.size()*12+4);
    uint32_t total=16+4+value_len;
    out.insert(out.end(),kRipKey.begin(),kRipKey.end());
    out.push_back(0x83);
    out.push_back(uint8_t(value_len>>16)); out.push_back(uint8_t(value_len>>8)); out.push_back(uint8_t(value_len));
    for (const auto& e:entries) { append32(out,e.first); append64(out,e.second); }
    append32(out,total);
}
}

void MxfRepair::repair(const string& reference,const string& damaged,const string& destination) {
    auto good=readAll(reference), bad=readAll(damaged);
    auto gp=partitions(good), bp=partitions(bad);
    const auto& gh=firstKind(gp,2,"header");
    const auto& gb=firstKind(gp,3,"body");
    const auto& db=firstKind(bp,3,"body");
    if (!sameOperationalPattern(good,gh,bad,db))
        throw runtime_error("reference and damaged MXF Operational Patterns differ");
    if (gb.body_sid!=db.body_sid || gb.index_sid!=db.index_sid)
        throw runtime_error("reference and damaged MXF BodySID/IndexSID differ");

    size_t good_body=size_t(gb.offset), bad_body=size_t(db.offset), bad_end=ripOffset(bad);
    if (bad_end<bad_body) throw runtime_error("damaged MXF Random Index Pack precedes body");
    vector<uint8_t> out;
    out.reserve(good_body+bad_end-bad_body+4096);
    out.insert(out.end(),good.begin(),good.begin()+good_body);
    out.insert(out.end(),bad.begin()+bad_body,bad.begin()+bad_end);

    int64_t delta=int64_t(good_body)-int64_t(bad_body);
    vector<Partition> copied;
    for (const auto& p:bp) if (p.offset>=bad_body && p.offset<bad_end) copied.push_back(p);
    uint64_t footer=0;
    for (const auto& p:copied) if (p.kind==4) footer=uint64_t(int64_t(p.offset)+delta);
    writeBe64(out,gh.value_offset+24,footer);
    uint64_t previous=0;
    for (auto p:copied) {
        p.value_offset=size_t(int64_t(p.value_offset)+delta);
        uint64_t here=uint64_t(int64_t(p.offset)+delta);
        writeBe64(out,p.value_offset+8,here);
        writeBe64(out,p.value_offset+16,previous);
        writeBe64(out,p.value_offset+24,footer);
        previous=here;
    }
    appendRip(out,copied,delta);

    string target=destination.empty()?damaged+"_fixed.mxf":destination;
    ofstream f(target,ios::binary|ios::trunc);
    if (!f || (!out.empty() && !f.write(reinterpret_cast<const char*>(out.data()),out.size())))
        throw runtime_error("could not write repaired MXF: "+target);
    cout<<"Recovered MXF using compatible reference metadata: "<<target<<std::endl;
    cout<<"Preserved "<<copied.size()<<" body/footer partition(s) and rebuilt the RIP\n";
}