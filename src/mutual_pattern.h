#ifndef MUTUAL_PATTERN_H
#define MUTUAL_PATTERN_H

#include "common.h"

class MutualPattern {
using ByteArr = std::vector<uchar>;
friend bool operator==(const MutualPattern& a, const MutualPattern& b);

public:
    friend std::ostream& operator<<(std::ostream& out, const MutualPattern& mp);
	MutualPattern(ByteArr& a, ByteArr& b);
	bool intersectBufIf(const ByteArr& buf, bool do_cnt=false);  // if len(intersect) not zero
	int cnt_ = 0, cnt_all_ = 0;
	uint size_mutual_;
	bool doesMatch(const uchar* buf);
	bool doesMatchApprox(const uchar* buf);

	std::vector<uchar> getDistinct() const;

	double successRate();

private:
	std::vector<bool> is_mutual;
	std::vector<uchar> data;

	void intersectBuf(const ByteArr& buf);
	uint intersectLen(const ByteArr& buf);
	uint intersectLen(const uchar* buf);

	uint first_mutual_ = 0;
	uint mutual_till_;
};

std::ostream& operator<<(std::ostream& out, const MutualPattern& mp);

bool operator==(const MutualPattern& a, const MutualPattern& b);
bool operator!=(const MutualPattern& a, const MutualPattern& b);

using patterns_t = std::vector<MutualPattern>;

patterns_t genRawPatterns(buffs_t buffs);
void countPatternsSuccess(patterns_t& patterns, buffs_t buffs);
void filterBySuccessRate(patterns_t& patterns, const std::string& label);

#endif // MUTUAL_PATTERN_H
