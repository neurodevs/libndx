#include <catch2/catch_all.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include "ndx/acquisition_backend.hpp"
#include "ndx/ndx_ffi.hpp"
#include "ndx/usb_provider.hpp"

namespace {

struct FakeUsbProvider : ndx::UsbProvider {
  ndx::OnDataCallback captured_on_data;
  bool should_fail_to_connect = false;

  void connect(const std::string&, ndx::OnDataCallback on_data, ndx::OnConnectedCallback, int wait_after_connect_ms = 0) override {
    if (should_fail_to_connect) throw std::runtime_error("could not open port");
    captured_on_data = std::move(on_data);
  }
  void disconnect() override {}

  speed_t configured_baud_rate = 0;
  bool configured_rts_cts = false;
  void configure_port(speed_t baud_rate, bool uses_rts_cts) override {
    configured_baud_rate = baud_rate;
    configured_rts_cts = uses_rts_cts;
  }

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

  void set_factory_throwing_on_stop() {
    set_usb_factory([](const std::string& uuid) -> std::shared_ptr<ndx::UsbBackend> {
      struct StopThrowingUsbBackend : ndx::UsbBackend {
        using ndx::UsbBackend::UsbBackend;
        void stop() override { throw std::runtime_error("hardware fault"); }
      };
      return std::make_shared<StopThrowingUsbBackend>(uuid, std::make_unique<FakeUsbProvider>());
    });
  }

