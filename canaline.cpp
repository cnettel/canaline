#include <array>
#include <boost/spirit/home/x3.hpp>
#include <iostream>
#include <fstream>

using namespace boost::spirit;
using namespace std;

using box = std::array<int, 4>;


auto constexpr bracketize(auto what)
{
	return '[' > what > ']';
}

auto boxrule = bracketize(x3::int_ > ',' > x3::int_ > ',' > x3::int_ > ',' > x3::int_);
auto innerboxlist = *(boxrule > ',') > boxrule;
auto fullboxlist = x3::lit("array") > '(' > bracketize(innerboxlist ) > ',' > "dtype" > '=' > "int64" > ')';

auto boxes = bracketize(*(fullboxlist));

auto scorelist = x3::lit("array") > '(' > bracketize(*(x3::double_)) > ')';
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

int main(int argc, char** argv)
{
	if (argc < 1)
	{
		return -1;
	}
	ifstream file(argv[1]);
}


