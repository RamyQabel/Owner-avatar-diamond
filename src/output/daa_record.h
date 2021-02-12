/****
DIAMOND protein aligner
Copyright (C) 2013-2017 Benjamin Buchfink <buchfink@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#ifndef DAA_RECORD_H_
#define DAA_RECORD_H_

#include <limits>
#include "daa_file.h"
#include "../basic/packed_sequence.h"
#include "../basic/value.h"
#include "../basic/translate.h"
#include "../basic/packed_transcript.h"
#include "../stats/score_matrix.h"
#include "../basic/match.h"

using std::string;
using std::vector;

/*void translate_query(const vector<Letter>& query, vector<Letter> *context)
{
	context[0] = query;
	context[1] = Translator::reverse(query);
}*/

inline void translate_query(const vector<Letter>& query, vector<Letter> *context)
{
	Translator::translate(query, context);
}

struct DAA_query_record
{

	struct Match : public Hsp
	{

		Match(const DAA_query_record &query_record) :
			hit_num(std::numeric_limits<uint32_t>::max()),
			subject_id(std::numeric_limits<uint32_t>::max()),
			parent_(query_record)
		{ }

		Hsp_context context()
		{
			return Hsp_context(*this,
				(unsigned)parent_.query_num,
				parent_.query_seq,
				parent_.query_name.c_str(),
				subject_id,
				subject_id,
				subject_name.c_str(),
				subject_len,
				hit_num,
				hsp_num,
				Sequence());
		}

		uint32_t hsp_num, hit_num, subject_id, subject_len;
		string subject_name;

	private:

		const DAA_query_record &parent_;
		friend BinaryBuffer::Iterator& operator>>(BinaryBuffer::Iterator &it, Match &r);

	};

	struct Match_iterator
	{
		Match_iterator(const DAA_query_record &parent, BinaryBuffer::Iterator it) :
			r_(parent),
			it_(it),
			good_(true)
		{
			operator++();
		}
		Match& operator*()
		{
			return r_;
		}
		Match* operator->()
		{
			return &r_;
		}
		bool good() const
		{
			return good_;
		}
		Match_iterator& operator++()
		{
			if (it_.good()) it_ >> r_; else good_ = false; return *this;
		}
	private:
		Match r_;
		BinaryBuffer::Iterator it_;
		bool good_;
	};

	DAA_query_record(const DAA_file& file, const BinaryBuffer &buf, size_t query_num):
		query_num (query_num),
		file_(file),
		it_(init(buf))
	{ }

	Match_iterator begin() const
	{
		return Match_iterator(*this, it_);
	}

	size_t query_len() const
	{
		return align_mode.query_translated ? source_seq.size() : context[0].size();
	}

	string query_name;
	size_t query_num;
	vector<Letter> source_seq;
	vector<Letter> context[6];
	TranslatedSequence query_seq;

private:

	BinaryBuffer::Iterator init(const BinaryBuffer &buf);

	const DAA_file& file_;
	const BinaryBuffer::Iterator it_;
	
	friend BinaryBuffer::Iterator& operator>>(BinaryBuffer::Iterator &it, Match &r);

};

BinaryBuffer::Iterator& operator>>(BinaryBuffer::Iterator &it, DAA_query_record::Match &r);

#endif /* DAA_RECORD_H_ */
