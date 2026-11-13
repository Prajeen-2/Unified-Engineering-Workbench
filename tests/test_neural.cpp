#include "neural/NeuralNetRegression.hpp"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cassert>

using namespace workbench::neural;

#define NEURAL_REQUIRE(cond, msg) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + (msg)); \
        } \
    } while(0)

void testInitialForwardPassAndLoss() {
    std::cout << "[TEST 1] Testing Initial Forward Pass & Loss Evaluation (x=1, y=2)..." << std::endl;

    NetworkParameters params{0.5, 0.0, 1.0, 0.0};
    double z = 0.0, h = 0.0, yHat = 0.0;
    NeuralNetRegression::forward(1.0, params, z, h, yHat);

    // Theoretical expected values
    double expectedZ = 0.5;
    double expectedH = std::tanh(0.5); // ~0.46211715726
    double expectedYHat = 1.0 * expectedH + 0.0;
    double expectedLoss = 0.5 * (expectedYHat - 2.0) * (expectedYHat - 2.0); // ~1.1825418

    NEURAL_REQUIRE(std::abs(z - expectedZ) < 1.0e-12, "z mismatch");
    NEURAL_REQUIRE(std::abs(h - expectedH) < 1.0e-12, "h mismatch");
    NEURAL_REQUIRE(std::abs(yHat - expectedYHat) < 1.0e-12, "yHat mismatch");

    double loss = NeuralNetRegression::computeLoss(yHat, 2.0);
    NEURAL_REQUIRE(std::abs(loss - expectedLoss) < 1.0e-12, "Loss mismatch");

    std::cout << "  -> Forward pass verified: z = " << z << ", h = " << h 
              << ", yHat = " << yHat << ", Loss = " << loss << std::endl;
    std::cout << "  -> Passed: Forward propagation and loss match exact analytical values." << std::endl;
}

void testAnalyticalChainRuleDerivatives() {
    std::cout << "[TEST 2] Testing Analytical Chain-Rule Gradient Evaluation at Step 0..." << std::endl;

    NetworkParameters params{0.5, 0.0, 1.0, 0.0};
    auto grads = NeuralNetRegression::computeGradients(1.0, 2.0, params);

    // Exact theoretical derivations:
    // e = yHat - y = tanh(0.5) - 2 ~ -1.53788284274
    // dJ/dv = e * h ~ -0.710682245
    // dJ/dc = e ~ -1.537882843
    // delta = e * v * (1 - h^2) ~ -1.537882843 * 1.0 * (1 - 0.462117^2) ~ -1.209464455
    // dJ/dw = delta * x ~ -1.209464455
    // dJ/db = delta ~ -1.209464455

    double h = std::tanh(0.5);
    double e = h - 2.0;
    double expected_dv = e * h;
    double expected_dc = e;
    double delta = e * 1.0 * (1.0 - h * h);
    double expected_dw = delta * 1.0;
    double expected_db = delta;

    NEURAL_REQUIRE(std::abs(grads.dJ_dw - expected_dw) < 1.0e-12, "dJ_dw gradient mismatch");
    NEURAL_REQUIRE(std::abs(grads.dJ_db - expected_db) < 1.0e-12, "dJ_db gradient mismatch");
    NEURAL_REQUIRE(std::abs(grads.dJ_dv - expected_dv) < 1.0e-12, "dJ_dv gradient mismatch");
    NEURAL_REQUIRE(std::abs(grads.dJ_dc - expected_dc) < 1.0e-12, "dJ_dc gradient mismatch");

    std::cout << "  -> Gradients at Step 0: dJ/dw = " << grads.dJ_dw 
              << ", dJ/db = " << grads.dJ_db 
              << ", dJ/dv = " << grads.dJ_dv 
              << ", dJ/dc = " << grads.dJ_dc << std::endl;
    std::cout << "  -> Passed: Chain-rule partial derivatives verified to machine precision." << std::endl;
}

