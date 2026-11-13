#include "neural/NeuralNetRegression.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace workbench::neural {

nlohmann::json NetworkParameters::toJson() const {
    return nlohmann::json{
        {"w", w},
        {"b", b},
        {"v", v},
        {"c", c}
    };
}

nlohmann::json NetworkGradients::toJson() const {
    return nlohmann::json{
        {"dJ_dw", dJ_dw},
        {"dJ_db", dJ_db},
        {"dJ_dv", dJ_dv},
        {"dJ_dc", dJ_dc}
    };
}

nlohmann::json GradientCheckResult::toJson() const {
    return nlohmann::json{
        {"passed", passed},
        {"epsilon", epsilon},
        {"max_tolerance", maxTolerance},
        {"max_error", maxError},
        {"w", {{"analytical", analytical_dw}, {"numerical", numerical_dw}, {"error", error_dw}}},
        {"b", {{"analytical", analytical_db}, {"numerical", numerical_db}, {"error", error_db}}},
        {"v", {{"analytical", analytical_dv}, {"numerical", numerical_dv}, {"error", error_dv}}},
        {"c", {{"analytical", analytical_dc}, {"numerical", numerical_dc}, {"error", error_dc}}}
    };
}

nlohmann::json TrainingStepRecord::toJson() const {
    return nlohmann::json{
        {"step", step},
        {"parameters", params.toJson()},
        {"z", z},
        {"h", h},
        {"y_hat", yHat},
        {"error", error},
        {"loss", loss},
        {"gradients", gradients.toJson()},
        {"gradient_check", gradCheck.toJson()}
    };
}

bool TrainingConfig::validate(std::string& outError) const {
    if (numSteps == 0 || numSteps > 10000) {
        outError = "Number of training steps must be between 1 and 10,000.";
        return false;
    }
    if (learningRate <= 0.0 || learningRate > 10.0) {
        outError = "Learning rate must be positive and <= 10.0.";
        return false;
    }
    if (epsilon <= 0.0 || epsilon >= 1.0) {
        outError = "Finite-difference epsilon must be between 0 and 1.";
        return false;
    }
    if (maxTolerance <= 0.0) {
        outError = "Gradient checking tolerance must be positive.";
        return false;
    }
    return true;
}

nlohmann::json TrainingConfig::toJson() const {
    return nlohmann::json{
        {"x", x},
        {"y", y},
        {"initial_parameters", initialParams.toJson()},
        {"learning_rate", learningRate},
        {"num_steps", numSteps},
        {"epsilon", epsilon},
        {"max_tolerance", maxTolerance}
    };
}

TrainingConfig TrainingConfig::fromJson(const nlohmann::json& j) {
    TrainingConfig cfg;
    if (j.contains("x")) cfg.x = j["x"].get<double>();
    if (j.contains("y")) cfg.y = j["y"].get<double>();
    if (j.contains("learning_rate")) cfg.learningRate = j["learning_rate"].get<double>();
    if (j.contains("num_steps")) cfg.numSteps = j["num_steps"].get<uint32_t>();
    if (j.contains("epsilon")) cfg.epsilon = j["epsilon"].get<double>();
    if (j.contains("max_tolerance")) cfg.maxTolerance = j["max_tolerance"].get<double>();
    if (j.contains("initial_parameters")) {
        const auto& p = j["initial_parameters"];
        if (p.contains("w")) cfg.initialParams.w = p["w"].get<double>();
        if (p.contains("b")) cfg.initialParams.b = p["b"].get<double>();
        if (p.contains("v")) cfg.initialParams.v = p["v"].get<double>();
        if (p.contains("c")) cfg.initialParams.c = p["c"].get<double>();
    }
    return cfg;
}

nlohmann::json TrainingResult::toJson() const {
    nlohmann::json jSteps = nlohmann::json::array();
    for (const auto& s : stepHistory) {
        jSteps.push_back(s.toJson());
    }

    return nlohmann::json{
        {"success", success},
        {"error_message", errorMessage},
        {"config", config.toJson()},
        {"initial_loss", initialLoss},
        {"final_loss", finalLoss},
        {"initial_prediction", initialPrediction},
        {"final_prediction", finalPrediction},
        {"max_gradient_check_error", maxGradientCheckError},
        {"all_gradient_checks_passed", allGradientChecksPassed},
        {"steps_recorded", stepHistory.size()},
        {"step_history", jSteps}
    };
}

