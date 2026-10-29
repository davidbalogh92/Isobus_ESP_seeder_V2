#include "aog_net.hpp"

#include "board.hpp"
#include "web.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace aognet {
namespace {
app::Shared *g_shared = nullptr;
esp_netif_t *g_netif = nullptr;
std::atomic<bool> g_link_up{false};
std::atomic<bool> g_have_ip{false};
std::atomic<std::uint32_t> g_ip{0}, g_mask{0};   // hálózati bájtsorrend

std::mutex g_mx;   // az alábbiakat védi
seeder::aog::Link g_link;
seeder::aog::PaintState g_paint;
bool g_enabled = false;
int g_ip_mode = 0;
char g_ip_text[16] = "";
bool g_reconfigure = false;
int g_sock = -1;

std::uint32_t now_ms() { return static_cast<std::uint32_t>(esp_timer_get_time() / 1000); }

void apply_ip_config(int mode, const char *text) {
    if (!g_netif) return;
    if (mode == 1) {
        esp_netif_dhcpc_start(g_netif);
        web::log("INFO", "Ethernet: DHCP");
        return;
    }
    std::uint8_t ip[4];
    if (!seeder::parse_ipv4(text, ip)) return;
    esp_netif_dhcpc_stop(g_netif);
    esp_netif_ip_info_t info{};
    IP4_ADDR(&info.ip, ip[0], ip[1], ip[2], ip[3]);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    IP4_ADDR(&info.gw, 0, 0, 0, 0);   // az AgIO hálózatán nincs átjáró
    if (esp_netif_set_ip_info(g_netif, &info) == ESP_OK) {
        g_ip = info.ip.addr;
        g_mask = info.netmask.addr;
        g_have_ip = true;
        web::log("INFO", "Ethernet: fix IP %s", text);
    } else {
        web::log("ERROR", "Ethernet: az IP-cím beállítása sikertelen (%s)", text);
    }
}

void on_event(void *, esp_event_base_t base, std::int32_t id, void *data) {
    if (base == ETH_EVENT) {
        if (id == ETHERNET_EVENT_CONNECTED) {
            g_link_up = true;
            web::log("INFO", "Ethernet: kábel csatlakoztatva");
        } else if (id == ETHERNET_EVENT_DISCONNECTED) {
            g_link_up = false;
            web::log("WARN", "Ethernet: kábel lecsatlakozott");
        }
    } else if (base == IP_EVENT && id == IP_EVENT_ETH_GOT_IP) {
        const auto *e = static_cast<ip_event_got_ip_t *>(data);
        g_ip = e->ip_info.ip.addr;
        g_mask = e->ip_info.netmask.addr;
        g_have_ip = true;
        web::log("INFO", "Ethernet IP: " IPSTR, IP2STR(&e->ip_info.ip));
    }
}

bool eth_init() {
    if (!board::HAS_ETH) return false;
    spi_bus_config_t bus{};
    bus.mosi_io_num = board::ETH_MOSI;
    bus.miso_io_num = board::ETH_MISO;
    bus.sclk_io_num = board::ETH_SCLK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        web::log("ERROR", "Ethernet: SPI busz hiba");
        return false;
    }
    spi_device_interface_config_t dev{};
    dev.mode = 0;
    dev.clock_speed_hz = 20 * 1000 * 1000;
    dev.spics_io_num = board::ETH_CS;
    dev.queue_size = 20;
    eth_w5500_config_t w5500 = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &dev);
    w5500.int_gpio_num = board::ETH_INT;
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.reset_gpio_num = -1;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500, &mac_cfg);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_cfg);
    if (!mac || !phy) {
        web::log("ERROR", "Ethernet: W5500 nem található");
        return false;
    }
    esp_eth_config_t cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t h = nullptr;
    if (esp_eth_driver_install(&cfg, &h) != ESP_OK) {
        web::log("ERROR", "Ethernet: meghajtó telepítés hiba");
        return false;
    }
    std::uint8_t mac_addr[6];
    if (esp_read_mac(mac_addr, ESP_MAC_ETH) == ESP_OK) esp_eth_ioctl(h, ETH_CMD_S_MAC_ADDR, mac_addr);
    esp_netif_config_t ncfg = ESP_NETIF_DEFAULT_ETH();
    g_netif = esp_netif_new(&ncfg);
    if (!g_netif || esp_netif_attach(g_netif, esp_eth_new_netif_glue(h)) != ESP_OK) {
        web::log("ERROR", "Ethernet: netif hiba");
        return false;
    }
    esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &on_event, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &on_event, nullptr);
    apply_ip_config(g_ip_mode, g_ip_text);
    if (esp_eth_start(h) != ESP_OK) {
        web::log("ERROR", "Ethernet: indítás hiba");
        return false;
    }
    return true;
}

