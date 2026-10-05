#include "book/cas-book.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

CASBook::CASBook(std::string symbol)
  : symbol_(std::move(symbol))
{}

void CASBook::add(Order order) { orders_.push_back(std::move(order)); }

const std::string& CASBook::symbol() const noexcept { return symbol_; }

/*
** Step 2: The auction price must be one of the order prices.
** Partition orders into limit buys/sells (price != 0) and market orders
** (price == 0). Collect every distinct limit price as a candidate.
*/
void CASBook::partition_orders(std::vector<const Order*>& buys,
                               std::vector<const Order*>& sells,
                               std::vector<double>& candidates,
                               std::uint64_t& market_buy_vol,
                               std::uint64_t& market_sell_vol,
                               std::uint64_t& oldest_market_buy,
                               std::uint64_t& oldest_market_sell) const
{
  constexpr auto MAX_TS = std::numeric_limits<std::uint64_t>::max();
  market_buy_vol = 0;
  market_sell_vol = 0;
  oldest_market_buy = MAX_TS;
  oldest_market_sell = MAX_TS;

  for (const auto& o : orders_)
  {
    bool is_market = (o.price == 0.0);
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
        candidates.push_back(o.price);
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
      {
        sells.push_back(&o);
        candidates.push_back(o.price);
      }
    }
  }

  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()),
                   candidates.end());
}

/*
** Sort limit orders by price (ascending) and build:
**   - buy_vol_prefix / sell_vol_prefix  (cumulative volumes)
**   - min_ts_buy_suffix / min_ts_sell_prefix (oldest eligible timestamp)
** These allow O(log N) volume and O(1) timestamp lookups per candidate.
*/
void CASBook::build_index(std::vector<const Order*>& buys,
                          std::vector<const Order*>& sells,
                          std::vector<std::uint64_t>& buy_vol_prefix,
                          std::vector<std::uint64_t>& sell_vol_prefix,
                          std::vector<std::uint64_t>& min_ts_buy_suffix,
                          std::vector<std::uint64_t>& min_ts_sell_prefix)
{
  constexpr auto MAX_TS = std::numeric_limits<std::uint64_t>::max();
  auto by_price = [](const Order* a, const Order* b) {
    return a->price < b->price;
  };
  std::sort(buys.begin(), buys.end(), by_price);
  std::sort(sells.begin(), sells.end(), by_price);

  // Volume prefix sums.
  buy_vol_prefix.assign(buys.size() + 1, 0);
  for (std::size_t i = 0; i < buys.size(); ++i)
    buy_vol_prefix[i + 1] = buy_vol_prefix[i] + buys[i]->volume;

  sell_vol_prefix.assign(sells.size() + 1, 0);
  for (std::size_t i = 0; i < sells.size(); ++i)
    sell_vol_prefix[i + 1] = sell_vol_prefix[i] + sells[i]->volume;

  // Oldest-timestamp suffix (buys) and prefix (sells) for Step 7.
  min_ts_buy_suffix.assign(buys.size() + 1, MAX_TS);
  for (std::size_t i = buys.size(); i > 0; --i)
    min_ts_buy_suffix[i - 1] =
      std::min(min_ts_buy_suffix[i], buys[i - 1]->timestamp);

  min_ts_sell_prefix.assign(sells.size() + 1, MAX_TS);
  for (std::size_t i = 0; i < sells.size(); ++i)
    min_ts_sell_prefix[i + 1] =
      std::min(min_ts_sell_prefix[i], sells[i]->timestamp);
}

