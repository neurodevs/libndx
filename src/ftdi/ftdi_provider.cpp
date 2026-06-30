#include "ndx/ftdi_provider.hpp"

namespace ndx {

namespace {

class NullFtdiProvider : public FtdiProvider {
public:
  void connect(const std::string&, OnDataCallback, OnConnectedCallback) override {}
  void disconnect() override {}
};

}

std::unique_ptr<FtdiProvider> create_ftdi_provider() {
  return std::make_unique<NullFtdiProvider>();
}

}
