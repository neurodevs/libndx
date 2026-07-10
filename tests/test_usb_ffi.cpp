#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include "ndx/acquisition_backend.hpp"
#include "ndx/ndx_ffi.hpp"
#include "ndx/usb_provider.hpp"

namespace {

struct FakeUsbProvider : ndx::UsbProvider {
  ndx::OnDataCallback captured_on_data;

  void connect(const std::string&, ndx::OnDataCallback on_data, ndx::OnConnectedCallback, int wait_after_connect_ms = 0) override {
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

FakeUsbProvider* g_fake_usb_provider = nullptr;

std::vector<uint8_t> g_received_data;
double g_received_timestamp = -1;

void capture_on_data(const uint8_t* data, size_t len, double timestamp_sec) {
  g_received_data.assign(data, data + len);
  g_received_timestamp = timestamp_sec;
}

}

struct UsbFfiFixture {
  std::string valid_serial;

  UsbFfiFixture() {
    reset_usb_backends();
    set_usb_factory([](const std::string& serial_number) {
      auto provider = std::make_unique<FakeUsbProvider>();
      g_fake_usb_provider = provider.get();
      return std::make_shared<ndx::UsbBackend>(serial_number, std::move(provider));
    });
    valid_serial = "ABCD1234";
  }

  nlohmann::json create_and_parse(const char* config_json) {
    const char* result = create_usb_backend(config_json);
    return nlohmann::json::parse(result);
  }

  nlohmann::json write_v() {
    return nlohmann::json::parse(write_usb_backend(valid_serial.c_str(), "v"));
  }

  nlohmann::json start() {
    const char* result = start_usb_backend(valid_serial.c_str(), [](const uint8_t*, size_t, double) {});
    return nlohmann::json::parse(result);
  }

  nlohmann::json stop() {
    const char* result = stop_usb_backend(valid_serial.c_str());
    return nlohmann::json::parse(result);
  }

  void set_throwing_factory() {
    set_usb_factory([](const std::string& uuid) -> std::shared_ptr<ndx::UsbBackend> {
      struct ThrowingUsbBackend : ndx::UsbBackend {
        using ndx::UsbBackend::UsbBackend;
        void start(ndx::OnDataCallback, ndx::OnConnectedCallback, int) override { throw std::runtime_error("hardware fault"); }
        void stop() override { throw std::runtime_error("hardware fault"); }
      };
      return std::make_shared<ThrowingUsbBackend>(uuid, ndx::create_usb_provider());
    });
  }
};

struct ValidUsbFixture : UsbFfiFixture {
  nlohmann::json json = create_and_parse("{\"serial_number\":\"ABCD1234\"}");
};

TEST_CASE_METHOD(ValidUsbFixture, "create_usb_backend returns ok") {
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidUsbFixture, "create_usb_backend constructs UsbBackend instance") {
    auto backend = get_usb_backend(valid_serial);
    REQUIRE(backend != nullptr);
}

TEST_CASE_METHOD(ValidUsbFixture, "create_usb_backend sets proper serial_number") {
    auto backend = get_usb_backend(valid_serial);
    REQUIRE(backend->device_id() == "ABCD1234");
}

TEST_CASE_METHOD(ValidUsbFixture, "create_usb_backend returns 400 if serial number is already registered") {
    auto json = create_and_parse("{\"serial_number\":\"ABCD1234\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "serial number already registered");
}

TEST_CASE_METHOD(UsbFfiFixture, "create_usb_backend returns 400 if serial number is not size 8") {
    auto json = create_and_parse("{\"serial_number\":\"not-size-eight\"}");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid serial number");
}

TEST_CASE_METHOD(UsbFfiFixture, "create_usb_backend returns 400 if JSON is malformed") {
    auto json = create_and_parse("{");
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "malformed JSON");
}

TEST_CASE_METHOD(ValidUsbFixture, "start_usb_backend returns ok") {
    auto json = UsbFfiFixture::start();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidUsbFixture, "start_usb_backend calls start on backend") {
    UsbFfiFixture::start();
    auto backend = get_usb_backend(valid_serial);
    REQUIRE(backend->is_running());
}

TEST_CASE_METHOD(ValidUsbFixture, "start_usb_backend forwards received packets to the on_data callback") {
  g_received_data.clear();
  g_received_timestamp = -1;

  start_usb_backend(valid_serial.c_str(), capture_on_data);

  ndx::Packet p;
  p.data = {1, 2, 3};
  p.timestamp_sec = 42.0;
  g_fake_usb_provider->simulate_packet(p);

  REQUIRE(g_received_data == std::vector<uint8_t>{1, 2, 3});
  REQUIRE(g_received_timestamp == 42.0);
}

TEST_CASE_METHOD(ValidUsbFixture, "write_usb_backend returns ok") {
  auto json = write_v();
  REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidUsbFixture, "write_usb_backend forwards bytes to the provider") {
  auto json = write_v();
  REQUIRE(g_fake_usb_provider->written_data == std::vector<uint8_t>{'v'});
}

TEST_CASE_METHOD(ValidUsbFixture, "stop_usb_backend returns ok") {
    UsbFfiFixture::start();
    auto json = UsbFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidUsbFixture, "stop_usb_backend calls stop on backend") {
    UsbFfiFixture::start();
    UsbFfiFixture::stop();
    auto backend = get_usb_backend(valid_serial);
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(UsbFfiFixture, "create_usb_backend returns 500 on unexpected throw") {
  set_usb_factory([](const std::string&) -> std::shared_ptr<ndx::UsbBackend> {
    throw std::runtime_error("hardware fault");
  });
  auto json = create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(UsbFfiFixture, "start_usb_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  auto json = start();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(UsbFfiFixture, "stop_usb_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  start();
  auto json = stop();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}