bool open_socket() {
    const int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return false;
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    timeval tv{};
    tv.tv_usec = 20000;   // 20 ms: a festés időzítéséhez
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(seeder::aog::PORT_FROM_AGIO);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, reinterpret_cast<sockaddr *>(&a), sizeof(a)) != 0) {
        close(s);
        return false;
    }
    g_sock = s;
    return true;
}

bool send_broadcast(const std::uint8_t *d, std::size_t n) {
    if (g_sock < 0 || !g_have_ip) return false;
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(seeder::aog::PORT_TO_AGIO);
    to.sin_addr.s_addr = (g_ip.load() & g_mask.load()) | ~g_mask.load();
    return sendto(g_sock, d, n, 0, reinterpret_cast<sockaddr *>(&to), sizeof(to)) == static_cast<int>(n);
}

void own_ip(std::uint8_t out[4]) {
    const std::uint32_t ip = g_ip.load();   // hálózati sorrend: az alsó bájt az első oktett
    for (int i = 0; i < 4; ++i) out[i] = static_cast<std::uint8_t>(ip >> (8 * i));
}

void task(void *) {
    if (!open_socket()) {
        web::log("ERROR", "AgOpenGPS: az UDP %u port nem nyitható", seeder::aog::PORT_FROM_AGIO);
        vTaskDelete(nullptr);
        return;
    }
    web::log("INFO", "AgOpenGPS: UDP %u figyelés, küldés a %u portra", seeder::aog::PORT_FROM_AGIO,
             seeder::aog::PORT_TO_AGIO);
    const seeder::aog::Link::Sender sender = &send_broadcast;
    std::uint8_t buf[600];
    for (;;) {
        sockaddr_in from{};
        socklen_t fl = sizeof(from);
        const int n = recvfrom(g_sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr *>(&from), &fl);
        bool reconf = false;
        int mode = 0;
        char ip_text[16];
        std::uint8_t sn[3];
        bool subnet = false;
        {
            std::lock_guard<std::mutex> g(g_mx);
            std::uint8_t ip[4];
            own_ip(ip);
            g_link.set_own_ip(ip);
            if (n > 0 && g_enabled) g_link.on_packet(buf, static_cast<std::size_t>(n), now_ms(), sender);
            seeder::aog::PaintState p = g_paint;
            p.enabled = p.enabled && g_enabled;
            g_link.update(now_ms(), p, sender);
            subnet = g_enabled && g_link.take_subnet_request(sn);
            reconf = g_reconfigure;
            g_reconfigure = false;
            mode = g_ip_mode;
            std::memcpy(ip_text, g_ip_text, sizeof(ip_text));
        }
        if (subnet && mode == 0) {
            // AgIO "alhálózat beállítása": az első három bájt cseréje, a saját (4.) bájt marad
            std::uint8_t cur[4];
            if (seeder::parse_ipv4(ip_text, cur)) {
                std::snprintf(ip_text, sizeof(ip_text), "%u.%u.%u.%u", sn[0], sn[1], sn[2], cur[3]);
                if (g_shared) {
                    std::lock_guard<std::mutex> g(g_shared->lock);
                    g_shared->core.set_param_str(seeder::P_aog_ip, ip_text);
                }
                web::log("INFO", "AgOpenGPS: új alhálózat az AgIO-tól → %s", ip_text);
                reconf = true;
            }
        }
        if (reconf) apply_ip_config(mode, ip_text);
    }
}
} // namespace

