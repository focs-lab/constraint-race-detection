#pragma once

#include <memory>
#include <vector>

#include "event.hpp"
#include "model_logger.hpp"
#include "trace.hpp"
#include "vec_transitive_closure.hpp"

/**
 * Abstract base class for race detection models.
 * This interface defines the common API that all race detection models must
 * implement.
 */
class Model {
   protected:
    Trace& trace_;
    ModelLogger& logger_;
    bool log_witness_;
    VecTransitiveClosure& hb_closure_;

   public:
    Model(Trace& trace, VecTransitiveClosure& vc_hb, ModelLogger& logger,
          bool log_witness)
        : trace_(trace),
          logger_(logger),
          log_witness_(log_witness),
          hb_closure_(vc_hb) {}

    virtual ~Model() = default;

    /**
     * Factory method to create the appropriate model based on type.
     */
    static std::unique_ptr<Model> createModel(const std::string& model_type,
                                              Trace& trace,
                                              VecTransitiveClosure& vc_hb,
                                              ModelLogger& logger,
                                              bool log_witness);

    /**
     * Solves the race detection problem using the model's specific approach.
     *
     * @param maxCOPCheck Maximum number of Conflicting operation pairs to check
     * @param maxRaceCheck Maximum number of races to detect
     * @return Number of races detected
     */
    virtual uint32_t solve(uint32_t maxCOPCheck, uint32_t maxRaceCheck) = 0;
};