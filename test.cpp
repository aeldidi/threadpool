#include <iostream>
#define THREADPOOL_IMPLEMENTATION
#include "threadpool.h"

int main() {
    std::cout << "making a threadpool with 8 threads" << std::endl;
    didi::threadpool pool(8);
    std::cout << "adding 10 000 tasks to threadpool" << std::endl;
    for (int i = 0; i < 5000; i++) {
        pool.add_job([](){
            std::cout << "a task was completed\n";
        });
        pool.add_job([](){
            std::cout << "a task was completed\n";
        });
    }

    std::cout << "waiting for threadpool" << std::endl;
    pool.wait();
    std::cout << "threadpool is done now" << std::endl;
    return 0;
}