bool start(app::Shared *shared, const seeder::Settings &s) {
    g_shared = shared;
    {
        std::lock_guard<std::mutex> g(g_mx);
        g_enabled = s.aog_enable != 0;
        g_ip_mode = s.aog_ip_mode;
        std::snprintf(g_ip_text, sizeof(g_ip_text), "%s", s.aog_ip);
    }
    if (!eth_init()) return false;
    xTaskCreatePinnedToCore(task, "aog_udp", 4096, nullptr, 4, nullptr, 0);
    return true;
}

void exchange(const seeder::Settings &s, const seeder::aog::PaintState &paint, app::HwInputs &hw) {
    std::lock_guard<std::mutex> g(g_mx);
    g_enabled = s.aog_enable != 0;
    g_paint = paint;
    if (s.aog_ip_mode != g_ip_mode || std::strncmp(s.aog_ip, g_ip_text, sizeof(g_ip_text)) != 0) {
        g_ip_mode = s.aog_ip_mode;
        std::snprintf(g_ip_text, sizeof(g_ip_text), "%s", s.aog_ip);
        g_reconfigure = true;
    }
    if (!g_enabled) return;
    const std::uint32_t now = now_ms();
    hw.aog.connected = g_link.connected(now);
    hw.aog.sections_valid = g_link.sections_valid(now);
    hw.aog.sections = g_link.sections();
    if (g_link.speed_valid(now)) hw.aog_speed = {true, g_link.speed_kmh(), g_link.speed_age_ms(now)};
}

std::string diag_json() {
    std::uint8_t ip[4];
    own_ip(ip);
    std::lock_guard<std::mutex> g(g_mx);
    const auto &st = g_link.stats();
    const std::uint32_t now = now_ms();
    char b[640];
    std::snprintf(b, sizeof(b),
                  "{\"eth\":%s,\"link\":%s,\"ip\":\"%u.%u.%u.%u\",\"enabled\":%s,\"connected\":%s,\"speed_valid\":%s,"
                  "\"speed\":%.1f,\"sections\":%u,\"sections_valid\":%s,\"rx\":%lu,\"rejected\":%lu,\"rx254\":%lu,"
                  "\"rx239\":%lu,\"hello\":%lu,\"scan\":%lu,\"tx_paint\":%lu,\"tx_failed\":%lu,\"last_pgn\":%u,"
                  "\"main\":%u,\"max_gap_ms\":%lu,\"outages\":%lu}",
                  g_netif ? "true" : "false", g_link_up.load() ? "true" : "false", ip[0], ip[1], ip[2], ip[3],
                  g_enabled ? "true" : "false", g_link.connected(now) ? "true" : "false",
                  g_link.speed_valid(now) ? "true" : "false", static_cast<double>(g_link.speed_kmh()),
                  static_cast<unsigned>(g_link.sections()), g_link.sections_valid(now) ? "true" : "false",
                  static_cast<unsigned long>(st.rx_packets), static_cast<unsigned long>(st.rx_rejected),
                  static_cast<unsigned long>(st.rx_254), static_cast<unsigned long>(st.rx_239),
                  static_cast<unsigned long>(st.rx_hello), static_cast<unsigned long>(st.rx_scan),
                  static_cast<unsigned long>(st.tx_paint), static_cast<unsigned long>(st.tx_failed),
                  static_cast<unsigned>(st.last_pgn), static_cast<unsigned>(g_link.last_main()),
                  static_cast<unsigned long>(st.max_gap_ms), static_cast<unsigned long>(st.outages));
    return b;
}

} // namespace aognet
