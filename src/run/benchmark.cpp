/****
Copyright (c) 2016-2017, Benjamin Buchfink
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
****/

#include "../basic/sequence.h"
#include "../data/sequence_set.h"
#include "../dp/floating_sw.h"
#include "../util/Timer.h"
#include "../dp/dp.h"
#include "../align/align.h"
#include "../align/extend_ungapped.h"
#include "../search/sse_dist.h"
#include "../dp/score_profile.h"
#include "../output/output_format.h"

void benchmark_cmp()
{
#ifdef __SSE2__
	const size_t n = 1000000000llu;
	__m128i r1 = _mm_set_epi8(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16);
	const __m128i r2 = _mm_set_epi8(0, 2, 3, 0, 0, 0, 0, 8, 0, 0, 0, 0, 13, 14, 0, 16);
	Timer t;
	t.start();
	unsigned x = 0;
	for (size_t i = 0; i < n; ++i) {
		r1 = _mm_set_epi32(x, x, x, x);
		//x += popcount32(_mm_movemask_epi8(_mm_cmpeq_epi8(r1, r2)));
		x += _mm_movemask_epi8(_mm_cmpeq_epi8(r1, r2));
	}
	cout << "x=" << x << " t=" << t.getElapsedTimeInMicroSec() * 1000 / n << endl;
#endif
}

int xdrop_ungapped2(const Letter *query, const Letter *subject)
{
	int score(0), st(0);

	const Letter *q(query), *s(subject);

	st = score;
	while (score - st < config.raw_ungapped_xdrop
		&& *q != '\xff'
		&& *s != '\xff')
	{
		st += score_matrix(*q, *s);
		score = std::max(score, st);
		++q;
		++s;

		st += score_matrix(*q, *s);
		score = std::max(score, st);
		++q;
		++s;

		st += score_matrix(*q, *s);
		score = std::max(score, st);
		++q;
		++s;
	}
	return score;
}

int xdrop_window(const Letter *query, const Letter *subject)
{
	static const int window = 40;
	int score(0), st(0), n = 0;

	const Letter *q(query), *s(subject);

	st = score;
	while (n < window
		&& *q != '\xff'
		&& *s != '\xff')
	{
		st += score_matrix(*q, *s);
		score = std::max(score, st);
		++q;
		++s;
		++n;

		st += score_matrix(*q, *s);
		score = std::max(score, st);
		++q;
		++s;
		++n;

		st += score_matrix(*q, *s);
		score = std::max(score, st);
		++q;
		++s;
		++n;
	}
	return st;
}

int xdrop_window2(const Letter *query, const Letter *subject)
{
	static const int window = 40;
	int score(0), st(0), n = 0, i = 0;

	const Letter *q(query), *s(subject);

	st = score;
	while (n < window
		&& *q != '\xff'
		&& *s != '\xff')
	{
		st += score_matrix(*q, *s);
		if (st > score) {
			score = st;
			i = n;
		}
		++q;
		++s;
		++n;

		st += score_matrix(*q, *s);
		if (st > score) {
			score = st;
			i = n;
		}
		++q;
		++s;
		++n;

		st += score_matrix(*q, *s);
		if (st > score) {
			score = st;
			i = n;
		}
		++q;
		++s;
		++n;
	}
	return st*i;
}

void benchmark_ungapped(const Sequence_set &ss, unsigned qa, unsigned sa)
{
	static const size_t n = 100000000llu;
	Timer t;
	t.start();

	const Letter *q = &ss[0][qa], *s = &ss[1][sa];
	int score=0;

	uint64_t mask = 0;
	for (unsigned i = 0; i < 64; ++i) {
		if (q[i] == '\xff' || s[i] == '\xff')
			break;
		if (q[i] == s[i])
			mask |= 1llu << i;
	}

	for (size_t i = 0; i < n; ++i) {

		score += xdrop_window2(q, s);
		//score += binary_ungapped(mask);

	}
	t.stop();

	cout << score << endl;
	cout << "t=" << t.getElapsedTimeInMicroSec() * 1000 / n << " ns" << endl;
}

void benchmark_greedy(const Sequence_set &ss, unsigned qa, unsigned sa)
{
	static const unsigned n = 10000;
	vector<Seed_hit> d;
	d.push_back(Seed_hit(0, 0, sa, qa, Diagonal_segment()));
	Long_score_profile qp(ss[0]);
	//greedy_align(ss[0], qp, ss[1], d[0], true);
	//greedy_align(ss[0], qp, ss[1], qa, sa, true);
	Hsp_data hsp;
	Hsp_traits traits;
	Bias_correction query_bc(ss[0]);
	greedy_align(ss[0], qp, query_bc, ss[1], d.begin(), d.end(), true, &hsp, traits);
	/*Text_buffer buf;
	Pairwise_format().print_match(Hsp_context(hsp, 0, ss[0], ss[0], "", 0, 0, "", 0, 0, 0), buf);
	buf << '\0';
	cout << buf.get_begin();*/
	Timer t;
	t.start();

	for (unsigned i = 0; i < n; ++i) {

		greedy_align(ss[0], qp, query_bc, ss[1], d.begin(), d.end(), false, &hsp, traits);
		hsp.score = 0;

	}
	t.stop();

	cout << " usec=" << t.getElapsedTimeInSec() / (double)n * 1000000.0 << endl;
	cout << "t=" << t.getElapsedTimeInMicroSec() << endl;
}

void benchmark_swipe(const Sequence_set &ss)
{
	static const unsigned n = 1000;
	vector<sequence> seqs;
	vector<int> score(64);
	for (int i = 0; i < 64; ++i)
		seqs.push_back(ss[1]);
	swipe(ss[0], seqs.begin(), seqs.end(), score.begin());
	cout << "Score = " << score[0] << endl;
	cout << needleman_wunsch(ss[0], ss[1], score[0], Local(), int()) << endl;
	return;
	Timer t;
	t.start();

	for (unsigned i = 0; i < n; ++i) {

		swipe(ss[0], seqs.begin(), seqs.end(), score.begin());

	}
	t.stop();

	cout << "usec=" << t.getElapsedTimeInSec() / (double)n * 1000000.0 << endl;
	cout << "gcups=" << 64*ss[0].length()*ss[1].length()*n / t.getElapsedTime() / 1e9 << endl;
}

void benchmark_floating(const Sequence_set &ss, unsigned qa, unsigned sa)
{
	static const unsigned n = 100000;
	uint64_t cell_updates = 0;
	local_match hsp(0, 0, &ss[1][sa]);

	{
		Timer t;
		t.start();

		for (unsigned i = 0; i < n; ++i) {

			floating_sw(&ss[0][qa],
				hsp.subject_,
				hsp,
				16,
				score_matrix.rawscore(config.gapped_xdrop),
				score_matrix.gap_open() + score_matrix.gap_extend(),
				score_matrix.gap_extend(),
				cell_updates,
				hsp.query_anchor_,
				hsp.subject_anchor,
				0,
				No_score_correction(),
				Score_only());

		}
		t.stop();

		cout << hsp.score << ' ' << cell_updates << endl;
		cout << "gcups=" << (double)cell_updates / 1e9 / t.getElapsedTimeInSec() << " n/sec=" << (double)n / t.getElapsedTimeInSec() << endl;
		cout << " usec=" << t.getElapsedTimeInSec() / (double)n * 1000000.0 << endl;
	}
}

