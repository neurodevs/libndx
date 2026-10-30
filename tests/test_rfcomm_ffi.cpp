#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include "ndx/acquisition_backend.hpp"
#include "ndx/rfcomm_provider.hpp"
#include "ndx/ndx_ffi.hpp"

namespace {

struct FakeRfcommProvider : ndx::RfcommProvider {
  ndx::OnDataCallback captured_on_data;
  bool should_fail_to_connect = false;

  bool is_powered_on() override { return true; }

  void connect(const std::string&, int, ndx::OnDataCallback on_data, ndx::OnConnectedCallback) override {
    if (should_fail_to_connect) throw std::runtime_error("could not open rfcomm channel");
    captured_on_data = std::move(on_data);
  }
  void disconnect() override {}

  std::vector<uint8_t> written_data;
  bool write(const uint8_t* data, size_t len) override {
    written_data.assign(data, data + len);
    return true;
  }

  void simulate_packet(const ndx::Packet& p) {
    if (captured_on_data) captured_on_data(p);
  }
};

FakeRfcommProvider* g_fake_rfcomm_provider = nullptr;
int g_created_port = -1;

std::vector<uint8_t> g_received_data;
double g_received_timestamp = -1;

void capture_on_data(const uint8_t* data, size_t len, double timestamp_sec) {
  g_received_data.assign(data, data + len);
  g_received_timestamp = timestamp_sec;
}

}

struct RfcommFfiFixture {
  std::string valid_address;

  RfcommFfiFixture() {
    reset_rfcomm_backends();
    set_rfcomm_factory([](const std::string& address, int port) {
      auto provider = std::make_unique<FakeRfcommProvider>();
      g_fake_rfcomm_provider = provider.get();
      g_created_port = port;
      return std::make_shared<ndx::RfcommBackend>(address, port, std::move(provider));
    });
    valid_address = "A0-E6-F8-A5-60-BF";
  }

  nlohmann::json create_and_parse(const char* config_json) {
    return nlohmann::json::parse(create_rfcomm_backend(config_json));
  }

  nlohmann::json start() {
    const char* result = start_rfcomm_backend(valid_address.c_str(), [](const uint8_t*, size_t, double) {});
    return nlohmann::json::parse(result);
  }

  nlohmann::json write(const std::vector<uint8_t>& bytes) {
    return nlohmann::json::parse(write_rfcomm_backend(valid_address.c_str(), bytes.data(), bytes.size()));
  }

  nlohmann::json stop() {
    return nlohmann::json::parse(stop_rfcomm_backend(valid_address.c_str()));
  }

  void set_throwing_factory() {
    set_rfcomm_factory([](const std::string& address, int port) -> std::shared_ptr<ndx::RfcommBackend> {
      struct ThrowingRfcommBackend : ndx::RfcommBackend {
        using ndx::RfcommBackend::RfcommBackend;
        void start(ndx::OnDataCallback, ndx::OnConnectedCallback) override { throw std::runtime_error("hardware fault"); }
        void stop() override { throw std::runtime_error("hardware fault"); }
        bool write(const uint8_t*, size_t) override { throw std::runtime_error("hardware fault"); }
      };
      return std::make_shared<ThrowingRfcommBackend>(address, port, std::make_unique<FakeRfcommProvider>());
    });
  }
};

struct ValidRfcommFixture : RfcommFfiFixture {
  nlohmann::json json = create_and_parse("{\"address\":\"A0-E6-F8-A5-60-BF\"}");
};

TEST_CASE_METHOD(ValidRfcommFixture, "create_rfcomm_backend returns ok") {
  REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidRfcommFixture, "create_rfcomm_backend constructs RfcommBackend instance") {
  REQUIRE(get_rfcomm_backend(valid_address) != nullptr);
}

TEST_CASE_METHOD(ValidRfcommFixture, "create_rfcomm_backend sets proper address") {
  REQUIRE(get_rfcomm_backend(valid_address)->device_id() == "A0-E6-F8-A5-60-BF");
}