std::string TrainingResult::toCsv() const {
    std::ostringstream ss;
    ss << "# Unified Engineering Workbench - Neural Network Manual Backpropagation Training Log\n";
    ss << "# Target Input: x=" << config.x << ", Ground Truth: y=" << config.y << "\n";
    ss << "# Learning Rate: " << config.learningRate << ", Steps: " << config.numSteps << "\n";
    ss << "# Central Difference Epsilon: " << config.epsilon << ", Gradient Tolerance: " << config.maxTolerance << "\n";
    ss << "# Max Gradient Check Error: " << std::scientific << std::setprecision(6) << maxGradientCheckError << "\n";
    ss << "# Verification: " << (allGradientChecksPassed ? "PASS" : "FAIL") << "\n";
    ss << "step,w,b,v,c,z,h,y_hat,error,loss,dJ_dw,dJ_db,dJ_dv,dJ_dc,grad_error_w,grad_error_b,grad_error_v,grad_error_c,grad_check_pass\n";

    for (const auto& s : stepHistory) {
        ss << s.step << ","
           << std::fixed << std::setprecision(8) << s.params.w << ","
           << std::fixed << std::setprecision(8) << s.params.b << ","
           << std::fixed << std::setprecision(8) << s.params.v << ","
           << std::fixed << std::setprecision(8) << s.params.c << ","
           << std::fixed << std::setprecision(8) << s.z << ","
           << std::fixed << std::setprecision(8) << s.h << ","
           << std::fixed << std::setprecision(8) << s.yHat << ","
           << std::fixed << std::setprecision(8) << s.error << ","
           << std::fixed << std::setprecision(8) << s.loss << ","
           << std::fixed << std::setprecision(8) << s.gradients.dJ_dw << ","
           << std::fixed << std::setprecision(8) << s.gradients.dJ_db << ","
           << std::fixed << std::setprecision(8) << s.gradients.dJ_dv << ","
           << std::fixed << std::setprecision(8) << s.gradients.dJ_dc << ","
           << std::scientific << std::setprecision(4) << s.gradCheck.error_dw << ","
           << std::scientific << std::setprecision(4) << s.gradCheck.error_db << ","
           << std::scientific << std::setprecision(4) << s.gradCheck.error_dv << ","
           << std::scientific << std::setprecision(4) << s.gradCheck.error_dc << ","
           << (s.gradCheck.passed ? "PASS" : "FAIL") << "\n";
    }
    return ss.str();
}

void NeuralNetRegression::forward(
    double x, const NetworkParameters& params, double& outZ, double& outH, double& outYHat) {
    outZ = params.w * x + params.b;
    outH = std::tanh(outZ);
    outYHat = params.v * outH + params.c;
}

double NeuralNetRegression::computeLoss(double yHat, double y) {
    double err = yHat - y;
    return 0.5 * err * err;
}

double NeuralNetRegression::evaluateLoss(double x, double y, const NetworkParameters& params) {
    double z = 0.0, h = 0.0, yHat = 0.0;
    forward(x, params, z, h, yHat);
    return computeLoss(yHat, y);
}

NetworkGradients NeuralNetRegression::computeGradients(
    double x, double y, const NetworkParameters& params) {
    
    double z = 0.0, h = 0.0, yHat = 0.0;
    forward(x, params, z, h, yHat);

    double e = yHat - y; // dJ / dyHat

    NetworkGradients grads;
    // Output layer gradients
    grads.dJ_dv = e * h;
    grads.dJ_dc = e;

    // Hidden layer backpropagation
    // dh / dz = 1 - tanh^2(z) = 1 - h^2
    double delta = e * params.v * (1.0 - h * h);
    grads.dJ_dw = delta * x;
    grads.dJ_db = delta;

    return grads;
}

