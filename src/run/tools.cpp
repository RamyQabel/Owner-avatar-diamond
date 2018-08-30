/****
DIAMOND protein aligner
Copyright (C) 2013-2017 Benjamin Buchfink <buchfink@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
****/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <set>
#include <iostream>
#include <sstream>
#include "tools.h"
#include "../basic/config.h"
#include "../data/sequence_set.h"
#include "../util/seq_file_format.h"
#include "../data/queries.h"
#include "../data/load_seqs.h"
#include "../data/reference.h"
#include "../extra/match_file.h"
#include "../basic/masking.h"
#include "../dp/dp.h"
#include "../util/tinythread.h"
#include "../basic/packed_transcript.h"

using std::cout;
using std::endl;

void get_seq()
{
	DatabaseFile db_file;
	db_file.get_seq();
}

void random_seqs()
{
	DatabaseFile db_file;
	vector<unsigned> v;
	db_file.load_seqs(v, std::numeric_limits<size_t>::max(), true);
	cout << "Sequences = " << ref_seqs::get().get_length() << endl;
	std::set<unsigned> n;
	const size_t count = atoi(config.seq_no[0].c_str());
	while (n.size() < count)
		n.insert((rand()*RAND_MAX + rand()) % ref_seqs::get().get_length());
	OutputFile out(config.output_file);
	unsigned j = 0;
	
	std::string s;
	for (std::set<unsigned>::const_iterator i = n.begin(); i != n.end(); ++i) {
		std::stringstream ss;
		ss << '>' << j++ << endl;
		if (config.reverse)
			; // ref_seqs::get()[*i].print(ss, value_traits, sequence::Reversed());
		else
			ss << ref_seqs::get()[*i];
		ss << endl;
		s = ss.str();
		out.write(s.data(), s.length());
	}
	out.close();
}

void sort_file()
{
	TextInputFile f(config.query_file);
	vector<Pair<unsigned, string> > data;
	while (f.getline(), !f.eof()) {
		unsigned query;
		sscanf(f.line.c_str(), "%u", &query);
		data.push_back(Pair<unsigned, string>(query, f.line));
	}
	std::stable_sort(data.begin(), data.end());
	for (vector<Pair<unsigned, string> >::const_iterator i = data.begin(); i != data.end(); ++i)
		cout << i->second << endl;
	f.close();
}

void db_stat()
{
	DatabaseFile db_file;
	vector<unsigned> v;
	db_file.load_seqs(v, std::numeric_limits<size_t>::max(), true);
	cout << "Sequences = " << ref_seqs::get().get_length() << endl;

	size_t letters = 0;
	vector<size_t> letter_freq(20);
	for (size_t i = 0; i < ref_seqs::get().get_length(); ++i) {
		const sequence seq = ref_seqs::get()[i];
		for (size_t j = 0; j < seq.length(); ++j) {
			if (seq[j] < 20) {
				++letters;
				++letter_freq[(int)seq[j]];
			}
		}
	}
	cout << "Frequencies = ";
	for (vector<size_t>::const_iterator i = letter_freq.begin(); i != letter_freq.end(); ++i)
		cout << (double)*i / letters << ',';
	cout << endl;

}

void match_file_stat()
{
	match_file file(config.match_file1.c_str());
	blast_match match;
	while (file.get(match, blast_format()));
	file.get_subst();
}

void run_masker()
{
	TextInputFile f(config.query_file);
	vector<Letter> seq, seq2;
	vector<char> id;
	const FASTA_format format;
	while (format.get_seq(id, seq, f)) {
		cout << '>' << string(id.data(), id.size()) << endl;
		seq2 = seq;
		Masking::get()(seq2.data(), seq2.size());
		/*for (size_t i = 0; i < seq.size(); ++i) {
			char c = value_traits.alphabet[(long)seq[i]];
			if (seq2[i] == value_traits.mask_char)
				c = tolower(c);
			cout << c;
		}
		cout << endl;*/
		cout << sequence(seq2.data(), seq2.size()) << endl;
	}
}

