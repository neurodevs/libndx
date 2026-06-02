#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <functional>
#include <unordered_map>
#include "ndx/acquisition_backend.hpp"
#include "ndx/ndx_ffi.hpp"

struct AlwaysOnBleProvider : ndx::BleProvider {
  std::unordered_map<std::string, std::function<void(const ndx::Packet&)>> callbacks;
  ndx::OnConnectedCallback on_connected;
  bool is_powered_on() override { return true; }
  void scan_for_peripheral(const std::string&, ndx::CharCallbacks cbs,  ndx::OnConnectedCallback connected) override {
    for (auto& e : cbs) callbacks[e.char_uuid] = std::move(e.on_data);
    on_connected = std::move(connected);
  }
  void scan_all(int, ndx::ScanResultCallback) override {}
  void simulate_packet(const ndx::Packet& p) {
    for (auto& [uuid, cb] : callbacks) cb(p);
  }
  void simulate_connected(ndx::Peripheral* peripheral = nullptr) {
    if (on_connected) on_connected(peripheral);
  }
  int rssi = 0;
  int read_rssi() override { return rssi; }

  std::string last_write_char_uuid;
  std::vector<uint8_t> last_write_data;
  void write_characteristic(const std::string& char_uuid, const uint8_t* data, size_t len) override {
    last_write_char_uuid = char_uuid;
    last_write_data.assign(data, data + len);
  }

  std::string disconnect_requested_for;
  void disconnect_peripheral(const std::string& uuid) override {
    disconnect_requested_for = uuid;
  }
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
    static CharCallback cb{"test-char", nullptr, [](const uint8_t*, size_t, double) {}};
    const char* result = start_ble_backend(valid_uuid.c_str(), nullptr, &cb, 1);
    return nlohmann::json::parse(result);
  }

  nlohmann::json write(const char* char_uuid = "char-uuid", const char* cmd = "p50") {
    const char* result = write_ble_characteristic(valid_uuid.c_str(), char_uuid, cmd);
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop() {
    const char* result = stop_ble_backend(valid_uuid.c_str());
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
        void start(ndx::CharCallbacks, ndx::OnConnectedCallback) override { throw std::runtime_error("internal server error"); }
        void stop() override { throw std::runtime_error("internal server error"); }
        int read_rssi() override { throw std::runtime_error("internal server error"); }
        void write_characteristic(const std::string&, const uint8_t*, size_t) override {
          throw std::runtime_error("internal server error");
        }
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

  static on_data_fn fn = [](const uint8_t* data, size_t len, double timestamp_ms) {
    received = ndx::Packet{
      std::vector<uint8_t>(data, data + len),
      static_cast<uint64_t>(timestamp_ms)
    };
  };
  static CharCallback cb{"test-char", nullptr, fn};

  start_ble_backend(valid_uuid.c_str(), nullptr, &cb, 1);
  provider->simulate_packet({{42, 43}, 1000});

  REQUIRE(received.data == std::vector<uint8_t>{42, 43});
  REQUIRE(received.timestamp_ms == 1000);
}

TEST_CASE_METHOD(ValidBleFixture, "start_ble_backend routes multiple callbacks to their respective char UUIDs") {
  static int a_calls = 0, b_calls = 0;
  static CharCallback cbs[] = {
    {"char-a", nullptr, [](const uint8_t*, size_t, double) { a_calls++; }},
    {"char-b", nullptr, [](const uint8_t*, size_t, double) { b_calls++; }},
  };

  start_ble_backend(valid_uuid.c_str(), nullptr, cbs, 2);
  provider->simulate_packet(ndx::Packet{});

  REQUIRE(a_calls == 1);
  REQUIRE(b_calls == 1);
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

TEST_CASE_METHOD(BleFfiFixture, "write_ble_characteristic returns 400 if backend not found") {
  auto json = nlohmann::json::parse(write_ble_characteristic("unknown-uuid", "char-uuid", ""));
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "backend not found");
}

TEST_CASE_METHOD(ValidBleFixture, "write_ble_characteristic forwards data to provider") {
  BleFfiFixture::write("273E0001-4C4D-454D-96BE-F03BAC821358", "p50");
  REQUIRE(provider->last_write_char_uuid == "273E0001-4C4D-454D-96BE-F03BAC821358");
  REQUIRE(provider->last_write_data == std::vector<uint8_t>{0x04, 'p', '5', '0', '\n'});
}

TEST_CASE_METHOD(BleFfiFixture, "write_ble_characteristic returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = BleFfiFixture::write();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
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

TEST_CASE_METHOD(ValidBleFixture, "start_ble_backend invokes on_connected callback with peripheral name") {
  static std::string received_name;
  static on_connected_fn fn = [](const Peripheral* p) {
    received_name = p && p->name ? p->name : "";
  };
  static CharCallback cb{"test-char", nullptr, [](const uint8_t*, size_t, double) {}};

  start_ble_backend(valid_uuid.c_str(), fn, &cb, 1);
  ndx::Peripheral peripheral{"test-uuid", "Muse-1234"};
  provider->simulate_connected(&peripheral);

  REQUIRE(received_name == "Muse-1234");
}