TEST_CASE_METHOD(ValidRfcommFixture, "create_rfcomm_backend opens rfcomm channel 1 by default") {
  REQUIRE(g_created_port == 1);
}

TEST_CASE_METHOD(RfcommFfiFixture, "create_rfcomm_backend passes port to the backend") {
  auto json = create_and_parse("{\"address\":\"A0-E6-F8-A5-60-BF\",\"port\":3}");
  REQUIRE(json["status"] == 200);
  REQUIRE(g_created_port == 3);
}

TEST_CASE_METHOD(RfcommFfiFixture, "create_rfcomm_backend accepts colon separated addresses") {
  auto json = create_and_parse("{\"address\":\"A0:E6:F8:A5:60:BF\"}");
  REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidRfcommFixture, "create_rfcomm_backend returns 400 if address is already registered") {
  auto json = create_and_parse("{\"address\":\"A0-E6-F8-A5-60-BF\"}");
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "address already registered");
}

TEST_CASE_METHOD(RfcommFfiFixture, "create_rfcomm_backend returns 400 if address is invalid") {
  auto bad_address = GENERATE("\"not-an-address\"", "\"A0-E6-F8-A5-60\"", "\"A0-E6-F8-A5-60-ZZ\"", "\"A0-E6:F8-A5-60-BF\"");
  auto config = std::string("{\"address\":") + bad_address + "}";
  auto json = create_and_parse(config.c_str());
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "invalid address");
}

TEST_CASE_METHOD(RfcommFfiFixture, "create_rfcomm_backend returns 400 if port is not a valid rfcomm channel") {
  auto bad_port = GENERATE("0", "31", "\"one\"", "1.5");
  auto config = std::string("{\"address\":\"A0-E6-F8-A5-60-BF\",\"port\":") + bad_port + "}";
  auto json = create_and_parse(config.c_str());
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "invalid port");
  REQUIRE(get_rfcomm_backend("A0-E6-F8-A5-60-BF") == nullptr);
}

TEST_CASE_METHOD(RfcommFfiFixture, "create_rfcomm_backend returns 400 if JSON is malformed") {
  auto json = create_and_parse("{");
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "malformed JSON");
}

TEST_CASE_METHOD(RfcommFfiFixture, "create_rfcomm_backend returns 500 on unexpected throw") {
  set_rfcomm_factory([](const std::string&, int) -> std::shared_ptr<ndx::RfcommBackend> {
    throw std::runtime_error("hardware fault");
  });
  auto json = create_and_parse("{\"address\":\"A0-E6-F8-A5-60-BF\"}");
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(RfcommFfiFixture, "discover_rfcomm_addresses returns the address of each paired device with the name prefix") {
  auto list_paired_devices = ndx::RfcommProviderSyscalls::list_paired_devices;
  ndx::RfcommProviderSyscalls::list_paired_devices = []() {
    return std::vector<ndx::Device>{
      {"a0-e6-f8-a5-69-9a", "BH BHT052376-C3"},
      {"90-9c-4a-db-10-95", "AirPods Max"},
      {"a0-e6-f8-a5-60-bf", "BH BHT052039-C2"},
    };
  };

  auto json = nlohmann::json::parse(discover_rfcomm_addresses("BH BHT"));
  ndx::RfcommProviderSyscalls::list_paired_devices = list_paired_devices;

  REQUIRE(json["status"] == 200);
  REQUIRE(json["addresses"] == std::vector<std::string>{"a0-e6-f8-a5-60-bf", "a0-e6-f8-a5-69-9a"});
}

TEST_CASE_METHOD(RfcommFfiFixture, "start_rfcomm_backend returns 400 if backend not found") {
  auto json = start();
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "backend not found");
}

TEST_CASE_METHOD(ValidRfcommFixture, "start_rfcomm_backend returns ok") {
  REQUIRE(RfcommFfiFixture::start()["status"] == 200);
}

