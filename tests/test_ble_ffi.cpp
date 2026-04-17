#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include "ndx/ndx_ffi.hpp"

struct AlwaysOnBleProvider : ndx::BleProvider {
  ndx::OnDataCallback on_data;
  bool isPoweredOn() override { return true; }
  void scanForPeripheral(const std::string&, ndx::OnDataCallback cb) override { on_data = cb; }
  void scanAll(int, ndx::ScanResultCallback) override {}
  void simulatePacket(const ndx::Packet& p) { if (on_data) on_data(p); }
  int rssi = 0;
  int readRssi() override { return rssi; }
};

struct BleFfiFixture {
  AlwaysOnBleProvider* provider = nullptr;

  BleFfiFixture() {
    resetBleBackends();
    setBleFactory([this](const std::string& uuid) {
      auto p = std::make_unique<AlwaysOnBleProvider>();
      provider = p.get();
      return std::make_shared<ndx::BleBackend>(uuid, std::move(p));
    });
    valid_uuid = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX";
  }

  nlohmann::json createAndParse(std::string uuid) {
    std::string config_json = "{\"uuid\":\"" + uuid + "\"}";
    const char* result = createBleBackend(config_json.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json createAndParseValid() {
    return createAndParse(valid_uuid);
  }

  nlohmann::json start() {
    const char* result = startBleBackend(valid_uuid.c_str(), [](const char* packet_json) {});
    return nlohmann::json::parse(result);
  }

  nlohmann::json write() {
    const char* result = writeBleCharacteristic("device-uuid", "char-uuid", "char-value");
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop() {
    const char* result = stopBleBackend(valid_uuid.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json destroy() {
    const char* result = destroyBleBackend(valid_uuid.c_str());
    return nlohmann::json::parse(result);
  }

  nlohmann::json readRssi() {
    const char* result = readBleRssi(valid_uuid.c_str());
    return nlohmann::json::parse(result);
  }

  std::string valid_uuid;

  void setThrowingFactory() {
    setBleFactory([](const std::string& uuid) -> std::shared_ptr<ndx::BleBackend> {
      struct ThrowingBleBackend : ndx::BleBackend {
        using ndx::BleBackend::BleBackend;
        void start(ndx::OnDataCallback) override { throw std::runtime_error("internal server error"); }
        void stop() override { throw std::runtime_error("internal server error"); }
        void destroy() override { throw std::runtime_error("internal server error"); }
        int readRssi() override { throw std::runtime_error("internal server error"); }
      };
      return std::make_shared<ThrowingBleBackend>(uuid, std::make_unique<AlwaysOnBleProvider>());
    });
  }
};

struct ValidBleFixture : BleFfiFixture {
  nlohmann::json json = createAndParseValid();
};

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns ok") {
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend constructs BleBackend instance") {
    auto backend = getBleBackend(valid_uuid);
    REQUIRE(backend != nullptr);
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend sets proper uuid on backend") {
    auto backend = getBleBackend(valid_uuid);
    REQUIRE(backend->device_id() == "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
}

TEST_CASE_METHOD(ValidBleFixture, "createBleBackend returns 400 if uuid is already registered") {
    auto json = createAndParseValid();
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "uuid already registered");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if uuid is not size 36") {
    auto json = createAndParse("not-a-valid-uuid");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid uuid");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if uuid does not contain 4 hyphens") {
    auto json = createAndParse("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid uuid");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 400 if JSON is malformed") {
    auto json = nlohmann::json::parse(createBleBackend("{"));
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "malformed JSON");
}

TEST_CASE_METHOD(BleFfiFixture, "createBleBackend returns 500 on unexpected throw") {
  setBleFactory([](const std::string&) -> std::shared_ptr<ndx::BleBackend> {
    throw std::runtime_error("internal server error");
  });
  auto json = createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}

TEST_CASE_METHOD(ValidBleFixture, "startBleBackend returns ok") {
    auto json = BleFfiFixture::start();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "startBleBackend calls start on backend") {
    BleFfiFixture::start();
    auto backend = getBleBackend(valid_uuid);
    REQUIRE(backend->is_running());
}

TEST_CASE_METHOD(ValidBleFixture, "startBleBackend invokes C callback when packet fires") {
  static ndx::Packet received;

  auto on_data = [](const char* packet_json) {
    auto j = nlohmann::json::parse(packet_json);
    received = ndx::Packet{
      j["data"].get<std::vector<uint32_t>>(),
      j["timestamp_ms"].get<uint64_t>()
    };
  };

  startBleBackend(valid_uuid.c_str(), on_data);
  provider->simulatePacket({{42, 43}, 1000});

  REQUIRE(received.data == std::vector<uint32_t>{42, 43});
  REQUIRE(received.timestamp_ms == 1000);
}

TEST_CASE_METHOD(BleFfiFixture, "startBleBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = BleFfiFixture::start();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}

TEST_CASE_METHOD(ValidBleFixture, "writeBleCharacteristic returns ok") {
    auto json = BleFfiFixture::write();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "readBleRssi returns ok") {
  auto json = readRssi();
  REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "readBleRssi returns rssi from provider") {
  provider->rssi = -75;
  auto json = readRssi();
  REQUIRE(json["rssi"] == -75);
}

TEST_CASE_METHOD(BleFfiFixture, "readBleRssi returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = readRssi();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}

TEST_CASE_METHOD(ValidBleFixture, "stopBleBackend returns ok") {
    BleFfiFixture::start();
    auto json = BleFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "stopBleBackend calls stop on backend") {
    BleFfiFixture::start();
    BleFfiFixture::stop();
    auto backend = getBleBackend(valid_uuid);
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(BleFfiFixture, "stopBleBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  start();
  auto json = stop();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}


TEST_CASE_METHOD(ValidBleFixture, "destroyBleBackend returns ok") {
    BleFfiFixture::destroy();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidBleFixture, "destroyBleBackend calls stop on backend if running") {
    BleFfiFixture::start();
    auto backend = getBleBackend(valid_uuid);
    BleFfiFixture::destroy();
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidBleFixture, "destroyBleBackend removes backend from registry") {
    BleFfiFixture::destroy();
    auto backend = getBleBackend(valid_uuid);
    REQUIRE(backend == nullptr);
}

TEST_CASE_METHOD(BleFfiFixture, "destroyBleBackend returns 500 on unexpected throw") {
  setThrowingFactory();
  createAndParse("XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX");
  auto json = destroy();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("internal server error") != std::string::npos);
}
