#include "application.h"

int main() {
    vulkan::Device device("rdr2", 1920, 1080);
    Application app(device);
}