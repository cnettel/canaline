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
	int getMapping(string word)
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
auto on_word = [&](auto& ctx) {
	return wordmap.getMapping(x3::_attr(ctx));
};
auto linefile = *(*(word_[on_word]) > x3::eol);

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		return -1;
	}
	ifstream file(argv[1]);
	vector<vector<box>> ourBoxes;
	vector<vector<double>> scoreVals;	

	parseToEndWithError(file, boxes > scores, as_const(std::forward_as_tuple(ourBoxes, scoreVals)));
	cout << "Read " << ourBoxes.size() << " box lists and " << scoreVals.size() << " score lists." << "\n";

	file = ifstream(argv[2]);
	vector<vector<int>> rows;
	parseToEndWithError(file, linefile, rows);
}