void benchmark_sw()
{
	Sequence_set ss;
	vector<Letter> s1, s2;
	unsigned qa = 0, sa = 0;
	goto aln1;
	
	/*
	> d2va1a_ c.73.1.0 (A:) automated matches {Ureaplasma parvum [TaxId: 
134821]}
Length=234

 Score = 26.2 bits (56),  Expect = 1.1
 Identities = 18/66 (27%), Positives = 28/66 (42%), Gaps = 4/66 (6%)

Query  24  QADATVATFFNGIDMPNQTNKTAA--FLCAALGGPNAWTGRNLKE--VHANMGVSNAQFT  79
           Q D+++  F    D+  Q  K +    +   LGG N W G   KE  +  N+  +     
Sbjct  16  QNDSSIIDFIKINDLAEQIEKISKKYIVSIVLGGGNIWRGSIAKELDMDRNLADNMGMMA  75

Query  80  TVIGHL  85
           T+I  L
Sbjct  76  TIINGL  81	*/
	
	s1 = sequence::from_string("SLFEQLGGQAAVQAVTAQFYANIQADATVATFFNGIDMPNQTNKTAAFLCAALGGPNAWTGRNLKEVHANMGVSNAQFTTVIGHLRSALTGAGVAAALVEQTVAVAETVRGDVVTV");
	s2 = sequence::from_string("RKQRIVIKISGACLKQNDSSIIDFIKINDLAEQIEKISKKYIVSIVLGGGNIWRGSIAKELDMDRNLADNMGMMATIINGLALENALNHLNVNTIVLSAIKCDKLVHESSANNIKKAIEKEQVMIFVAGTGFPYFTTDSCAAIRAAETESSIILMGKNGVDGVYDSDPKINPNAQFYEHITFNMALTQNLKVMDATALALCQENNINLLVFNIDKPNAIVDVLEKKNKYTIVSK");
	qa = 23;
	sa = 15;
	goto ende;	

	/*

	Query= d1g2na_ a.123.1.1 (A:) Ultraspiracle protein, usp {Tobacco budworm
	(Heliothis virescens) [TaxId: 7102]}

	> sp|Q6DHP9|RXRGB_DANRE Retinoic acid receptor RXR-gamma-B OS=Danio
	rerio GN=rxrgb PE=2 SV=1
	Length=452

	Score = 189 bits (479),  Expect = 6e-055
	Identities = 101/249 (41%), Positives = 153/249 (61%), Gaps = 24/249 (10%)

	Query  4    QELSIERLLEMESLVADPSEEFQFLRVGPDSNVPPKFRAPVSSLCQIGNKQIAALVVWAR  63
	            +++ ++++L+ E  V   +E +       +S+       PV+++C   +KQ+  LV WA+
	Sbjct  221  EDMPVDKILDAELSVEPKTETYT------ESSPSNSTNDPVTNICHAADKQLFTLVEWAK  274

	Query  64   DIPHFSQLEMEDQILLIKGSWNELLLFAIAWRSMEFLTEERDGVDGTGNRTTSPPQLMCL  123
	             IPHFS L ++DQ++L++  WNELL+ + + RS+      +DG+               L
	Sbjct  275  RIPHFSDLPLDDQVILLRAGWNELLIASFSHRSITV----KDGI--------------LL  316

	Query  124  MPGMTLHRNSALQAGVGQIFDRVLSELSLKMRTLRVDQAEYVALKAIILLNPDVKGLKNR  183
	              G+ +HR+SA  AGVG IF+RVL+EL  KM+ +++D+ E   L+AI+L NPD KGL N
	Sbjct  317  GTGLHVHRSSAHSAGVGSIFNRVLTELVSKMKDMQMDKTELGCLRAIVLFNPDAKGLSNS  376

	Query  184  QEVEVLREKMFLCLDEYCRRSRSSEEGRFAALLLRLPALRSISLKSFEHLFFFHLVADTS  243
	             EVE LREK++  L+ Y ++    + GRFA LLLRLPALRSI LK  EHLFFF L+ DT
	Sbjct  377  LEVEALREKVYASLETYTKQKYPDQPGRFAKLLLRLPALRSIGLKCLEHLFFFKLIGDTP  436

	Query  244  IAGYIRDAL  252
	            I  ++ + L
	Sbjct  437  IDTFLMEML  445
	*/

	s1 = sequence::from_string("aavqelsierllemeslvadpseefqflrvgpdsnvppkfrapvsslcqignkqiaalvv\
wardiphfsqlemedqillikgswnelllfaiawrsmeflteerdgvdgtgnrttsppql\
mclmpgmtlhrnsalqagvgqifdrvlselslkmrtlrvdqaeyvalkaiillnpdvkgl\
knrqevevlrekmflcldeycrrsrsseegrfaalllrlpalrsislksfehlfffhlva\
dtsiagyirdalrnha");

	s2 = sequence::from_string("MDTHDTYLHLHSSPLNSSPSQPPVMSSMVGHPSVISSSRPLPSPMSTLGSSMNGLPSPYS\
VITPSLSSPSISLPSTPSMGFNTLNSPQMNSLSMNGNEDIKPPPGLAPLGNMSSYQCTSP\
GSLSKHICAICGDRSSGKHYGVYSCEGCKGFFKRTIRKDLTYTCRDIKECLIDKRQRNRC\
QYCRYQKCLAMGMKREAVQEERQRGKEKSDTEVETTSRFNEDMPVDKILDAELSVEPKTE\
TYTESSPSNSTNDPVTNICHAADKQLFTLVEWAKRIPHFSDLPLDDQVILLRAGWNELLI\
ASFSHRSITVKDGILLGTGLHVHRSSAHSAGVGSIFNRVLTELVSKMKDMQMDKTELGCL\
RAIVLFNPDAKGLSNSLEVEALREKVYASLETYTKQKYPDQPGRFAKLLLRLPALRSIGL\
KCLEHLFFFKLIGDTPIDTFLMEMLEAPHQIT");

	qa = 3;
	sa = 220;

	goto ende;

	/*Query= d1mpxa2 c.69.1.21 (A:24-404) Alpha-amino acid ester hydrolase
	{Xanthomonas citri [TaxId: 346]}

	> sp|Q9L9D7|COCE_RHOSM Cocaine esterase OS=Rhodococcus sp. (strain
	MB1 Bresler) GN=cocE PE=1 SV=1
	Length=574

	Score = 94.0 bits (232),  Expect = 1e-019
	Identities = 103/380 (27%), Positives = 157/380 (41%), Gaps = 51/380 (13%)

	Query  20   NDYIKREVMIPMRDGVKLHTVIVLPKGAKNAPIVLTRTPYDASGRTERLA-SPHMKDLLS  78
	            N  +   VM+PMRDGV+L   +  P      P++L R PYD   + +  A S    + L
	Sbjct  5    NYSVASNVMVPMRDGVRLAVDLYRPDADGPVPVLLVRNPYD---KFDVFAWSTQSTNWLE  61

	Query  79   AGDDVFVEGGYIRVFQDVRGKYGSEGDYVMTRPLRGPLNPSEVDHATDAWDTIDWLVKNV  138
	                 FV  GY  V QD RG + SEG++V             VD   DA DT+ W+++
	Sbjct  62   -----FVRDGYAVVIQDTRGLFASEGEFV-----------PHVDDEADAEDTLSWILEQ-  104

	Query  139  SESNGKVGMIGSSYEGFTVVMALTNPHPALKVAVPESPMIDGWMGDDWFNYGAFRQVNFD  198
	            +  +G VGM G SY G T   A  +    LK   P     D +    W  YG    ++ +
	Sbjct  105  AWCDGNVGMFGVSYLGVTQWQAAVSGVGGLKAIAPSMASADLYRA-PW--YGPGGALSVE  161

	Query  199  YFTGQLSKRGKGAGIARQG--HDDYSNFLQ-AGSAGDFAKAAGLEQL----------PW-  244
	               G  +  G G   +R     +D ++F+Q A    D A AA +  L          PW
	Sbjct  162  ALLGWSALIGTGLITSRSDARPEDAADFVQLAAILNDVAGAASVTPLAEQPLLGRLIPWV  221

	Query  245  WHKLTEHAAYDAFWQEQALDKVMA--RTPLKVPTMWLQGLWDQEDMWGAIHSYAAMEPRD  302
	              ++ +H   D  WQ  +L + +    TP  +   W  G   +     ++ ++ A+
	Sbjct  222  IDQVVDHPDNDESWQSISLFERLGGLATPALITAGWYDGFVGE-----SLRTFVAV----  272

	Query  303  KRNTLNYLVMGPWRHSQVNYDGSALGALNFEGDTARQFRHDVLRPFFDQYL-VDGAPKAD  361
	            K N    LV+GPW HS +    +A            Q    + + FFD++L  +    A
	Sbjct  273  KDNADARLVVGPWSHSNLT-GRNADRKFGIAATYPIQEATTMHKAFFDRHLRGETDALAG  331

	Query  362  TPPVFIYNTGENHWDRLKAW  381
	             P V ++  G + W     W
	Sbjct  332  VPKVRLFVMGIDEWRDETDW  351

	*/

	s1 = sequence::from_string("tspmtpditgkpfvaadasndyikrevmipmrdgvklhtvivlpkgaknapivltrtpyd\
asgrterlasphmkdllsagddvfveggyirvfqdvrgkygsegdyvmtrplrgplnpse\
vdhatdawdtidwlvknvsesngkvgmigssyegftvvmaltnphpalkvavpespmidg\
wmgddwfnygafrqvnfdyftgqlskrgkgagiarqghddysnflqagsagdfakaagle\
qlpwwhkltehaaydafwqeqaldkvmartplkvptmwlqglwdqedmwgaihsyaamep\
rdkrntlnylvmgpwrhsqvnydgsalgalnfegdtarqfrhdvlrpffdqylvdgapka\
dtppvfiyntgenhwdrlkaw");

	s2 = sequence::from_string("MVDGNYSVASNVMVPMRDGVRLAVDLYRPDADGPVPVLLVRNPYDKFDVFAWSTQSTNWL\
EFVRDGYAVVIQDTRGLFASEGEFVPHVDDEADAEDTLSWILEQAWCDGNVGMFGVSYLG\
VTQWQAAVSGVGGLKAIAPSMASADLYRAPWYGPGGALSVEALLGWSALIGTGLITSRSD\
ARPEDAADFVQLAAILNDVAGAASVTPLAEQPLLGRLIPWVIDQVVDHPDNDESWQSISL\
FERLGGLATPALITAGWYDGFVGESLRTFVAVKDNADARLVVGPWSHSNLTGRNADRKFG\
IAATYPIQEATTMHKAFFDRHLRGETDALAGVPKVRLFVMGIDEWRDETDWPLPDTAYTP\
FYLGGSGAANTSTGGGTLSTSISGTESADTYLYDPADPVPSLGGTLLFHNGDNGPADQRP\
IHDRDDVLCYSTEVLTDPVEVTGTVSARLFVSSSAVDTDFTAKLVDVFPDGRAIALCDGI\
VRMRYRETLVNPTLIEAGEIYEVAIDMLATSNVFLPGHRIMVQVSSSNFPKYDRNSNTGG\
VIAREQLEEMCTAVNRIHRGPEHPSHIVLPIIKR");

	qa = 19;
	sa = 4;
	goto ende;

	/*

	Query= 488:2:1:298:839

	Length=114

	>sp|Q820R1|RS3_NITEU 30S ribosomal protein S3 OS=Nitrosomonas europaea (strain ATCC 19718 / NBRC 14298) GN=rpsC PE=3 SV=1
Length=215

 Score = 46.6 bits (109),  Expect = 6.8e-05
 Identities = 22/34 (64%), Positives = 27/34 (79%), Gaps = 5/34 (14%)
 Frame = 2

Query    1  PLHTLRADIDYGT--ARALYPGAGIIGVQVWIYK 32
            PLHTLRA++DYGT  AR  Y   GIIGV+VW++K
Sbjct  174  PLHTLRAEVDYGTSEARTTY---GIIGVKVWVFK 204
	*/

aln1:

	s1 = sequence::from_string("PLHTLRADIDYGTARALYPGAGIIGVQVWIYK");
	s2 = sequence::from_string("MGQKINPTGFRLSVLKNWSSRWYTNTKKFSDFLNEDISVRQYLQKKLAHASVGSIIIERPSKNAKITIHTSRPGVVIGKKGEDIEILRRNVEKLMNVPVHINIEEIRKPEIDAQLIAASITQQLEKRIMFRRAMKRAIQNAMRLGAQGIKIMSSGRLNGIEIARTEWYREGRVPLHTLRAEVDYGTSEARTTYGIIGVKVWVFKGEQLGIKERQN");

	qa = 0;
	sa = 173;
	goto ende;

	/*

	Score = 1694.5 bits (4387),  Expect = 0.0e+00
	Identities = 0/2122 (0%), Positives = 0/2122 (0%), Gaps = 0/2122 (0%)

	Query  4975  PATRPERVPLSFAQQRLWFLHRMQGPSPTYNVPVVLRLDGELHRDALVAAVRDVVVRHES 5034
				 P T+  +  L+ AQ  +WF  ++   +P YN    + ++G ++      A+R V+   ES
	Sbjct     2  PDTKDLQYSLTGAQTGIWFAQQLDPDNPIYNTAEYIEINGPVNIALFEEALRHVIKEAES 61

	Query  5035  LRTVFPDVEGTPYQHVLAEFEPAVSFVD-TTDLDADLTELARHAFDLATELPI------R 5087
				 L   F +    P+Q +    +  +  +D +++ D + T L     DLA  + +
	Sbjct    62  LHVRFGENMDGPWQMINPSPDVQLHVIDVSSEPDPEKTALNWMKADLAKPVDLGYAPLFN 121

	Query  5088  VTVLSTSPTEHALLLLTHHIASDGWSTEPLSRDFAHAYAARTRGEQPE---FTPLPVQYA 5144
				   +    P         HHIA DG+    +++  A  Y A  +G+  +   F  L
	Sbjct   122  EALFIAGPDRFFWYQRIHHIAIDGFGFSLIAQRVASTYTALIKGQTAKSRSFGSLQAILE 181

	Query  5145  DYTLWQQDLLGSEQDPTSLLSRQVEFWRTALADLPELLQLPTDRPRPAVASYEGGALDFE 5204
				 + T    D  GSEQ       +  +FW    AD PE++ L    PR + +     A
	Sbjct   182  EDT----DYRGSEQ-----YEKDRQFWLDRFADAPEVVSLADRAPRTSNSFLRHTAY--- 229

	Query  5205  FTPELHRGVTELAERTGTTVFMVMQAALSTLFTKLGAGTDIPLGTPIAGRTDEALEELVG 5264
				   P     + E A     +   VM A  +    ++    D+ LG P+ GR   A   +
	Sbjct   230  LPPSDVNALKEAARYFSGSWHEVMIAVSAVYVHRMTGSEDVVLGLPMMGRIGSASLNVPA 289

	Query  5265  FFVNTLVLRTDTSGDPGFGQVLERVREANLAAYAHQDVPFERLVEVLNPTRSLAHHPLF- 5323
	               +N L LR   S    F ++++++     +   H     E L   L       +H LF
	Sbjct   290  MVMNLLPLRLTVSSSMSFSELIQQISREIRSIRRHHKYRHEELRRDLKLIGE--NHRLFG 347

	Query  5324  -QVMMTLHNSSADGPGLEGVDTGVA---TVDLQFTLQESFDANGSPAGLGGDVEYATDLF 5379
	              Q+ +   +   D  G+ G    ++     DL   + +  D     +GL  DV+   +++
	Sbjct   348  PQINLMPFDYGLDFAGVRGTTHNLSAGPVDDLSINVYDRTDG----SGLRIDVDANPEVY 403

	Query  5380  GPDSVRLLLTRLETLLAAVVADPRRPISRIDLLTAQERTQVLRTWNDTAREVPALTVPQL 5439
	                 ++L   R+  LL    A     I +++LL  +E+ +V+  WN+TA+    +++  +
	Sbjct   404  SESDIKLHQQRILQLLQTASAGEDMLIGQMELLLPEEKEKVISKWNETAKSEKLVSLQDM 463

	Query  5440  FQAHAQGSPEATALVFGAEQVSYVELNVRANQLAHHLIAQGVGPERIVAVALPRSVDLVV 5499
	             F+  A  +PE  AL+    QV+Y +LN  AN+LA  LI +G+GPE+ VA+ALPRS ++V
	Sbjct   464  FEKQAVLTPERIALMCDDIQVNYRKLNEEANRLARLLIEKGIGPEQFVALALPRSPEMVA 523

	Query  5500  ALLAVLKTGAAYLPIDPGYPAERIGYILADAQPVCLLSTGNGPRT-----EVPTVLLDSA 5554
	             ++L VLKTGAAYLP+DP +PA+RI Y+L DA+P C+++T     +      VP ++LD A
	Sbjct   524  SMLGVLKTGAAYLPLDPEFPADRISYMLEDAKPSCIITTEEIAASLPDDLAVPELVLDQA 583

	Query  5555  AKQAELAELSGADPESTVDLRNPAYTIYTSGSTGRPKGVVVTVGDLANFLAAMTDRIGLT 5614
	               Q  +   S  + + +V L +PAY IYTSGSTGRPKGVVVT   L+NFL +M +   L
	Sbjct   584  VTQEIIKRYSPENQDVSVSLDHPAYIIYTSGSTGRPKGVVVTQKSLSNFLLSMQEAFSLG 643

	Query  5615  PEDRLLAVTTVAFDIAGLEIHLPLVSGARVVLAAESQVRDPAALTALARTSGATVMQATP 5674
	              EDRLLAVTTVAFDI+ LE++LPL+SGA++V+A +  +R+P AL  +       +MQATP
	Sbjct   644  EEDRLLAVTTVAFDISALELYLPLISGAQIVIAKKETIREPQALAQMIENFDINIMQATP 703

	Query  5675  SLWQAALAEDAESLRGMRMLVGGEALPAALAGTMAEVGAEVVNLYGPTETTIWSASAAIS 5734
	             +LW A +  + E LRG+R+LVGGEALP+ L   + ++   V NLYGPTETTIWSA+A +
	Sbjct   704  TLWHALVTSEPEKLRGLRVLVGGEALPSGLLQELQDLHCSVTNLYGPTETTIWSAAAFLE 763

	Query  5735  DG--SVPPIGRPIANTAVYVLDSALAPVPVGVTGELYIAGDGLARGYANRPGLTAERFSA 5792
	             +G   VPPIG+PI NT VYVLD+ L PVP GV GELYIAG GLARGY +RP LTAERF A
	Sbjct   764  EGLKGVPPIGKPIWNTQVYVLDNGLQPVPPGVVGELYIAGTGLARGYFHRPDLTAERFVA 823

	Query  5793  DPFGEPGSRMYRTGDLARWRADGQLEYLARVDDQVKLRGFRIELGEIESVLTAHPEVTRA 5852
	             DP+G PG+RMYRTGD ARWRADG L+Y+ R D Q+K+RGFRIELGEI++VL  HP + +A
	Sbjct   824  DPYGPPGTRMYRTGDQARWRADGSLDYIGRADHQIKIRGFRIELGEIDAVLANHPHIEQA 883

	Query  5853  AVLVREE-----RLVAYVVGSA--DVSGLRALAQSRLPEYMVPSAYVSLDTLPLTPNGKL 5905
	             AV+VRE+     RL AYVV  A  D + LR    + LP+YMVPSA+V +D LPLTPNGKL
	Sbjct   884  AVVVREDQPGDKRLAAYVVADAAIDTAELRRYMGASLPDYMVPSAFVEMDELPLTPNGKL 943

	Query  5906  DRKALPAPDFGAESGGRAARTPAEQVLCGLFADVLGAERVGIEDNFFELGGHSLLATKLI 5965
	             DRKALPAPDF      RA RTP E++LC LFA+VLG  RVGI+D+FFELGGHSLLA +L+
	Sbjct   944  DRKALPAPDFSTSVSDRAPRTPQEEILCDLFAEVLGLARVGIDDSFFELGGHSLLAARLM 1003

	Query  5966  SRIRVALGVEVPIQALFEAPTVAGLAERLTGAATGRRALEPMARPQRVPLSYAQRRLWFL 6025
	             SRIR  +G E+ I  LF+ PTVAGLA  L  A +   AL+   RP+++PLS+AQRRLWFL
	Sbjct  1004  SRIREVMGAELGIAKLFDEPTVAGLAAHLDLAQSACPALQRAERPEKIPLSFAQRRLWFL 1063

	Query  6026  NRLEGPSATYNLPLVLRLSGSVDASALAAALRDVVGRHESLRTVFPDSSADPHQIILSPA 6085
	             + LEGPS TYN+P+ +RLSG +D   L AAL D+V RHESLRT+FP+S    +Q IL
	Sbjct  1064  HCLEGPSPTYNIPVAVRLSGELDQGLLKAALYDLVCRHESLRTIFPESQGTSYQHILDAD 1123

	Query  6086  EAQPRFDTVELSAAELDGAIAEAAQYRFDLAAELPIRAWLFTVSPTEHAVVLLMHHIASD 6145
	              A P     E++  EL   +AEA +Y FDLAAE   RA LF + P E+ ++LL+HHI  D
	Sbjct  1124  RACPELHVTEIAEKELSDRLAEAVRYSFDLAAEPAFRAELFVIGPDEYVLLLLVHHIVGD 1183

	Query  6146  GWSMTPLSQDLAHAYTARCRGEEPVFAPLPVQYADYTLWQHEVLGDEQDADSVLAQQVAH 6205
	             GWS+TPL++DL  AY ARC G  P +APL VQYADY LWQ E+LG+E D +S++A Q+A
	Sbjct  1184  GWSLTPLTRDLGTAYAARCHGRSPEWAPLAVQYADYALWQQELLGNEDDPNSLIAGQLAF 1243

	Query  6206  WQRTLAGAPELLELPTDRPRPAVASYQGGILEFELDYEVHKGLSTLAKRSGTTLFMVVQS 6265
	             W+ TL   P+ LELPTD  RPA  S+ G  + F ++ E HK L  LA+ +  +LFMV+QS
	Sbjct  1244  WKETLKNLPDQLELPTDYSRPAEPSHDGDTIHFRIEPEFHKRLQELARANRVSLFMVLQS 1303

	Query  6266  ALATLFTKLGAGTDIPLGTAIAGRTDEAVEDLIGFFVNTLVLRTDTSGDPSFTELLGRVR 6325
	              LA L T+LGAGTDIP+G+ IAGR D+A+ DL+G F+NTLVLRTDTSGDPSF ELL RVR
	Sbjct  1304  GLAALLTRLGAGTDIPIGSPIAGRNDDALGDLVGLFINTLVLRTDTSGDPSFRELLDRVR 1363

	Query  6326  QTDLAAFAHQEVPFERLVEVLNPARSLAHHPLFQVLLTVQNTEQAQLRLPGAEVEFDGAG 6385
	             + +LAA+ +Q++PFERLVEVLNPARS A HPLFQ++L  QNT  A+L LP  E
	Sbjct  1364  EVNLAAYDNQDLPFERLVEVLNPARSRATHPLFQIMLAFQNTPDAELHLPDMESSLRINS 1423

	Query  6386  TGVAKFDLAFSLEE------SGEGLEGLVEYAADLFDRDTVQRLVDRLITLLRGVITDPE 6439
	              G AKFDL   + E      +  G+EGL+EY+ DLF R+T Q L DRL+ LL    +DP+
	Sbjct  1424  VGSAKFDLTLEISEDRLADGTPNGMEGLLEYSTDLFKRETAQALADRLMRLLEAAESDPD 1483

	Query  6440  LPISQLDVLSDTERGQLLGEWNETAASTEDGVLAELFAARVRRDPQAPALAFEGTTLTYG 6499
	               I  LD+L+  E   ++ +W   +       L E F  +    P A A+ +E   L+Y
	Sbjct  1484  EQIGNLDILAPEEHSSMVTDWQSVSEKIPHACLPEQFEKQAALRPDAIAVVYENQELSYA 1543

	Query  6500  ELDARANRLAHKLVELGAGPERFVAVAVPRSVEMVVALLAVAKSGAAYVPVDPSYPADRV 6559
	             EL+ RANRLA  ++  G GPE+FVA+A+PRS+EM V LLAV K+GAAY+P+DP YPADR+
	Sbjct  1544  ELNERANRLARMMISEGVGPEQFVALALPRSLEMAVGLLAVLKAGAAYLPLDPDYPADRI 1603

	Query  6560  AYMLSDAAPSLVLTTSGTG---LAGLRLDELELDGPDTAPAVTGLGLGSP---------- 6606
	             A+ML DA P+ ++T +           + ++ LD P+ A  +     G+P
	Sbjct  1604  AFMLKDAQPAFIMTNTKAANHIPPVENVPKIVLDDPELAEKLNTYPAGNPKNKDRTQPLS 1663

	Query  6607  ----AYVIYTSGSTGRPKGVVVTHSGLASLVAAQVGAFGVGPGSRVLQFASLSFDAASWE 6662
	                 AYVIYTSGSTG PKGV++ H  +  L AA    F    G     F S +FD + WE
	Sbjct  1664  PLNTAYVIYTSGSTGVPKGVMIPHQNVTRLFAATEHWFRFSSGDIWTMFHSYAFDFSVWE 1723

	Query  6663  VCMGLLSGACLVVAPADRVLPGEPLAELVAEHAVTHVTLPPTAL-----AALPANGLPEG 6717
                 +   LL G  LV+ P       E    L+ +  VT +   P+A      A      L +
	Sbjct  1724  IWGPLLHGGRLVIVPHHVSRSPEAFLRLLVKEGVTVLNQTPSAFYQFMQAEREQPDLGQA 1783

	Query  6718  MTL---VVAGEATQPSTVEQW-----SAGRTMINAYGPTETTVCATMSGPLSGAVVA--- 6766
	             ++L   +  GEA + S +E W          +IN YG TETTV  +    L  ++ A
	Sbjct  1784  LSLRYVIFGGEALELSRLEDWYNRHPENRPQLINMYGITETTVHVSYI-ELDRSMAALRA 1842

	Query  6767  --PIGRPITNSRVYVLDAGLRPVPPGTTGELYVAGASLARGYHNRPGLTAERFVASPFG- 6823
	                IG  I +  VYVLD  L+PVPPG  GELYV+GA LARGY  RPGLT+ERF+A PFG
	Sbjct  1843  NSLIGCGIPDLGVYVLDERLQPVPPGVAGELYVSGAGLARGYLGRPGLTSERFIADPFGP 1902

	Query  6824  VGERLYRTGDLAKWRVDGQLEYVGRADHQVKVRGFRIELGEIESVLAAHPAIAEVAAVVR 6883
	              G R+YRTGD+A+ R DG L+YVGRADHQVK+RGFRIELGEIE+ L  HP + + A +VR
	Sbjct  1903  PGTRMYRTGDVARLRADGSLDYVGRADHQVKIRGFRIELGEIEAALVQHPQLEDAAVIVR 1962

	Query  6884  EDQPGDRRIVAYLVAAGQAPGSAELRSVVGAALPEYMVPSAFVVLPAIPLLPNGKVDRKA 6943
	             EDQPGD+R+ AY++ + +   +AELR      LP+YMVP+AFV +  +PL PNGK+DRKA
	Sbjct  1963  EDQPGDKRLAAYVIPSEETFDTAELRRYAAERLPDYMVPAAFVTMKELPLTPNGKLDRKA 2022

	Query  6944  LPAPDFTAVSTGRAPRTSREELLCGLYAEVLGLPEVGIDANFFELGGDSILSLQVVSRAR 7003
	             LPAPDF A  TGR PRT +EE+LC L+ EVL LP VGID  FF+LGG S+L++Q++SR R
	Sbjct  2023  LPAPDFAAAVTGRGPRTPQEEILCDLFMEVLHLPRVGIDDRFFDLGGHSLLAVQLMSRIR 2082

	Query  7004  NA-GIVISARDVFRYGTPAALA 7024
	              A G+ +S  ++F   T A LA
	Sbjct  2083  EALGVELSIGNLFEAPTVAGLA 2104
	
	*/

	s1 = sequence::from_string("MDGSTMRVDEGTASLAALVQARAERAPGRRALVFEDTALTYRELTEQAHRLARFLLARGVAPGQRVALALPRSVSMIVAMLAVAEVGAAYVPVDPDYPADRIAFMVADSAPALLLTDSTVAGALPELAAPRLLLDDPEIAAAVASQEDTGVGIEVSPASPAYVIYTSGSTGRPKGVVVPHAGVVNHMLWQAEAWDVDEQDVVLARTAFSFDAAGSEIWLPLLAGATICLAPSTVTRDPEALVAYAARHGVTVAQFVPSLLAVTAEAIARAENLALRLVFVAGEVLPPTLAEQVVSEWGVRLAHLYGPTEASVDVTGYEARPGCGNAPLPIGRPVWNTSAYVLDEALRPVELGVTGELYVAGVQLAHGYLNRPGLSAERFVADPFGEPGTRMYRTGDLARWNANDELDFLGRADDQVKVRGFRIELGEIEAALAQCEGVRRVAVLVREDQPGDKRIVAYVVSDVDSGELRTALGERLPEYMVPSAIVALEDFPLAPNGKLDRQALPRPRYETTRGASTEQEQVLCGLFAEVLGVAEVGAEDNFFDLGGHSLLAARLIARLRSQLRVELPVRALFEAPTVAELARRLDAAVTARPALSKVDRPERLPLSPAQQGLWFLHRLEGPSATYNIPLALTMTGQVDRDALRLALADVVDRHESLRTVFPDEEGRPYQHVLASGPDLSFVDLKPGAVEQAARHAFDLAAEAPLRATLFSEGDRHVLLLVMHHIASDGWSERPLIADLSTAYQARLHGAAPDWAPLPVQYADYTLWQQHDLTEGLAFWREALAGVAELAEFPSDRPRPAVASHRGDELTFAFEADLHRAVTDLARSTGSTVFMVVQAALATLLTRLGAGTDIPLGTPAAGRTDEALTDLVGYFVNTLVLRTDTSGDPSFRELLARVREADLAAYAHQDVPFDRLVEELNPQRSLANTPLFQVMLTFENLGEAAPRLPGLDVAVSEIPTGVAKFDMDLRLQERHTADGLPDGVTGIVEYATDLFEHDTVRTITEALAAVLRAATAAPDQSIARIEVPAIGAAPLGEQVRVRGALADLGRLESVLRKHPAVDQAAVRAFGDKLAAYVVPLPGLTVRGTDLREHVAREVPEYMVPAAFRTVDSLGGELPEPDFGAVEQRPMSPRERVLADLFADLLGLPAVGLDEGFFDLGGDSIVSIQLVSRARKAGLVITPRNVFEHKTVARLAAVATEAAAPVASAVVDVAVGEVVPTPIMHALREHGGPTDAYHQSVLLRVPAGLGAENLTAAVQALLDKHDVLRLRVDDQWRMTVLAPGSVDASACVQRITGSTVDEARAEAQQRLSPRDGIMVQAVWFEDTDRLLVLVHHLAVDGVSWRILLPDLRAAWEAVSGGRTPELAPVGTSFRRWTEQLRAQSAARAEELALWQEVLEGEDPLIGVRALDHARDVRGTARTVTVSLPAEYTGPLLTTLPAAVHGGVNDVLLTALAVAVADWRRDRGEEVSGLLVDLEGHGREEIAEGLDLSRTVGWFTSVYPVRLDAAVSDLDELRVGGASLGRAVKRVKEQLRRLPDNGIGYGLLRHLNERTRDTLAGYGTPQIGFNYLGRFSAPEGADWATAPEGESLGGGADAGMPLEYALEINALTEDLADGPRLSATFTWPDGLLSDQDVRKLARTWLRALELLATHATDRGVAGHTPSDLPLVSLAQHEIERIEAAVPALADVLPLAPLQDGLLFHALFDDQAPDIYAVQEVFRLDGPLDPAALRRAAAGVLRRRDNLRAGFWHEGVSRPVQFVPAEVELPWTEHDLSTVDAVAQRDAVTAITTEDRQRRFDLAKPPLLRFTLIKLGAEQHRFVITNHHVLFDGWSMQSLLDELFDLYAGREPGAVAPYRDYLAWLSAQDASTAEQAWRTALSELDGPTLLAPNAAGTPVRPEQVMLRLTEQFTAELLAAARSHGLTLNTLVQGAWAMLLALRTGRTDVVFGATVSGRPSEIPGIESMIGMFINTLPVRIRLDREESLTGLLARVQDEQSQLLAHHHVGLADLQRLTGLGELFDTLTIVENYPHEAGSSWPVAPGLTVAGLEDHDATHFPLTLSVLPGAQLGLRLEHRPDLLDHAATVELSQHLERLLRAVVSTPDSTIGELVELNWDGTAEQAEVEEVAQQAVRRAPRTPQEEILCGLFGDVLGVPEVGIDDSFFELGGHSLSAIRMLSRIRSMFGVELTIRNLFESPTVAGLVERFSAAGTARVPLERAQRPEQHMPLSFAQWRLWFLHRLEEDSATYTEPLILRLTGELDRPAFAAAVADVVDRHETLRTIYPEHDGTPYQLILPLAQAQPVITEVATDAETLPVLLAEAGRYSFDLAAEVPLRVTVFSSSPTDHVVLLLLHHIASDGWSDAPLSRDLSTAYKARLRGQAPDWAPLPVQYADFTLWQQKLLGDEKDPDSLLNEQLTFWKKALAELPEQLSLPTDRPRPAVASYRGDEVEFTIDARLHLGLVELARQTNSTLFMVVQSALSALLTRLGAGTDIPMGSVSAARTDEALEDLVGFFVNTLVLRTDTSGNPSFRQLVGRVREADLAAYAHQEVPFERLVEVLNPVRSLSHHPLFQVMVLFQNNAEAELDLPGLRASFEDIGSGSSKFDLDFDFRESYAEDGTPLGMAGLIEYATDLFDRETVLTIAGRLVRLLTAVVADPDRPIGQIDVLTPAERQRMLVEWNGAEADTTQETLTGVFEAQVARDPSAIAVVFEDTRLSYADLNERANRLAHHLIAQGVGTEDLVALAMPRCAEMVVAVLATIKAGAAYLPIDPAYPADRIAYVLADAKPAAVVTSSQVSVELPDSVPVVVLDQAELSTYPSGNPELGALSPDNAAYVIYTSGSTGRPKGVLIPHRNAVRLFTATEQWFHFGPEDVWTLFHSYAFDFSVWELWGPLLHGGRLVVVPHATTRSPEEFLALLSQERVTVLNQTPSAFYQLIQADKDNPKPLSLRYVVFGGEALELRRMTEWYARHAADAPVLVNMYGITETTVHVTHIALDEEIAAGGGASLIGTGIPDLRVYLLDDCLQPVPPGVTGELYVAGAGLARGYLGRPGLSGERFVADPFGEPGTRMYRSGDLARWTTDGTLDYLGRADHQVKIRGFRIELGEIEGVIERHPEVEQVTVLVREDRPGDKRLVAYVIGGVDHAELKAHAQGSLPEYMVPSAFVTIDAFPLTANGKLDRKALPAPDLSAAAGGREPRNQREEVLCGLFADVLGVERVGIDDSFFDLGGHSLLVTRLVSRIRSAFGAELALRALFEAPTVAGLSERLDAAERARTALRPMPREGRLPLSFVQQGLWFLGQLEGPSATYNVPLVMRMSGDLDLDALREALADVVARHESLRTLFPDVDGTPYQVVLDPVQGRPELHVEHVSAEELTEALREASRYHFDLTTELPLRTTLFCLGGNEYALLPLMHHIASDGWSESLLGNDLSEAYAARLRGQAPGWAPLPVQYVDYTLWQRELLGSEEDPNSEISRQMAFWRTTLAGLPEKIELPTDRARPAVASYGGDEAELRISPALHAKMAELAKQTNTTLFMLIQAGLSALLTRVGAGTDIPLGTPIAGRTDEALEDLFGFFVNTLVLRTDTSGDPSFRTLLDRVRETDLAAFAHQDLPFERIVEVVNPTRSLAHHPLFQIMLTFENFDEVDVRLPGLEVGLEDVIAGVAKFAMDIRLQERYSADGEPAGIVGEIEYATDLFDRATIDGFGERLMRVLDEAVSNPDRTIGELDVLDPAERQRITVDWNGPAVTSTTTVGELVEAQDPAATAVICGEQRLSYADLRERIGTVSELPQELSVDYVAAALAAQWSAAVTVVEPILPTDRVLLHTASTAVRDLVWALTSGAAVVIGSTEGVTVASYTPTQLAASTVDLRLVFCAGEPLPAALARRFHGTLVNLHEVAGRPVAATRFVTTDPAALVPIGRPLPGVDVSVVDGEGRLLAPGVLGQLVVDGERTGDQVRWRSDGQLELVETAVVRGLLIQPAEVEAALLERPDVTQAAVVVRDGQLVAHLVGRDVDVAGVRQYVDAVLPEYLVPTTVVVAAELPLTERGTVDREALDVSTGAAESRSARTPQEEILCGLFAEVLGVERVGIDDSFFVLGGHSLSAIRLLSRIRATFGAELTVRALFGEPTVAGLVERLDSAAASTRVPLVPVQRPERLPLSFAQQRLWFLHRLEGPSSTYNVPLVLRLSGRLDRAALRAAIGDVVRRHETLRTIFPEAGDTPYQLVLNEVSPELDVVRTSEDGLAAALETASRRAFDLTGEIPLRATLFELGTDEHVLLLLVHHIASDGWSEGPLARDVSVAYAARSQGAAPRWTPLPVQYADYTIWQRQVLGAEEDPESQLAKQIDYWKQALTGLPEVLPLPVDRVRPAVASYQGDEIAFTISEPLHQAMAGLARRTGSTVFMVMQAAFAALLTKLGAGTDIPIGTPVAGRGDEALDELVGFFVNTVVLRTDTSGDPSFRDLLARVRESDLAALAHQDVPFEHLVDAVNPVRSLAHHPLFQVMLVFETNSAGVFAMPGLDARFAEDVDMSAARFDLTLHLDEHRGADDTPHGITGFLEFATDLFERPTVERMVQRLITVLASVVDDAEQPISALEVLLADEHRAVPARELPMAGVAERFAAQVARTPDATAVVHEDVELSYVELNERANRLAHKLVGAEQVAVDLPRSADLVVAVLAAVKAGATLVPTAEVVVTSVDAEGEPSTDPAVTARAVVLVTAEGLAIPATAIAEQAPERVALYGESAELEILSTLVSGGCVVVPTEQVDDTELLGWLEWFEVQRLTAPAAVVDRLLAVAEEQSYDLDDVRRVIRVAQPWGAQVLDSALRPVPPGVVGDLYVGDEALGYQGEPARTAAQFVANPFGAPGTRLHRTGEKARWSVDGVLLPPLASTVEESEVDAGREARTPQERRLCELFAEVLEVESFGVTDSFFAWGGHSLRATQLISRIRSAFAVELPVRAIFEAPTPAALAEQLAAAGGARTALAPATRPERVPLSFAQQRLWFLHRMQGPSPTYNVPVVLRLDGELHRDALVAAVRDVVVRHESLRTVFPDVEGTPYQHVLAEFEPAVSFVDTTDLDADLTELARHAFDLATELPIRVTVLSTSPTEHALLLLTHHIASDGWSTEPLSRDFAHAYAARTRGEQPEFTPLPVQYADYTLWQQDLLGSEQDPTSLLSRQVEFWRTALADLPELLQLPTDRPRPAVASYEGGALDFEFTPELHRGVTELAERTGTTVFMVMQAALSTLFTKLGAGTDIPLGTPIAGRTDEALEELVGFFVNTLVLRTDTSGDPGFGQVLERVREANLAAYAHQDVPFERLVEVLNPTRSLAHHPLFQVMMTLHNSSADGPGLEGVDTGVATVDLQFTLQESFDANGSPAGLGGDVEYATDLFGPDSVRLLLTRLETLLAAVVADPRRPISRIDLLTAQERTQVLRTWNDTAREVPALTVPQLFQAHAQGSPEATALVFGAEQVSYVELNVRANQLAHHLIAQGVGPERIVAVALPRSVDLVVALLAVLKTGAAYLPIDPGYPAERIGYILADAQPVCLLSTGNGPRTEVPTVLLDSAAKQAELAELSGADPESTVDLRNPAYTIYTSGSTGRPKGVVVTVGDLANFLAAMTDRIGLTPEDRLLAVTTVAFDIAGLEIHLPLVSGARVVLAAESQVRDPAALTALARTSGATVMQATPSLWQAALAEDAESLRGMRMLVGGEALPAALAGTMAEVGAEVVNLYGPTETTIWSASAAISDGSVPPIGRPIANTAVYVLDSALAPVPVGVTGELYIAGDGLARGYANRPGLTAERFSADPFGEPGSRMYRTGDLARWRADGQLEYLARVDDQVKLRGFRIELGEIESVLTAHPEVTRAAVLVREERLVAYVVGSADVSGLRALAQSRLPEYMVPSAYVSLDTLPLTPNGKLDRKALPAPDFGAESGGRAARTPAEQVLCGLFADVLGAERVGIEDNFFELGGHSLLATKLISRIRVALGVEVPIQALFEAPTVAGLAERLTGAATGRRALEPMARPQRVPLSYAQRRLWFLNRLEGPSATYNLPLVLRLSGSVDASALAAALRDVVGRHESLRTVFPDSSADPHQIILSPAEAQPRFDTVELSAAELDGAIAEAAQYRFDLAAELPIRAWLFTVSPTEHAVVLLMHHIASDGWSMTPLSQDLAHAYTARCRGEEPVFAPLPVQYADYTLWQHEVLGDEQDADSVLAQQVAHWQRTLAGAPELLELPTDRPRPAVASYQGGILEFELDYEVHKGLSTLAKRSGTTLFMVVQSALATLFTKLGAGTDIPLGTAIAGRTDEAVEDLIGFFVNTLVLRTDTSGDPSFTELLGRVRQTDLAAFAHQEVPFERLVEVLNPARSLAHHPLFQVLLTVQNTEQAQLRLPGAEVEFDGAGTGVAKFDLAFSLEESGEGLEGLVEYAADLFDRDTVQRLVDRLITLLRGVITDPELPISQLDVLSDTERGQLLGEWNETAASTEDGVLAELFAARVRRDPQAPALAFEGTTLTYGELDARANRLAHKLVELGAGPERFVAVAVPRSVEMVVALLAVAKSGAAYVPVDPSYPADRVAYMLSDAAPSLVLTTSGTGLAGLRLDELELDGPDTAPAVTGLGLGSPAYVIYTSGSTGRPKGVVVTHSGLASLVAAQVGAFGVGPGSRVLQFASLSFDAASWEVCMGLLSGACLVVAPADRVLPGEPLAELVAEHAVTHVTLPPTALAALPANGLPEGMTLVVAGEATQPSTVEQWSAGRTMINAYGPTETTVCATMSGPLSGAVVAPIGRPITNSRVYVLDAGLRPVPPGTTGELYVAGASLARGYHNRPGLTAERFVASPFGVGERLYRTGDLAKWRVDGQLEYVGRADHQVKVRGFRIELGEIESVLAAHPAIAEVAAVVREDQPGDRRIVAYLVAAGQAPGSAELRSVVGAALPEYMVPSAFVVLPAIPLLPNGKVDRKALPAPDFTAVSTGRAPRTSREELLCGLYAEVLGLPEVGIDANFFELGGDSILSLQVVSRARNAGIVISARDVFRYGTPAALAAAAGSQGARAEEPDAGIGAMPLTPIMRWLREQGGQYTGFNQSMVVQVPADLGMDRLTTAVQALLDHHDALRGKLSDAALVIAPRGAVRAKDVVQRVDVTRLDLAQAVTEHGGYARDRLAPADGVMVQIVWFDAGPNESGRLLLVLHHLVVDGVSWRILLPDLAAAWHTAARGDQPVLDPVRTSLRTWAHALATQVPARRAERPLWTSMLGGTDPLLSGRALDPAVDVASTVREVELTLSTKDTEALLTAVPAMFHAGVNDVLLTGLALAVARWREQRGLSGDGVLVDLEGHGREEIVDGLDLTRTVGWFTSLFPVRLDPGTPSWQEVQAGGHAVGQAVKQVKEQLRALPDHGIGYGLLRYLDPEPHLDPVKPQIGFNYLGRFGAGTAGDWALAEDVSGPDGRDPRMPLAHALEVNAVTEDRADGPALTATWSWPDALFTEPEVRELAEAWFEALHALVEHAQAPGSGGFTPSDLSLVSLSQDEIEDLEEELGSFE");
	s2 = sequence::from_string("MPDTKDLQYSLTGAQTGIWFAQQLDPDNPIYNTAEYIEINGPVNIALFEEALRHVIKEAESLHVRFGENMDGPWQMINPSPDVQLHVIDVSSEPDPEKTALNWMKADLAKPVDLGYAPLFNEALFIAGPDRFFWYQRIHHIAIDGFGFSLIAQRVASTYTALIKGQTAKSRSFGSLQAILEEDTDYRGSEQYEKDRQFWLDRFADAPEVVSLADRAPRTSNSFLRHTAYLPPSDVNALKEAARYFSGSWHEVMIAVSAVYVHRMTGSEDVVLGLPMMGRIGSASLNVPAMVMNLLPLRLTVSSSMSFSELIQQISREIRSIRRHHKYRHEELRRDLKLIGENHRLFGPQINLMPFDYGLDFAGVRGTTHNLSAGPVDDLSINVYDRTDGSGLRIDVDANPEVYSESDIKLHQQRILQLLQTASAGEDMLIGQMELLLPEEKEKVISKWNETAKSEKLVSLQDMFEKQAVLTPERIALMCDDIQVNYRKLNEEANRLARLLIEKGIGPEQFVALALPRSPEMVASMLGVLKTGAAYLPLDPEFPADRISYMLEDAKPSCIITTEEIAASLPDDLAVPELVLDQAVTQEIIKRYSPENQDVSVSLDHPAYIIYTSGSTGRPKGVVVTQKSLSNFLLSMQEAFSLGEEDRLLAVTTVAFDISALELYLPLISGAQIVIAKKETIREPQALAQMIENFDINIMQATPTLWHALVTSEPEKLRGLRVLVGGEALPSGLLQELQDLHCSVTNLYGPTETTIWSAAAFLEEGLKGVPPIGKPIWNTQVYVLDNGLQPVPPGVVGELYIAGTGLARGYFHRPDLTAERFVADPYGPPGTRMYRTGDQARWRADGSLDYIGRADHQIKIRGFRIELGEIDAVLANHPHIEQAAVVVREDQPGDKRLAAYVVADAAIDTAELRRYMGASLPDYMVPSAFVEMDELPLTPNGKLDRKALPAPDFSTSVSDRAPRTPQEEILCDLFAEVLGLARVGIDDSFFELGGHSLLAARLMSRIREVMGAELGIAKLFDEPTVAGLAAHLDLAQSACPALQRAERPEKIPLSFAQRRLWFLHCLEGPSPTYNIPVAVRLSGELDQGLLKAALYDLVCRHESLRTIFPESQGTSYQHILDADRACPELHVTEIAEKELSDRLAEAVRYSFDLAAEPAFRAELFVIGPDEYVLLLLVHHIVGDGWSLTPLTRDLGTAYAARCHGRSPEWAPLAVQYADYALWQQELLGNEDDPNSLIAGQLAFWKETLKNLPDQLELPTDYSRPAEPSHDGDTIHFRIEPEFHKRLQELARANRVSLFMVLQSGLAALLTRLGAGTDIPIGSPIAGRNDDALGDLVGLFINTLVLRTDTSGDPSFRELLDRVREVNLAAYDNQDLPFERLVEVLNPARSRATHPLFQIMLAFQNTPDAELHLPDMESSLRINSVGSAKFDLTLEISEDRLADGTPNGMEGLLEYSTDLFKRETAQALADRLMRLLEAAESDPDEQIGNLDILAPEEHSSMVTDWQSVSEKIPHACLPEQFEKQAALRPDAIAVVYENQELSYAELNERANRLARMMISEGVGPEQFVALALPRSLEMAVGLLAVLKAGAAYLPLDPDYPADRIAFMLKDAQPAFIMTNTKAANHIPPVENVPKIVLDDPELAEKLNTYPAGNPKNKDRTQPLSPLNTAYVIYTSGSTGVPKGVMIPHQNVTRLFAATEHWFRFSSGDIWTMFHSYAFDFSVWEIWGPLLHGGRLVIVPHHVSRSPEAFLRLLVKEGVTVLNQTPSAFYQFMQAEREQPDLGQALSLRYVIFGGEALELSRLEDWYNRHPENRPQLINMYGITETTVHVSYIELDRSMAALRANSLIGCGIPDLGVYVLDERLQPVPPGVAGELYVSGAGLARGYLGRPGLTSERFIADPFGPPGTRMYRTGDVARLRADGSLDYVGRADHQVKIRGFRIELGEIEAALVQHPQLEDAAVIVREDQPGDKRLAAYVIPSEETFDTAELRRYAAERLPDYMVPAAFVTMKELPLTPNGKLDRKALPAPDFAAAVTGRGPRTPQEEILCDLFMEVLHLPRVGIDDRFFDLGGHSLLAVQLMSRIREALGVELSIGNLFEAPTVAGLAERLEMGSSQSALDVLLPLRTSGDKPPLFCVHPAGGLSWCYAGLMTNIGTDYPIYGLQARGIGQREELPKTLDDMAADYIKQIRTVQPKGPYHLLGWSLGGNVVQAMATQLQNQGEEVSLLVMLDAYPNHFLPIKEAPDDEEALIALLALGGYDPDSLGEKPLDFEAAIEILRRDGSALASLDETVILNLKNTYVNSVGILGSYKPKTFRGNVLFFRSTIIPEWFDPIEPDSWKPYINGQIEQIDIDCRHKDLCQPEPLAQIGKVLAVKLEELNK");
	qa = 4975;
	sa = 2;
	goto ende;

	ende:
	ss.push_back(s1);
	ss.push_back(s2);
	ss.finish_reserve();

	//benchmark_floating(ss, qa, sa);
	//benchmark_greedy(ss, qa, sa);
	//benchmark_cmp();
	//benchmark_ungapped(ss, qa, sa);
	benchmark_swipe(ss);

}