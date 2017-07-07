#include "gtest/gtest.h"
#include "remb_processor.h"
#include "stream_service/orbit/transport_delegate.h"

namespace orbit {
namespace {

class RembProcessorTest : public testing::Test {
 protected:
  virtual void SetUp() override {
  }

  virtual void TearDown() override {
  }
};

TEST_F(RembProcessorTest, correctRembProcessorTestDefaults) {
  orbit::TransportDelegate *trans_delegate;
  orbit::RembProcessor remb_processor(trans_delegate);

}

}
} /* namespace orbit */
