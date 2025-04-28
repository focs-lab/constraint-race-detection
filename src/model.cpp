#include "model.hpp"

#include "rv_predict_model.hpp"
#include "naive_model.hpp"
std::unique_ptr<Model> Model::createModel(const std::string& modelType,
                                          Trace& trace,
                                          VecTransitiveClosure& vc_hb,
                                          ModelLogger& logger,
                                          bool log_witness) {
    if (modelType == "rv_predict") {
        return std::make_unique<RVPredictModel>(trace, vc_hb, logger,
                                             log_witness);
    } else if (modelType == "naive") {
        return std::make_unique<NaiveModel>(trace, vc_hb, logger, log_witness);
    } else {
        throw std::runtime_error("Unknown model type: " + modelType);
    }
}