#include <algorithm>
#include <complex>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <random>
#include <string>
#include <vector>

#include "timer.h"

const int MAX_ITERATIONS = 1000;

struct Point {
    double x;
    double y;
};

struct ThreadArgs {
    int rank;
    std::size_t first_point;
    std::size_t point_count;
    std::vector<Point>* points;
};

bool is_in_mandelbrot(double x, double y) {
    const std::complex<double> c(x, y);
    std::complex<double> z(0.0, 0.0);

    for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration) {
        z = z * z + c;
        if (std::norm(z) >= 4.0) {
            return false;
        }
    }

    return true;
}

void* find_points(void* raw_args) {
    ThreadArgs* args = static_cast<ThreadArgs*>(raw_args);

    std::mt19937 generator(1234567u + 1009u * args->rank);
    std::uniform_real_distribution<double> x_distribution(-2.0, 1.0);
    std::uniform_real_distribution<double> y_distribution(-1.5, 1.5);

    std::size_t found = 0;
    while (found < args->point_count) {
        const double x = x_distribution(generator);
        const double y = y_distribution(generator);

        if (is_in_mandelbrot(x, y)) {
            (*args->points)[args->first_point + found] = {x, y};
            ++found;
        }
    }

    return nullptr;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " nthreads npoints\n";
        return 1;
    }

    int thread_count;
    std::size_t point_count;

    try {
        thread_count = std::stoi(argv[1]);
        point_count = std::stoull(argv[2]);
    } catch (...) {
        std::cerr << "nthreads and npoints must be integers\n";
        return 1;
    }

    if (thread_count <= 0 || point_count == 0) {
        std::cerr << "nthreads and npoints must be positive\n";
        return 1;
    }

    std::vector<Point> points(point_count);
    std::vector<pthread_t> threads(thread_count);
    std::vector<ThreadArgs> args(thread_count);

    const std::size_t points_per_thread = point_count / thread_count;
    const std::size_t remainder = point_count % thread_count;

    double start;
    double finish;
    GET_TIME(start);

    int created_threads = 0;
    for (int rank = 0; rank < thread_count; ++rank) {
        const std::size_t extra_point = rank < static_cast<int>(remainder) ? 1 : 0;
        const std::size_t first_point =
            rank * points_per_thread + std::min<std::size_t>(rank, remainder);

        args[rank] = {rank, first_point, points_per_thread + extra_point, &points};

        const int error = pthread_create(
            &threads[rank], nullptr, find_points, &args[rank]);
        if (error != 0) {
            std::cerr << "Could not create thread " << rank << "\n";
            for (int i = 0; i < created_threads; ++i) {
                pthread_join(threads[i], nullptr);
            }
            return 1;
        }
        ++created_threads;
    }

    for (pthread_t thread : threads) {
        pthread_join(thread, nullptr);
    }

    GET_TIME(finish);

    std::ofstream output("mandelbrot.csv");
    if (!output) {
        std::cerr << "Could not create mandelbrot.csv\n";
        return 1;
    }

    output << std::setprecision(17);
    for (std::size_t i = 0; i < point_count; ++i) {
        output << points[i].x << ',' << points[i].y << '\n';
    }

    std::cout << "Saved " << point_count << " points to mandelbrot.csv\n";
    std::cout << "Calculation time: " << (finish - start) << " seconds\n";
    return 0;
}
