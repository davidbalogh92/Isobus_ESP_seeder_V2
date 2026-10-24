// Az object pool ellenőrzése az AgIsoStack++ VT-szerver parserével (ugyanaz a kód, amit
// egy VT-szerver futtatna feltöltéskor). Hibás objektumnál a parser naplója megmutatja az ID-t.
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/isobus/isobus_virtual_terminal_server_managed_working_set.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

class StdoutLogger : public isobus::CANStackLogger {
public:
    void sink_CAN_stack_log(LoggingLevel level, const std::string &text) override {
        if (level >= LoggingLevel::Warning) std::cout << "[VT parser] " << text << "\n";
    }
};

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Használat: iop_check <object_pool.iop>\n";
        return 2;
    }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.empty()) {
        std::cerr << "Üres vagy hiányzó fájl: " << argv[1] << "\n";
        return 2;
    }
    static StdoutLogger logger;
    isobus::CANStackLogger::set_can_stack_logger_sink(&logger);
    isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Warning);
    isobus::VirtualTerminalServerManagedWorkingSet ws;
    const bool ok = ws.parse_iop_into_objects(data.data(), static_cast<std::uint32_t>(data.size()));
    std::cout << "Object pool (" << data.size() << " bájt): " << (ok ? "ÉRVÉNYES" : "HIBÁS") << "\n";
    if (ok) {
        // gyökér ellenőrzése: working set + aktív maszk
        const auto &tree = ws.get_object_tree();
        auto it = tree.find(0);
        if (it == tree.end()) {
            std::cout << "Hiányzik a working set objektum\n";
            return 1;
        }
        std::cout << "Objektumok száma: " << tree.size() << "\n";
    }
    return ok ? 0 : 1;
}
