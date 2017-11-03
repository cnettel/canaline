#include <array>
#include <boost/fusion/adapted/array.hpp>
#include <boost/fusion/adapted/std_array.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/spirit/home/x3.hpp>
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

struct hmmtype
{
	vector<vector<box>> ourBoxes;
	vector<vector<double>> scoreVals;
	vector<bool> lineEnd;

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

	void doEmissions(stateVec& state, int pos)
	{
		int i = 0;
		for (auto& val : state)
		{
			val *= max(1e-9, scoreVals[pos][i++]);
		}
	}

	static double transProb(const box& a, const box& b, bool linebreak)
	{
		// TODO
	}

	void doTransitions(const stateVec& fState, stateVec& toState, int fromPos, int toPos)
	{
		for (int i = 0; i < scoreVals[toPos].size(); i++)
		{
			toState[i] = 0;
		}

		for (int i = 0; i < scoreVals[fromPos].size(); i++)
		{
			for (int j = 0; j < scoreVals[toPos].size(); j++)
			{

			}
		}
	};
} hmm;

using stateVec = array<double, 1000>;

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		return -1;
	}
	ifstream file(argv[1]);
	
	parseToEndWithError(file, boxes > scores, as_const(std::forward_as_tuple(hmm.ourBoxes, hmm.scoreVals)));
	cout << "Read " << hmm.ourBoxes.size() << " box lists and " << hmm.scoreVals.size() << " score lists." << "\n";

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

	stateVec fwbw[2][1000] = { 0 };

	for (int i = 0; i < hmm.scoreVals[0].size(); i++)
	{
		fwbw[0][0][i] = 1;
	}

	for (int i = 0; i < hmm.scoreVals[hmm.scoreVals.size() - 1].size(); i++)
	{
		fwbw[1][0][i] = 1;
	}
}