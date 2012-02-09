#include "sdm/basics.hpp"

#include <gtest/gtest.h>

#include <iostream>
#include <stdexcept>

#include <np-divs/div-funcs/div_l2.hpp>
#include <np-divs/div_params.hpp>

#include "sdm/kernel_projection.hpp"
#include "sdm/kernels/gaussian.hpp"
#include "sdm/sdm.hpp"

#include <svm.h>

namespace {

using namespace sdm;
using namespace std;

TEST(UtilitiesTest, PSDProjection) {
    double mat[25] = {
        0.8404, -0.6003, -2.1384,  0.1240,  2.9080,
       -0.8880,  0.4900, -0.8396,  1.4367,  0.8252,
        0.1001,  0.7394,  1.3546, -1.9609,  1.3790,
       -0.5445,  1.7119, -1.0722, -0.1977, -1.0582,
        0.3035, -0.1941,  0.9610, -1.2078, -0.4686
    };
    double psd[25] = {
        1.5021, -0.4976, -0.5390, -0.2882,  0.7051,
       -0.4976,  1.0476, -0.1796,  0.7752, -0.2417,
       -0.5390, -0.1796,  1.9070, -1.0633,  0.6634,
       -0.2882,  0.7752, -1.0633,  1.0847, -0.6603,
        0.7051, -0.2417,  0.6634, -0.6603,  0.8627
    };
    project_to_symmetric_psd(mat, 5);

    for (size_t i = 0; i < 25; i++)
        EXPECT_NEAR(mat[i], psd[i], 1e-4) << "i = " << i;
}

TEST(UtilitiesTest, CovarianceProjection) {
    double mat[25] = {
        0.8404, -0.6003, -2.1384,  0.1240,  2.9080,
       -0.8880,  0.4900, -0.8396,  1.4367,  0.8252,
        0.1001,  0.7394,  1.3546, -1.9609,  1.3790,
       -0.5445,  1.7119, -1.0722, -0.1977, -1.0582,
        0.3035, -0.1941,  0.9610, -1.2078, -0.4686
    };
    double kernel[25] = {
        1.0000, -0.4095, -0.2387, -0.2668,  0.6339,
       -0.4095,  1.0000, -0.2026,  0.7296, -0.2502,
       -0.2387, -0.2026,  1.0000, -0.7841,  0.5518,
       -0.2668,  0.7296, -0.7841,  1.0000, -0.6903,
        0.6339, -0.2502,  0.5518, -0.6903,  1.0000
    };
    project_to_covariance(mat, 5);

    for (size_t i = 0; i < 25; i++)
        EXPECT_NEAR(mat[i], kernel[i], 1e-4) << "i = " << i;
}

#define NUM_TRAIN 10
#define TRAIN_SIZE 10
#define NUM_TEST 2
class SmallSDMTest : public ::testing::Test {
    protected:

    typedef flann::Matrix<double> Matrix;

    size_t num_train;
    double train_raw[NUM_TRAIN][TRAIN_SIZE];
    Matrix train[NUM_TRAIN];
    std::vector<int> labels;

    size_t num_test;
    double **test_raw;
    Matrix test[NUM_TEST];
    std::vector<int> test_labels;

    NPDivs::DivParams div_params;
    svm_parameter svm_params;

    SmallSDMTest() :
        num_train(NUM_TRAIN),
        num_test(NUM_TEST),
        div_params(),
        svm_params(default_svm_params)
    {
        double train_bleh[NUM_TRAIN][TRAIN_SIZE] = {
            // 10 samples from N(0, 0)
            { 0.5377, 1.8339,-2.2588, 0.8622, 0.3188,-1.3077,-0.4336, 0.3426, 3.5784, 2.7694},
            { 0.8884,-1.1471,-1.0689,-0.8095,-2.9443, 1.4384, 0.3252,-0.7549, 1.3703,-1.7115},
            {-0.1022,-0.2414, 0.3192, 0.3129,-0.8649,-0.0301,-0.1649, 0.6277, 1.0933, 1.1093},
            {-0.8637, 0.0774,-1.2141,-1.1135,-0.0068, 1.5326,-0.7697, 0.3714,-0.2256, 1.1174},
            {-1.0891, 0.0326, 0.5525, 1.1006, 1.5442, 0.0859,-1.4916,-0.7423,-1.0616, 2.3505},

            // 10 samples from N(.5, .5)
            { 1.2097, 0.6458, 0.5989, 1.2938, 0.0978, 0.8483, 0.9175, 0.3781, 0.6078,-0.0829},
            {-0.0740, 0.5524, 0.8611, 1.7927, 0.1666, 0.5937, 0.4588,-0.4665, 0.2805,-0.3973},
            { 0.9202, 0.0560, 0.5500, 0.2277, 0.6518, 0.1998, 0.7450, 0.8697, 1.3559, 0.4029},
            {-0.5692, 0.0802, 1.1773,-0.0361, 0.9805, 0.5620, 1.2183,-0.4804, 0.4012,-0.1039},
            { 1.9540, 0.9126, 1.1895,-0.0291, 0.2657, 0.3638, 1.0492, 0.3611, 0.8508,-0.5259}
        };

        for (size_t i = 0; i < NUM_TRAIN; i++) {
            std::copy(train_bleh[i], &train_bleh[i][TRAIN_SIZE], train_raw[i]);
            train[i] = Matrix(train_raw[i], TRAIN_SIZE, 1);
        }

        labels.resize(NUM_TRAIN);
        for (size_t i = 0; i < 5; i++) {
            labels[i] = 0;
            labels[i+5] = 1;
        }


        test_labels.resize(NUM_TEST);

        test_raw = new double*[NUM_TEST];

        // 10 from N(0, 1)
        double test1[10] = {-1.3499, 3.0349, 0.7254,-0.0631, 0.7147,-0.2050,-0.1241, 1.4897, 1.4090, 1.4172};
        test_raw[0] = new double[10]; std::copy(test1, test1+10, test_raw[0]);
        test[0] = Matrix(test_raw[0], 10, 1);
        test_labels[0] = 0;

        // 5 from N(.5, .5)
        double test2[5] = { 0.8357,-0.1037, 0.8586, 1.3151, 0.7444 };
        test_raw[1] = new double[5]; std::copy(test2, test2+5, test_raw[1]);
        test[1] = Matrix(test_raw[1], 5, 1);
        test_labels[1] = 1;
    }

    ~SmallSDMTest() {
        for (size_t i = 0; i < NUM_TEST; i++)
            delete[] test_raw[i];
        delete[] test_raw;
    }
};

TEST_F(SmallSDMTest, BasicTrainingTesting) {
    typedef flann::Matrix<double> Matrix;

    // set up parameters
    NPDivs::DivL2 div_func;

    std::vector<double> sigs(1, .00671082);
    GaussianKernelGroup kernel_group(sigs, false);

    std::vector<double> cs(1, 1./512.);

    svm_params.C = 1./512.;
    svm_params.probability = 0;

    // train a model
    SDM<double> *model =
        train_sdm(train, num_train, labels, div_func, kernel_group, div_params,
                cs, svm_params);

    // predict on the test data
    vector< vector<double> > vals(2);
    vector<int> pred_lab = model->predict(test, 2, vals);

    for (size_t i = 0; i < num_test; i++) {
        EXPECT_EQ(test_labels[i], pred_lab[i]);
        EXPECT_EQ(0, vals[i][1]);
    }

    EXPECT_NEAR(.0022, vals[0][0], .0005);
    EXPECT_NEAR(-.0056, vals[1][0], .0005);

    model->destroyModelAndProb();
    delete model;
}

} // end namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
