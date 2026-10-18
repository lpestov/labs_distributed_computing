#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <pthread.h>
#include <string>
#include <vector>

#include "timer.h"

const double GRAVITATIONAL_CONSTANT = 6.67430e-11;
const double TIME_STEP = 0.01;

struct Vector3 {
    double x;
    double y;
    double z;
};

struct Body {
    double mass;
    Vector3 position;
    Vector3 velocity;
};

struct ForceArgs {
    int rank;
    int thread_count;
    const std::vector<Body>* bodies;
    std::vector<Vector3>* forces;
};

void* calculate_force_part(void* raw_args) {
    ForceArgs* args = static_cast<ForceArgs*>(raw_args);
    const std::vector<Body>& bodies = *args->bodies;
    std::vector<Vector3>& forces = *args->forces;

    for (std::size_t i = args->rank; i < bodies.size(); i += args->thread_count) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const double dx = bodies[j].position.x - bodies[i].position.x;
            const double dy = bodies[j].position.y - bodies[i].position.y;
            const double dz = bodies[j].position.z - bodies[i].position.z;
            const double distance_squared = dx * dx + dy * dy + dz * dz;

            if (distance_squared == 0.0) {
                continue;
            }

            const double distance = std::sqrt(distance_squared);
            const double multiplier = GRAVITATIONAL_CONSTANT
                * bodies[i].mass * bodies[j].mass
                / (distance_squared * distance);

            const Vector3 force = {
                multiplier * dx,
                multiplier * dy,
                multiplier * dz
            };

            forces[i].x += force.x;
            forces[i].y += force.y;
            forces[i].z += force.z;
            forces[j].x -= force.x;
            forces[j].y -= force.y;
            forces[j].z -= force.z;
        }
    }

    return nullptr;
}

bool calculate_forces(
    const std::vector<Body>& bodies,
    int thread_count,
    std::vector<std::vector<Vector3>>& thread_forces) {

    std::vector<pthread_t> threads(thread_count);
    std::vector<ForceArgs> args(thread_count);

    int created_threads = 0;
    for (int rank = 0; rank < thread_count; ++rank) {
        std::fill(
            thread_forces[rank].begin(),
            thread_forces[rank].end(),
            Vector3{0.0, 0.0, 0.0});

        args[rank] = {rank, thread_count, &bodies, &thread_forces[rank]};
        const int error = pthread_create(
            &threads[rank], nullptr, calculate_force_part, &args[rank]);
        if (error != 0) {
            std::cerr << "Could not create thread " << rank << "\n";
            for (int i = 0; i < created_threads; ++i) {
                pthread_join(threads[i], nullptr);
            }
            return false;
        }
        ++created_threads;
    }

    for (pthread_t thread : threads) {
        pthread_join(thread, nullptr);
    }

    return true;
}

void write_state(std::ofstream& output, double time, const std::vector<Body>& bodies) {
    output << time;
    for (const Body& body : bodies) {
        output << ',' << body.position.x << ',' << body.position.y;
    }
    output << '\n';
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " nthreads tend input_file\n";
        return 1;
    }

    int thread_count;
    double end_time;

    try {
        thread_count = std::stoi(argv[1]);
        end_time = std::stod(argv[2]);
    } catch (...) {
        std::cerr << "nthreads must be an integer and tend must be a number\n";
        return 1;
    }

    if (thread_count <= 0 || end_time < 0.0) {
        std::cerr << "nthreads must be positive and tend must be non-negative\n";
        return 1;
    }

    std::ifstream input(argv[3]);
    if (!input) {
        std::cerr << "Could not open input file: " << argv[3] << '\n';
        return 1;
    }

    std::size_t body_count;
    if (!(input >> body_count) || body_count == 0) {
        std::cerr << "The first line must contain a positive number of bodies\n";
        return 1;
    }

    std::vector<Body> bodies(body_count);
    for (std::size_t i = 0; i < body_count; ++i) {
        Body& body = bodies[i];
        if (!(input >> body.mass
                    >> body.position.x >> body.position.y >> body.position.z
                    >> body.velocity.x >> body.velocity.y >> body.velocity.z)) {
            std::cerr << "Invalid data for body " << (i + 1) << '\n';
            return 1;
        }
        if (body.mass <= 0.0) {
            std::cerr << "Body mass must be positive\n";
            return 1;
        }
    }

    std::ofstream output("nbody.csv");
    if (!output) {
        std::cerr << "Could not create nbody.csv\n";
        return 1;
    }
    output << std::setprecision(17);

    std::vector<std::vector<Vector3>> thread_forces(
        thread_count,
        std::vector<Vector3>(body_count, {0.0, 0.0, 0.0}));
    std::vector<Vector3> total_forces(body_count, {0.0, 0.0, 0.0});

    double time = 0.0;
    double calculation_time = 0.0;
    write_state(output, time, bodies);

    while (time < end_time) {
        const double step = std::min(TIME_STEP, end_time - time);
        double start;
        double finish;
        GET_TIME(start);

        if (!calculate_forces(bodies, thread_count, thread_forces)) {
            return 1;
        }

        std::fill(
            total_forces.begin(),
            total_forces.end(),
            Vector3{0.0, 0.0, 0.0});

        for (int rank = 0; rank < thread_count; ++rank) {
            for (std::size_t i = 0; i < body_count; ++i) {
                total_forces[i].x += thread_forces[rank][i].x;
                total_forces[i].y += thread_forces[rank][i].y;
                total_forces[i].z += thread_forces[rank][i].z;
            }
        }

        for (std::size_t i = 0; i < body_count; ++i) {
            Body& body = bodies[i];

            body.position.x += body.velocity.x * step;
            body.position.y += body.velocity.y * step;
            body.position.z += body.velocity.z * step;

            body.velocity.x += total_forces[i].x / body.mass * step;
            body.velocity.y += total_forces[i].y / body.mass * step;
            body.velocity.z += total_forces[i].z / body.mass * step;
        }

        GET_TIME(finish);
        calculation_time += finish - start;

        time += step;
        if (std::abs(end_time - time) < 1e-12) {
            time = end_time;
        }
        write_state(output, time, bodies);
    }

    std::cout << "Saved trajectories to nbody.csv\n";
    std::cout << "Calculation time: " << calculation_time << " seconds\n";
    return 0;
}
