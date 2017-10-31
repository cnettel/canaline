#include <array>
#include <boost/spirit/home/x3.hpp>

using namespace boost::spirit;

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

int main()
{
	scores.has_action;
}