void testCentralDifferenceGradientChecking() {
    std::cout << "[TEST 3] Testing Numerical Gradient Checking (eps=1e-6, Tol <= 1e-5)..." << std::endl;

    NetworkParameters params{0.5, 0.0, 1.0, 0.0};
    auto check = NeuralNetRegression::checkGradients(1.0, 2.0, params, 1.0e-6, 1.0e-5);

    NEURAL_REQUIRE(check.passed, "Gradient check must pass at step 0");
    NEURAL_REQUIRE(check.maxError <= 1.0e-5, "Maximum gradient error exceeds tolerance of 1e-5");

    std::cout << "  -> Error dJ/dw: " << check.error_dw << " (Analytical: " << check.analytical_dw << ", Num: " << check.numerical_dw << ")" << std::endl;
    std::cout << "  -> Error dJ/db: " << check.error_db << " (Analytical: " << check.analytical_db << ", Num: " << check.numerical_db << ")" << std::endl;
    std::cout << "  -> Error dJ/dv: " << check.error_dv << " (Analytical: " << check.analytical_dv << ", Num: " << check.numerical_dv << ")" << std::endl;
    std::cout << "  -> Error dJ/dc: " << check.error_dc << " (Analytical: " << check.analytical_dc << ", Num: " << check.numerical_dc << ")" << std::endl;
    std::cout << "  -> Peak Discrepancy: " << check.maxError << " (Tol: 1e-5)" << std::endl;

    NEURAL_REQUIRE(check.maxError < 1.0e-9, "Central difference error with eps=1e-6 should be < 1e-9 due to O(eps^2) accuracy");
    std::cout << "  -> Passed: Central difference gradient check confirms exact analytical derivation." << std::endl;
}

void testSimultaneousGradientDescentUpdate() {
    std::cout << "[TEST 4] Testing Simultaneous Parameter Update Step (eta = 0.1)..." << std::endl;

    NetworkParameters params{0.5, 0.0, 1.0, 0.0};
    auto grads = NeuralNetRegression::computeGradients(1.0, 2.0, params);
    auto nextParams = NeuralNetRegression::stepGradientDescent(params, grads, 0.1);

    // Expected simultaneous update:
    // w1 = 0.5 - 0.1 * (-1.209464455) = 0.6209464455
    // b1 = 0.0 - 0.1 * (-1.209464455) = 0.1209464455
    // v1 = 1.0 - 0.1 * (-0.710682245) = 1.0710682245
    // c1 = 0.0 - 0.1 * (-1.537882843) = 0.1537882843

    double expectedW1 = 0.5 - 0.1 * grads.dJ_dw;
    double expectedB1 = 0.0 - 0.1 * grads.dJ_db;
    double expectedV1 = 1.0 - 0.1 * grads.dJ_dv;
    double expectedC1 = 0.0 - 0.1 * grads.dJ_dc;

    NEURAL_REQUIRE(std::abs(nextParams.w - expectedW1) < 1.0e-12, "w1 mismatch");
    NEURAL_REQUIRE(std::abs(nextParams.b - expectedB1) < 1.0e-12, "b1 mismatch");
    NEURAL_REQUIRE(std::abs(nextParams.v - expectedV1) < 1.0e-12, "v1 mismatch");
    NEURAL_REQUIRE(std::abs(nextParams.c - expectedC1) < 1.0e-12, "c1 mismatch");

    // Check that loss strictly decreased on step 1
    double loss0 = NeuralNetRegression::evaluateLoss(1.0, 2.0, params);
    double loss1 = NeuralNetRegression::evaluateLoss(1.0, 2.0, nextParams);
    NEURAL_REQUIRE(loss1 < loss0, "Loss must strictly decrease after gradient descent step");

    std::cout << "  -> Parameters at Step 1: w = " << nextParams.w << ", b = " << nextParams.b 
              << ", v = " << nextParams.v << ", c = " << nextParams.c << std::endl;
    std::cout << "  -> Loss: " << loss0 << " -> " << loss1 << " (Strictly Decreased)" << std::endl;
    std::cout << "  -> Passed: Simultaneous parameter update verified." << std::endl;
}

