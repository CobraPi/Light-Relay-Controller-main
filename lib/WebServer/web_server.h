#include <Arduino.h>
#include <Wifi.h>


class WebServer:

    public:
        WebServer(uint16_t port);
        void init();
        void run_loop();

    private:
        static void _connect();
        static void _listen();
        static void _display_webpage();
        static void _handle_webpage();
        WiFiServer _server; // Wifi Server object
        const char* _ssid = "LUCINI";
        const char* _password = "jackie1229";
        uint16_t _port;
        String _header;