void fastq2fasta()
{
	auto_ptr<TextInputFile> f(new TextInputFile(config.query_file));
	vector<Letter> seq;
	vector<char> id;
	const FASTQ_format format;
	input_value_traits = value_traits = nucleotide_traits;
	size_t n = 0, max = atoi(config.seq_no[0].c_str());
	while (n < max && format.get_seq(id, seq, *f)) {
		cout << '>' << string(id.data(), id.size()) << endl;
		cout << sequence(seq.data(), seq.size()) << endl;
		++n;
	}
}

void test_io()
{
	const size_t buf_size = 1;
	Timer t;
	t.start();
	/*InputFile f(config.query_file, InputFile::BUFFERED);
	char buf[buf_size];	
	size_t total = 0;
	while (f.read(buf, buf_size) == buf_size)
		total += buf_size;*/

	DatabaseFile db;
	vector<unsigned> v;
	db.load_seqs(v, std::numeric_limits<size_t>::max(), true);

	size_t total = ref_seqs::get().raw_len() + ref_ids::get().raw_len();
	cout << "MBytes/sec = " << total / 1e6 / t.getElapsedTime() << endl;
	cout << "Time = " << t.getElapsedTime() << "s" << endl;
}

void read_sim()
{
	const double ID = 0.35;
	srand((unsigned)time(0));
	TextInputFile in(config.query_file);
	OutputFile out(config.output_file);
	FASTA_format format;
	vector<char> id, seq;
	input_value_traits = nucleotide_traits;
	TextBuffer buf;
	while (format.get_seq(id, seq, in)) {
		buf << '>' << id << '\n';
		for (size_t i = 0; i < seq.size(); ++i) {
			if ((double)rand() / RAND_MAX <= ID)
				buf << nucleotide_traits.alphabet[(size_t)seq[i]];
			else
				buf << nucleotide_traits.alphabet[rand() % 4];
		}
		buf << '\n';
		out.write(buf.get_begin(), buf.size());
		buf.clear();
	}
	out.close();
}

void info()
{
	vector<string> arch_flags;
#ifdef __SSE2__
	arch_flags.push_back("sse2");
#endif
#ifdef __SSE3__
	arch_flags.push_back("sse3");
#endif
#ifdef __SSSE3__
	arch_flags.push_back("ssse3");
#endif
#ifdef __POPCNT__
	arch_flags.push_back("popcnt");
#endif

	cout << "Architecture flags: ";
	for (vector<string>::const_iterator i = arch_flags.begin(); i != arch_flags.end(); ++i)
		cout << *i << ' ';
	cout << endl;
}

void pairwise_worker(TextInputFile *in, tthread::mutex *input_lock, tthread::mutex *output_lock) {
	FASTA_format format;
	vector<char> id_r, id_q, ref, query;

	while(true) {
		input_lock->lock();
		if (!format.get_seq(id_r, ref, *in)) {
			input_lock->unlock();
			return;
		}
		if (!format.get_seq(id_q, query, *in)) {
			input_lock->unlock();
			return;
		}
		input_lock->unlock();
		const string ir = blast_id(string(id_r.data(), id_r.size())), iq = blast_id(string(id_q.data(), id_q.size()));
		Hsp hsp;
		smith_waterman(sequence(query), sequence(ref), hsp);
		Hsp_context context(hsp, 0, TranslatedSequence(query), "", 0, 0, "", 0, 0, 0, sequence());
		Hsp_context::Iterator it = context.begin();
		std::stringstream ss;
		while (it.good()) {
			if (it.op() == op_substitution)
				ss << ir << '\t' << iq << '\t' << it.subject_pos << '\t' << it.query_pos.translated << '\t' << it.query_char() << endl;
			else if (it.op() == op_deletion)
				ss << ir << '\t' << iq << '\t' << it.subject_pos << '\t' << "-1" << '\t' << '.' << endl;
			++it;
		}
		output_lock->lock();
		cout << ss.str();
		output_lock->unlock();
	}
}

void pairwise()
{
	input_value_traits = nucleotide_traits;
	value_traits = nucleotide_traits;;
	score_matrix = Score_matrix("DNA", 4, 2, 0);

	TextInputFile in(config.query_file);
	tthread::mutex input_lock, output_lock;
	Thread_pool threads;
	for (unsigned i = 0; i < config.threads_; ++i) {
		threads.push_back(launch_thread(pairwise_worker, &in, &input_lock, &output_lock));
	}
	threads.join_all();
}
