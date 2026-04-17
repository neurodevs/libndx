#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

struct AlwaysOnBleProvider : ndx::BleProvider {
  ndx::OnDataCallback on_data;
  bool is_powered_on() override { return true; }
  void scan_for_peripheral(const std::string&, ndx::OnDataCallback cb) override { on_data = cb; }
  void scan_all(int, ndx::ScanResultCallback) override {}
  void simulate_packet(const ndx::Packet& p) { if (on_data) on_data(p); }
  int rssi = 0;
  int read_rssi() override { return rssi; }
};

struct BleFfiFixture {
  AlwaysOnBleProvider* provider = nullptr;

  BleFfiFixture() {
    reset_ble_backends();
    set_ble_factory([this](const std::string& uuid) {
      auto p = std::make_unique<AlwaysOnBleProvider>();
      provider = p.get();
      return std::make_shared<ndx::BleBackend>(uuid, std::move(p));
    });
    valid_uuid = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX";
  }

  nlohmann::json create_and_parse(std::string uuid) {
    std::string config_json = "{\"uuid\":\"" + uuid + "\"}";
    const char* result = create_ble_backend(config_json.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json create_and_parse_valid() {
    return create_and_parse(valid_uuid);
  }

  nlohmann::json start() {
    const char* result = start_ble_backend(valid_uuid.c_str(), [](const char* packet_json) {});
    return nlohmann::json::parse(result);
  }

  nlohmann::json write() {
    const char* result = write_ble_characteristic("device-uuid", "char-uuid", "char-value");
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop() {
    const char* result = stop_ble_backend(valid_uuid.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json destroy() {
    const char* result = destroy_ble_backend(valid_uuid.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json read_rssi() {
    const char* result = read_ble_rssi(valid_uuid.c_str());
    return nlohmann::json::parse(result);
  }

  std::string valid_uuid;

  void set_throwing_factory() {
    set_ble_factory([](const std::string& uuid) -> std::shared_ptr<ndx::BleBackend> {
      struct ThrowingBleBackend : ndx::BleBackend {
        using ndx::BleBackend::BleBackend;
        void start(ndx::OnDataCallback) override { throw std::runtime_error("internal server error"); }
        void stop() override { throw std::runtime_error("internal server error"); }
        void destroy() override { throw std::runtime_error("internal server error"); }
        int read_rssi() override { throw std::runtime_error("internal server error"); }
      };
      return std::make_shared<ThrowingBleBackend>(uuid, std::make_unique<AlwaysOnBleProvider>());
    });
  }
};

struct ValidBleFixture : BleFfiFixture {
  nlohmann::json json = create_and_parse_valid();
};

TEST_CASE_METHOD(ValidBleFixture, "create_ble_backend returns ok") {
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "create_ble_backend constructs BleBackend instance") {
    auto backend = get_ble_backend(valid_uuid);
    REQUIRE(backend != nullptr);
}

TEST_CASE_METHOD(ValidBleFixture, "create_ble_backend sets proper uuid on backend") {
    auto backend = get_ble_backend(valid_uuid);
    REQUIRE(backend->device_id() == "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
}

TEST_CASE_METHOD(ValidBleFixture, "create_ble_backend returns 400 if uuid is already registered") {
    auto json = create_and_parse_valid();
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "uuid already registered");
}

TEST_CASE_METHOD(BleFfiFixture, "create_ble_backend returns 400 if uuid is not size 36") {
    auto json = create_and_parse("not-a-valid-uuid");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid uuid");
}

TEST_CASE_METHOD(BleFfiFixture, "create_ble_backend returns 400 if uuid does not contain 4 hyphens") {
    auto json = create_and_parse("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid uuid");
}

TEST_CASE_METHOD(BleFfiFixture, "create_ble_backend returns 400 if JSON is malformed") {
    auto json = nlohmann::json::parse(create_ble_backend("{"));
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "malformed JSON");
}

TEST_CASE_METHOD(BleFfiFixture, "create_ble_backend returns 500 on unexpected throw") {
  set_ble_factory([](const std::string&) -> std::shared_ptr<ndx::BleBackend> {
    throw std::runtime_error("internal server error");
  });
  auto json = create_and_parse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}

TEST_CASE_METHOD(ValidBleFixture, "start_ble_backend returns ok") {
    auto json = BleFfiFixture::start();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "start_ble_backend calls start on backend") {
    BleFfiFixture::start();
    auto backend = get_ble_backend(valid_uuid);
    REQUIRE(backend->is_running());
}

TEST_CASE_METHOD(ValidBleFixture, "start_ble_backend invokes C callback when packet fires") {
  static ndx::Packet received;

  auto on_data = [](const char* packet_json) {
    auto j = nlohmann::json::parse(packet_json);
    received = ndx::Packet{
      j["data"].get<std::vector<uint32_t>>(),
      j["timestamp_ms"].get<uint64_t>()
    };
  };

  start_ble_backend(valid_uuid.c_str(), on_data);
  provider->simulate_packet({{42, 43}, 1000});

  REQUIRE(received.data == std::vector<uint32_t>{42, 43});
  REQUIRE(received.timestamp_ms == 1000);
}

TEST_CASE_METHOD(BleFfiFixture, "start_ble_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = BleFfiFixture::start();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}

TEST_CASE_METHOD(ValidBleFixture, "write_ble_characteristic returns ok") {
    auto json = BleFfiFixture::write();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "read_ble_rssi returns ok") {
  auto json = read_rssi();
  REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "read_ble_rssi returns rssi from provider") {
  provider->rssi = -75;
  auto json = read_rssi();
  REQUIRE(json["rssi"] == -75);
}

TEST_CASE_METHOD(BleFfiFixture, "read_ble_rssi returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = read_rssi();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}

TEST_CASE_METHOD(ValidBleFixture, "stop_ble_backend returns ok") {
    BleFfiFixture::start();
    auto json = BleFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "stop_ble_backend calls stop on backend") {
    BleFfiFixture::start();
    BleFfiFixture::stop();
    auto backend = get_ble_backend(valid_uuid);
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(BleFfiFixture, "stop_ble_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  start();
  auto json = stop();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}


TEST_CASE_METHOD(ValidBleFixture, "destroy_ble_backend returns ok") {
    BleFfiFixture::destroy();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "destroy_ble_backend calls stop on backend if running") {
    BleFfiFixture::start();
    auto backend = get_ble_backend(valid_uuid);
    BleFfiFixture::destroy();
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidBleFixture, "destroy_ble_backend removes backend from registry") {
    BleFfiFixture::destroy();
    auto backend = get_ble_backend(valid_uuid);
    REQUIRE(backend == nullptr);
}

TEST_CASE_METHOD(BleFfiFixture, "destroy_ble_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = destroy();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}
