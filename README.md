# Closing Auction Session

This project implements an equity trading auction engine. On most equity exchanges, at the end of the trading session, a closing auction determines a cross price used as the reference price for that trading day. This engine calculates this cross price given a set of buy and sell orders.

## Prerequisites

- **CMake**: Minimum version 3.16
- **C++ Compiler**: Must support **C++17** (e.g., GCC, Clang, MSVC)

## How to clone the project

```bash
git clone <repository_url>
cd closing-auction-session
```

## How to build the project

You can build the project using CMake:

```bash
rm -rf build
cmake -S . -B build
cmake --build build --config Release
```

## How to run the project

After building, you can run the `auction` executable. You need to provide an input file containing the orders and a reference price.

```bash
./build/auction -i input.txt -r 275.99
```
*(Note: On Windows with MSVC, the executable might be located under `./build/Release/auction.exe`)*

## How to run the tests

To run the test suite, you can use CTest (provided with CMake) after building the project:

```bash
cd build
ctest --output-on-failure
```

Alternatively, you can directly execute the test binary:
```bash
./build/run_tests
```

## Build and run the tests in a one-liner

```bash
rm -rf build && cmake -S . -B build && cmake --build build && cd build && ctest --output-on-failure
```

## Algorithm and Limitations

Please [refer to ALGORITHM.md](ALGORITHM.md) for a comprehensive description of the algorithm used to find the auction price, its time and space complexities, as well as the known limitations of the implementation.

## Tests Overview

The `tests/data` directory contains various scenarios to validate the algorithm. Here is a description of each test based on the test suite:

- **AAPL - Provided**: The default AAPL test case provided in the initial subject specifications.
- **AAPL - Market Orders Only**: A scenario where the book contains exclusively market buy and market sell orders.
- **AAPL - Limit Orders Only**: A scenario where the book contains exclusively limit buy and limit sell orders.
- **AAPL - Single Order**: An edge case where the order book only receives a single order.
- **AAPL - Same Candidate Volume**: Tests the tie-breaking rules when multiple prices yield the exact same crossed volume.
- **AAPL - Multiple Limit Orders at Same Price**: Verifies correct volume aggregation when multiple limit orders are placed at the exact same price.
- **AAPL - Market Buy vs Limit Sell**: A simple cross between a market buy order and a limit sell order.
- **AAPL - Multiple buy and Sell Price levels w/ Partial Fills**: A complex book with multiple price levels resulting in orders being only partially filled.
- **AAPL - Market Orders Mixed w/ Limit Orders**: A realistic book combining both market and limit orders on both sides.
- **ARKW - Provided**: Another provided large test case with a high volume of orders for the ARKW symbol.

### Test Run Output Example

```bash
Test project <path>
    Start 1: AllTests
1/1 Test #1: AllTests .........................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.03 sec
```

### ARKW Output Example

```bash
❯ ./build/auction -i tests/data/test_arkw.csv -r 51.84
Price: 52.94
Crossed volume: 315004
Imbalance: -21008
```