/*
** Steps 3–7: For a single candidate price p, compute crossed volume and
** imbalance, then decide whether p beats the current best according to the
** auction tie-breaking cascade.
*/
void CASBook::evaluate_candidate(
  double p,
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
  bool& have_best)
{
  constexpr auto MAX_TS = std::numeric_limits<std::uint64_t>::max();

  // Step 4: Eligible buys have price >= p, eligible sells have price <= p.
  auto bi = static_cast<std::size_t>(std::distance(
    buys.begin(),
    std::lower_bound(buys.begin(), buys.end(), p,
                     [](const Order* o, double v) { return o->price < v; })));
  auto si = static_cast<std::size_t>(std::distance(
    sells.begin(),
    std::upper_bound(sells.begin(), sells.end(), p,
                     [](double v, const Order* o) { return v < o->price; })));

  std::uint64_t bq =
    market_buy_vol + (buy_vol_prefix.back() - buy_vol_prefix[bi]);
  std::uint64_t sq = market_sell_vol + sell_vol_prefix[si];

  // Step 3: Crossed volume = minimum of total eligible buy / sell volume.
  std::uint64_t volume = std::min(bq, sq);

  // Step 5: Signed imbalance (positive = buy surplus, negative = sell surplus).
  auto imbalance =
    static_cast<std::int64_t>(bq) - static_cast<std::int64_t>(sq);
  std::uint64_t abs_imb = (bq >= sq) ? (bq - sq) : (sq - bq);

  // Step 7: Determine the side of the oldest eligible order.
  std::uint64_t oldest_ts = MAX_TS;
  OrderType oldest_side = OrderType::BUY;
  auto consider = [&](std::uint64_t ts, OrderType side) {
    if (ts < oldest_ts || (ts == oldest_ts && side == OrderType::BUY))
    {
      oldest_ts = ts;
      oldest_side = side;
    }
  };

  if (oldest_market_buy != MAX_TS)
    consider(oldest_market_buy, OrderType::BUY);
  if (oldest_market_sell != MAX_TS)
    consider(oldest_market_sell, OrderType::SELL);
  if (bi < buys.size())
    consider(min_ts_buy_suffix[bi], OrderType::BUY);
  if (si > 0)
    consider(min_ts_sell_prefix[si], OrderType::SELL);

  /*
  ** Steps 3 to 7 cascade: maximize volume, then minimize |imbalance|,
  ** then minimize distance to reference, then apply oldest-order tie-break.
  */
  double distance = std::abs(p - reference_price);
  bool better = !have_best;

  if (!better && volume > best.crossed_volume)
    better = true; // Step 3.
  if (!better && volume == best.crossed_volume)
  {
    if (abs_imb < best_abs_imbalance)
      better = true; // Step 5.
    else if (abs_imb == best_abs_imbalance)
    {
      if (distance < best_distance)
        better = true; // Step 6.
      else if (distance == best_distance)
      {
        better = (oldest_side == OrderType::BUY && p < best.price)
          || (oldest_side == OrderType::SELL && p > best.price); // Step 7.
      }
    }
  }

  if (better)
  {
    have_best = true;
    best = {p, volume, imbalance};
    best_abs_imbalance = abs_imb;
    best_distance = distance;
  }
}

CASBook::Stats CASBook::uncross(double reference_price) const
{
  std::vector<const Order*> buys, sells;
  std::vector<double> candidates;
  std::uint64_t market_buy_vol, market_sell_vol;
  std::uint64_t oldest_market_buy, oldest_market_sell;

  // Phase 1 – partition.
  partition_orders(buys, sells, candidates, market_buy_vol, market_sell_vol,
                   oldest_market_buy, oldest_market_sell);
  if (candidates.empty())
    return {};

  // Phase 2 – sort & build prefix/suffix arrays.
  std::vector<std::uint64_t> buy_vol_prefix, sell_vol_prefix;
  std::vector<std::uint64_t> min_ts_buy_suffix, min_ts_sell_prefix;
  build_index(buys, sells, buy_vol_prefix, sell_vol_prefix, min_ts_buy_suffix,
              min_ts_sell_prefix);

  // Phase 3 – evaluate every candidate price.
  bool have_best = false;
  Stats best{};
  std::uint64_t best_abs_imbalance = 0;
  double best_distance = 0.0;

  for (double p : candidates)
    evaluate_candidate(p, reference_price, buys, sells, market_buy_vol,
                       market_sell_vol, oldest_market_buy, oldest_market_sell,
                       buy_vol_prefix, sell_vol_prefix, min_ts_buy_suffix,
                       min_ts_sell_prefix, best, best_abs_imbalance,
                       best_distance, have_best);

  return have_best ? best : Stats{};
}