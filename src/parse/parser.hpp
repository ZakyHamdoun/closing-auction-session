#pragma once

#include <string>
#include <vector>

#include "order/order.hpp"

namespace parse
{
  std::vector<Order> parse_orders(const std::string& file_path);
}
