#include "mutual_pattern.h"

#include <numeric>
using namespace std;

MutualPattern::MutualPattern(ByteArr& a, ByteArr& b) {
	is_mutual.resize(a.size(), true);
	mutual_till_ = a.size();
	size_mutual_ = a.size();
	data = a;
	intersectBuf(b);
}

bool MutualPattern::intersectBufIf(const ByteArr& other, bool do_cnt) {
	auto mutual_len = intersectLen(other);
	if (mutual_len && mutual_len < size_mutual_) {
		intersectBuf(other);
		cnt += do_cnt;
		return true;
	}
	else if (mutual_len == size_mutual_)
		cnt += do_cnt;
	return false;
}

void MutualPattern::intersectBuf(const ByteArr& other) {
	uint first_mutual = numeric_limits<uint>::max();
	uint last_mutual = first_mutual;
	for (uint i=first_mutual_; i < mutual_till_; i++) {
		if (!is_mutual[i]) continue;

		if (data[i] != other[i]) {
			data[i] = 63;
			is_mutual[i] = false;
			size_mutual_--;
		} else {
			first_mutual = min(first_mutual, i);
			last_mutual = i;
		}
	}
	first_mutual_ = first_mutual;
	mutual_till_ = last_mutual+1;
}

uint MutualPattern::intersectLen(const ByteArr& other) {
	return intersectLen(&other[0]);
}

uint MutualPattern::intersectLen(const uchar* other) {
	uint sum = 0;
//	cout << first_mutual_ << ' ' << last_mutual_ << '\n';
	for (uint i=first_mutual_; i < mutual_till_; i++) {
		if (!is_mutual[i]) continue;
		if (data[i] == other[i]) sum++;
	}
	return sum;
}

bool MutualPattern::doesMatch(const uchar* buf) {
	return intersectLen(buf) == size_mutual_;
}

bool MutualPattern::doesMatchApprox(const uchar* buf) {
	vector<uchar> mutual;
	for (uint i=first_mutual_; i < mutual_till_; i++) {
		if (!is_mutual[i]) continue;
		if (data[i] == buf[i]) mutual.emplace_back(data[i]);
	}

	if (mutual.size() == size_mutual_) return true;
	if ((double) mutual.size() / size_mutual_ < 0.79) return false;

	return calcEntropy(mutual) > 0.75;
}

vector<uchar> MutualPattern::getDistinct() const {
	vector<uchar> r;
	for (uint i=first_mutual_; i < mutual_till_; i++) {
		if (!is_mutual[i]) continue;
		r.emplace_back(data[i]);
	}
	return r;
}

std::ostream& operator<<(std::ostream& out, const MutualPattern& mp) {
	for (uint i=0; i < mp.data.size(); i++) {
		if (i % 4 == 0) out << ' ';
		out << (mp.is_mutual[i]? mkHexStr(&mp.data[i], 1) : "__");
	}
	return out;
}

bool operator==(const MutualPattern& a, const MutualPattern& b) {
	for (uint i=0; i < a.data.size(); i++)
		if (a.data[i] != b.data[i]) return false;
	return true;
}

bool operator!=(const MutualPattern& a, const MutualPattern& b) {
	return !(a == b);
}
