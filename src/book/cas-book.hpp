#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "order/order.hpp"

class CASBook
{
public:
  struct Stats
  {
    double price{0.0};
    std::uint64_t crossed_volume{0};
    std::int64_t imbalance{0};
  };

  explicit CASBook(std::string symbol);

  void add(Order order);

  Stats uncross(double reference_price) const;

  const std::string& symbol() const noexcept;

private:
  std::string symbol_;
  std::vector<Order> orders_;

  /*
  ** Step 2: Partition orders into limit buys/sells, collect candidate prices
  ** and market order aggregates.
  */
  void partition_orders(std::vector<const Order*>& buys,
                        std::vector<const Order*>& sells,
                        std::vector<double>& candidates,
                        std::uint64_t& market_buy_vol,
                        std::uint64_t& market_sell_vol,
                        std::uint64_t& oldest_market_buy,
                        std::uint64_t& oldest_market_sell) const;

  /*
  ** Sort limit orders by price and build prefix/suffix arrays for
  ** O(log N) volume and O(1) oldest-timestamp queries per candidate.
  */
  static void build_index(std::vector<const Order*>& buys,
                          std::vector<const Order*>& sells,
                          std::vector<std::uint64_t>& buy_vol_prefix,
                          std::vector<std::uint64_t>& sell_vol_prefix,
                          std::vector<std::uint64_t>& min_ts_buy_suffix,
                          std::vector<std::uint64_t>& min_ts_sell_prefix);

  /*
  ** Steps 3–7: Evaluate a single candidate cross price and update the
  ** running best if it is strictly superior.
  */
  static void
  evaluate_candidate(double p,
                     double reference_price,
                     const std::vector<const Order*>& buys,
                     const std::vector<const Order*>& sells,
                     std::uint64_t market_buy_vol,
                     std::uint64_t market_sell_vol,
                     std::uint64_t oldest_market_buy,
                     std::uint64_t oldest_market_sell,
                     const std::vector<std::uint64_t>& buy_vol_prefix,
                     const std::vector<std::uint64_t>& sell_vol_prefix,
                     const std::vector<std::uint64_t>& min_ts_buy_suffix,
                     const std::vector<std::uint64_t>& min_ts_sell_prefix,
                     Stats& best,
                     std::uint64_t& best_abs_imbalance,
                     double& best_distance,
                     bool& have_best);
};