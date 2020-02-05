#include <math.h>
#include <eosio/eosio.hpp>

std::tuple<double, double> find_quadratic_roots(double a, double b, double c) {
    double discriminant = b*b - 4*a*c;
    check(discriminant >= 0, "imaginary numbers are not supported");
    
    return std::tuple(
        (-b + sqrt(discriminant)) / (2*a),
        (-b - sqrt(discriminant)) / (2*a)
    );
}