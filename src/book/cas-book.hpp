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

  void partition_orders(std::vector<const Order*>& buys,
                        std::vector<const Order*>& sells,
                        std::uint64_t& limit_buy_vol,
                        std::uint64_t& market_buy_vol,
                        std::uint64_t& market_sell_vol,
                        std::uint64_t& oldest_market_buy,
                        std::uint64_t& oldest_market_sell) const;

  static void build_index(std::vector<const Order*>& buys,
                          std::vector<const Order*>& sells,
                          std::vector<std::uint64_t>& min_ts_buy_suffix);

  static void evaluate_candidate(double p,
                                 double reference_price,
                                 std::uint64_t buy_qty,
                                 std::uint64_t sell_qty,
                                 std::uint64_t oldest_buy_ts,
                                 std::uint64_t oldest_sell_ts,
                                 Stats& best,
                                 std::uint64_t& best_abs_imbalance,
                                 double& best_distance,
                                 bool& have_best);
};