#include "mutual_pattern.h"

#include <numeric>
using namespace std;

MutualPattern::MutualPattern(ByteArr& a, ByteArr& b) {
	is_mutual_.resize(a.size(), true);
	mutual_till_ = a.size();
	size_mutual_ = a.size();
	size_mutual_half_ = a.size() / 2;
	data_ = a;
	intersectBuf(b);
}

bool MutualPattern::intersectBufIf(const ByteArr& other, bool do_cnt) {
	cnt_all_ += do_cnt;
	auto mutual_len = intersectLen(other);
	if (mutual_len && mutual_len < size_mutual_) {
		intersectBuf(other);
		cnt_ += do_cnt;
		return true;
	}
	else if (mutual_len == size_mutual_)
		cnt_ += do_cnt;
	return false;
}

void MutualPattern::intersectBuf(const ByteArr& other) {
	uint first_mutual = numeric_limits<uint>::max();
	uint last_mutual = first_mutual;
	for (uint i=first_mutual_; i < mutual_till_; i++) {
		if (!is_mutual_[i]) continue;

		if (data_[i] != other[i]) {
			data_[i] = 63;
			is_mutual_[i] = false;
			size_mutual_--;
			if (i >= data_.size() / 2) size_mutual_half_--;
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
		if (!is_mutual_[i]) continue;
		if (data_[i] == other[i]) sum++;
	}
	return sum;
}

uint MutualPattern::intersectLenHalf(const uchar* other) {
	uint start_idx = data_.size() / 2, sum = 0;
	for (uint i=start_idx; i < mutual_till_; i++) {
		if (!is_mutual_[i]) continue;
		if (data_[i] == other[i-start_idx]) sum++;
	}
	return sum;
}

bool MutualPattern::doesMatch(const uchar* buf) {
	return intersectLen(buf) == size_mutual_;
}

bool MutualPattern::doesMatchHalf(const uchar* buf) {
//	auto r = intersectLenHalf(buf);
//	cout << r << " == " << size_mutual_ << '\n';
	return intersectLenHalf(buf) == size_mutual_half_;
}

bool MutualPattern::doesMatchApprox(const uchar* buf) {
	vector<uchar> mutual;
	for (uint i=first_mutual_; i < mutual_till_; i++) {
		if (!is_mutual_[i]) continue;
		if (data_[i] == buf[i]) mutual.emplace_back(data_[i]);
	}

	if (mutual.size() == size_mutual_) return true;
	if ((double) mutual.size() / size_mutual_ < 0.79) return false;

	return calcEntropy(mutual) > 0.75;
}

vector<uchar> MutualPattern::getDistinct() const {
	vector<uchar> r;
	for (uint i=first_mutual_; i < mutual_till_; i++) {
		if (!is_mutual_[i]) continue;
		r.emplace_back(data_[i]);
	}
	return r;
}

double MutualPattern::successRate() {
	return (double)cnt_ / cnt_all_;
}

std::ostream& operator<<(std::ostream& out, const MutualPattern& mp) {
	for (uint i=0; i < mp.data_.size(); i++) {
		if (i % 4 == 0) out << ' ';
		out << (mp.is_mutual_[i]? mkHexStr(&mp.data_[i], 1) : "__");
	}
	return out;
}

bool operator==(const MutualPattern& a, const MutualPattern& b) {
	for (uint i=0; i < a.data_.size(); i++)
		if (a.data_[i] != b.data_[i]) return false;
	return true;
}

bool operator!=(const MutualPattern& a, const MutualPattern& b) {
	return !(a == b);
}


// utility functions

patterns_t genRawPatterns(buffs_t buffs) {
	patterns_t patterns;
	auto gen = getRandomGenerator();
	auto dis = uniform_int_distribution<size_t>(0, buffs.size()-1);
	shuffle(buffs.begin(), buffs.end(), gen);

	for (uint i=0; i+1 < buffs.size(); i++) {
		auto a = buffs[i], b = buffs[i+1];
		auto p = MutualPattern(a, b);
		if (!p.size_mutual_) continue;

		for (int n=4; n; n--) {  // remove (some) noise
			uint idx = dis(gen);
			p.intersectBufIf(buffs[idx]);
		}

		if (!contains(patterns, p))
			patterns.emplace_back(p);
	}
	return patterns;
}

/* also further intersects patterns as needed */
void countPatternsSuccess(patterns_t& patterns, buffs_t buffs) {
	for (auto& buff : buffs) {
		for (auto it = patterns.begin(); it != patterns.end();)
			if (it->intersectBufIf(buff, true) && count(patterns.begin(), patterns.end(), *it) > 1)
				it = patterns.erase(it);
			else it++;
	}
}

void filterBySuccessRate(patterns_t& patterns, const string& label) {
	double total_p = 0;
	for (auto it = patterns.begin(); it != patterns.end();) {
		auto p = it->successRate();
		if (p < 0.2) patterns.erase(it);
		else  {
			it++;
			total_p += p;
		}
	}

	if (total_p > 1.1) {  // probably just random noise
		logg(V, "ignoring all ", patterns.size(), " patterns for ", label, ".. they overlap too much (", total_p, ")\n");
		patterns.clear();
	}
}
