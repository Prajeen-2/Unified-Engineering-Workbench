#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cmath>
#include "nlohmann/json.hpp"

namespace workbench::neural {

struct NetworkParameters {
    double w{0.5}; // Hidden layer weight
    double b{0.0}; // Hidden layer bias
    double v{1.0}; // Output layer weight
    double c{0.0}; // Output layer bias

    nlohmann::json toJson() const;
};

struct NetworkGradients {
    double dJ_dw{0.0};
    double dJ_db{0.0};
    double dJ_dv{0.0};
    double dJ_dc{0.0};

    nlohmann::json toJson() const;
};

struct GradientCheckResult {
    bool passed{true};
    double epsilon{1.0e-6};
    double maxTolerance{1.0e-5};
    double maxError{0.0};

    // Per-parameter details
    double analytical_dw{0.0};
    double numerical_dw{0.0};
    double error_dw{0.0};

    double analytical_db{0.0};
    double numerical_db{0.0};
    double error_db{0.0};

    double analytical_dv{0.0};
    double numerical_dv{0.0};
    double error_dv{0.0};

    double analytical_dc{0.0};
    double numerical_dc{0.0};
    double error_dc{0.0};

    nlohmann::json toJson() const;
};

struct TrainingStepRecord {
    uint32_t step{0};
    NetworkParameters params;
    double z{0.0};         // Linear pre-activation: w*x + b
    double h{0.0};         // Hidden activation: tanh(z)
    double yHat{0.0};      // Output prediction: v*h + c
    double error{0.0};     // e = yHat - y
    double loss{0.0};      // J = 0.5 * e^2
    NetworkGradients gradients;
    GradientCheckResult gradCheck;

    nlohmann::json toJson() const;
};

struct TrainingConfig {
    double x{1.0};
    double y{2.0};
    NetworkParameters initialParams{0.5, 0.0, 1.0, 0.0};
    double learningRate{0.1};
    uint32_t numSteps{100};
    double epsilon{1.0e-6};
    double maxTolerance{1.0e-5};

    bool validate(std::string& outError) const;
    nlohmann::json toJson() const;
    static TrainingConfig fromJson(const nlohmann::json& j);
};

struct TrainingResult {
    bool success{false};
    std::string errorMessage;
    TrainingConfig config;
    double initialLoss{0.0};
    double finalLoss{0.0};
    double initialPrediction{0.0};
    double finalPrediction{0.0};
    double maxGradientCheckError{0.0};
    bool allGradientChecksPassed{true};
    std::vector<TrainingStepRecord> stepHistory;

    nlohmann::json toJson() const;
    std::string toCsv() const;
};

class NeuralNetRegression {
public:
    // Forward pass: evaluates linear combinations and activations
    static void forward(double x, const NetworkParameters& params, double& outZ, double& outH, double& outYHat);

    // Half mean squared error loss: J = 0.5 * (yHat - y)^2
    static double computeLoss(double yHat, double y);

    // End-to-end forward evaluation to loss
    static double evaluateLoss(double x, double y, const NetworkParameters& params);

    // Analytical backward pass via exact chain-rule derivations
    static NetworkGradients computeGradients(double x, double y, const NetworkParameters& params);

    // Numerical gradient checking using two-sided central finite differences
    static GradientCheckResult checkGradients(
        double x, double y, const NetworkParameters& params,
        double epsilon = 1.0e-6, double maxTolerance = 1.0e-5);

    // Simultaneous gradient descent step: theta <- theta - eta * grad
    static NetworkParameters stepGradientDescent(
        const NetworkParameters& current, const NetworkGradients& grads, double learningRate);

    // Complete training orchestrator executing 100 steps
    static TrainingResult train(const TrainingConfig& config);
};

} // namespace workbench::neural

