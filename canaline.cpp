#include <array>
#include <boost/lexical_cast.hpp>
#include <Magick++.h>
#include <boost/fusion/adapted/array.hpp>
#include <boost/fusion/adapted/std_array.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/format.hpp>
#include <boost/spirit/include/support_istream_iterator.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>

using namespace boost::spirit;
using namespace std;

using box = std::array<int, 4>;

// According to https://wandbox.org/permlink/poeusnilIOwmiEBo
namespace boost {
	namespace spirit {
		namespace x3 {
			namespace traits {
				// It can't be specialized because X3 implements is_container as a template aliases,
				// thus we need QUITE TERRIBLE DIRTY hack for fixed length container.
				//template <> struct is_container<Vertex const> : mpl::false_ { };
				//template <> struct is_container<Vertex> : mpl::false_ { };
				namespace detail {
					template <> struct has_type_value_type<box> : mpl::false_ { };
				}
			}
		}
	}
}


template<class T> auto constexpr bracketize(T what)
{
	return '[' > what > ']';
}

// Vector of four ints
auto boxrule = bracketize(x3::int_ > ',' > x3::int_ > ',' > x3::int_ > ',' > x3::int_);
// Comma-separated list of such vectors
auto innerboxlist = (boxrule % ',');
auto fullboxlist = x3::lit("array") > '(' > bracketize(innerboxlist ) > ',' > "dtype" > '=' > "int64" > ')';
// The whole first section of the data file
auto boxes = bracketize(*(fullboxlist));

// Array of lists of doubles
auto scorelist = x3::lit("array") > '(' > bracketize(x3::double_ % ',') > ')';
auto scores = bracketize(*(scorelist));

template<class RuleType, class AttrType> void parseToEndWithError(istream& file, const RuleType& rule, AttrType& target)
{
	auto parseriter = boost::spirit::istream_iterator(file);
	boost::spirit::istream_iterator end;

	bool res = phrase_parse(parseriter, end, rule, x3::space - x3::eol, target);

	if (!res)
	{
		std::string val;
		file >> val;
		throw logic_error("Parsing failed. " + (std::string) __func__ + " " + val);
	}
}

struct wordmapper
{
private:
	map<string, int> mapping;
	map<int, string> inversemapping;
public:
	int getMapping(const string& word)
	{
		auto i = mapping.find(word);
		if (i != mapping.end())
		{
			return i->second;
		}

		int index = mapping.size();
		mapping[word] = index;
		inversemapping[index] = word;

		return index;
	}

	string getWord(int mapping)
	{
		return inversemapping[mapping];
	}
} wordmap;

// Coming in C++ 17
template <class T>
constexpr std::add_const_t<T>& as_const(const T& t) noexcept
{
	return t;
}

auto word_ = x3::lexeme[+(x3::char_ - x3::space)];
auto linefile = *word_ % x3::eol;
using stateVec = array<double, 1000>;

struct hmmtype
{
	vector<vector<box>> ourBoxes;
	vector<vector<double>> scoreVals;
	vector<bool> lineEnd;

	stateVec fwbw[2][1000] = { 0 };

	void prepareLineEnd(const vector<vector<int>>& introws)
	{
		lineEnd.reserve(scoreVals.size());
		for (const auto& r : introws)
		{
			for (int i : r)
			{
				lineEnd.push_back(false);
			}
			*(lineEnd.end() - 1) = true;
		}
	}

	void emit(stateVec& state, int pos)
	{
		for (int i = 0; i < scoreVals[pos].size(); i++)
		{
			state[i] *= max(1e-9, scoreVals[pos][i]);
		}
	}

	static double transProb(const box& a, const box& b, bool linebreak)
	{
		bool ok = false;
		int height = max(a[3] - a[3], b[3] - b[1]);
		if (linebreak)
		{
			if (b[1] > a[1]) ok = true;
		}
		else
		{
			if (b[0] > a[0] && b[1] > a[1] - height && b[1] < a[1] + height) ok = true;
		}

		return ok ? 1 : 1e-5;
	}

	template<int dir> void transition(const stateVec& fState, stateVec& tState, int fromPos, int toPos)
	{
		static_assert(dir == -1 || dir == 1);

		for (int i = 0; i < scoreVals[toPos].size(); i++)
		{
			tState[i] = 0;
		}

		for (int i = 0; i < scoreVals[fromPos].size(); i++)
		{
			for (int j = 0; j < scoreVals[toPos].size(); j++)
			{
				box* fromBox = &ourBoxes[fromPos][i];
				box* toBox = &ourBoxes[toPos][j];
				int fIndex = fromPos;

				if (dir == -1)
				{
					fIndex = toPos;
					swap(fromBox, toBox);
				}

				tState[j] += fState[i] * transProb(*fromBox, *toBox, lineEnd[fIndex]);
			}
		}
	}

	void normalize(stateVec& state, int pos)
	{
		double sum = 0;
		for (int i = 0; i < scoreVals[pos].size(); i++)
		{
			sum += state[i];
		}
		if (isnan(sum)) cerr << "Normalization error at " << pos << "\n";
		if (sum == 0) cerr << "Zero at " << pos << "\n";
		cout << "Sum at " << pos << ":" << sum << "\n";

		if (sum < 1e-10)
		{
			for (int i = 0; i < scoreVals[pos].size(); i++)
			{
				state[i] *= 1e40;
			}
		}
	}