TEST_CASE_METHOD(ValidRfcommFixture, "start_rfcomm_backend calls start on backend") {
  RfcommFfiFixture::start();
  REQUIRE(get_rfcomm_backend(valid_address)->is_running());
}

TEST_CASE_METHOD(ValidRfcommFixture, "start_rfcomm_backend forwards received packets to the on_data callback") {
  g_received_data.clear();
  g_received_timestamp = -1;

  start_rfcomm_backend(valid_address.c_str(), capture_on_data);

  ndx::Packet p;
  p.data = {1, 2, 3};
  p.timestamp_sec = 42.0;
  g_fake_rfcomm_provider->simulate_packet(p);

  REQUIRE(g_received_data == std::vector<uint8_t>{1, 2, 3});
  REQUIRE(g_received_timestamp == 42.0);
}

TEST_CASE_METHOD(ValidRfcommFixture, "start_rfcomm_backend returns 400 if backend is already running") {
  RfcommFfiFixture::start();
  auto json = RfcommFfiFixture::start();
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "rfcomm backend already running");
  REQUIRE(get_rfcomm_backend(valid_address)->is_running());
}

TEST_CASE_METHOD(ValidRfcommFixture, "start_rfcomm_backend returns 500 when the channel cannot be opened") {
  g_fake_rfcomm_provider->should_fail_to_connect = true;
  auto json = RfcommFfiFixture::start();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("could not open rfcomm channel") != std::string::npos);
}

TEST_CASE_METHOD(ValidRfcommFixture, "start_rfcomm_backend unregisters the backend when the channel cannot be opened") {
  g_fake_rfcomm_provider->should_fail_to_connect = true;
  RfcommFfiFixture::start();
  REQUIRE(get_rfcomm_backend(valid_address) == nullptr);
}

TEST_CASE_METHOD(ValidRfcommFixture, "write_rfcomm_backend returns ok") {
  REQUIRE(write({0x02})["status"] == 200);
}

TEST_CASE_METHOD(ValidRfcommFixture, "write_rfcomm_backend forwards every byte to the provider, including zeros") {
  write({0x02, 0x23, 0x00, 0x00, 0x03});
  REQUIRE(g_fake_rfcomm_provider->written_data == std::vector<uint8_t>{0x02, 0x23, 0x00, 0x00, 0x03});
}

TEST_CASE_METHOD(RfcommFfiFixture, "write_rfcomm_backend returns 400 if backend not found") {
  auto json = write({0x02});
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "backend not found");
}

TEST_CASE_METHOD(RfcommFfiFixture, "write_rfcomm_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("{\"address\":\"A0-E6-F8-A5-60-BF\"}");
  auto json = write({0x02});
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(ValidRfcommFixture, "stop_rfcomm_backend returns ok") {
  RfcommFfiFixture::start();
  REQUIRE(RfcommFfiFixture::stop()["status"] == 200);
}

TEST_CASE_METHOD(ValidRfcommFixture, "stop_rfcomm_backend calls stop on backend") {
  RfcommFfiFixture::start();
  auto backend = get_rfcomm_backend(valid_address);
  RfcommFfiFixture::stop();
  REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidRfcommFixture, "stop_rfcomm_backend unregisters the backend") {
  RfcommFfiFixture::start();
  RfcommFfiFixture::stop();
  REQUIRE(get_rfcomm_backend(valid_address) == nullptr);
}

TEST_CASE_METHOD(ValidRfcommFixture, "create_rfcomm_backend returns ok again after stop_rfcomm_backend") {
  RfcommFfiFixture::start();
  RfcommFfiFixture::stop();
  REQUIRE(create_and_parse("{\"address\":\"A0-E6-F8-A5-60-BF\"}")["status"] == 200);
}

TEST_CASE_METHOD(RfcommFfiFixture, "pump_rfcomm_events returns ok") {
  REQUIRE(nlohmann::json::parse(pump_rfcomm_events())["status"] == 200);
}
