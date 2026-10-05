#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "book/cas-book.hpp"
#include "parse/parser.hpp"

int main(int argc, char** argv)
{
  std::string input_file;
  double reference_price = std::numeric_limits<double>::quiet_NaN();

  for (int i = 1; i < argc; ++i)
  {
    std::string_view arg = argv[i];
    if (arg == "-i" && i + 1 < argc)
      input_file = argv[++i];
    else if (arg == "-r" && i + 1 < argc)
      reference_price = std::stod(argv[++i]);
    else
    {
      std::cerr << "Usage: auction -i <input_file> -r <reference_price>\n";
      return 1;
    }
  }

  if (input_file.empty() || std::isnan(reference_price)
      || reference_price < 0.0)
  {
    std::cerr << "Usage: auction -i <input_file> -r <reference_price>\n";
    return 1;
  }

  try
  {
    auto orders = parse::parse_orders(input_file);

    if (orders.empty())
    {
      std::cerr << "Input file contains no valid orders.\n";
      return 1;
    }

    CASBook book(orders.front().symb);
    for (auto& order : orders)
      book.add(std::move(order));

    auto result = book.uncross(reference_price);

    std::cout << std::setprecision(15) << "Price: " << result.price << "\n"
              << "Crossed volume: " << result.crossed_volume << "\n"
              << "Imbalance: " << result.imbalance << "\n";
  }
  catch (const std::exception& e)
  {
    std::cerr << "auction error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
