#include <array>

using box = std::array<int, 4>;

auto prelude = x3::lit('[') ;
auto box = x3::lit('[') > x3::int_ > ',' x3::int_ > ',' x3::int_ > ',' x3::int_ > ']';
auto innerboxlist = *(box > ',') > box;
auto fullboxlist = x3::lit("array") > '(' > '[' > boxlist > ']' > ',' > "dtype" > '=' > "int64" > ')';