  void set_throwing_factory() {
    set_usb_factory([](const std::string& uuid) -> std::shared_ptr<ndx::UsbBackend> {
      struct ThrowingUsbBackend : ndx::UsbBackend {
        using ndx::UsbBackend::UsbBackend;
        void start(ndx::OnDataCallback, ndx::OnConnectedCallback, int) override { throw std::runtime_error("hardware fault"); }
        void stop() override { throw std::runtime_error("hardware fault"); }
        bool write(const uint8_t*, size_t) override { throw std::runtime_error("hardware fault"); }
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

TEST_CASE_METHOD(ValidUsbFixture, "create_usb_backend opens the port at 115200 baud without flow control by default") {
    REQUIRE(g_fake_usb_provider->configured_baud_rate == 115200);
    REQUIRE(!g_fake_usb_provider->configured_rts_cts);
}

TEST_CASE_METHOD(UsbFfiFixture, "create_usb_backend passes baud rate and rts/cts flow control to the port") {
    auto json = create_and_parse("{\"serial_number\":\"ABCD1234\",\"baud_rate\":1000000,\"flow_control\":\"rts_cts\"}");
    REQUIRE(json["status"] == 200);
    REQUIRE(g_fake_usb_provider->configured_baud_rate == 1000000);
    REQUIRE(g_fake_usb_provider->configured_rts_cts);
}

TEST_CASE_METHOD(UsbFfiFixture, "create_usb_backend returns 400 if baud rate is not a positive integer") {
    auto bad_baud_rate = GENERATE("0", "-9600", "\"fast\"", "9600.5");
    auto config = std::string("{\"serial_number\":\"ABCD1234\",\"baud_rate\":") + bad_baud_rate + "}";
    auto json = create_and_parse(config.c_str());
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid baud rate");
    REQUIRE(get_usb_backend(valid_serial) == nullptr);
}

TEST_CASE_METHOD(UsbFfiFixture, "create_usb_backend returns 400 if flow control is not none or rts_cts") {
    auto bad_flow_control = GENERATE("\"xon_xoff\"", "true", "1");
    auto config = std::string("{\"serial_number\":\"ABCD1234\",\"flow_control\":") + bad_flow_control + "}";
    auto json = create_and_parse(config.c_str());
    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "invalid flow control");
    REQUIRE(get_usb_backend(valid_serial) == nullptr);
}

TEST_CASE_METHOD(UsbFfiFixture, "start_usb_backend returns 400 if backend not found") {
    auto json = start();
    REQUIRE(json["status"] == 400);
}

TEST_CASE_METHOD(UsbFfiFixture, "discover_usb_serial_numbers returns the serial number of each usb serial port") {
    auto list_dev_names = ndx::UsbProviderSyscalls::list_dev_names;
    ndx::UsbProviderSyscalls::list_dev_names = []() {
        return std::vector<std::string>{"cu.usbserial-ZZZZ9999", "tty.usbserial-ZZZZ9999", "cu.Bluetooth-Incoming-Port", "cu.usbserial-AAAA1111"};
    };

    auto json = nlohmann::json::parse(discover_usb_serial_numbers());
    ndx::UsbProviderSyscalls::list_dev_names = list_dev_names;

    REQUIRE(json["status"] == 200);
    REQUIRE(json["serial_numbers"] == std::vector<std::string>{"AAAA1111", "ZZZZ9999"});
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

TEST_CASE_METHOD(UsbFfiFixture, "write_usb_backend returns 400 if backend not found") {
  auto json = nlohmann::json::parse(write_usb_backend("unknown1", "v"));
  REQUIRE(json["status"] == 400);
  REQUIRE(json["error"] == "backend not found");
}

TEST_CASE_METHOD(UsbFfiFixture, "write_usb_backend returns 500 on unexpected throw") {
  set_throwing_factory();
  create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  auto json = write_v();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}

TEST_CASE_METHOD(ValidUsbFixture, "stop_usb_backend returns ok") {
    UsbFfiFixture::start();
    auto json = UsbFfiFixture::stop();
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidUsbFixture, "stop_usb_backend calls stop on backend") {
    UsbFfiFixture::start();
    auto backend = get_usb_backend(valid_serial);
    UsbFfiFixture::stop();
    REQUIRE(!backend->is_running());
}

TEST_CASE_METHOD(ValidUsbFixture, "stop_usb_backend unregisters the backend") {
    UsbFfiFixture::start();
    UsbFfiFixture::stop();
    REQUIRE(get_usb_backend(valid_serial) == nullptr);
}

TEST_CASE_METHOD(ValidUsbFixture, "create_usb_backend returns ok again after stop_usb_backend") {
    UsbFfiFixture::start();
    UsbFfiFixture::stop();
    auto json = create_and_parse("{\"serial_number\":\"ABCD1234\"}");
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidUsbFixture, "start_usb_backend returns 500 when the port cannot be opened") {
    g_fake_usb_provider->should_fail_to_connect = true;
    auto json = UsbFfiFixture::start();
    REQUIRE(json["status"] == 500);
    REQUIRE(json["error"].get<std::string>().find("could not open port") != std::string::npos);
}

TEST_CASE_METHOD(ValidUsbFixture, "start_usb_backend unregisters the backend when the port cannot be opened") {
    g_fake_usb_provider->should_fail_to_connect = true;
    UsbFfiFixture::start();
    REQUIRE(get_usb_backend(valid_serial) == nullptr);
}

TEST_CASE_METHOD(ValidUsbFixture, "create_usb_backend returns ok again after the port could not be opened") {
    g_fake_usb_provider->should_fail_to_connect = true;
    UsbFfiFixture::start();
    auto json = create_and_parse("{\"serial_number\":\"ABCD1234\"}");
    REQUIRE(json["status"] == 200);
}

TEST_CASE_METHOD(ValidUsbFixture, "start_usb_backend returns 400 if backend is already running") {
    UsbFfiFixture::start();
    auto json = UsbFfiFixture::start();

    REQUIRE(json["status"] == 400);
    REQUIRE(json["error"] == "usb backend already running");
}

TEST_CASE_METHOD(ValidUsbFixture, "start_usb_backend keeps a running backend registered when started twice") {
    UsbFfiFixture::start();
    UsbFfiFixture::start();
    auto backend = get_usb_backend(valid_serial);

    REQUIRE(backend != nullptr);
    REQUIRE(backend->is_running());
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
  set_factory_throwing_on_stop();
  create_and_parse("{\"serial_number\":\"ABCD1234\"}");
  start();
  auto json = stop();
  REQUIRE(json["status"] == 500);
  REQUIRE(json["error"].get<std::string>().find("hardware fault") != std::string::npos);
}