void testFullTrainingConvergence100Steps() {
    std::cout << "[TEST 5] Testing Full 100-Step Training Convergence & Gradient Verification..." << std::endl;

    TrainingConfig cfg;
    cfg.x = 1.0;
    cfg.y = 2.0;
    cfg.initialParams = {0.5, 0.0, 1.0, 0.0};
    cfg.learningRate = 0.1;
    cfg.numSteps = 100;
    cfg.epsilon = 1.0e-6;
    cfg.maxTolerance = 1.0e-5;

    auto result = NeuralNetRegression::train(cfg);
    NEURAL_REQUIRE(result.success, "Training execution failed");
    NEURAL_REQUIRE(result.stepHistory.size() == 101, "History must contain 101 records (step 0 through 100)");
    NEURAL_REQUIRE(result.allGradientChecksPassed, "All gradient checks must pass across all 100 steps");
    NEURAL_REQUIRE(result.maxGradientCheckError <= 1.0e-5, "Maximum gradient error exceeds 1e-5 tolerance");

    // Verify convergence
    std::cout << "  -> Initial Loss: " << result.initialLoss << " (yHat = " << result.initialPrediction << ")" << std::endl;
    std::cout << "  -> Final Loss:   " << result.finalLoss << " (yHat = " << result.finalPrediction << ")" << std::endl;
    std::cout << "  -> Max Gradient Check Error across 100 steps: " << result.maxGradientCheckError << " (PASS)" << std::endl;

    NEURAL_REQUIRE(result.finalLoss < 0.01, "Final loss must converge to < 0.01 after 100 steps");
    NEURAL_REQUIRE(std::abs(result.finalPrediction - 2.0) < 0.15, "Final prediction must closely approach target 2.0");

    std::string csv = result.toCsv();
    NEURAL_REQUIRE(!csv.empty(), "CSV export must not be empty");
    NEURAL_REQUIRE(csv.find("step,w,b,v,c") != std::string::npos, "CSV header mismatch");

    std::cout << "  -> Passed: Full 100-step training convergence and gradient verification confirmed." << std::endl;
}

void testDeterministicReproducibility() {
    std::cout << "[TEST 6] Testing Deterministic Run Reproducibility Across 100 Steps..." << std::endl;

    TrainingConfig cfg;
    cfg.x = 1.0;
    cfg.y = 2.0;
    cfg.initialParams = {0.5, 0.0, 1.0, 0.0};
    cfg.learningRate = 0.1;
    cfg.numSteps = 100;
    cfg.epsilon = 1.0e-6;
    cfg.maxTolerance = 1.0e-5;

    auto run1 = NeuralNetRegression::train(cfg);
    auto run2 = NeuralNetRegression::train(cfg);

    NEURAL_REQUIRE(run1.success && run2.success, "Both training runs must succeed");
    NEURAL_REQUIRE(run1.stepHistory.size() == run2.stepHistory.size(), "History size mismatch");

    for (size_t i = 0; i < run1.stepHistory.size(); ++i) {
        const auto& s1 = run1.stepHistory[i];
        const auto& s2 = run2.stepHistory[i];
        NEURAL_REQUIRE(s1.step == s2.step, "Step index mismatch");
        NEURAL_REQUIRE(std::abs(s1.params.w - s2.params.w) < 1.0e-14, "Weight w bit-mismatch");
        NEURAL_REQUIRE(std::abs(s1.params.b - s2.params.b) < 1.0e-14, "Bias b bit-mismatch");
        NEURAL_REQUIRE(std::abs(s1.params.v - s2.params.v) < 1.0e-14, "Weight v bit-mismatch");
        NEURAL_REQUIRE(std::abs(s1.params.c - s2.params.c) < 1.0e-14, "Bias c bit-mismatch");
        NEURAL_REQUIRE(std::abs(s1.loss - s2.loss) < 1.0e-14, "Loss bit-mismatch");
    }

    std::cout << "  -> Verified bit-identical parameter state across all 100 steps." << std::endl;
    std::cout << "  -> Passed: Deterministic convergence repeatability confirmed." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  UNIFIED ENGINEERING WORKBENCH - NEURAL LAB     " << std::endl;
    std::cout << "   MANUAL BACKPROPAGATION & GRADIENT CHECK TESTS  " << std::endl;
    std::cout << "==================================================" << std::endl;

    try {
        testInitialForwardPassAndLoss();
        testAnalyticalChainRuleDerivatives();
        testCentralDifferenceGradientChecking();
        testSimultaneousGradientDescentUpdate();
        testFullTrainingConvergence100Steps();
        testDeterministicReproducibility();

        std::cout << "\n>>> ALL 6/6 MANUAL BACKPROPAGATION TESTS PASSED SUCCESSFULLY! <<<" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL NEURAL TEST EXCEPTION: " << e.what() << std::endl;
        return 1;
    }
}
