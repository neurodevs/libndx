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
  bool discover_ble_uuid_called = false;
  std::function<void(const std::string&)> discover_ble_uuid_callback;
  void discover_ble_uuid(const std::string&, std::function<void(const std::string&)> cb) override {
    discover_ble_uuid_called = true;
    discover_ble_uuid_callback = std::move(cb);
  }
  void simulate_discovery(const std::string& uuid) {
    if (discover_ble_uuid_callback) discover_ble_uuid_callback(uuid);
  }
  void simulate_packet(const ndx::Packet& p) {
    for (auto& [uuid, cb] : callbacks) cb(p);
  }
  void simulate_connected(ndx::Device* peripheral = nullptr) {
    if (on_connected) on_connected(peripheral);
  }
  int rssi = 0;
  int read_rssi() override { return rssi; }
  bool rssi_interval_active = false;
  int rssi_interval_ms = 0;
  std::function<void(int)> on_rssi;
  void set_rssi_interval(int interval_ms, std::function<void(int)> cb) override {
    rssi_interval_active = true;
    rssi_interval_ms = interval_ms;
    on_rssi = std::move(cb);
  }
  void stop_rssi_interval() override { rssi_interval_active = false; }
  void simulate_rssi(int value) { if (on_rssi) on_rssi(value); }

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

  nlohmann::json set_rssi_interval(int interval_ms = 5000) {
    static on_rssi_fn fn = [](int) {};
    const char* result = set_ble_rssi_interval(valid_uuid.c_str(), interval_ms, fn);
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop_rssi_interval() {
    const char* result = stop_ble_rssi_interval(valid_uuid.c_str());
    return nlohmann::json::parse(result);
  }

  std::string valid_uuid;

  void set_throwing_factory() {
    set_ble_factory([](const std::string& uuid) -> std::shared_ptr<ndx::BleBackend> {
      struct ThrowingBleBackend : ndx::BleBackend {
        using ndx::BleBackend::BleBackend;
        void start(ndx::CharCallbacks, ndx::OnConnectedCallback) override { throw std::runtime_error("internal server error"); }
        void stop() override { throw std::runtime_error("internal server error"); }
        int read_rssi() override { return 0; }
        void set_rssi_interval(int, std::function<void(int)>) override { throw std::runtime_error("internal server error"); }
        void stop_rssi_interval() override { throw std::runtime_error("internal server error"); }
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
  static double received_timestamp_sec = 0.0;
  static std::vector<uint8_t> received_data;

  static on_data_fn fn = [](const uint8_t* data, size_t len, double timestamp_sec) {
    received_data.assign(data, data + len);
    received_timestamp_sec = timestamp_sec;
  };
  static CharCallback cb{"test-char", nullptr, fn};

  start_ble_backend(valid_uuid.c_str(), nullptr, &cb, 1);
  provider->simulate_packet({{42, 43}, 1.0});

  REQUIRE(received_data == std::vector<uint8_t>{42, 43});
  REQUIRE(received_timestamp_sec == Catch::Approx(1.0));
}

TEST_CASE_METHOD(ValidBleFixture, "start_ble_backend preserves sub-millisecond timestamp precision") {
  static double received_timestamp_sec = 0.0;

  static on_data_fn fn = [](const uint8_t*, size_t, double timestamp_sec) {
    received_timestamp_sec = timestamp_sec;
  };
  static CharCallback cb{"test-char", nullptr, fn};

  start_ble_backend(valid_uuid.c_str(), nullptr, &cb, 1);
  provider->simulate_packet({{}, 1750123456.7891234});

  REQUIRE(received_timestamp_sec == Catch::Approx(1750123456.7891234).epsilon(1e-9));
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

TEST_CASE_METHOD(ValidBleFixture, "set_ble_rssi_interval returns ok") {
  auto json = BleFfiFixture::set_rssi_interval();
  REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "set_ble_rssi_interval starts interval on provider with given ms") {
  BleFfiFixture::set_rssi_interval(3000);
  REQUIRE(provider->rssi_interval_active);
  REQUIRE(provider->rssi_interval_ms == 3000);
}

TEST_CASE_METHOD(ValidBleFixture, "set_ble_rssi_interval invokes callback with rssi value") {
  static int received = 0;
  static on_rssi_fn fn = [](int rssi) { received = rssi; };
  set_ble_rssi_interval(valid_uuid.c_str(), 1000, fn);
  provider->simulate_rssi(-68);
  REQUIRE(received == -68);
}

TEST_CASE_METHOD(BleFfiFixture, "set_ble_rssi_interval returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = BleFfiFixture::set_rssi_interval();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}

TEST_CASE_METHOD(ValidBleFixture, "stop_ble_rssi_interval returns ok") {
  BleFfiFixture::set_rssi_interval();
  auto json = BleFfiFixture::stop_rssi_interval();
  REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "stop_ble_rssi_interval stops interval on provider") {
  BleFfiFixture::set_rssi_interval();
  BleFfiFixture::stop_rssi_interval();
  REQUIRE(!provider->rssi_interval_active);
}

TEST_CASE_METHOD(BleFfiFixture, "stop_ble_rssi_interval returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  BleFfiFixture::set_rssi_interval();
  auto json = BleFfiFixture::stop_rssi_interval();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}

TEST_CASE_METHOD(ValidBleFixture, "stop_ble_backend returns ok") {
    BleFfiFixture::start();
    auto json = BleFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "stop_ble_backend calls stop on backend") {
    auto backend = get_ble_backend(valid_uuid);
    BleFfiFixture::start();
    BleFfiFixture::stop();
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
  static std::string received_uuid;
  static std::string received_name;
  static on_connected_fn fn = [](const char* uuid, const char* name) {
    received_uuid = uuid ? uuid : "";
    received_name = name ? name : "";
  };
  static CharCallback cb{"test-char", nullptr, [](const uint8_t*, size_t, double) {}};

  start_ble_backend(valid_uuid.c_str(), fn, &cb, 1);
  ndx::Device peripheral{.id = "test-uuid", .name = "Muse-1234"};
  provider->simulate_connected(&peripheral);

  REQUIRE(received_uuid == "test-uuid");
  REQUIRE(received_name == "Muse-1234");
}

TEST_CASE_METHOD(ValidBleFixture, "create_ble_backend runs again after stop_ble_backend") {
  create_and_parse_valid();
  start();
  stop();

  auto json3 = create_and_parse_valid();
  REQUIRE(json3["status"] == 200);
}

// Auto-discover tests

struct AutoDiscoverFixture : BleFfiFixture {
  static constexpr const char* name_prefix = "Muse";
  static constexpr const char* discovered_uuid = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX";

  AutoDiscoverFixture() {
    set_ble_provider_factory([this]() {
      auto p = std::make_unique<AlwaysOnBleProvider>();
      provider = p.get();
      return p;
    });
  }

  nlohmann::json start_discover(on_discovered_fn cb = nullptr) {
    const char* result = discover_ble_uuid(name_prefix, cb);
    return nlohmann::json::parse(result);
  }

  void simulate_discovery(const std::string& uuid = discovered_uuid) {
    provider->simulate_discovery(uuid);
  }
};

TEST_CASE_METHOD(AutoDiscoverFixture, "discover_ble_uuid returns 200 immediately") {
  auto json = start_discover();
  REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(AutoDiscoverFixture, "discover_ble_uuid calls discover_ble_uuid on provider") {
  start_discover();
  REQUIRE(provider != nullptr);
  REQUIRE(provider->discover_ble_uuid_called);
}

TEST_CASE_METHOD(AutoDiscoverFixture, "discover_ble_uuid invokes on_discovered with discovered uuid") {
  static std::string received_uuid;
  received_uuid = "";
  static on_discovered_fn cb = [](const char* uuid) { received_uuid = uuid; };

  start_discover(cb);
  REQUIRE(provider != nullptr);
  simulate_discovery();

  REQUIRE(received_uuid == discovered_uuid);
}

TEST_CASE_METHOD(BleFfiFixture, "create_ble_backend returns 400 if uuid missing from json") {
  auto json = nlohmann::json::parse(create_ble_backend("{}"));
  REQUIRE(json["status"] == 400);
}