GradientCheckResult NeuralNetRegression::checkGradients(
    double x, double y, const NetworkParameters& params,
    double epsilon, double maxTolerance) {
    
    GradientCheckResult res;
    res.epsilon = epsilon;
    res.maxTolerance = maxTolerance;

    // 1. Analytical gradients
    NetworkGradients analytical = computeGradients(x, y, params);
    res.analytical_dw = analytical.dJ_dw;
    res.analytical_db = analytical.dJ_db;
    res.analytical_dv = analytical.dJ_dv;
    res.analytical_dc = analytical.dJ_dc;

    // 2. Central finite differences for each parameter: [J(theta + eps) - J(theta - eps)] / (2*eps)
    // Check w
    {
        NetworkParameters pPlus = params;
        pPlus.w += epsilon;
        NetworkParameters pMinus = params;
        pMinus.w -= epsilon;
        res.numerical_dw = (evaluateLoss(x, y, pPlus) - evaluateLoss(x, y, pMinus)) / (2.0 * epsilon);
        res.error_dw = std::abs(res.analytical_dw - res.numerical_dw);
    }

    // Check b
    {
        NetworkParameters pPlus = params;
        pPlus.b += epsilon;
        NetworkParameters pMinus = params;
        pMinus.b -= epsilon;
        res.numerical_db = (evaluateLoss(x, y, pPlus) - evaluateLoss(x, y, pMinus)) / (2.0 * epsilon);
        res.error_db = std::abs(res.analytical_db - res.numerical_db);
    }

    // Check v
    {
        NetworkParameters pPlus = params;
        pPlus.v += epsilon;
        NetworkParameters pMinus = params;
        pMinus.v -= epsilon;
        res.numerical_dv = (evaluateLoss(x, y, pPlus) - evaluateLoss(x, y, pMinus)) / (2.0 * epsilon);
        res.error_dv = std::abs(res.analytical_dv - res.numerical_dv);
    }

    // Check c
    {
        NetworkParameters pPlus = params;
        pPlus.c += epsilon;
        NetworkParameters pMinus = params;
        pMinus.c -= epsilon;
        res.numerical_dc = (evaluateLoss(x, y, pPlus) - evaluateLoss(x, y, pMinus)) / (2.0 * epsilon);
        res.error_dc = std::abs(res.analytical_dc - res.numerical_dc);
    }

    res.maxError = std::max({res.error_dw, res.error_db, res.error_dv, res.error_dc});
    res.passed = (res.maxError <= maxTolerance);

    return res;
}

NetworkParameters NeuralNetRegression::stepGradientDescent(
    const NetworkParameters& current, const NetworkGradients& grads, double learningRate) {
    
    NetworkParameters updated;
    updated.w = current.w - learningRate * grads.dJ_dw;
    updated.b = current.b - learningRate * grads.dJ_db;
    updated.v = current.v - learningRate * grads.dJ_dv;
    updated.c = current.c - learningRate * grads.dJ_dc;
    return updated;
}

TrainingResult NeuralNetRegression::train(const TrainingConfig& config) {
    TrainingResult result;
    result.config = config;

    std::string err;
    if (!config.validate(err)) {
        result.success = false;
        result.errorMessage = err;
        return result;
    }

    result.stepHistory.reserve(config.numSteps + 1);
    result.allGradientChecksPassed = true;
    result.maxGradientCheckError = 0.0;

    NetworkParameters currentParams = config.initialParams;

    for (uint32_t step = 0; step <= config.numSteps; ++step) {
        TrainingStepRecord rec;
        rec.step = step;
        rec.params = currentParams;

        forward(config.x, currentParams, rec.z, rec.h, rec.yHat);
        rec.error = rec.yHat - config.y;
        rec.loss = computeLoss(rec.yHat, config.y);
        rec.gradients = computeGradients(config.x, config.y, currentParams);
        rec.gradCheck = checkGradients(config.x, config.y, currentParams, config.epsilon, config.maxTolerance);

        if (!rec.gradCheck.passed) {
            result.allGradientChecksPassed = false;
        }
        if (rec.gradCheck.maxError > result.maxGradientCheckError) {
            result.maxGradientCheckError = rec.gradCheck.maxError;
        }

        result.stepHistory.push_back(rec);

        if (step == 0) {
            result.initialLoss = rec.loss;
            result.initialPrediction = rec.yHat;
        }

        // Perform simultaneous gradient descent update for next step
        if (step < config.numSteps) {
            currentParams = stepGradientDescent(currentParams, rec.gradients, config.learningRate);
        } else {
            result.finalLoss = rec.loss;
            result.finalPrediction = rec.yHat;
        }
    }

    result.success = true;
    return result;
}

} // namespace workbench::neural
