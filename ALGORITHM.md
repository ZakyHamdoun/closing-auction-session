## Auction Algorithm

The implementation determines the closing auction (cross) price from the orders stored in a `CASBook`. The algorithm follows the auction rules by evaluating every distinct limit-order price as a possible auction price and selecting the best candidate according to the required tie-breaking rules.

### 1. Order classification

The first step is performed by `partition_orders()`.

Orders are divided into:

* limit buy orders;
* limit sell orders;
* market buy orders;
* market sell orders.

A market order is identified by a price of `0.0`.

The total volume of market orders is accumulated separately because market orders are compatible with every possible auction price. The implementation also records the timestamp of the oldest market buy and market sell order, which is needed by the final tie-breaking rule.

Limit buy and sell orders are stored as pointers to the original orders rather than being copied. This reduces unnecessary memory usage.

### 2. Sorting and index construction

The limit buy and sell orders are independently sorted by price in ascending order.

For buy orders, the implementation additionally constructs a suffix-minimum timestamp array:

```text
min_ts_buy_suffix[i]
    = minimum timestamp among buy orders from i to the end
```

Since buy orders are sorted in ascending price order, the orders eligible at a candidate price `p` are precisely the buy orders from the first index whose price is at least `p` to the end of the array.

Consequently, the suffix array allows the timestamp of the oldest eligible buy order to be obtained in constant time, without scanning all eligible buy orders for every candidate price.

### 3. Initial eligible quantities

Before evaluating candidate prices, the implementation initializes:

```text
buy_qty  = total market-buy volume + total limit-buy volume
sell_qty = total market-sell volume
```

At the lowest candidate price, all limit buys are initially eligible, while no limit sells have yet become eligible.

The algorithm then progressively moves through the sorted candidate prices. Instead of recalculating the eligible volume from scratch for every price, it incrementally updates the two quantities.

For a candidate price `p`:

* a buy order remains eligible when `buy_price >= p`;
* a sell order becomes eligible when `sell_price <= p`;
* market orders are always eligible.

Thus, as the candidate price increases:

* buy orders with prices below `p` are removed from `buy_qty`;
* sell orders with prices up to `p` are added to `sell_qty`.

Each order is therefore processed only once during this phase.

### 4. Candidate prices

The auction price must be one of the prices appearing in a limit order. The implementation merges the two sorted price sequences and processes each distinct price once.

At each iteration, the next candidate price is the smaller of the next unprocessed buy and sell price. Orders having the same price are skipped together after that price has been evaluated.

This produces all possible auction prices in ascending order without requiring a separate list of candidate prices.

### 5. Evaluation of a candidate price

For every candidate price `p`, `evaluate_candidate()` calculates:

#### Crossed volume

The maximum quantity that can be matched at the price is:

```text
crossed volume = min(eligible buy volume, eligible sell volume)
```

This corresponds to the maximum number of shares that can be exchanged at that price.

#### Imbalance

The remaining imbalance is calculated as:

```text
imbalance = eligible buy volume - eligible sell volume
```

A positive value represents a buy surplus, while a negative value represents a sell surplus.

The implementation also calculates the absolute value of the imbalance because the auction rules use the magnitude when comparing candidate prices.

### 6. Selection of the auction price

Candidate prices are compared using the following priority order.

#### First criterion: maximum crossed volume

The price producing the largest crossed volume is preferred.

This is the primary auction objective: maximize the number of shares exchanged.

#### Second criterion: minimum imbalance

If several prices produce the same maximum crossed volume, the price with the smallest absolute imbalance is selected.

In other words, among equally executable prices, the algorithm prefers the one leaving the fewest eligible shares unmatched.

#### Third criterion: distance from the reference price

If the crossed volume and absolute imbalance are both equal, the implementation selects the price closest to the supplied reference price:

```text
abs(candidate_price - reference_price)
```

This implements the third auction criterion.

#### Fourth criterion: oldest eligible order

If two prices are still tied, the timestamp of the oldest eligible order is considered.

The implementation determines the oldest eligible buy order using the precomputed suffix-minimum timestamp array and the oldest eligible sell order by maintaining the minimum timestamp while sell orders become eligible.

The final rule is:

* if the oldest eligible order is a buy, choose the lower price;
* if the oldest eligible order is a sell, choose the higher price.

This completes the tie-breaking cascade.

### 7. Overall algorithm

The complete process can therefore be summarized as:

