#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "book/cas-book.hpp"
#include "parse/parser.hpp"

struct TestCase
{
  std::string name;
  std::string filename;
  double reference_price;
  double expected_price;
  std::uint64_t expected_volume;
  std::int64_t expected_imbalance;
};

bool run_test(const TestCase& tc)
{
  try
  {
    auto orders = parse::parse_orders(tc.filename);
    if (orders.empty() && tc.expected_price != 0)
    {
      std::cerr << "FAIL: " << tc.name << " (no orders)\n";
      return false;
    }

    CASBook book(orders.empty() ? "TEST" : orders.front().symb);
    for (auto& o : orders)
      book.add(std::move(o));

    auto result = book.uncross(tc.reference_price);

    if (std::abs(result.price - tc.expected_price) > 1e-9
        || result.crossed_volume != tc.expected_volume
        || result.imbalance != tc.expected_imbalance)
    {
      std::cerr << "FAIL: " << tc.name << "\n"
                << "  Expected: Price=" << tc.expected_price
                << ", Vol=" << tc.expected_volume
                << ", Imb=" << tc.expected_imbalance << "\n"
                << "  Got:      Price=" << result.price
                << ", Vol=" << result.crossed_volume
                << ", Imb=" << result.imbalance << "\n";
      return false;
    }
    std::cout << "PASS: " << tc.name << "\n";
    return true;
  }
  catch (const std::exception& e)
  {
    std::cerr << "FAIL: " << tc.name << " (Exception: " << e.what() << ")\n";
    return false;
  }
}

int main()
{
  std::vector<TestCase> tests = {
    {"AAPL - Provided", "tests/data/test_aapl_default.csv", 275.99, 270.39, 100,
     100},
    {"AAPL - Market Orders Only", "tests/data/test_aapl_market_only.csv", 100.0,
     0.0, 0, 0},
    {"AAPL - Limit Orders Only", "tests/data/test_aapl_limit_only.csv", 100.0,
     101.0, 70, 30},
    {"AAPL - Single Order", "tests/data/test_aapl_single.csv", 100.0, 100.0, 0,
     150},
    {"AAPL - Same Candidate Volume",
     "tests/data/test_aapl_same_candidate_vol.csv", 100.0, 98.0, 100, 0},
    {"AAPL - Multiple Limit Orders at Same Price",
     "tests/data/test_aapl_multiple_same_price.csv", 100.0, 50.0, 50, 50},
    {"AAPL - Market Buy vs Limit Sell",
     "tests/data/test_aapl_market_buy_vs_limit_sell.csv", 100.0, 75.0, 25, -5},
    {"AAPL - Multiple buy and Sell Price levels w/ Partial Fills",
     "tests/data/test_aapl_multiple_buy_sell_partial_fills.csv", 100.0, 50.0,
     70, 30},
    {"AAPL - Market Orders Mixed w/ Limit Orders",
     "tests/data/test_aapl_both_market_limit.csv", 100.0, 99.0, 70, -40},
    {"ARKW - Provided", "tests/data/test_arkw.csv", 51.84, 52.94, 315004,
     -21008},
  };

  int failed = 0;
  for (const auto& tc : tests)
  {
    if (!run_test(tc))
      failed++;
  }

  if (failed > 0)
  {
    std::cerr << failed << " tests failed!\n";
    return 1;
  }

  std::cout << "All tests passed!\n";
  return 0;
}
