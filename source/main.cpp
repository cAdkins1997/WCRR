#include "SCGL.h"
#include "pipelines/descriptors.h"
#include "application.h"

int main() {
    vulkan::Device device("Test Application", 1920, 1080);
    Application app(device);
}