1. Separate market and limit orders.
2. Accumulate market-order volumes and oldest market-order timestamps.
3. Sort limit buys and sells by price.
4. Build a suffix minimum-timestamp index for buy orders.
5. Initialize the eligible buy and sell volumes.
6. Traverse all distinct limit-order prices in ascending order.
7. Incrementally update the eligible buy and sell volumes.
8. Calculate crossed volume and imbalance for the current price.
9. Compare the candidate with the current best price using:
   * maximum crossed volume;
   * minimum absolute imbalance;
   * minimum distance from the reference price;
   * oldest-order tie breaker.
10. Return the best candidate, or zero-valued statistics if no candidate price exists.

## Time Complexity

Let `N` be the total number of orders.

Partitioning the orders requires one pass over the input:

```text
O(N)
```

The limit buy and sell orders are then sorted. In the worst case, sorting dominates the complexity:

```text
O(N log N)
```

Building the buy suffix timestamp array takes:

```text
O(N)
```

The candidate-price traversal is linear. Although there are nested `while` loops, each buy or sell order advances through its corresponding array at most once. Therefore, the total cost of the traversal is:

```text
O(N)
```

Candidate evaluation itself takes constant time, `O(1)`, because all required quantities and timestamps are maintained or indexed.

Consequently, the overall time complexity is:

```text
O(N log N)
```

The sorting step is the dominant operation.

## Space Complexity

The implementation stores pointers to the original orders in the buy and sell vectors, requiring:

```text
O(N)
```

additional space.

The buy suffix timestamp array also requires up to `O(N)` storage.

No matrix or price-by-price order book is constructed, and the algorithm does not create copies of the orders themselves.

Therefore, the overall auxiliary space complexity is:

```text
O(N)
```

## Algorithm Used to Find the Auction Price

The implementation uses a **sorted sweep-line / incremental aggregation algorithm**.

Instead of testing every possible pair of buy and sell orders, it observes that the auction price can only be one of the distinct limit-order prices. After sorting the orders by price, the algorithm sweeps through these prices in ascending order.

At any point in the sweep, it maintains the aggregate volume of all orders eligible at the current price:

```text
eligible buys  = market buys + buys with price >= p
eligible sells = market sells + sells with price <= p
```

The crossed volume and imbalance can therefore be obtained directly from two aggregate quantities:

```text
crossed volume = min(buy_qty, sell_qty)
imbalance = buy_qty - sell_qty
```

This avoids repeatedly scanning the entire order book for each candidate price.

The algorithm is consequently significantly more efficient than a naïve implementation that evaluates every candidate price by scanning all orders, which could require `O(N²)` time.

The implemented method reduces the auction calculation to `O(N log N)` time through sorting followed by a linear sweep.

## Correctness of the Price Selection

For every candidate limit price, the implementation considers exactly the orders that are compatible with that price:

* market buys and sells are always eligible;
* buy orders with `price >= p` are eligible;
* sell orders with `price <= p` are eligible.

Therefore, `min(buy_qty, sell_qty)` represents the maximum executable volume at that price.

Since every distinct limit-order price is considered, the algorithm evaluates every possible auction price specified by the auction rules. The comparison performed by `evaluate_candidate()` then applies the required selection criteria in their specified priority order.

The resulting price is consequently the best candidate according to:

```text
maximum crossed volume
        ↓
minimum absolute imbalance
        ↓
minimum distance from reference price
        ↓
oldest-order price direction
```

## Known Limitations and Implementation Considerations

The implementation assumes that the aggregate order volumes fit in `uint64_t`. If the sum of volumes exceeds the maximum value representable by `uint64_t`, the arithmetic can overflow.

Similarly, the final signed imbalance is represented by `int64_t`. Extremely large aggregate volumes could therefore exceed the range of the signed representation when converting the unsigned difference to `int64_t`.

The implementation also uses `double` for prices. Because floating-point values are compared directly, the behavior depends on the exact binary representation of the parsed prices. This is generally acceptable when the input price precision is controlled, but a fixed-point representation (for example, integer ticks representing the smallest allowed price increment) would provide stronger numerical guarantees for a production trading system.

When two eligible orders have exactly the same timestamp, the code resolves the oldest-order direction in favor of a buy. The input specification does not define a separate rule for identical timestamps, so this is an implementation-defined resolution of that otherwise ambiguous case.

Finally, if there is no limit-order price from which a valid auction price can be selected, `uncross()` returns zero-valued statistics. This is consistent with the interface requirement that zero be returned when no auction price can be calculated.
