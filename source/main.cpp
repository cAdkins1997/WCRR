#include "application.h"

int main() {
    vulkan::Device device("WCGL", 1920, 1080);
    Application app(device);
}