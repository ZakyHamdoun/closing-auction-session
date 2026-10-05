#include "book/cas-book.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

CASBook::CASBook(std::string symbol)
  : symbol_(std::move(symbol))
{}

void CASBook::add(Order order) { orders_.push_back(std::move(order)); }

const std::string& CASBook::symbol() const noexcept { return symbol_; }

void CASBook::partition_orders(std::vector<const Order*>& buys,
                               std::vector<const Order*>& sells,
                               std::uint64_t& limit_buy_vol,
                               std::uint64_t& market_buy_vol,
                               std::uint64_t& market_sell_vol,
                               std::uint64_t& oldest_market_buy,
                               std::uint64_t& oldest_market_sell) const
{
  constexpr auto MAX_TS = std::numeric_limits<std::uint64_t>::max();

  limit_buy_vol = 0;
  market_buy_vol = 0;
  market_sell_vol = 0;

  oldest_market_buy = MAX_TS;
  oldest_market_sell = MAX_TS;

  buys.reserve(orders_.size());
  sells.reserve(orders_.size());

  for (const auto& o : orders_)
  {
    const bool is_market = (o.price == 0.0);

    if (o.side == OrderType::BUY)
    {
      if (is_market)
      {
        market_buy_vol += o.volume;
        oldest_market_buy = std::min(oldest_market_buy, o.timestamp);
      }
      else
      {
        buys.push_back(&o);
        limit_buy_vol += o.volume;
      }
    }
    else
    {
      if (is_market)
      {
        market_sell_vol += o.volume;
        oldest_market_sell = std::min(oldest_market_sell, o.timestamp);
      }
      else
        sells.push_back(&o);
    }
  }
}

void CASBook::build_index(std::vector<const Order*>& buys,
                          std::vector<const Order*>& sells,
                          std::vector<std::uint64_t>& min_ts_buy_suffix)
{
  constexpr auto MAX_TS = std::numeric_limits<std::uint64_t>::max();

  auto by_price = [](const Order* a, const Order* b) {
    return a->price < b->price;
  };

  std::sort(buys.begin(), buys.end(), by_price);
  std::sort(sells.begin(), sells.end(), by_price);

  min_ts_buy_suffix.assign(buys.size() + 1, MAX_TS);

  for (std::size_t i = buys.size(); i > 0; --i)
  {
    min_ts_buy_suffix[i - 1] =
      std::min(min_ts_buy_suffix[i], buys[i - 1]->timestamp);
  }
}

/*
** Steps 3–7: Evaluate one candidate price.
** buy_qty and sell_qty are the quantities currently eligible at p.
*/
void CASBook::evaluate_candidate(double p,
                                 double reference_price,
                                 std::uint64_t buy_qty,
                                 std::uint64_t sell_qty,
                                 std::uint64_t oldest_buy_ts,
                                 std::uint64_t oldest_sell_ts,
                                 Stats& best,
                                 std::uint64_t& best_abs_imbalance,
                                 double& best_distance,
                                 bool& have_best)
{
  constexpr auto MAX_TS = std::numeric_limits<std::uint64_t>::max();
  const std::uint64_t volume = std::min(buy_qty, sell_qty);
  const std::uint64_t abs_imbalance =
    (buy_qty >= sell_qty) ? (buy_qty - sell_qty) : (sell_qty - buy_qty);
  const std::int64_t imbalance = (buy_qty >= sell_qty)
    ? static_cast<std::int64_t>(abs_imbalance)
    : -static_cast<std::int64_t>(abs_imbalance);

  std::uint64_t oldest_ts = MAX_TS;
  OrderType oldest_side = OrderType::BUY;

  if (oldest_buy_ts < oldest_ts)
  {
    oldest_ts = oldest_buy_ts;
    oldest_side = OrderType::BUY;
  }

  if (oldest_sell_ts < oldest_ts)
  {
    oldest_ts = oldest_sell_ts;
    oldest_side = OrderType::SELL;
  }
  else if (oldest_sell_ts == oldest_ts && oldest_sell_ts != MAX_TS)
    oldest_side = OrderType::BUY;

  const double distance = std::abs(p - reference_price);

  /*
  ** Tie-breaking cascade:
  **
  ** 1. Maximum crossed volume
  ** 2. Minimum absolute imbalance
  ** 3. Minimum distance from reference price
  ** 4. Oldest eligible BUY -> lower price
  **    Oldest eligible SELL -> higher price
  */
  bool better = !have_best;

  if (!better && volume > best.crossed_volume)
    better = true;

  if (!better && volume == best.crossed_volume)
  {
    if (abs_imbalance < best_abs_imbalance)
      better = true;
    else if (abs_imbalance == best_abs_imbalance)
    {
      if (distance < best_distance)
        better = true;
      else if (distance == best_distance)
      {
        better = (oldest_side == OrderType::BUY && p < best.price)
          || (oldest_side == OrderType::SELL && p > best.price);
      }
    }
  }

  if (better)
  {
    have_best = true;
    best = {p, volume, imbalance};
    best_abs_imbalance = abs_imbalance;
    best_distance = distance;
  }
}

