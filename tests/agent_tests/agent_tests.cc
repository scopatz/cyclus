#include "agent_tests.h"

#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "infile_tree.h"
#include "xml_parser.h"

/// this function should be called by all most derived agents in order to get
/// access to the functionality of the agent unit test library
extern int ConnectAgentTests() { return 0; }

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_P(AgentTests, Clone) {
  cyclus::Agent* clone = agent_->Clone();
  delete clone;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_P(AgentTests, Print) {
  std::string s = agent_->str();
  EXPECT_NO_THROW(std::string s = agent_->str());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_P(AgentTests, Schema) {
  std::stringstream schema;
  schema << ("<element name=\"foo\">\n");
  schema << agent_->schema();
  schema << "</element>\n";
  cyclus::XMLParser p;
  EXPECT_NO_THROW(p.Init(schema));
}

TEST_P(AgentTests, Annotations) {
  Json::Value anno = agent_->annotations();
  EXPECT_TRUE(anno.isObject());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
TEST_P(AgentTests, GetAgentType) {
  EXPECT_NE(std::string("Agent"), agent_->kind());
}
