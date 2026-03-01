#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <vector>

enum class AgentRole
{
  VisualUnderstanding,
  Physics,
  Rigging,
  RenderExecution,
  BlenderBridge,
};

struct AgentNode
{
  AgentRole role = AgentRole::VisualUnderstanding;
  std::string capabilities;
  std::string lastAction;
  bool online = true;
  float latencyMs = 0.0f;
};

struct RenderJob
{
  std::string objPath;
  bool requiresPhysicsBake = true;
  bool requiresRigging = true;
  bool dispatchToRuntime = true;
};

struct AgentOrchestrator
{
  std::vector<AgentNode> agents;
  std::deque<std::string> eventLog;
  size_t maxLogEntries = 14;
  bool initialized = false;

  void logEvent(const std::string& event);
  AgentNode* find(AgentRole role);
  bool ensureOnline(AgentRole role, const std::string& missingMessage);
  void bootstrap();
  bool submit(const RenderJob& job);
};

const char* agentRoleName(AgentRole role);
