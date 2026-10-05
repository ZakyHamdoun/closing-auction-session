#include "parse/parser.hpp"

#include <charconv>
#include <fstream>
#include <stdexcept>
#include <string_view>

namespace parse
{
  std::vector<Order> parse_orders(const std::string& file_path)
  {
    std::vector<Order> orders;
    orders.reserve(100000);

    std::ifstream in(file_path);
    if (!in)
      throw std::runtime_error("Cannot open input file: " + file_path);

    std::string line;
    while (std::getline(in, line))
    {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (line.empty())
        continue;

      Order o;
      const char* ptr = line.data();
      const char* end = ptr + line.size();

      // 1. Timestamp.
      auto res = std::from_chars(ptr, end, o.timestamp);
      ptr = res.ptr + 1;

      // 2. Symbol.
      const char* sym_start = ptr;
      while (ptr < end && *ptr != ',')
        ++ptr;
      o.symb = std::string(sym_start, ptr - sym_start);
      ++ptr;

      // 3. Side.
      o.side = (*ptr == 'B') ? OrderType::BUY : OrderType::SELL;
      ptr += 2;

      // 4. Volume.
      res = std::from_chars(ptr, end, o.volume);
      ptr = res.ptr + 1;

      // 5. Price.
      o.price = std::stod(std::string(ptr, end - ptr));

      orders.push_back(std::move(o));
    }

    return orders;
  }

} // namespace parse