	void computeFB()
	{
		// FW
		for (int i = 0; i < scoreVals[0].size(); i++)
		{
			fwbw[0][0][i] = 1;
		}

		for (int i = 0; i < scoreVals.size(); i++)
		{
			emit(fwbw[0][i], i);
			if (i != scoreVals.size() - 1)
			{
				transition<1>(fwbw[0][i], fwbw[0][i + 1], i, i + 1);
			}

			normalize(fwbw[0][i + 1], i);
		}

		// BW
		for (int i = 0; i < scoreVals[scoreVals.size() - 1].size(); i++)
		{
			fwbw[1][scoreVals.size() - 1][i] = 1;
		}

		for (int i = scoreVals.size() - 1; i != 0; i--)
		{
			stateVec copy = fwbw[1][i];
			emit(copy, i);
			transition<-1>(copy, fwbw[1][i - 1], i, i - 1);			

			normalize(fwbw[1][i - 1], i - 1);
		}
	}

	void fakeFB()
	{
		// FW
		for (int j = 0; j < scoreVals.size(); j++)
		{
			for (int i = 0; i < scoreVals[j].size(); i++)
			{
				fwbw[0][j][i] = 1;
			}

			emit(fwbw[0][j], j);
		}	

		// BW
		for (int j = 0; j < scoreVals.size(); j++)
		{
			for (int i = 0; i < scoreVals[j].size(); i++)
			{
				fwbw[1][j][i] = 1;
			}
		}
	}

	stateVec getProbs(int pos)
	{
		stateVec toret;
		double sum = 0;
		for (int i = 0; i < scoreVals[pos].size(); i++)
		{
			toret[i] = fwbw[0][pos][i] * fwbw[1][pos][i];
			sum += toret[i];
		}

		sum = 1 / sum;
		for (int i = 0; i < scoreVals[pos].size(); i++)
		{
			toret[i] *= sum;
		}

		return toret;
	}
} hmm;

void writeWithSeparator(const string& separator, Magick::Image img, string path)
{	
	std::list<Magick::Drawable> drawList;
	drawList.push_back(Magick::DrawableFillOpacity(0));
	drawList.push_back(Magick::DrawableStrokeWidth(5));	
	drawList.push_back(Magick::DrawableStrokeColor("black"));
	array<string, 3> colors = { "red", "green", "blue" };
	img.strokeWidth(0.5);
	//drawList.push_back(Magick::DrawableStrokeOpacity(0.25));
	for (int i = 0; i < hmm.scoreVals.size(); i++)
	{
		stateVec states = hmm.getProbs(i);
		int maxindex = 0;
		for (int i = 0; i < hmm.scoreVals[i].size(); i++)
		{
			if (states[i] > states[maxindex]) maxindex = i;
		}
		cout << maxindex << ":" << states[maxindex];
		/*for (int j = 0; j < 4; j++)
		{
		cout << ":" << hmm.ourBoxes[i][maxindex][j];
		}*/
		//img.fillColor(Magick::Color::Color(0, 0, 0, 65535 - states[maxindex] * 65535));
		const box& b = hmm.ourBoxes[i][maxindex];
		drawList.push_back(Magick::DrawableRectangle(b[0], b[1], b[2], b[3]));		
		img.draw(drawList);
		drawList.pop_back();
		
		img.strokeColor(colors[i % 3]);
		img.draw(Magick::DrawableText(b[0] + 20, b[1] + 20, boost::lexical_cast<string>(i) + ":" + boost::str(boost::format("%.2f") % states[maxindex])));
		cout << " ";
		if (hmm.lineEnd[i]) cout << separator << "\n";
	}
	img.write(path);
}

int main(int argc, char** argv)
{
	Magick::InitializeMagick(0);
	if (argc < 3)
	{
		return -1;
	}
	ifstream file(argv[1]);
	
	parseToEndWithError(file, boxes > scores, as_const(std::forward_as_tuple(hmm.ourBoxes, hmm.scoreVals)));
	cout << "Read " << hmm.ourBoxes.size() << " box lists and " << hmm.scoreVals.size() << " score lists." << "\n";

	// Do soft-max
	for (auto& scoreList : hmm.scoreVals)
	{
		double maxVal = 0;
		for (double& score : scoreList)
		{
			score *= 10;
			maxVal = max(score, maxVal);
		}

		double sum = 0;
		for (double score : scoreList)
		{
			sum += exp(score - maxVal);
		}

		for (double& score : scoreList)
		{
			score = exp(score - maxVal) / sum;
		}
	}

	file = ifstream(argv[2]);
	vector<vector<int>> introws;
	vector<vector<string>> rows;
	file >> noskipws;
	parseToEndWithError(file, linefile, rows);

	transform(rows.begin(), rows.end(), back_inserter(introws),
		[](vector<string>& row)
	{
		vector<int> introw;
		transform(row.begin(), row.end(), back_inserter(introw),
			[](const string& word)
		{
			return wordmap.getMapping(word);
		});

		return introw;
	});

	hmm.prepareLineEnd(introws);

	Magick::Image origImg(argv[3]);

	hmm.computeFB();	
	writeWithSeparator("##", origImg, string(argv[3]) + ".fb.png");
	
	hmm.fakeFB();
	writeWithSeparator("//", origImg, string(argv[3]) + ".naive.png");
}