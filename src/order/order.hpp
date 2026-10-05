#pragma once

#include <cstdint>
#include <string>

enum class OrderType : std::uint8_t
{
  BUY,
  SELL
};

struct Order
{
  std::uint64_t timestamp{};
  std::string symb;
  OrderType side{};
  std::uint64_t volume{};
  double price{};
};