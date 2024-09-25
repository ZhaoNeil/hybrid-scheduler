#include <iostream>
#include <unistd.h>
#include <cstdlib>

unsigned long long fibonacci(int n) {
    if (n <= 1) {
        return 1;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main(int argc, char *argv[]) {

    // get the pid of the current process
    std::string pid = std::to_string(getpid());
    // Add the process to enclave_1
    std::string command = "echo " + pid + " > /sys/fs/ghost/enclave_1/tasks";

    int ret = std::system(command.c_str());
    if (ret == 0) {
        // if successful, print the command
        std::cout << command << std::endl;
    } else {
        // if failed, print the error message
        std::cout << "Failed to add to enclave_1" << std::endl;
    }

    int arg = atoi(argv[1]);
    unsigned long long n = fibonacci(arg);
    std::cout << n << std::endl;
    return 0;
}