CASBook::Stats CASBook::uncross(double reference_price) const
{
  std::vector<const Order*> buys;
  std::vector<const Order*> sells;

  std::uint64_t limit_buy_vol;
  std::uint64_t market_buy_vol;
  std::uint64_t market_sell_vol;
  std::uint64_t oldest_market_buy;
  std::uint64_t oldest_market_sell;

  partition_orders(buys, sells, limit_buy_vol, market_buy_vol, market_sell_vol,
                   oldest_market_buy, oldest_market_sell);

  if (buys.empty() && sells.empty())
    return {};

  std::vector<std::uint64_t> min_ts_buy_suffix;

  build_index(buys, sells, min_ts_buy_suffix);

  std::uint64_t buy_qty = market_buy_vol + limit_buy_vol;
  std::uint64_t sell_qty = market_sell_vol;
  std::size_t buy_candidate_idx = 0;
  std::size_t sell_candidate_idx = 0;
  std::size_t buy_eligible_idx = 0;
  std::size_t sell_eligible_idx = 0;
  std::uint64_t oldest_eligible_sell = oldest_market_sell;

  bool have_best = false;
  Stats best{};
  std::uint64_t best_abs_imbalance = 0;
  double best_distance = 0.0;

  while (buy_candidate_idx < buys.size() || sell_candidate_idx < sells.size())
  {
    double p;

    if (sell_candidate_idx >= sells.size())
      p = buys[buy_candidate_idx]->price;
    else if (buy_candidate_idx >= buys.size())
      p = sells[sell_candidate_idx]->price;
    else
    {
      p = std::min(buys[buy_candidate_idx]->price,
                   sells[sell_candidate_idx]->price);
    }

    while (buy_eligible_idx < buys.size() && buys[buy_eligible_idx]->price < p)
    {
      buy_qty -= buys[buy_eligible_idx]->volume;
      ++buy_eligible_idx;
    }

    while (sell_eligible_idx < sells.size()
           && sells[sell_eligible_idx]->price <= p)
    {
      sell_qty += sells[sell_eligible_idx]->volume;

      oldest_eligible_sell =
        std::min(oldest_eligible_sell, sells[sell_eligible_idx]->timestamp);

      ++sell_eligible_idx;
    }

    const std::uint64_t oldest_eligible_buy =
      std::min(oldest_market_buy, min_ts_buy_suffix[buy_eligible_idx]);

    evaluate_candidate(p, reference_price, buy_qty, sell_qty,
                       oldest_eligible_buy, oldest_eligible_sell, best,
                       best_abs_imbalance, best_distance, have_best);

    while (buy_candidate_idx < buys.size()
           && buys[buy_candidate_idx]->price == p)
      ++buy_candidate_idx;

    while (sell_candidate_idx < sells.size()
           && sells[sell_candidate_idx]->price == p)
      ++sell_candidate_idx;
  }

  return have_best ? best : Stats{};
}